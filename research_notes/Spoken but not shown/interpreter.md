# Small local "interpreter" (intent -> markup): research notes
Date: 2026-10-22. Labels: [V] = I read the numbers in the source (abstract, model card or table). [S] = secondary or self-reported. [E] = my own estimate.

## 1. Fine-tuning small LMs for NL -> DSL
- OmniSQL-7B (VLDB 2025). Trained on 2.54M synthetic samples over 16.6k synthetic DBs, plus Spider/BIRD train. Spider dev 81.2 vs GPT-4o 70.9; Spider test 87.9 vs 83.2; BIRD dev 63.9 vs 61.9 [V]. Data-size ablation (20-100%): performance "consistently improves", still rising at 100%. This is a broad task (any schema), which is why it needs millions of samples. https://arxiv.org/html/2503.02240v2
- SQLCoder-7B (Defog). Beats GPT-4 only when fine-tuned on a given schema. On novel schemas GPT-4 wins 82 vs 71 [S]. Lesson: narrow-domain fine-tunes win in-domain and lose off-distribution. https://huggingface.co/CHaerem/wasm-sqcoder (mirror of the model card)
- 2-stage decomposed 7B text-to-SQL: "comparable to GPT-4 few-shot" [S]. https://arxiv.org/html/2402.01117v1
- Text2Cypher. CYQUARK (2026) synthetic grounded data: a 4B model goes from 37.9 to 76.2 avg; a 0.8B model reaches 73.3; Claude/GPT-5 score about 78.5 [S, abstract]. https://arxiv.org/pdf/2606.14325 . Neo4j's 2024 dataset paper warns that one fine-tuned model's score was inflated by about 14% test leakage. https://arxiv.org/pdf/2412.10064 , https://neo4j.com/blog/developer/benchmarking-neo4j-text2cypher-dataset/
- Production DSL (2026). Fine-tuned Mistral-class SLMs on NL/DSL pairs beat larger models on accuracy and latency, and further customer-specific tuning did not hurt them [S, abstract only, no numbers]. https://arxiv.org/abs/2604.09952
- Domain text-to-code comparison (2026). Prompting/RAG improves style but barely moves accuracy. LoRA gives the best accuracy. https://arxiv.org/abs/2603.16526
- Robotics DSLs. Robo-Instruct (2024): small open models fine-tuned on verifier-filtered synthetic programs "match or surpass" GPT-3.5/Gemini-Pro. https://arxiv.org/abs/2405.20179 . CLAIRify notes that NL->DSL datasets are hard to get and uses a verifier loop instead. https://arxiv.org/pdf/2303.14100
- **Closest analogue to our problem.** scenesmith-qwen3-4b is a LoRA on 4-bit Qwen3-4B, trained on a 16 GB Mac: 851 train examples, all render-verified, 60% synthetic from 12 parameterized scene families. Results [V]:
  - Render pass 14% -> 63% overall; in-distribution held-out 10% -> 81%.
  - House style 0/43 -> 39-43/43.
  - **Novel concepts 25% -> 17%: worse.** It invents nonexistent APIs.
  - Peak memory 5.5 GB; 650 iterations.
  - https://huggingface.co/albertobarnabo/scenesmith-qwen3-4b
- Diagram DSL fine-tunes:
  - MermaidMistral 7B / MermaidSolar: about 800 examples; "beats GPT-4" is self-claimed [S]. https://featherless.ai/models/TroyDoesAI/MermaidMistral
  - Gemma-12B TikZ LoRA: about 95k examples [S]. https://featherless.ai/models/Praha-Labs/Gemma-4-12B-TikZ-LoRA
  - Manim 7B: 2.4k examples, no metrics.
  - Zero-shot Manim is hard: in ManiBench, Qwen2.5-Coder-32B renders 0/12. https://arxiv.org/pdf/2603.13251
- How much data:
  - Baseten (Oct 2025), Qwen3-4B LoRA at 1k/5k/10k/30k examples: quality scales log-linearly with data; LoRA matches full fine-tuning except at very low rank. https://www.baseten.co/research/practical-lora-research/
  - Format/style usually locks in at a few hundred to about 1k examples (scenesmith: 851).
  - Semantic coverage (choosing the right idiom for an unseen concept) keeps improving to 10k+ and is the part that generalizes worst.

