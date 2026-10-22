# Should we train a model? Research notes (2026-10-22)

The setup: Claude (subscription, so it cannot be fine-tuned) writes the DSL. We have deterministic checkers (parse, decodability, axiom lints, spoken-but-not-shown) and a VLM judge (per-sentence binding, plus layout, accuracy and teaching scores). The problem is that Claude imitates the examples instead of composing primitives. Prior results here: an editor model and a translator model both hurt layout, and untrained Qwen3-8B produced valid DSL for about 53% of beats.

## 1. SFT memorizes, RL generalizes: what the evidence says
- Chu et al., ICML 2025 (https://arxiv.org/abs/2501.17161). With outcome rewards, RL generalized to unseen rule and visual variants, while SFT memorized. The tasks were toys (GeneralPoints card arithmetic and V-IRL navigation). The paper also says SFT is still needed first, to stabilize the output format so that RL can work.
- RL's Razor (Shenfeld et al., ICLR 2026, https://arxiv.org/abs/2509.04259). On-policy RL forgets less than SFT because it stays close to the base model in KL.
- Counterweight: Yue et al., NeurIPS 2025 oral (https://arxiv.org/abs/2504.13837). RLVR raises pass@1, but the base model wins at large pass@k. RL sharpens behavior the base model can already sample. It does not create new reasoning patterns. **Implication: RL only helps if the base model sometimes composes correctly. Measure that first (pass@k with our verifiers).**
- Breaking the SFT plateau, chart-to-code (https://arxiv.org/abs/2508.13587). Adding more SFT data gives diminishing returns. Adding RL with rule-based and visual rewards then gave +6.2% (ChartMimic) and +9.9% (ReachQA).
- ChartMaster (https://arxiv.org/abs/2508.17608). GRPO with a chart-similarity reward made a 7B model rival GPT-4o on chart-to-code. Caveat: chart-to-code has a reference image, and we have none.
- Text-to-SQL:
  - Reasoning-SQL (https://arxiv.org/abs/2503.23157) used GRPO with partial rewards. RL-only training beat SFT at every model size, and a 14B model beat o3-mini by 4 points on BIRD.
  - SQL-R1 (NeurIPS 2025, https://neurips.cc/virtual/2025/poster/116624).
  - A 3rd-party Qwen3-4B SFT-then-GRPO run gained only +2.3 points on Spider (https://huggingface.co/j2521402/SQL-R1).
- VisCoder (EMNLP Findings 2025, https://arxiv.org/abs/2506.03930) used SFT on 200K plot examples plus self-debug dialogues. It approached GPT-4o-mini, not frontier models.
- Manim and animation, the closest analogue:
  - ManimTrainer/ManimAgent (Rammuni Silva et al., 2026, https://deeplearn.org/arxiv/815516/training-and-agentic-inference-strategies-for-llm-based-manim-animation-generation) ran SFT then GRPO with a reward fusing code and visual signals, over 17 open models under 30B.
    - SFT improved code quality. GRPO improved the visuals and made the model respond better to renderer feedback.
    - Best result: Qwen3-Coder-30B with GRPO and a documentation-augmented renderer loop, at 94% render success and 85.7% visual similarity, 3 points above GPT-4.1.
    - The metric is similarity to reference videos, not open-ended teaching.
  - SimuScene (https://arxiv.org/abs/2602.10840) used a VLM verification-question reward on rendered videos. The best of 10 models scored 21.5% Avg@8. Training helped, but the abstract gives no numbers.
  - OmniManim (https://arxiv.org/abs/2605.15585) finds spatial layout is the bottleneck even with multi-agent setups, which matches our editor and translator results.
  - LLM2Manim (https://arxiv.org/abs/2604.05266) uses prompt constraints plus targeted regeneration, with no training. It reports learning gains d=0.67.
- Low-resource DSL: TileLang (https://cs224r.stanford.edu/projects/pdfs/224R_Final_Report%20(2).pdf) used synthetic data, then DPO, then RLVR. Compile errors fell from 91% to 54%, but semantic pass@5 stayed under 2%. **Validity is easy to train; meaning is not.**
- DSL survey (https://arxiv.org/abs/2410.03981).

## 2. RL-training Qwen3 8B/14B/32B on our reward
- Reward: engine checks as a gate (0 if parse fails), plus binding-judge fraction, layout score, and penalties for element count and length.
- Recipe: a small SFT for format, as R1 and Chu do, then GRPO.
- LoRA is enough for RL, even at rank 1: Thinking Machines (https://thinkingmachines.ai/blog/lora/), reproduced in TRL (https://huggingface.co/docs/trl/main/en/lora_without_regret).
- Tools:
  - TRL GRPOTrainer
  - Unsloth GRPO (https://unsloth.ai/blog/grpo): a single GPU, roughly 8 GB for 8B QLoRA at short context.
  - verl, OpenRLHF, prime-rl
  - Tinker managed API (https://thinkingmachines.ai/tinker/pricing/): Qwen3-8B costs about $0.40-0.44 per million training tokens per https://www.beam.cloud/blog/tinker-model-pricing (unverified).
- Compute data points:
  - Full GRPO on Qwen3-8B took 8xH100 for about 5 days per run in one paper (https://arxiv.org/pdf/2604.28020).
  - Another GRPO run took about 20 h on 8xH200.
  - H100 rents for about $2.5/h.
- My estimate for one LoRA GRPO attempt on 8-14B, assuming 300 steps x 16 prompts x 8 samples, about 38K rollouts:
  - GPU: 1-2 H100 for 1-3 days, about $100-400.
  - Vision judge calls: 38K x 3-10 judged frames at roughly $0.01-0.03 each, about $500-2,000. **The judge, not the GPU, is the cost driver.**
  - Expect 3-5 attempts to tune the reward. Realistic total: $2-8K and 4-8 weeks of engineering.
  - 32B needs multiple GPUs and roughly 3x the cost.
- Data: prompts only, no answers. That means 500-2,000 diverse teaching questions, with held-out *topic families* so that composition is actually tested.
- Reward hacking (rubric/judge RL hacking: https://arxiv.org/abs/2606.04923; MSRL chose a 72B judge to resist it):
  - Predictable exploits:
    - Showing a literal text label for every spoken noun (binding passes, teaching worse)
    - Clutter
    - Saying less so less needs showing
    - Degenerate safe layouts
    - Judge prompt sensitivity
  - Mitigations:
    - Dual judges, with a held-out judge for evaluation
    - Deterministic penalties on text-element share and element count
    - KL to the base model
    - Human spot-checks every N steps
- Gap to Claude: a tuned 8-32B can likely reach above 90% valid DSL and do well on *binding* metrics within the distribution it was trained on. On open-ended teaching (correct science, choosing the right picture, pedagogy) it will very likely stay below Claude Sonnet. No 2025-26 paper shows a tuned model under 32B beating frontier models on open-ended educational animation, only on reference-matched metrics. "Beats Claude" is unlikely at any horizon for the whole tutor. For the narrow composition sub-task it is plausible but unproven.

## 3. Search without training (works with Claude)
- Verifier-guided best-of-N for code works because execution is a strong verifier (https://arxiv.org/abs/2602.04254). LLM-judge reranking plateaus earlier than execution-checked reranking.
- The ManimAgent result shows a renderer-in-the-loop self-correction plus documentation loop is a large part of the gain even without training.
- In a live tutor:
  - Deterministic checks are milliseconds, so check each beat as it streams, and re-ask only the failing beat ("repair this beat; error X").
  - Best-of-N on a whole streamed answer costs N times the tokens and delays the first word. Do it per beat, pipelined: while beat k is spoken, generate and check beat k+1, with 2 samples if the check fails.
  - The vision judge (seconds) is too slow for the live path. Use it offline, or asynchronously for the next beat.
- Subscription rate limits cap N.

## 4. DPO from redo/accept
- Not possible on Claude. It is possible on a local model.
- Implicit-feedback DPO is noisy:
  - Mixed results from WildChat/LMSYS logs (https://aclanthology.org/2025.emnlp-main.133.pdf).
  - Proxy signals push models toward bad behavior (Pang et al. 2023, https://arxiv.org/abs/2307.14117).
- Redo/accept data needs volume (thousands of pairs) and confound control: a redo may be about pacing, not visuals.
- Best use right now: log redo and accept as eval labels, and as a source of negative and positive examples for the prompt.

## 5. Hybrid: Claude plans, a small model composes
- Evidence is thin:
  - Plan-cued small coder (https://arxiv.org/abs/2403.13271).
  - Manimator's two-stage describe-then-code (https://arxiv.org/abs/2507.14306).
  - SPEAC: target an intermediate language the LLM already knows, then compile (https://people.eecs.berkeley.edu/~sseshia/pubs/b2hd-mora-neurips24.html).
- Our own editor and translator experiments failed at layout, and OmniManim says layout is the bottleneck. A third model in the loop adds a handoff where binding between speech and picture gets lost, because the picture must match Claude's sentences.
- The only defensible version: an RL-trained composer that takes an explicit structure spec from Claude ("array x, kernel over x[0:3], slide") and emits DSL, judged by the engine. That moves the composition problem into Claude's spec language, so it is not obviously a win.

## Verdict
See the final report. In short: don't train yet.
1. Measure pass@1 against pass@k (k=8) for Claude, plus Qwen3-8B and 14B, on about 200 held-out novel-topic prompts, scored by the checkers and the judge. This is the go/no-go for RL (Yue et al.) and the eval for everything else.
2. Ship per-beat check-and-repair plus pipelined best-of-2 for Claude.
3. Fix imitation at the prompt level: fewer whole-scene examples, more primitive-composition snippets, varied examples drawn from failure data.
4. Only if the base model's pass@k is much higher than its pass@1 on composition, run one Unsloth or Tinker LoRA GRPO pilot on Qwen3-14B for the narrow beat-composition job (about $1-3K, 3-4 weeks). Compare it with Claude plus repair on held-out topics before adopting it.
