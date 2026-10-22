# Why the scene writer copies examples instead of composing primitives (research notes, 2026-10-22)

Marks: [V] = checked this session (search/fetch). [K] = from prior knowledge, URL is the standard arXiv id; spot-check before citing externally.

## 1. Exemplar anchoring / copying
- Min et al. 2022, "Rethinking the Role of Demonstrations" [K] https://arxiv.org/abs/2202.12837 -- the demos mostly convey label space, input distribution and FORMAT; correct input-label mapping matters little. Implication: what the model takes from a gold scene is its *shape*. A half-move example redefines the shape of "the convolution move", which matches what we saw when sends were dropped.
- Wei et al. 2023, "Larger LMs do in-context learning differently" [K] https://arxiv.org/abs/2303.03846 -- large models DO follow what the demos show, even over their priors (flipped labels). Stronger models copy more faithfully, which is both good and bad.
- Olsson et al. 2022, induction heads [K] https://arxiv.org/abs/2209.11895 -- the mechanism: prefix-match-and-copy circuits; a nearby matching example gets copied token-for-token.
- Ali, Wolf, Titov 2024/EACL 2026, "Mitigating Copy Bias in ICL" [V] https://arxiv.org/abs/2410.01288 -- models copy answers from demos instead of inferring the rule; identifiable "copying neurons". Pruning them improves ICL.
- Hsieh et al. 2023 (below) [V]: demos "can result in undesirable biased usage if the wrong demonstration is chosen". This is our half-move example.
- DeepSeek-R1 report 2025 [K/V-secondary] https://arxiv.org/abs/2501.12948 §Limitations: "Few-shot prompting consistently degrades its performance"; recommends zero-shot + clear format spec. Together AI docs echo it [V] https://docs.together.ai/docs/prompting-deepseek-r1 . Relevant because Sonnet with thinking is also a reasoning model.
- Anthropic, Claude prompting best practices (current, covers Sonnet 4.6-5.5) [V] https://platform.claude.com/docs/en/build-with-claude/prompt-engineering/claude-prompting-best-practices
  - "Examples are one of the most reliable ways to steer Claude's output format, tone, and structure." Make them Relevant, **Diverse: "vary enough that Claude doesn't pick up unintended patterns"**, Structured in <example> tags; 3-5 examples. (We have 9 + 50.)
  - "Providing context or motivation... Claude can generalize from the explanation." (explain the WHY.)
  - "Tell Claude what to do instead of what not to do."
  - With thinking: "Present each example as a problem, the method to apply, and the expected answer" (method, not just output). "Prefer general instructions over prescriptive steps."
  - On generalizing: "write a high-quality, general-purpose solution using the standard tools available... Tests are there to verify correctness, not to define the solution". Same idea transfers: "examples illustrate the language; they don't define the answer".
  - Frontend section: "You tend to converge toward generic, 'on distribution' outputs... make unexpected choices"; Anthropic names the convergence tendency outright and counters it in the prompt.
  - Model pages for Sonnet 5 / Opus 4.8 list "literal instruction following" as a difference: newer models do what is shown and said, literally.
- Human design fixation: Jansson & Smith 1991, Design Studies 12(1):3-11 [V-biblio] https://doi.org/10.1016/0142-694X(91)90003-F -- designers shown an example reproduce its features, **including its flaws**, even when told not to. Exact analogue of our half-move.
- GenAI design fixation: "Understanding Design Fixation in Generative AI" 2025 [V] https://arxiv.org/abs/2502.05870 ; ChatGPT-4 in a hackathon showed design fixation [V] https://arxiv.org/abs/2404.18479 .

## 2. Documentation vs demonstrations for unseen tools
- Hsieh et al. 2023, "Tool Documentation Enables Zero-Shot Tool-Usage" [V] https://arxiv.org/abs/2308.00675 -- docs-only zero-shot matched few-shot on existing benchmarks and BEAT demos on a new set with hundreds of APIs; from docs alone, LLMs re-invented Grounded-SAM / Track-Anything pipelines. This is the closest evidence that docs drive composition and demos drive imitation.
- DocPrompting (Zhou et al., ICLR 2023) [V] https://arxiv.org/abs/2207.05987 -- retrieve the relevant doc entries per intent, then generate; big gains on unseen functions.
- Gorilla 2023 [K] https://arxiv.org/abs/2305.15334 -- retrieval of API docs reduces hallucinated/wrong calls, adapts to doc changes.
- EasyTool 2024 [K] https://arxiv.org/abs/2401.06201 -- concise, unified tool instructions (purpose + when to use + params) beat long raw docs. Our 7k reference could gain from per-primitive "use it for / what it shows" lines.
- Takeaway: docs that state *purpose and effect* ("send = values travel from A to B; use when a value is computed from others") support composition; demos support format fidelity.