## 2. Teacher + verifier synthetic data
- This works and is standard practice:
  - Llama 3 used rejection sampling against execution feedback.
  - OmniSQL, CYQUARK and Robo-Instruct all filter synthetic pairs with an executor or verifier.
  - Survey: https://arxiv.org/pdf/2503.14023
  - Practitioner notes: https://rlhfbook.com/teach/course/lec7-chap12-synthetic-data/
- Pitfalls:
  - **Diversity collapse.** Teacher outputs pile up on their high-probability modes and the long tail disappears. Mitigations: multi-source generation, seeded diversity (sample topics, verbs and structures explicitly), and deduplication. https://tianpan.co/blog/2026-04-12-synthetic-data-pipelines-that-dont-collapse ; strong model collapse, ICLR 2025: https://arxiv.org/html/2410.04840v2
  - **Teacher errors are copied.** A verifier only catches errors it can see. `--check` catches syntax and reference errors. It does NOT catch "narrated X, drew nothing about X", which is exactly the current ~50% failure. The judge must score coverage (every claim in the intent has an object, relation or cue), or the student learns Claude's omissions.
  - **Filter bias.** Rejection sampling keeps the easy cases. The student then under-draws on hard intents (scenesmith novel-concept drop).
  - **Very small models** (<1B) are the most fragile under synthetic data (same ICLR 2025 paper).
- Cheap trick: generate intents BACKWARDS. Take good existing markup beats and have Claude write the vague, plain-language intent they realize. This gives realistic vague inputs paired with complete outputs, and it directly trains "expand a vague intent into full markup".

## 3. Constrained decoding locally
- **llama.cpp GBNF** (`grammar` field in llama-server; also `--grammar-file`): arbitrary CFGs, so a line-based DSL works. It guarantees every emitted string parses under the grammar. It cannot guarantee semantics: ids that exist, labels matching the narration, cues matching `|cue|` words. Dynamic id sets can be injected by regenerating the grammar per beat (alternation of the current ids). https://github.com/ggml-org/llama.cpp/blob/master/grammars/README.md
- **Ollama** structured outputs accept a JSON schema only, not a raw GBNF. To use our DSL we would call llama-server directly, or wrap lines in JSON (wasteful).
- **XGrammar** (EBNF, pushdown automaton, near-zero per-token overhead; default in vLLM/SGLang). https://arxiv.org/pdf/2411.15100
- **Outlines**: regex/CFG, has an MLX backend.
- Overhead:
  - Overhead is small for simple grammars at batch 1. Heavy-batch servers see up to about 37% extra latency (Pre^3, batch 512). https://arxiv.org/pdf/2506.03887
  - Older GBNF builds had slow cases with large alternations and deep nesting [S]. Keep the grammar flat (one rule per verb).
  - JSONSchemaBench compares frameworks including llama.cpp: https://arxiv.org/html/2501.10868v2
- Quality caveats:
  - Grammar masking distorts the distribution (Grammar-Aligned Decoding, NeurIPS 2024). https://arxiv.org/abs/2405.21047
  - Strict formats can hurt reasoning (Let Me Speak Freely, EMNLP 2024). https://arxiv.org/pdf/2408.02442
  - StructureBench (IJCAI 2026): constraints guarantee syntax but do not raise semantic accuracy, and can hurt small models on complex grammars. https://www.ijcai.org/proceedings/2026/0267.pdf
- Verdict: a fine-tuned model rarely needs the grammar (scenesmith fixed its formatting by training alone). Use the grammar as a cheap safety net plus `--check`. It does not solve completeness.

## 4. LoRA on Apple Silicon; inference speed
- MLX LoRA reference rates:
  - Mistral-7B at about 250 tok/s train on M1 Max 32 GB; Llama-7B at about 475 tok/s on M2 Ultra [V, mlx-examples]. https://github.com/ml-explore/mlx-examples/blob/main/lora
  - Qwen2.5-Coder-3B LoRA: 28k iterations, 17 h, 11 GB peak (machine unstated). https://huggingface.co/nicholashat/qwen2.5-coder-3b-oss-instruct-lora
  - 4B 4-bit QLoRA peaks at about 5.5 GB (scenesmith).
- M5 vs M4 (Apple, MLX, 24 GB machines, 4096-token prompt) [V]:
  - Time to first token is 3.6-4.1x faster (Qwen3-8B-4bit: 3.97x), from the GPU neural accelerators.
  - Decode is only 1.24x faster (bandwidth 153 vs 120 GB/s).
  - Qwen3-8B-4bit uses 5.6 GB.
  - Prefill gains need MLX (or llama.cpp builds with Metal tensor support) on macOS 26.2+.
  - https://machinelearning.apple.com/research/exploring-llms-mlx-m5
