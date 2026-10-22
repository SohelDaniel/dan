# Small / fine-tuned models for visual-explanation, animation-code and diagram-DSL generation vs frontier models (state as of Oct 2026)

Research notes. Network access was flaky during this session: several arXiv full-text and Hugging Face fetches timed out or were blocked, so some per-model benchmark tables could not be read and are listed under Gaps. Every claim below cites the page it came from. Where a figure comes from a search-engine summary of a page and not from my own reading of the page, it says so.

## 1. Manim / educational animation generation: success rates and failure modes

### Takeaway
Every strong Manim result from 2025-2026 is a frontier model inside an agent loop (planner, coder, critic, retries). The only published small fine-tuned Manim models reach about 62% render success on a 417-example dataset, against 82-94% for frontier agent pipelines. The main failure mode for all models is spatial layout (overlap, misalignment, out-of-frame elements), not syntax. Several 2026 papers fix this by taking layout away from the LLM: a symbolic scene graph, a layout predictor, or a deterministic renderer.

### Cited Findings
- **TheoremExplainAgent / TheoremExplainBench (Feb 2025, ACL 2025).** The benchmark has 240 theorems across STEM disciplines. The o3-mini agent reaches a 93.8% success rate and an overall score of 0.77. Success depends on retries: with 0 retries the success rate is "extremely low", with N=5 retries it is up to about 90% (from a search summary of the paper's ablation table). — [arXiv 2502.19400](https://arxiv.org/pdf/2502.19400); [ACL Anthology](https://aclanthology.org/2025.acl-long.332/)
- An author of TheoremExplainAgent says the approach produces long-form videos (over 5 minutes) and that "layout issues remain". — [Max Ku on X](https://x.com/vinesmsuic/status/1896808818518147263)
- **Code2Video (Oct 2025, NeurIPS 2025).** It uses three agents: a Planner (lecture flow and assets), a Coder (Python/Manim with "scope-guided auto-fixing") and a Critic (a VLM with "visual anchor prompts" that refines layout). It reports a 40% improvement over direct code generation. Its MMMC benchmark has 117 topics inspired by 3Blue1Brown. Its TeachQuiz metric makes a VLM "unlearn" a concept and then recover it from the video. — [arXiv abs 2510.01174](https://arxiv.org/abs/2510.01174); [NeurIPS page](https://neurips.cc/virtual/2025/131666)
- Code2Video model comparison: Claude Opus 4.1 gets higher aesthetic (AES) and TeachQuiz (TQ) scores, "highlighting code reliability". GPT-5 writes concise, efficient, low-cost code but "underperforms in attractiveness". The official README says the best Manim code quality comes from Claude-4-Opus. — [ICML 2026 slides](https://icml.cc/media/icml-2026/Slides/65050.pdf); [Code2Video GitHub mirror](https://github.com/midikang/Code2Video)
- Code2Video run on TheoremExplainBench with GPT-4o (appendix, from a search summary): Element Layout rises from 0.59 (TheoremExplainAgent) to 0.91, and Visual Relevance from 0.79 to 0.91. — [arXiv 2510.01174 PDF](https://arxiv.org/pdf/2510.01174)
- **ManimBench v1, small fine-tuned models (2025).** The dataset has 417 description and code pairs from the official ManimCE docs, split into basic, intermediate and advanced. Seven open-source compact LLMs were fine-tuned with LoRA and QLoRA. The fine-tuned models "consistently outperform the base models, achieving a 62% Manim render success rate". — [ResearchGate: LLM Approaches to Educational Video Generation Using Manim](https://www.researchgate.net/publication/399370691_Large_Language_Model_Approaches_to_Educational_Video_Generation_Using_Manim); [HF dataset SuienR/ManimBench-v1](https://huggingface.co/datasets/SuienR/ManimBench-v1)
- **Community fine-tune of Qwen2.5-Coder-14B-Instruct for Manim.** It uses QLoRA with rank 8 and was trained on BibbyResearch/3blue1brown-manim (2,407 examples), targeting Manim v0.19.0. The model card makes its own claims and gives no independent evaluation. — [Ollama maternion/qwen2.5-coder-manim](https://ollama.com/maternion/qwen2.5-coder-manim:Q4_K_M)
- **ManiBench (Mar 2026).** It targets "syntactic hallucinations": valid Python that calls Manim APIs which don't exist or are deprecated (often confusion between ManimGL and ManimCE). It also targets "visual-logic drift". It is a pilot of 12 problems at 5 difficulty levels, with 9 models evaluated. — [arXiv 2603.13251](https://arxiv.org/html/2603.13251); [GitHub](https://github.com/nabin2004/ManiBench)
- PhysicsSolutionAgent (Jan 2026) names Manim code hallucinations as a main failure category. — [arXiv 2601.13453](https://arxiv.org/pdf/2601.13453)
- **See Before You Code / OmniManim (May 2026).** LLM-written animation code "often produces renders with element overlap, misalignment, and broken animation continuity". The method adds a Vision Agent that predicts sparse keyframe layouts (bounding boxes, coarse-to-fine denoising, with an objective that accounts for interpolation between keyframes) before any code is written. The data is ManimLayout-1K, built from open-source Manim repos (22,579 samples), and the test set is EduRequire-500 (written by experts). It reports better render quality than single-model and multi-agent baselines. I could not read the exact numbers. — [arXiv 2605.15585](https://arxiv.org/pdf/2605.15585)
- **SGA, Symbolic Geometric Agent (Jul 2026).** It partly executes LLM-written Manim code to extract a symbolic scene graph, finds spatial conflicts, and asks for refinement. Peak MVQS (a spatial-integrity score that needs no rendering) is 73.11, with Code2Video + GPT-5.1. That is +16.1% relative over the raw baseline, and MVQS improved in 7 of 8 backbone × pipeline configurations. Humans preferred SGA output over the raw baseline in 84.4% of comparisons and over a VLM critic in 65.0%. It beats a VLM critic by 15.0 points (TheoremExplainAgent pipeline) and 10.5 points (Code2Video pipeline) on a sub-score. — [arXiv 2607.18116](https://arxiv.org/abs/2607.18116)

### Inferences
- The layout failures described in this literature (overlap, misalignment, broken continuity) are what this project's custom DSL already removes by having no coordinates and letting the C++ engine lay out the scene. A small model writing this DSL would not face the hardest part of the Manim task. The remaining risks are semantic: wrong maths, a poor choice of visual metaphor, weak structure.
- A gap of 62% (small fine-tuned) against 82-94% (frontier agents with retries) on Manim suggests a small model would need a validator and repair loop to be usable. That loop adds latency and cuts into the speed gain.
- Frontier rankings (Claude Opus best for Manim code reliability and aesthetics in Code2Video) support keeping a frontier model for the creative and planning step.

### Gaps
- No exact per-model TheoremExplainBench table (GPT-4o, Claude 3.5 Sonnet, Gemini). The full-text fetch failed.
- No Code2Video numeric tables (TeachQuiz and AES per backbone). The fetch failed.
- The OmniManim numbers and whether its Vision Agent is a small fine-tuned model were not confirmed.
- I could not research "Math-To-Manim" or the Generative Manim repo (github.com/marcelo-earth/generative-manim, which was found but not read). No evaluated fine-tuned 7B Manim model with published numbers beyond ManimBench was found.

## 2. Diagram / vector / chart / layout generation: fine-tuned 7-13B vs frontier

### Takeaway
On narrow, well-defined visual-code tasks with large datasets (TikZ for scientific figures, SVG icons, chart-to-code), fine-tuned 7B-class models match or beat frontier models of their time on fidelity, using 100k-2M training examples. Frontier models still lead on open-ended instructions, on robustness across libraries, and on self-debugging.

### Cited Findings
- **AutomaTikZ (2023-2024, older work)** fine-tunes LLaMA on the DaTikZ dataset. The fine-tuned CLiMA/LLaMA models beat GPT-4 and Claude 2 on similarity to human-made figures. GPT-4 and Claude 2 "tend to generate more simplistic figures". GPT-4 and Claude 2 score up to 2.1 points higher on CLIPScore, but the authors call this misleading because the frontier models use "typographic attacks" (writing the prompt words as text into the figure). — [ResearchGate AutomaTikZ](https://www.researchgate.net/publication/387763274_AutomaTikZ_Text-Guided_Synthesis_of_Scientific_Vector_Graphics_with_TikZ)
- **DeTikZify (NeurIPS 2024)** turns figures and sketches into TikZ and "substantially outperforms" GPT-4V and Claude 3 in both automatic and human evaluation. GPT-4V's average score is 0.612 on figures and 0.15 on sketches. — [arXiv 2405.15306](https://arxiv.org/pdf/2405.15306)
- **TikZero (2025)** combined with fine-tuned AutomaTikZ v2 "matches the performance of commercial models like GPT-4o on key metrics", which is parity rather than a win. **TikZilla (Mar 2026)** builds on DeTikZify-V2 and adds GRPO reinforcement learning on the larger DaTikZ-V4 dataset. — [TikZero (ResearchGate)](https://www.researchgate.net/publication/389894385_TikZero_Zero-Shot_Text-Guided_Graphics_Program_Synthesis); [TikZilla arXiv 2603.03072](https://arxiv.org/html/2603.03072v2)
- **StarVector (CVPR 2025)** uses a StarCoder-based LLM for image→SVG and text→SVG. Its authors say it beats prior methods, compared against GPT-4-V, not GPT-4o. The README warns that it does not work on natural images or illustrations. — [CVPR 2025 paper](https://openaccess.thecvf.com/content/CVPR2025/html/Rodriguez_StarVector_Generating_Scalable_Vector_Graphics_Code_from_Images_and_Text_CVPR_2025_paper.html); [README](https://cdn.jsdelivr.net/gh/joanrod/star-vector@main/README.md)
- **OmniSVG (2025)** reports that it beats GPT-4o and StarVector on image-to-SVG across DINO, SSIM, LPIPS and MSE, by its own evaluation. For text-to-SVG it reports FID 145.89 against VectorFusion 218.76 and SVGDreamer 193.42. GPT-4o is not a text-to-SVG baseline. — [OmniSVG review](https://liner.com/review/omnisvg-unified-scalable-vector-graphics-generation-model); [alphaXiv 2504.06263](https://alphaxiv.org/overview/2504.06263v1)
- **VisCoder (EMNLP Findings 2025, TIGER-Lab)** is fine-tuned on VisCode-200K for Python plotting. Execution pass rates without self-debug on PandasPlotBench: GPT-4o 94.9 / 83.4 / 87.4 (Matplotlib / Seaborn / Plotly), VisCoder-7B 87.4 / 76.6 / 74.3, GPT-4o-mini 81.7 / 62.3 / 69.1. These figures come from a secondary summary. With self-debug VisCoder-7B reaches 91.4 / 90.3 / 81.7, and GPT-4o approaches 100%. The paper finds "proprietary models outperform open-source models by a wide margin". VisCoder-7B beats GPT-4o-mini on seaborn and plotly but not on matplotlib. — [arXiv 2506.03930](https://arxiv.org/html/2506.03930); [emergentmind summary](https://www.emergentmind.com/topics/viscoder); [HF model card](https://huggingface.co/TIGER-Lab/VisCoder-7B)
- **ChartCoder (ACL 2025)** is a 7B model trained on Chart2Code-160k (160k chart-code pairs, 27 chart types). It claims to beat open-source MLLMs, not GPT-4o. **ChartMaster (Aug 2025, 7B)** "rivals GPT-4o" on chart-to-code benchmarks. ChartMaster's paper notes that Chart2Code-160k was itself generated by prompting GPT-4o, so the small model is a distillation of the frontier model. — [ChartCoder arXiv 2501.06598](https://arxiv.org/html/2501.06598v1); [ChartMaster arXiv 2508.17608](https://arxiv.org/html/2508.17608v1)

### Inferences
- The small models that win share three traits: a narrow output language, a large paired dataset (around 160k-200k pairs and up), and metrics based on fidelity to a target image. Text-to-explanation tutoring has no target image, and quality there is pedagogical. That is closer to the open-ended setting where frontier models lead.
- Distillation from a frontier model (as with Chart2Code-160k) is the standard way these datasets are built. For this project that means logging Claude's own DSL scenes as training data.

### Gaps
- LayoutGPT (NeurIPS 2023, older), SlideGen, PosterLLaVA and LLM4SVG: no numbers retrieved in this session.
- Plot2Code: not retrieved.
- Exact DaTikZ dataset sizes (v1-v4) were not confirmed.

## 3. Teaching a small model a custom DSL: data needs and constrained decoding

### Takeaway
Grammar-constrained decoding can push syntax errors close to zero even on 2-7B models with no fine-tuning. Semantic quality is what needs data. The evidence on low-resource languages shows big gains from about 25k synthetic, test-verified examples per language. A few hundred examples counts as "low-resource" and is where constrained decoding helps most. I found no study that gives a clean 1k / 10k / 100k learning curve for a brand-new DSL.

### Cited Findings
- **SynCode (2024, TMLR 2025)** constrains decoding with an EBNF grammar. It cuts syntax errors in generated Python and Go by 96.07%. It removes JSON syntax errors entirely for Llama-2-7B-chat and Gemma2-2B-it, which were syntactically invalid on 98% and 59% of completions without it. It supports custom grammars written in EBNF. — [arXiv 2403.01632](https://arxiv.org/pdf/2403.01632); [TMLR](https://mlanthology.org/tmlr/2025/ugare2025tmlr-syncode)
- **MultiPL-T (OOPSLA 2024)** translates Python data into low-resource languages and keeps only test-verified items. It sampled 25,000 items per language. For the lowest-resource languages (Racket, OCaml), performance "increases substantially after a single epoch", while Lua, which has more existing code, needs more data for modest gains. A secondary summary says fine-tuning often more than doubled pass@1 for OCaml and Racket. — [arXiv 2308.09895](https://arxiv.org/pdf/2308.09895); [project page](https://ccs.neu.edu/~arjunguha/main/papers/cassano-multipl-t.html)
- **Grammar Prompting (NeurIPS 2023, older)** covers DSL generation with 16-32 in-context examples. Constrained decoding gave small semantic gains and needed about 3× more API calls. — [NeurIPS 2023](https://proceedings.neurips.cc/paper_files/paper/2023/file/cd40d0d65bfebb894ccc9ea822b47fa8-Paper-Conference.pdf)
- A Frontiers in AI study (2024) frames "low-resource" as a few hundred training examples. In that range, grammar-constrained decoding clearly helped encoder-decoder structure-prediction models, though not every architecture. — [Frontiers 2024](https://www.frontiersin.org/journals/artificial-intelligence/articles/10.3389/frai.2024.1406857/full)
- A survey of code generation for low-resource and domain-specific languages (2024) covers this field. — [arXiv 2410.03981](https://arxiv.org/pdf/2410.03981)
- The ManimBench result (417 examples, LoRA, 62% render success) is a data point for very small datasets. — [ResearchGate](https://www.researchgate.net/publication/399370691_Large_Language_Model_Approaches_to_Educational_Video_Generation_Using_Manim)

### Inferences
- The tutor's DSL is small and line-based (mark / link / group / morph / Say). An EBNF grammar plus constrained decoding (SynCode, XGrammar, llama.cpp GBNF) should make syntax validity almost free. The real question is whether a small model chooses the right marks, links and cues, and that needs on the order of thousands to tens of thousands of real Claude-written scenes. This is an estimate from MultiPL-T's 25k per language and ManimBench's 417 being too few, not a measured figure.

### Gaps
- I found no paper measuring syntax validity and semantic quality against training-set size (1k / 10k / 100k) for a new DSL.

## 4. Creativity and teaching quality in small fine-tuned models

### Takeaway
I found no direct evidence on whether small fine-tuned models keep pedagogical quality, such as the choice of visual metaphor or the structure of an explanation. Related evidence says diversity collapse comes from the data and format, not from size alone. Structured output formats reduce diversity, and fine-tuning moves diversity toward that of the training data. A model distilled from a narrow set of Claude scenes would therefore copy that set's templates.

### Cited Findings
- **"Fine-Tuning Fixes Mode Collapse and Over-Dispersion in LLMs" (Sep 2026).** SFT does not inherently cause collapse. The direction depends on the model and the data, and more target data moves output diversity toward the target's level. Tested on survey and code data. — [arXiv 2609.16454](https://arxiv.org/abs/2609.16454v1)
- **"The Price of Format" (2025).** Structured prompts consistently give lower semantic and topical diversity than simple prompts. — [arXiv 2505.18949](https://arxiv.org/html/2505.18949v1)
- **"Language Models are Lazy Poets" (2026).** In 1.1M poems from 16 LLMs (3B-120B), models over-rely on stock words, openings and rhymes, and scale alone does not fix it. — [arXiv 2610.24223](https://arxiv.org/html/2610.24223)
- In Code2Video, the strongest frontier model (Claude Opus 4.1) got the best TeachQuiz score, the metric for whether a video teaches. That points to teaching quality tracking model capability. — [ICML 2026 slides](https://icml.cc/media/icml-2026/Slides/65050.pdf)
- AutomaTikZ notes that frontier models produce *simpler* figures than fine-tuned models (older work). So fine-tuned models can be more detailed in a narrow domain. — [ResearchGate AutomaTikZ](https://www.researchgate.net/publication/387763274_AutomaTikZ_Text-Guided_Synthesis_of_Scientific_Vector_Graphics_with_TikZ)

### Inferences
- The choice of metaphor and the structure of an explanation are the parts that rely most on reasoning. They are also the parts where most of the ~9 s of Claude thinking goes, so a small model is least likely to help there without losing quality.

### Gaps
- No study evaluates the pedagogical quality of small vs frontier models for visual explanations.

## 5. Hybrid architectures: frontier plans, small model or deterministic system fills in

### Takeaway
The best 2025-2026 systems split the work. An LLM writes content or plans. A deterministic or specialised component handles layout and rendering. Critics check the result visually or symbolically. ALGOGEN's 99.8% vs 82.5% is the clearest number for "LLM writes semantics, templates do layout". That is the architecture this project already uses.

### Cited Findings
- **ALGOGEN (May 2026).** The LLM writes a Python tracker that outputs traces in VTA-JSON (Visualization Trace Algebra, a monoid over visual states and operations). A Rendering Style Language holds layout templates, and a deterministic renderer compiles traces into Manim, TikZ or Three.js. On 200 LeetCode algorithm-visualisation tasks it reaches 99.8% success against 82.5% for direct Manim generation, an average gain of 17.3%. It matches Code2Video's rendering success with higher aesthetic scores and "significantly shorter generation time". The motivation: asking one model to both simulate the algorithm and handle layout "causes hallucinations… overlapping elements, and inconsistencies between frames". — [arXiv 2605.12159](https://arxiv.org/abs/2605.12159); [HTML](https://arxiv.org/html/2605.12159v1)
- **Code2Video** uses Planner → Coder → VLM Critic and gains 40% over direct code generation. — [arXiv 2510.01174](https://arxiv.org/abs/2510.01174)
- **SGA** uses symbolic checking without rendering and beats a VLM critic (65.0% human preference). It works as a plug-in for existing pipelines. — [arXiv 2607.18116](https://arxiv.org/abs/2607.18116)
- **OmniManim** predicts layout (bounding boxes) first and writes code second ("see before you code"). — [arXiv 2605.15585](https://arxiv.org/pdf/2605.15585)
- **TheoremExplainAgent** depends heavily on retries: about 90% success at N=5 retries, "extremely low" at N=0. — [arXiv 2502.19400](https://arxiv.org/pdf/2502.19400)
- **Self-debug lifts small models:** VisCoder-7B Seaborn pass rate goes from 76.6 to 90.3 with self-debug. — [arXiv 2506.03930](https://arxiv.org/html/2506.03930)

### Inferences
- For this tutor, the literature points toward keeping Claude for the plan and metaphor and using small or deterministic parts for mechanical sub-tasks: expanding a short plan into full DSL lines, generating cue timings, fixing invalid lines under a grammar, or acting as a fast critic. Taking over the whole scene is the least supported option.
- Symbolic critics (SGA-style) beat VLM critics and need no rendering. The C++ engine already has the scene graph, so an overlap or legibility check there would be cheaper than a VLM pass.

### Gaps
- I found no published latency numbers for a frontier-planner + small-filler split in educational animation. ALGOGEN only says "significantly shorter", and I could not read its timing table.