## 3. Compositional generalization
- SCAN (Lake & Baroni 2018) [K] https://arxiv.org/abs/1711.00350 ; COGS (Kim & Linzen 2020) [K] https://arxiv.org/abs/2010.05465 -- seq models fail to recombine known primitives in new structures.
- Levy, Bogin, Berant ACL 2023, "Diverse Demonstrations Improve In-context Compositional Generalization" [V] https://arxiv.org/abs/2212.06800 -- choosing demos *similar* to the input fails when nothing is close; choosing a *diverse set that covers the needed structures* substantially helps. Direct guidance for example selection.
- Least-to-most (Zhou et al. 2022) [K] https://arxiv.org/abs/2205.10625 ; Dynamic least-to-most (Drozdov et al. 2022) [K] https://arxiv.org/abs/2209.15003 -- decompose first, solve subproblems, ~SOTA on SCAN/CFQ with prompting. Decomposition before writing is what elicits composition.
- Self-Discover (Zhou et al., NeurIPS 2024) [V] https://arxiv.org/abs/2402.03620 -- model selects atomic modules and composes an explicit structure, then follows it; up to +32% over CoT.
- Analogical prompting (Yasunaga et al., ICLR 2024) [V] https://arxiv.org/abs/2310.01714 -- model self-generates relevant exemplars; beats manual few-shot. "Relevant or Random" [V] https://arxiv.org/abs/2404.12728 and "Not the Example, but the Process" [V] https://www.alphaxiv.org/abs/2602.15863 -- the gain comes from the generation process, not from example relevance.
- PlanSearch (Wang et al., ICLR 2025) [V] https://arxiv.org/abs/2409.03733 -- code search fails from lack of diversity; generating natural-language observations -> plans -> code diversifies solutions; gains predicted by idea diversity. Plan in prose, then code.
- Skill/abstraction libraries: Voyager [K] https://arxiv.org/abs/2305.16291 (code skills retrieved by description, composed into new skills); LILO [K] https://arxiv.org/abs/2310.19791 (auto-documented abstractions are what made the library usable); DreamCoder [K] https://arxiv.org/abs/2006.08381 . Lesson: idioms work better as named, documented *functions with a purpose* than as raw example text to imitate.

## 4. Prompt designs that reduce copying
- Varied/diverse examples (Anthropic; Levy 2023).
- Contrastive examples: Contrastive CoT [K] https://arxiv.org/abs/2311.09277 -- valid + invalid demos with an explanation of the difference improve reasoning. Use sparingly: negatives can be copied too ("pink elephant"); Anthropic says to phrase as what to do.
- Method + why per example (Anthropic "problem, method, answer"; "Claude can generalize from the explanation").
- Schematic/placeholder examples: little direct evidence; Min 2022 suggests format transfers anyway, so a placeholder skeleton conveys the *pattern* without a concrete topic to copy. Inference, not measured.
- Retrieve near examples vs none: near-example retrieval helps i.i.d. but hurts compositional splits (Levy 2023); docs retrieval (DocPrompting) helps unseen. Hsieh: zero demos + good docs is competitive.
- Rotate/randomize subsets per request: no direct paper found; follows from Levy (coverage) + Olsson (copying the nearest match). Treat as an experiment.
- Explain-then-write: PlanSearch, least-to-most, Self-Discover all support "plan the mechanism in prose, map to primitives, then write".
- Critique loop: Anthropic "verify your answer against [test criteria]" (not on Opus 5; fine for Sonnet). Our check: every spoken step has a visual event; every computed value is shown arriving.

## 5. Homogenization / diversity
- Doshi & Hauser, Science Advances 2024 [V] https://pmc.ncbi.nlm.nih.gov/articles/PMC11244532 -- AI ideas raise individual quality but collapse collective diversity.
- Padmakumar & He, ICLR 2024 [V] https://arxiv.org/abs/2309.05196 -- the instruction-tuned model (not the base model) reduced diversity.
- Artificial Hivemind (Jiang et al., NeurIPS 2025) [V] https://arxiv.org/abs/2510.22954 -- strong intra- and inter-model homogeneity in open-ended output.
- Wenger & Kenett, creative homogeneity across LLMs [V] https://arxiv.org/abs/2501.19361 .
- The Price of Format: diversity collapse from structured templates [V] https://arxiv.org/abs/2505.18949 -- rigid format templates shrink output diversity.

## Gap
I found no controlled study isolating "partial example -> model drops the omitted step". Our observation fits Min 2022 + Jansson & Smith 1991 (flaw copying). Worth a small in-house ablation (tools/eval prompts-conv style).