- llama.cpp, M4 base (10 GPU cores), Llama-7B Q4_0: pp512 = 221 t/s, tg128 = 24 t/s [V]. No M5 base row yet. https://github.com/ggml-org/llama.cpp/discussions/4167
- [E] M5 base, MLX 4-bit:

  | Model | Prefill | Decode |
  |---|---|---|
  | 8B | ~700-900 t/s | ~28-32 t/s |
  | 4B | ~1.5-2k t/s | ~55-65 t/s |
  | 1.7B | ~4k t/s | ~110+ t/s |

- [E] Training cost on M5 32 GB, about 5k examples x about 700 tokens = 3.5M tokens per epoch:
  - 4B 4-bit: about 500-900 tok/s, so 1-2 h per epoch and about 3-6 h for 3 epochs (overnight).
  - 8B: about 2x that, 8-12 h.
  - Memory is fine either way (under 16 GB with grad-checkpoint and batch 1-4).

## 5. Text-to-vis / diagram / relation -> visual idiom
- DataVisT5 (2024): T5 fine-tune beats larger LLMs on nvBench NL2VIS [S]. https://arxiv.org/html/2408.07401v2
- "Plots Made Quickly" (LREC-COLING 2024): small T5 models hit SOTA on nvBench at real-time latency, while LLMs miss interactive latency. https://preview.aclanthology.org/setup/2024.lrec-main.1119
- A counterpoint shows few-shot LLMs sometimes beat fine-tunes. https://api.crossref.org/works/10.1145%2F3654992
- DiagrammerGPT (COLM 2024): LLM plans entities + relations + boxes, an auditor loop refines, a separate renderer draws. Planning made the diagrams more accurate. https://arxiv.org/abs/2310.12128
- Text2Vis (2025): benchmark plus agentic refinement from feedback. https://arxiv.org/pdf/2507.19969
- Visual metaphor work maps verbs/metaphors to images: "I Spy a Metaphor" (2023) uses an LLM elaboration then diffusion; "Mind's Eye" (NeurIPS 2025) is reward-based. https://arxiv.org/abs/2305.14724
- **Gap.** I found no paper that learns "talks to" -> arrow exchange, "doubles" -> morph, "cancels" -> strike as a mapping. NL4DV, Draco and VizML learn data-attribute -> encoding, not verb -> motion idiom. In practice this mapping is a curated lexicon or few-shot table, and it is the part a small model generalizes worst (scenesmith novel-concept drop). Best guess: hand-write the verb -> idiom table (it is small) and train the model to apply it.

## 6. Split planner / executor evidence
- Code2Video (NeurIPS 2025): Planner (storyboard) -> Coder (Manim, with scoped auto-fix) -> Critic (VLM layout). About 40% better than direct code generation. Best coder was Claude Opus 4; the coder is a big model. https://arxiv.org/pdf/2510.01174
- TheoremExplainAgent (ACL 2025): planner + coder with up to 5 fixes; agentic planning is "essential"; o3-mini 93.8% success. Most videos still have layout issues. https://aclanthology.org/2025.acl-long.332
- Plan-and-Act (ICML 2025): separate planner and executor, planner trained on synthetic plans; SOTA on WebArena-Lite at 57.6%. https://arxiv.org/html/2503.09572v3
- ALGOGEN (2026): verifiable traces + repair, Pass@1 91% -> 99.5% (large models). https://arxiv.org/pdf/2605.12159
- Caveat: in all of these the executor is as large as, or larger than, the planner. **I found no evidence of a split where a small executor is the one that raises completeness.** The gains come from explicit planning (an enumerated storyboard), not from a small executor.
- Interface failure modes (reported or inferred):
  - Plan too vague, so the executor invents or under-draws.
  - Executor drops items it cannot express.
  - Executor adds clutter (our earlier finding).
  - Planner and executor disagree on ids, so later beats reference objects that do not exist.
  - Timing: cues must match spoken words the executor never saw verbatim.
- Mitigation: the intent must be an ENUMERATED claim list ("show: client box; server box; arrow SYN client->server; cue on 'answers'"), not prose. At that point it is nearly markup, which raises the question of why Claude does not just write it.

## Verdict
Feasible, but it fixes a different problem than ~50% coverage, and only if the intent is structured. Details in the summary.
