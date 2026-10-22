# Specialist components beside Claude for a live visual tutor

Scope: which narrow sub-tasks frontier LLMs are weak or slow at, and where a program, a small trained model, or a verifier does better. Mapped to the tutor's weak spots: (1) binding spoken numbers to the stage, (2) numeric and spatial errors in plots, (3) generic box-and-arrow pictures, (4) factual slips, (5) 5-14 s of thinking before the first word, (6) judging its own visual output.

Research note: about 20 tool calls. Most figures come from search snippets of primary papers (arXiv, ACL, ICLR, NeurIPS). Where only a secondary source gave a number, it is flagged. One fetch, of the VisionReward abstract, returned nothing.

## Spatial/numeric reasoning: are LLMs weak at coordinates, geometry, plotting and arithmetic, and should computation go to a program?

### Takeaway
Yes. Benchmarks from 2025 show LLMs breaking down on coordinate and grid tasks as size or the number of constraints grows. The strongest fix is still to move the computation out of the model: the LLM writes the program or expressions, and an interpreter or CAS (computer algebra system) computes them (PAL). For the tutor, intersections, ranges, clipping and axis bounds should come from code (sympy/numpy in the server, or the C++ engine), not from numbers Claude writes.

### Cited Findings
- "Stuck in the Matrix" (2025) probes grid and coordinate reasoning. Accuracy drops as grids get larger, and models do well only on small boards. Models had trouble finding the centre lines of a grid (Quadrant test) and made frequent arithmetic errors in the Distance test. The authors read this as a gap in spatial understanding, not just miscounting. — [arXiv 2510.20198](https://arxiv.org/pdf/2510.20198)
- MazeEval (July 2025) is maze navigation from coordinates only, with no image. o3 navigated perfectly up to 30x30; other models "exhibit catastrophic failure beyond 9x9", mostly by looping and revisiting cells. (Figures are from the abstract.) — [arXiv 2507.20395](https://arxiv.org/abs/2507.20395v1)
- FloorplanQA/PlanQA (2025) uses structured JSON/XML indoor layouts. Models pass shallow queries but "often fail to respect physical constraints and preserve spatial coherence". This is close to what the tutor's markup asks for (things with positions and relations). — [arXiv 2507.07644](https://arxiv.org/abs/2507.07644v1)
- A study of GPT-5 across 8 spatial benchmarks found "unprecedented strength", but still short of human performance across a broad range of tasks. — [arXiv 2508.13142](https://arxiv.org/pdf/2508.13142v1.pdf)
- PAL (Program-aided LMs, ICML 2023, older): the LLM breaks the problem into a Python program and an interpreter computes the answer. The motivation is that chain-of-thought models "often make logical and arithmetic mistakes in the solution part, even when the problem is decomposed correctly." PAL with Codex beat PaLM-540B chain-of-thought on GSM8K by 15 points absolute (top-1). — [arXiv 2211.10435](https://arxiv.org/pdf/2211.10435)
- PAL figures from a secondary source, not checked against the paper: 72.0% vs 65.6% CoT on GSM8K with the same Codex model, and 61.2% vs 23.1% on GSM-hard (larger numbers). The GSM-hard gap is the relevant one for "big or awkward numbers". — [beancount.io research log](https://beancount.io/bean-labs/research-logs/2026/04/23/pal-program-aided-language-models)
- MatPlotAgent (ACL Findings 2024): plot generation as code, with iterative debugging, plus a visual-feedback step in which a multimodal model looks at the rendered draft. On the Qwen-Agent Code Interpreter benchmark, the full system scored 72.62 (Viz-Hard) and 68.35 (Viz-Easy), against 66.67 and 65.82 with visual feedback disabled. The GPT-4 baseline was 66.7 and 60.8. Its MatPlotBench has 100 human-verified cases scored by GPT-4V, which the authors report correlates strongly with human scores. — [arXiv 2402.11453](https://arxiv.org/pdf/2402.11453)

### Inferences
- For weak spot (2): let Claude state intent symbolically (e.g. "curve y=x^2-2, line y=x, mark their intersections, show both crossings"). The server then solves with sympy (roots, intersections, extrema) and derives ranges and padding so nothing is clipped. That is the PAL pattern, and the evidence says it beats asking the model for numbers, especially larger or less round ones (the GSM-hard gap).
- A deterministic checker (does every labelled point lie on its curve, is every point inside the range, are asymptotes handled) is cheaper and more reliable than any learned model for this. The existing layout/lint checker is the natural home for it.
- The visual-feedback gain in MatPlotAgent is real but modest, about 1-6 points. Rendering and critiquing helps less than computing correctly in the first place.

### Gaps
- I found no benchmark specific to Claude on plotting coordinates or function-intersection accuracy. The figures above are from other models or older Claude-era models.
- I found no head-to-head of a small specialist geometry or plot model against a frontier LLM with a CAS tool for this task. The literature generally prefers tool use over a specialist model for exact computation.

## Visual critics and judges: small fine-tuned VLMs as aesthetic or layout scorers; how small, how many labels, accuracy vs GPT-4V/Claude

### Takeaway
GPT-4V-class judges are moderately reliable for pairwise comparisons (about 0.64-0.77 agreement with humans) but weak at absolute scoring (Pearson about 0.49). LLMs also judge their own outputs poorly, preferring them (self-preference bias) and failing to self-correct without external feedback. Small reward models trained on human preference pairs (HPSv2 and ImageReward, both CLIP/BLIP-scale, and a 7B Qwen2.5-VL reward model) reach about 65% on ImageReward's test set and about 65-84% on HPDv2. Data for UI or diagram critics is small: UICrit has 3,059 critiques over 983 UIs, and its 55% gain came from prompting, not fine-tuning.

### Cited Findings
- MLLM-as-a-Judge (ICML 2024) covers 11 MLLMs, including GPT-4V, Gemini, LLaVA, CogVLM and Qwen-VL. In pair comparison, GPT-4V agreed with humans at about 0.636 with ties allowed and 0.773 without (from an alphaXiv summary; verify in the paper). In absolute scoring, Pearson with humans was 0.490. GPT-4V's consistency was 0.675 in pair comparison and lower for scoring and batch ranking. The paper reports position, length and slight egocentric biases, and "MLLMs require further improvement in score evaluation and batch ranking." — [arXiv 2402.04788](https://arxiv.org/pdf/2402.04788); [alphaXiv overview](https://alphaxiv.org/overview/2402.04788v3)
- Self-preference bias (2024): GPT-4 "exhibits a significant degree of self-preference bias" as a judge. The proposed mechanism is that LLMs rate low-perplexity (familiar-looking) outputs higher than humans do. A later EMNLP 2025 paper argues the standard measure conflates self-preference with response quality. — [arXiv 2410.21819](https://arxiv.org/pdf/2410.21819); [EMNLP 2025](https://preview.aclanthology.org/setup/2025.emnlp-main.86)
- "LLMs Cannot Self-Correct Reasoning Yet" (ICLR 2024): without external feedback, performance "even deteriorates" after self-correction in most cases. Earlier reported gains often relied on oracle labels. Models have trouble judging whether their own answers are correct. — [arXiv 2310.01798](https://arxiv.org/pdf/2310.01798)
- HPSv2 (2023, older) reports preference accuracy for ImageReward of 65.1 (ImageReward test set) and 74.0 (HPDv2), and for HPSv2 of 65.7 and 83.3. HPS v2.1 is listed at 66.8 and 84.1. These are CLIP-based scorers, far smaller than frontier VLMs. — [arXiv 2306.09341](https://arxiv.org/pdf/2306.09341); [hpsv2 on PyPI](https://pypi.org/project/hpsv2/)
- A Qwen2.5-VL-7B reward model with reasoning and soft rewards (2025) reached 64.9% on the ImageReward test set and 65.4% on HPSv2 while trained on about 25% of the data. That is close to the specialist scorers with a quarter of the labels. — [arXiv 2503.19948](https://arxiv.org/abs/2503.19948)
- VisionReward (Dec 2024) fine-tunes CogVLM2 and CogVLM-Video into multi-dimensional, interpretable preference scorers for image and video. Its MonetBench test sets have 500 image pairs and 1,000 video pairs. I could not retrieve the accuracy figures (the fetch failed). — [arXiv 2412.21059](https://arxiv.org/pdf/2412.21059)
- UICrit (UIST 2024): 3,059 critiques and quality ratings over 983 mobile UIs from seven experienced designers, grounded with bounding boxes. Few-shot and visual prompting with the dataset gave a 55% improvement in LLM feedback quality, still below human experts. Fine-tuning a critic or reward model is listed only as future work. — [arXiv 2407.08850](https://arxiv.org/abs/2407.08850)
- A follow-up multi-LLM pipeline with iterative visual-prompt refinement (Gemini-1.5-pro, GPT-4o) consistently improved design-critique quality on UICrit, on both automatic metrics and expert ratings. — [arXiv 2412.16829](https://arxiv.org/pdf/2412.16829)
- MatPlotAgent's GPT-4V plot scorer correlates strongly with human scores on 100 plot cases. Frontier VLMs are usable as reference-based plot graders. — [arXiv 2402.11453](https://arxiv.org/abs/2402.11453v3)

### Inferences
- For weak spot (6): do not let Claude grade its own render in isolation. The evidence points to (a) deterministic checks first (overlap, clipping, unbound numbers, off-screen), (b) a different judge model, and (c) pairwise judging ("A vs B, which is clearer?") rather than 1-10 scores, which is where VLM judges are most reliable.
- Training a small aesthetic or layout critic looks feasible at the scale of a few thousand human preference pairs, going by UICrit-scale data and the 7B model doing well on about 25% of HPS data. Off-the-shelf HPS/ImageReward judge photographic text-to-image aesthetics, though, not diagram clarity, so they would need re-training on the tutor's own renders. A cheap way to get labels: keep the user's choice when two scene variants are shown side by side.
- Weak spot (3), generic box-and-arrow pictures, is about content and depiction, not something a scorer fixes. A critic can flag "this is a box diagram of a physical thing" but cannot draw the better picture. That points to asset libraries or retrieval (out of scope here).

### Gaps
- I found no published small model fine-tuned specifically as a judge of educational diagrams or animations, and no layout-quality model compared directly with Claude.
- VisionReward's accuracy numbers and parameter count were not retrieved.
- I found no data on how many labels are needed to train a diagram or layout critic to match a frontier judge.

## Speech-visual sync: aligning narration words to visual events (forced alignment, cue prediction)

### Takeaway
Aligning words to audio is solved by non-LLM tools: MFA (Montreal Forced Aligner, classic GMM-HMM) or WhisperX's wav2vec2 alignment. In one 2024 head-to-head, MFA was more accurate than WhisperX and MMS. Whisper's own timestamps are coarse. Since the tutor synthesises speech itself, the TTS engine's own word timings (when available) beat any aligner. I found no published small model that predicts where visual cues belong in narration. That part stays a semantic job for Claude, with a deterministic binding check afterwards.

### Cited Findings
- Rousso et al. (Interspeech 2024) compared WhisperX and MMS against MFA on hand-aligned TIMIT and Buckeye, counting only correctly recognised words. MFA outperformed both end-to-end systems, and the end-to-end systems needed their outputs reshaped to get word-level alignment. — [arXiv 2406.19363](https://arxiv.org/html/2406.19363v1)
- The WhisperX README says raw Whisper timestamps are utterance-level and "can be inaccurate by several seconds". The wav2vec2 phoneme alignment step adds word-level timing, and a larger alignment model was "not found to be that helpful." — [whisperx on PyPI](https://pypi.org/project/whisperx)

### Inferences
- For weak spot (1), binding: this is a checking problem, not a model problem. A lint rule can extract every number or quantity in Say lines (regex plus number words) and require a matching on-stage label or value bound to a cue word, then return failures to Claude or auto-insert labels. A small NER/tagger model would only be needed for fuzzy cases ("twice as big", "half").
- If the TTS provider returns word timestamps, use them directly for cue timing. Otherwise run forced alignment on the synthesised audio (MFA for accuracy, wav2vec2/WhisperX for convenience). Both are fast, local and need no LLM.

### Gaps
- I found no paper on learned cue placement (predicting which word a visual event should fire on).
- I found no word-boundary error figures in ms for WhisperX from a reliable source. The 50-100 ms figure seen was from a low-authority glossary and is excluded.

## Latency: fast first response while the big model thinks; routing; on-device speeds; prompt caching; distilled non-thinking models

### Takeaway
Dual-system designs (a fast talker plus a slow reasoner) are an established pattern for hiding thinking latency, but published TTFT (time-to-first-token) numbers are scarce. Routing (RouteLLM) cuts expensive-model calls by 35-85% while keeping 95% of GPT-4 quality. On Apple Silicon, an 8B model at 4-bit runs at roughly 45-90 tok/s with llama.cpp, and MLX is often faster, but measurements are inconsistent and mostly from blogs. Prompt caching reliably cuts cost and modestly cuts TTFT (13-31% in a rigorous 2026 study; vendors quote up to 85%). Reasoning distilled into a 7B model keeps much of the quality on maths (R1-Distill-Qwen-7B: AIME24 55.5, MATH-500 92.8).

### Cited Findings
- Talker-Reasoner (NeurIPS 2024 workshop) splits the agent into a fast conversational "Talker" (System 1) and a slower planning "Reasoner" (System 2). Lower latency is a stated benefit, but the paper gives no figures. — [arXiv 2410.08328](https://arxiv.org/abs/2410.08328v1)
- A 2026 SIGDIAL paper notes that tool-using LLM agents often take several seconds per turn, and that sequential conversational-to-reasoning handoff leaves users in silence. Its architecture is built to avoid that. — [SIGDIAL 2026](https://aclanthology.org/2026.sigdial-1.14.pdf)
- VoiceAgentRAG has a fast Talker answering from a local cache while a "Slow Thinker" prefetches in the background. It claims sub-millisecond cache lookups (summary source). — [alphaXiv overview 2603.02206](https://www.alphaxiv.org/overview/2603.02206)
- RouteLLM (LMSYS/Berkeley, July 2024) uses routers trained on preference data to choose between GPT-4 and Mixtral-8x7B. Cost reductions were over 85% on MT-Bench, 45% on MMLU and 35% on GSM8K against GPT-4 alone, while keeping 95% of GPT-4's performance. Secondary write-ups cite 14% or 26% of calls going to GPT-4 depending on the configuration, and note that some savings are measured against a random-routing baseline. — [LMSYS blog](https://lmsys.org/blog/2024-07-01-routellm/); [arXiv 2406.18665](https://arxiv.org/html/2406.18665v4)
- Apple Silicon speeds (weak sources, treat as rough):
  - Llama-3.1-8B Q4_K_M on M4 Max with llama.cpp/Metal: about 75-90 tok/s by one source, 45-58 tok/s for M3 Max/M4-class by another. Both pages carry corrections. — [markaicode](https://markaicode.com/benchmarks/hugging-face-llama-31-m4-max-throughput-benchmark/)
  - A single-user M4 Max run of Qwen3.5-35B-A3B (MoE): llama.cpp 72 tok/s vs mlx-lm 131.8 tok/s. — [compute-market](https://www.compute-market.com/blog/mlx-vs-llama-cpp-apple-silicon-2026)
  - Ollama's MLX backend is reported to go from 58 to 112 tok/s. — [levelup.gitconnected](https://levelup.gitconnected.com/ollama-mlx-nearly-doubles-llm-speed-on-your-mac-58-to-112-tok-s-and-your-old-models-get-none-of-fd1a9ec531fb)
  - An older M2 Max test found MLX behind llama.cpp for quantised generation. — [Medium](https://medium.com/@andreask_75652/benchmarking-apples-mlx-vs-llama-cpp-bbbebdc18416)
- Prompt caching:
  - AWS reports up to 85% lower latency and up to 90% lower cost for Claude on Bedrock (a best-case ceiling, for long prompts). — [AWS blog](https://aws.amazon.com/blogs/machine-learning/effectively-use-prompt-caching-on-amazon-bedrock)
  - An arXiv evaluation of agentic workloads with 10k-token system prompts found 41-80% lower cost and 13-31% better TTFT across providers. Caching dynamic content can increase latency, so put the static prefix first. — [arXiv 2601.06007](https://arxiv.org/html/2601.06007v2)
  - The cache needs an identical prefix. Anthropic's minimum is 1,024 tokens per checkpoint and the default TTL is 5 minutes (secondary source; check the Anthropic docs). — [morphllm](https://www.morphllm.com/prompt-caching)
- DeepSeek-R1-Distill-Qwen-7B, fine-tuned on R1-generated samples from Qwen2.5-Math-7B: AIME 2024 pass@1 55.5 (cons@64 83.3), MATH-500 pass@1 92.8. A third party measured 53.3 and 91.4 under a different setup. — [model card mirror](https://huggingface.co/calcuis/deepseek-r1); [DataLearner](https://www.datalearner.com/en/ai-models/pretrained-models/DeepSeek-R1-Distill-Qwen-7B/analysis); original paper [arXiv 2501.12948](https://arxiv.org/abs/2501.12948)

### Inferences
- For weak spot (5), the main fix is architectural: start speaking within about 1 s using a fast path, while Claude's full beat streams in. Options:
  - Claude itself without thinking for the opening line.
  - A small local model (an 8B on MLX at about 50-100 tok/s produces a 20-word line in well under a second once loaded).
  - A cached or templated acknowledgement.
- The risk is that the fast opening contradicts the slow content. Talker-Reasoner keeps the talker's output to framing and acknowledgement, not substance.
- Routing by question difficulty (RouteLLM-style) maps onto "thinking on or off, and how big a budget". A trained router could skip thinking for easy follow-ups. Its savings figures are for cost, not latency, but per-call latency drops by the same mechanism.
- Prompt caching of the long system prompt (the markup spec) is close to free and should already be on. Expect a TTFT gain of tens of percent, not the elimination of thinking time.
- The thinking time cannot be cached away. Moving numeric work to code (section 1) may shrink the thinking budget Claude needs, because it no longer has to compute intersections in its head.

### Gaps
- I found no authoritative Claude TTFT figures with and without extended thinking. The tutor's own logs are the better source.
- No reliable, controlled MLX vs llama.cpp 8B benchmark on M-series. Running `mlx_lm.generate` and `llama-bench` locally is recommended.
- I found no study measuring user-perceived quality of a fast-then-slow tutor reply.

## Distillation from a frontier teacher: fraction of quality retained, sample counts, rejection sampling with an automatic checker

### Takeaway
Distillation with rationales, plus filtering by an automatic correctness check, is well supported. Small students can match or beat few-shot giants on narrow tasks (770M T5 beat 540B PaLM). As few as 1,000 curated teacher traces fine-tune a 32B model past o1-preview on maths (s1). Generate, filter with a binary checker, fine-tune, repeat (ReST-EM) beats training on human data alone, but gains flatten after 2-3 iterations. The tutor's layout/lint checker is exactly the binary filter these methods need. The open question is whether a 1-8B student keeps the creativity and pedagogy, which the literature does not measure.

### Cited Findings
- Distilling Step-by-Step (Google, 2023, older): an LLM generates rationales, and the small model is trained on rationale generation plus label prediction. A 770M T5 beat few-shot CoT PaLM-540B using only 80% of the benchmark's training data, a more than 700x size reduction. Standard fine-tuning of the same T5 could not match PaLM even with 100% of the data. The 11B T5 also beat PaLM. — [arXiv 2305.02301](https://arxiv.org/html/2305.02301v2); [Google Research blog](https://research.google/blog/distilling-step-by-step-outperforming-larger-language-models-with-less-training-data-and-smaller-model-sizes/)
- s1 (2025): 1,000 questions with reasoning traces distilled from Gemini Thinking, chosen for difficulty, diversity and quality. Supervised fine-tuning of Qwen2.5-32B-Instruct took 26 minutes on 16 H100s. With budget forcing the model beats o1-preview by up to 27% on MATH and AIME24, and AIME24 rises from 50% to 57% with forcing. A later analysis argues the scaling behaviour comes mainly from enforcing a maximum generation length. — [arXiv 2501.19393](https://arxiv.org/pdf/2501.19393); [critique arXiv 2507.14419](https://arxiv.org/html/2507.14419v1)
- ReST-EM (Google DeepMind, TMLR 2024): sample solutions, keep the ones a binary checker marks correct, fine-tune, repeat. On MATH and APPS with PaLM-2 it "significantly surpass[es] fine-tuning only on human data", and gains grow with model size. Improvement diminishes beyond a couple of iterations because of overfitting on few problems. It transfers to GSM8K, HumanEval and Big-Bench Hard. — [arXiv 2312.06585](https://arxiv.org/abs/2312.06585)
- R1-Distill-Qwen-7B (above) shows that distilling a reasoning teacher into a 7B student keeps strong maths performance. AIME24 55.5 is well below the teacher's but far above the base model (base-model figure not retrieved). — [arXiv 2501.12948](https://arxiv.org/abs/2501.12948)

### Inferences
- A practical recipe for the tutor:
  1. Log Claude's scene markups.
  2. Run the layout/lint and binding checker.
  3. Keep the passing ones (ReST/RFT-style rejection sampling), plus any the user rated well.
  4. Fine-tune a 3-8B open model (e.g. Qwen) on prompt-to-markup pairs.
- s1 and LIMO-scale results suggest 1k-10k high-quality filtered samples is the right order of magnitude for a narrow format. That is an inference from maths reasoning, not tested on scene markup.
- The likeliest win for a student is narrow sub-tasks: markup repair or "fix this lint error", number binding, and plot-spec filling. Generating whole lessons is less likely, because depiction creativity and factual breadth (weak spots 3 and 4) are teacher-model strengths that small students tend to lose.
- Factual slips (4) are better handled by retrieval or a verifier than by a smaller model. Small students inherit and often amplify the teacher's factual gaps. This is an inference; no direct source was found here.

### Gaps
- I found no study distilling Claude or GPT into a 1-8B student for structured visual or scene-markup generation, with retained-quality percentages.
- LIMO's (817 samples) exact numbers were not retrieved.
- I found no evidence on whether distilled students keep "creativity" or diversity. Rejection sampling with a correctness filter is known to narrow diversity (ReST-EM pass@1 vs pass@k), but the exact pass@k behaviour was seen only in a secondary summary and is unverified.
