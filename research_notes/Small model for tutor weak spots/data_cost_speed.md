# Small model for the tutor: data sources, cleaning effort, training cost, inference speed

Research date: 2026-10-21. Note on method: direct page fetches to github.com, docs.anthropic.com and together.ai were blocked from this environment, so several numbers come from search-result summaries of secondary sources (flagged). Claude pricing comes from the bundled Claude API skill reference (cached 2026-10-06), which is first-party.

## 1. Existing data that could be converted to the DSL or used for training/taste

### Takeaway
There is no ready (question -> semantic scene) corpus anywhere. The closest raw material is small: the 3b1b/videos repo (manimgl, often broken against current manim, one author), a few thousand prompt->manim pairs on Hugging Face (3k-ish, quality unknown), and big but off-target diagram corpora (DaTikZ 120k+ TikZ figures; AI2D ~4.9k, ScienceQA ~10k images, TQA ~3.5k diagrams). All of it needs an LLM translation pass into the DSL plus the existing checker as a filter. Treat these as seed/inspiration, not as the training set.

### Cited Findings
- 3b1b/videos is "code for the manim-generated scenes used in 3blue1brown videos"; it targets 3b1b's own manimgl, not Manim Community, and "older projects may have code dependent on older versions of manim, and so may not run out of the box"; to run an old project you check out the commit that completed it. Licence is CC BY-NC-SA 4.0 (irrelevant here: personal use). — [ossinsight 3b1b/videos](https://ossinsight.io/analyze/3b1b/videos); [mirror README](https://github.com/romitg2/videos)
- A Hugging Face scrape of the 3b1b code exists: pravsels/manim_3b1b_code (row count not shown in results). The same uploader has pravsels/manim_ManimCommunity_code, under 1K rows. — [manim_3b1b_code](https://huggingface.co/datasets/pravsels/manim_3b1b_code/viewer/default/train?p=0); [manim_ManimCommunity_code](https://huggingface.co/datasets/pravsels/manim_ManimCommunity_code/tree/main)
- Hugging Face prompt->manim datasets are small: BechusRantus/manim-code-dataset has 3,145 train rows with fields prompt, category, final code, attempts count; thanhkt/manim_code is 1K-10K rows (~2.4 MB parquet); generaleoley/manim-codegen ~2.3 MB JSON; RY7Games/ry_manim_codegen ~8 MB JSONL; jensinjames/MAD_data card says it is empty. — [BechusRantus](https://huggingface.co/datasets/BechusRantus/manim-code-dataset/blob/main/README.md); [thanhkt](https://huggingface.co/datasets/thanhkt/manim_code/tree/main/data); [generaleoley](https://www.huggingface.co/datasets/generaleoley/manim-codegen/tree/main); [RY7Games](https://huggingface.co/datasets/RY7Games/ry_manim_codegen/blob/main/ry_data.jsonl); [MAD_data](https://huggingface.co/datasets/jensinjames/MAD_data)
- TheoremExplainBench (Feb 2025): 240 theorems across STEM fields, 5 automatic metrics. Planner agent + coding agent that writes Manim. o3-mini agent: 93.8% success rate, overall score 0.77. "Most generated videos had minor problems with visual element layout." — [arXiv 2502.19400](https://arxiv.org/abs/2502.19400) (2025)
- DaTikZ (AutomaTikZ, 2023): "120k TikZ drawings aligned with captions". The Hub copies (nllg/datikz, HuggingFaceM4/datikz) are tagged 10K-100K rows; HuggingFaceM4 copy is ~620 MB parquet. DaTikZ-V4 (TikZilla, ICLR 2026) is "roughly four times the size" of DaTikZ-V3, with LLM-generated figure descriptions; exact count not in results. — [AutomaTikZ summary](https://sotaverified.org/papers/automatikz-text-guided-synthesis-of); [HuggingFaceM4/datikz](https://www.huggingface.co/datasets/HuggingFaceM4/datikz/tree/main); [TikZilla arXiv 2603.03072](https://arxiv.org/abs/2603.03072); [TikZilla collection](https://huggingface.co/collections/nllg/tikzilla)
- AI2D: 4,903 diagrams, 4,563 questions, 4,903 annotations (AWS registry says 4,817 diagrams; conflict). — [AI2 diagram understanding](https://prior.allenai.org/projects/diagram-understanding); [AWS registry](https://registry.opendata.aws/collab/ai2/)
- ScienceQA: 21,208 questions, 10,332 images. TQA (per ScienceQA comparison table): 26,260 questions, 3,455 diagram images. — [ScienceQA paper arXiv 2209.09513](https://arxiv.org/pdf/2209.09513)
- Code translation to a low-resource language: MultiPL-T translated Python into Julia, Lua, OCaml, R, Racket with a code LLM and kept only translations that passed synthesized tests, producing "tens of thousands of validated training items"; the authors note many raw translations are wrong before filtering. — [arXiv 2308.09895](https://arxiv.org/abs/2308.09895) (OOPSLA 2024)
- An ICPC 2025 study on R and Racket found fine-tuning was generally the strongest option for smaller models, capped by how much training data exists. — [arXiv 2501.19085](https://arxiv.org/html/2501.19085v1)

### Inferences
- Manim -> DSL is not code-to-code translation in the usual sense: manim is coordinate-based imperative code and the DSL is semantic with no coordinates. An LLM has to recover intent ("this circle is the unit circle, this arrow links A to B") and drop positions. Expect it to work for simple scenes and lose a lot on 3b1b's hand-tuned ones. The MultiPL-T pattern (translate, then keep only what passes an automatic check) maps directly onto our parser + lints + layout score.
- 3b1b's repo has no question per scene. A question would have to be written by Claude from the scene and the voiceover (e.g. YouTube transcript), so each pair is two LLM steps away from the source.
- The best value of the 3b1b corpus is probably as taste reference (what gets shown when, how many objects, pacing) and as few-shot exemplars, not as bulk SFT data.
- DaTikZ/AI2D/TQA give static diagrams, not animated beats with speech cues. They could train a vision critic ("is this a clean diagram?") or supply question topics, but not the scene format.
- Rough conversion cost: translating ~400 3b1b files with Sonnet 5.5 at, say, 20k tokens in / 3k out each is about 400 x ($0.04 + $0.03), around $30 (assumed token counts, not measured).

### Gaps
- Exact row counts for pravsels/manim_3b1b_code, DaTikZ-V4, and the RY7Games set: not shown in results.
- The 3b1b/videos file count (~400 per the brief) could not be checked because GitHub fetches were blocked.
- No published success rate for LLM translation of manim into a different animation DSL was found. YouTube transcript datasets for Khan Academy or Organic Chemistry Tutor were not searched (call budget).

## 2. Synthetic (question -> scene) data: yields, cost, how many examples

### Takeaway
The cheapest realistic path is to have Claude generate scenes, filter them with the existing checker and a judge, and fine-tune on a few hundred to a few thousand survivors. At October 2026 prices, Sonnet 5.5 generation should cost about $20-40 per 1k raw samples. With a 30-50% keep rate, that is roughly $50-130 per 1k kept. Haiku 5.5 is about 20x cheaper. LIMA-style evidence says 1k carefully chosen examples can be enough for style and format.

### Cited Findings
- Claude list prices per MTok in/out (first-party, cached 2026-10-06): Opus 5.5 $4/$20 (cache reads $0.20); Sonnet 5.5 $2/$10 (cache reads $0.20); Haiku 5.5 $0.10/$0.50 (prompts up to 100K tokens); Haiku 4.5 $1/$5; Sonnet 4.6 $3/$15; Opus 5 $5/$25; Fable 5.1 $10/$50. — Claude API skill reference (Anthropic, cached 2026-10-06); secondary confirmation of older rows at [finout](https://finout.io/blog/anthropic-api-pricing), [morphllm](https://www.morphllm.com/anthropic-claude-api-pricing)
- Batch API is 50% off, results within 24h (e.g. Haiku 4.5 batch $0.50/$2.50; Sonnet 4.6 batch $1.50/$7.50; Opus batch $2.50/$12.50). — [finout](https://finout.io/blog/claude-pricing-in-2026-for-individuals-organizations-and-developers); [cut-the-saas](https://cut-the-saas.com/the-cut/claude-api-pricing) (secondary)
- LIMA: a 65B LLaMa fine-tuned on 1,000 curated prompt/response pairs, with no RLHF, was competitive with GPT-4 in human preference tests. The authors note that writing such examples takes real effort and that the model is less robust. — [arXiv 2305.11206](https://arxiv.org/pdf/2305.11206) (2023, older)
- MultiPL-T: filter-by-tests synthetic translation gave tens of thousands of items and improved small code models on MultiPL-E. — [arXiv 2308.09895](https://arxiv.org/abs/2308.09895)
- TheoremExplainAgent's o3-mini pipeline succeeded 93.8% of the time at producing a runnable video, but layout was the common flaw. So "it runs" is easy and "it's well laid out" is the filter that matters. — [arXiv 2502.19400](https://arxiv.org/abs/2502.19400)

### Inferences
- Cost per raw sample (assumptions: 6k-token system prompt with the DSL spec, cached; 300-token question; ~600-token scene plus ~1.5k thinking/output):
  - Sonnet 5.5: input ~$0.001-0.012 depending on cache hits, output 2.1k x $10/M ≈ $0.021. About $0.025-0.035 per sample, $25-35 per 1k, about half that with batch (whether the batch discount stacks with cache-read pricing was not confirmed).
  - Opus 5.5: about 2x Sonnet, $50-70 per 1k.
  - Haiku 5.5: about $1-2 per 1k.
  - Judge pass (scene + rendered frame, ~3k in / 300 out on Sonnet 5.5): ~$0.01 per sample, $10 per 1k.
- Yield: no published yield for this task. The checker already exists, so measure it: run 100 questions x best-of-4 and count how many pass parse + lints + layout threshold + judge. A plausible prior is 30-60% kept for Sonnet-class output. With best-of-4 and keep-the-best, nearly every question gets one usable scene for 4x the generation cost.
- Size target: 500-2,000 high-quality pairs for a LoRA on a 3-8B model, in line with LIMA (1k) and typical few-thousand DSL fine-tunes. Total synthetic budget is roughly $100-300 on Sonnet 5.5 including judging, which is small next to the user's time.
- Distillation caveat: the small model learns Claude's average scene, not better ones, unless the filter selects the top fraction. Best-of-n plus judge acts as rejection sampling and can push the student above Claude's mean.

### Gaps
- No published numbers for "how many examples a DSL LoRA needs" specific to visual or scene DSLs. Text-to-SQL 1k-vs-10k comparisons were searched for but not found.
- The Anthropic pricing page could not be fetched directly to confirm batch and cache stacking.

## 3. Preference / taste data: how many labels for a reward model or aesthetic scorer

### Takeaway
Public reward models use 100k+ labels (ImageReward 137k comparisons; LAION aesthetic ~400k ratings). Personalised work relies on shared pretrained components so that each user needs far fewer labels, but no source gave a solid "N labels is enough" figure. For one person, the practical route is a frozen embedding (SigLIP/CLIP or the judge's scores as features) with a small head trained on a few hundred of the user's own pairwise picks. Claude-judge pre-labels and active learning should spend those picks on the disagreements.

### Cited Findings
- ImageReward: 137k expert comparisons, prompts from DiffusionDB. — [arXiv 2304.05977](https://arxiv.org/abs/2304.05977v1); [PyPI image-reward](https://pypi.org/project/image-reward/0.94) (2023)
- LAION aesthetic predictor: a small MLP on CLIP image embeddings, rating 1-10, trained on ~400k image-rating pairs (AVA, Simulacra Aesthetic Captions, LAION-logos). — [LAION blog](https://laion.ai/blog/laion-stable-horde/)
- PAMELA personalised dataset: ~70,000 ratings over 5,000 images, 15 raters per image, trained jointly with existing aesthetic subsets. — [arXiv 2511.19458](https://arxiv.org/html/2511.19458v1)
- 2026 personalised T2I reward paper: user-level SROCC 0.4514 vs HPSv3 0.4019 on unseen users. Gains are real but modest even with personalisation. — [arXiv 2604.07427](https://arxiv.org/pdf/2604.07427)
- Low-data personalisation methods (variational preference learning, low-rank reward models) represent users as combinations of shared components. — [arXiv 2602.00394](https://arxiv.org/html/2602.00394v1)

### Inferences
- A linear or small-MLP head on frozen embeddings, like the LAION predictor, is the right shape for a personal critic. With a few hundred pairs it can learn coarse preferences (clutter, balance, too much text) but not subtle taste. Reported personalised gains are modest even at scale.
- The cheapest labels come from what the user already sees: each time they reject or redo a scene, that is a free pairwise label.
- Workflow: have the Claude judge pre-score candidate pairs, and show the user only pairs where the judge is uncertain or where the judge and geometric score disagree. Active learning like this is the standard way to cut label counts, though no source here gave a number for this setting.
- HPSv2 (~798k) and Pick-a-Pic (~500k) sizes from the brief were not confirmed in this search. Treat them as unverified.

### Gaps
- No source gave a measured "labels needed vs accuracy" curve for a single-user reward model at 100-1,000 labels.
- SigLIP linear-probe few-shot numbers were not found in the call budget.

## 4. Training cost and time: Mac (MLX) vs cloud vs fine-tuning services

### Takeaway
A LoRA on a 3-8B model with ~1k examples fits on an Apple Silicon Mac with 16-32 GB and takes on the order of 1-4 hours on an M3/M4 Max. Cloud GPUs do the same job for a few dollars ($2-4/hr H100). Anthropic models can't be fine-tuned except Claude 3 Haiku on Bedrock (an old model), so a "fine-tuned Claude" is not a practical option.

### Cited Findings
- MLX LoRA throughput (blog estimates, unverified): 8B LoRA ~320 tok/s on M3 Max 48GB, ~380 tok/s on M4 Max 128GB. A 500-iteration run on 300 examples takes 15-45 minutes. Memory ~14 GB for LoRA on Llama 3.1 8B, ~7 GB for QLoRA. — [InsiderLLM MLX guide](https://insiderllm.com/guides/fine-tuning-mac-lora-mlx/)
- "7B to 13B models can be trained in a few hours" on M3 Max 64GB (secondary, unverified). — [bestllmfor](https://bestllmfor.com/guide/mlx-apple-silicon-llm/)
- RunPod (July 2026 page): A100 80GB $1.19/hr, H100 80GB from $1.99/hr, billed per second. An October 2026 change log shows H100 SXM up from $3.49 to $3.99/hr and A100 from $1.59 to $1.79/hr (tiers differ, so the sources conflict). — [RunPod pricing](https://www.runpod.io/gpu-cloud/pricing); [pricingsaas RunPod diff 2026W42](https://pricingsaas.com/companies/runpod/diffs/2026W42)
- Modal H100 $0.001097/sec ≈ $3.95/hr. A100 80GB is around $2.50/hr per Modal's change log, or $3.72/hr per a third party (conflict). — [Spheron on Modal](https://www.spheron.network/blog/modal-gpu-pricing-2026-per-second-billing/); [pricingsaas Modal](https://pricingsaas.com/companies/modal/diffs/2025Q1)
- Generic estimate for a 7B fine-tune on 100k examples: ~7 h on H100 ($14) or ~20 h on A100 ($24). — [DeployBase](https://deploybase.ai/articles/a100-price) (secondary)
- OpenAI fine-tuning, GPT-4.1 mini: ~$0.80 per 1M training tokens; fine-tuned inference ~$0.80 in / $3.20 out per MTok, about 2x base (aggregators, not OpenAI's page). — [costbench](https://www.costbench.com/software/fine-tuning-apis/openai-fine-tuning/); [itsourcecode](https://itsourcecode.com/web-dev/fine-tuning-openai-models-2026/)
- Anthropic fine-tuning: only Claude 3 Haiku, through Amazon Bedrock, GA 2024-11-01, US West (Oregon) only, text-only, up to 32K context. No 2025-2026 expansion was found. — [Anthropic news](https://www.anthropic.com/news/fine-tune-claude-3-haiku-ga); [AWS what's new](https://aws.amazon.com/about-aws/whats-new/2024/11/fine-tuning-anthropics-claude-3-haiku-amazon-bedrock)
- Qwen2.5-VL-3B LoRA memory: ~12 GB FP16 LoRA, ~6 GB INT8 (calculator estimate). One tutorial suggests a GPU with at least 18 GB for QLoRA. SpatiaLab fine-tuned it in 4-bit with gradient checkpointing. — [Spheron GPU guide](https://www.spheron.network/tools/gpu-recommender/Qwen/Qwen2.5-VL-3B-Instruct/); [F22 Labs](https://www.f22labs.com/blogs/complete-guide-to-fine-tuning-qwen2-5-vl-model); [SpatiaLab arXiv 2602.03916](https://arxiv.org/pdf/2602.03916)

### Inferences
- Worked example on a Mac: 1,000 examples x ~1.5k tokens (question + scene + short system prompt) is 1.5M tokens per epoch. At ~380 tok/s (8B, M4 Max) that is ~66 min per epoch, so 3 epochs take ~3.3 h. A 3B model should be roughly 2-3x faster, about 1-1.5 h. On a base M-series chip with 16 GB, expect 3B QLoRA only, and several times slower. These are extrapolations from blog numbers.
- The same job in the cloud: ~4.5M tokens on one H100 takes well under an hour, so about $2-4 per run plus setup time. Cost is not the deciding factor; convenience and iteration speed are.
- A vision critic (SigLIP embeddings + small head) trains in minutes on a Mac. A Qwen2.5-VL-3B LoRA critic is feasible locally with 32 GB+, but the training-time figure is not sourced.
- OpenAI fine-tuning is cheap (~$1-4 per run at these sizes) but the result is a hosted model, so it brings no latency gain over Claude Haiku 5.5, which costs $0.10/$0.50.

### Gaps
- Together AI and Fireworks fine-tuning prices: the pages could not be fetched and no figures were retrieved. Check them directly.
- No controlled MLX LoRA benchmark (tok/s by chip and model size) from a primary source. The MLX examples repo could not be fetched.
- Qwen2.5-VL LoRA training hours are not sourced.

## 5. Inference speed: local Mac and hosted fast inference; time for a ~600-token scene

### Takeaway
On an M4 Max, a 7-8B model at 4-bit generates ~70-85 tok/s with llama.cpp, so a 600-token scene takes ~7-9 s of generation. A 3-4B model on M4 Pro or Max is roughly 2x faster, ~3-5 s. Prompt processing runs ~700-900 tok/s, so a 6k-token DSL-spec prompt adds ~7 s TTFT unless it is cached. A fine-tuned model that no longer needs the spec in its prompt avoids that. Cerebras at ~2,000+ tok/s on Llama 3.1 8B would produce the scene in ~0.3 s.

### Cited Findings
- llama.cpp, Llama 2 7B, llama-bench -p 512 -n 128, older fixed build: M4 Max 32-core GPU (410 GB/s) Q4_0 PP 714 t/s, TG 70 t/s; Q8_0 TG 44; F16 TG 24. M4 Max 40-core GPU (546 GB/s) Q4_0 PP 886, TG 83; Q8_0 TG 54; F16 TG 32. Numbers cite discussion #4167 secondhand; newer builds are faster. — [markaicode M4 Max llama.cpp](https://markaicode.com/benchmarks/llamacpp-m4-max-benchmark/); [Medium summary](https://medium.com/@andreask_75652/llama-cpp-performance-apple-silicon-051241dd6eae); original thread [ggml-org/llama.cpp#4167](https://github.com/ggml-org/llama.cpp/discussions/4167) (not opened)
- MLX vs llama.cpp on M4 Max 128GB, Qwen3.5-35B-A3B (MoE, 3B active): mlx-lm 131.8 tok/s vs raw llama.cpp 72.4 tok/s (March 2026). — [Ollama MLX article](https://levelup.gitconnected.com/ollama-mlx-nearly-doubles-llm-speed-on-your-mac-58-to-112-tok-s-and-your-old-models-get-none-of-fd1a9ec531fb); [techplained](https://www.techplained.com/qwen-3-5-apple-silicon)
- Qwen3-8B / Llama 3.1-8B at 50-80 tok/s on base M4 (guide). Community table: M4 Pro 64GB Qwen3.5-4B 8-bit ~48 tok/s at 1k context; M4 Pro 24GB Qwen3.5-9B 8-bit ~25 tok/s. — [bestllmfor MLX guide](https://bestllmfor.com/guides/mlx-local-llm-apple-silicon/); [omlx community benchmarks](https://omlx.ai/benchmarks?page=6680)
- Cerebras Llama 3.1 8B: ~2,337 tok/s (Artificial Analysis 2025 figure via secondary), $0.10/MTok in and out, free tier 1M tokens/day. Cerebras claims 2,100-2,500 tok/s on Llama 3.3 70B vs Groq at 403-750. Artificial Analysis measured Groq at ~294 tok/s on 70B. — [morphllm Cerebras](https://www.morphllm.com/cerebras-pricing); [deploybase Groq vs Cerebras](https://deploybase.ai/articles/groq-vs-cerebras-pricing-speed-and-benchmark-comparison)
- Groq Llama 3.1 8B Instant: $0.05 in / $0.08 out per MTok (old page snapshot). A secondary source says NVIDIA acquired Groq in late 2025 (unconfirmed). — [Groq pricing](https://groq.com/pricing); [vantaige](https://vantaige.io/es/ai-tool/cerebras)
- Claude Opus 5.5 fast mode runs at up to 2.5x output tok/s for $8/$40 per MTok (Claude API only). — Claude API skill reference (Anthropic, cached 2026-10-06)

### Inferences
- Time for a 600-token scene, generation only:
  - 8B Q4 on M4 Max: 600/70-83 ≈ 7-9 s.
  - 8B on M4 Pro or base M4: ~600/40-60 ≈ 10-15 s.
  - 3-4B 4-bit on M4 Pro/Max: likely ~100+ tok/s, so ~4-6 s (extrapolated).
  - MLX is often 1.3-1.8x faster than llama.cpp on the same Mac.
  - Cerebras 8B: ~0.3 s plus network.
  - Groq 8B: likely under 1 s.
- TTFT: prefill runs ~700-900 tok/s for 7B on M4 Max. A 6k-token system prompt costs ~7-8 s, a 300-token prompt ~0.4 s. Prompt caching (mlx-lm, llama.cpp KV cache reuse) can remove most of the system-prompt cost after the first call.
- Streaming matters more than total time. The tutor already streams Say lines (voice prefetch per recent commits), so the first beat's speed decides perceived latency, not the full 600 tokens.
- The user's Mac chip and RAM are not specified. All local numbers scale roughly with memory bandwidth: base M-series ~120 GB/s, Pro ~273, Max 410-546.

### Gaps
- No primary benchmark for mlx-lm TTFT on 3B/8B dense 4-bit models was found. Run `mlx_lm.generate --verbose` on the user's Mac to get exact numbers.
- Current (Oct 2026) Groq speed for 8B and Groq's ownership status were not verified.
