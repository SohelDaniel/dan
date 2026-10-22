# ML options for "spoken but not shown" (binding) — research notes, 2026-10-22

Problem: Claude streams beats (one `Say:` + stage lines); the engine plays each beat as soon as it is written.
In ~30/44 scenes Claude says a number / step / comparison that never appears on stage (binding 3.06/5).
The plan block usually names what to show; execution drops it. `markup/binding.cpp` catches numbers,
sameness, each-own, symbols — but only in `--check` after the turn (HANDOFF §9: "teaches only the next turn").
Prior project report: `reports/Small model for tutor weak spots.md` (don't train yet; maybe a 1.5-3B binding repair).

Labels used below: [S] = stated by the source; [I] = my inference/estimate.

---------------------------------------------------------------------------------------------------------------

## 1. How explainer-video systems handle narration-visual alignment

Short version: **none of the published systems measures narration-to-visual binding the way this project does.**
They either (a) bind by construction (a script item carries narration and visuals together, compiled
deterministically), or (b) add a VLM critic for *layout*, not for "did you show what you said".

- **TheoremExplainAgent** (ACL 2025): planner -> per-scene plan (visual elements, animations, "animation and
  narration plan") -> coding agent -> Manim + TTS voiceover; up to 5 code-fix retries. [S]
  Metrics: accuracy/depth and logical flow judged on the *SRT transcript*; visual relevance and element layout on
  keyframes; visual consistency via Gemini on chunks. **No narration-visual consistency metric**; failures analysed
  are code hallucinations, LaTeX errors, generic coding errors. [S]
  https://arxiv.org/abs/2502.19400 , https://arxiv.org/html/2502.19400v2
- **Code2Video** (ICML 2026): Planner -> storyboard of sections, each "lecture lines and corresponding animations"
  (paired by construction); Coder per section; Critic = VLM with **visual anchor prompting** (6x6 grid of anchor
  points the code must use). Ablation (GPT-4.1, Aesthetics/TeachQuiz): full 79.0/82.0; w/o Planner 38.1/40.5;
  w/o visual anchor 69.2/55.2; w/o Critic 72.5/60.7. ~14-15 min and 30-43K tokens per topic. [S]
  No narration-visual sync metric reported. [S] Lesson [I]: *the planner pairing lecture lines with animations* is
  the biggest lever; the critic helps but is slow (offline only).
  https://arxiv.org/html/2510.01174v1 , https://icml.cc/virtual/2026/poster/65050
- **Paper2Video / PaperTalker** (NeurIPS 2025): alignment of *cursor* to speech via a GUI-grounding model +
  WhisperX word timings (spatial-temporal grounding). Closest published analogue to "point at what you say",
  but it grounds a pointer onto an existing slide; it doesn't add missing content. [S]
  https://arxiv.org/html/2510.05096v2
- **LASEV** (arXiv 2602.11790, 2026; deployed, >1M videos/day): "executable video script" S=(P,N,A):
  content P, narration segments N (each tied to one logical step), alignment rules A; **deterministically compiled**
  by templates. No sync metric; 92-96% "publishable" by experts; 3.58% failure/filter rate in production. [S]
  Lesson [I]: at scale, binding is solved by making every narration segment *own* a step whose visual is templated,
  not by a model. https://arxiv.org/html/2602.11790
- **ManiBench** (arXiv 2603.13251): defines "visual-logic drift" (omitted visual events, inverted order, bad timing)
  and an Alignment Score = weighted fraction of required visual events present and timed. Measured by keyword
  matching on code; **saturates (>0.9 for most models)** so it doesn't discriminate. [S] Warning for this project:
  a keyword/regex binding metric saturates or mis-correlates (cf. the project's own −0.20 lint-vs-judge
  correlation). https://arxiv.org/html/2603.13251v1
- **Manimator** (2507.14306): LLM scene description -> code LLM; no alignment metric. [S]
  https://arxiv.org/pdf/2507.14306
- **LLM2Manim** (2604.05266): human-in-the-loop; user study suggests synchronized narration + signaling lowers
  extraneous load — i.e. binding matters pedagogically. [S] https://arxiv.org/html/2604.05266
- **Training small Manim models** (arXiv 2604.18364): 17 open models <30B, SFT + GRPO + renderer-in-the-loop;
  best Qwen3-Coder-30B GRPO: 94% render success, 85.7% visual similarity (+3 pts over GPT-4.1). SFT improves code,
  GRPO improves visuals. Nothing <3B reported. [S] https://deeplearn.org/arxiv/815516/training-and-agentic-inference-strategies-for-llm-based-manim-animation-generation
- **manim-voiceover**: the de-facto Manim practice binds animation to words with `<bookmark mark="A"/>` in the
  narration and `wait_until_bookmark("A")`, timed by TTS word boundaries / Whisper. [S] This is exactly the
  project's `|cue|` + `@cue` mechanism — the project is already at the state of the art for *timing*; the gap is
  *content* (the bookmark is never placed). https://voiceover.manim.community/en/stable/api.html ,
  https://voiceover.manim.community/en/stable/
- **DiagrammerGPT** (COLM 2024): planner-auditor loop over an explicit plan (entities, relations, boxes) before
  rendering. [S] https://arxiv.org/html/2310.12128v2

Code-then-narrate vs narrate-then-code: published pipelines all plan *paired* units first (storyboard /
script item), then generate both. None narrates freely and then derives visuals. [S for the above systems; I for
the generalisation.]

## 2. Constrained / structured decoding

- Claude API structured outputs: JSON schema compiled to a grammar, **constrained decoding**, GA on Sonnet 4.5/4.6,
  Opus 4.5-4.8, Haiku 4.5. `pattern` regex supported (classes, quantifiers, groups) but **no backreferences,
  lookaround, `\b`**; no recursive schemas; enum of primitives; first-use compile latency, grammar cached 24 h,
  invalidated when schema structure changes; streaming works (accumulate then parse). [S]
  https://platform.claude.com/docs/en/build-with-claude/structured-outputs
- Strict tool use (`strict: true`) guarantees tool input matches `input_schema`. [S] Same page.
- Fine-grained tool streaming (`eager_input_streaming: true`) streams tool params without buffering/validation —
  you'd see a beat's JSON as it's written. [S]
  https://platform.claude.com/docs/en/agents-and-tools/tool-use/fine-grained-tool-streaming
- **`claude -p --json-schema` is NOT constrained decoding.** Claude Code adds a `StructuredOutput` tool, validates
  the object, and on mismatch returns `is_error` and the agent calls it again; result only in final
  `structured_output`; error subtype `error_max_structured_output_retries`. [S]
  https://code.claude.com/docs/en/agent-sdk/structured-outputs , https://code.claude.com/docs/en/headless.md
  -> On the subscription path you get *validate-and-retry at the end of the turn*, not a grammar, and not per beat.
- Open engines: XGrammar (<40 µs/token mask for JSON schema, CFG support) https://arxiv.org/pdf/2411.15100 ;
  llguidance (~1.5 ms full mask, no precompute) https://github.com/guidance-ai/llguidance ; JSONSchemaBench
  https://arxiv.org/abs/2501.10868v2 . Only usable with a *local* model, not with Claude.
- Format restrictions can hurt reasoning: "Let Me Speak Freely?" (EMNLP 2024 Industry) — stricter formats ->
  larger reasoning drops; NL-then-convert mitigates. [S] https://arxiv.org/pdf/2408.02442
  (Unknown/unverified: whether Claude's *thinking* stays unconstrained under output grammars — the docs only say
  the JSON is in the text block and reasoning should stay in thinking blocks.)

**Can a grammar enforce "a number in Say must reference a stage value"?** Not directly: "this number equals some
value already declared" is a cross-reference (context-sensitive); JSON-schema regex has no backreferences. [S for
the regex limits; I for the conclusion.] What *is* expressible:
  - Split Say into segments: `{"t": "text without digits"}` (pattern `^[^0-9]*$`) | `{"ref": "w1"}` (an id pattern).
    A grammar then guarantees every *digit* in speech comes from a reference; the engine resolves `ref` to the
    live stage value (spoken through TTS, shown on stage, binding guaranteed). Existence of the id is checked
    deterministically (or with a per-scene `enum`, at the cost of recompiling the grammar per scene).
  - Gaps: number *words* ("half", "twice", "doubles", "ten times"), comparisons ("bigger"), "they share weights" —
    a grammar can't see meaning. Spelling out numbers ("five") also escapes a digit-only pattern unless word
    numbers are also banned.
  - **The same effect needs no grammar**: make it a *language* rule — `Say: "the weight is {w1}"` where `{id}` reads
    the stage value — and have the engine lint bare digits in Say. Since the language is the project's own,
    a parse-time rule enforced by the engine is as "can't be wrong" as a grammar, minus Claude's compliance on the
    first try. [I] (Precedent for "speech rendered from structured data": LASEV's template compilation [S, above];
    `\val{}` already exists in this engine for formulas — binding.cpp line ~75.)
  - Requires moving off `claude -p` to the API (API key, pay per token) to get real constrained decoding. [S/I]

## 3. Verify-and-repair loops; small local models

- **Self-Refine**: same LLM generates, critiques, refines; ~20% absolute avg gain over 7 tasks (GPT-3.5/4). [S]
  https://arxiv.org/abs/2303.17651 , https://proceedings.neurips.cc/paper_files/paper/2023/hash/91edff07232fb1b55a505a9e9f6c0ff3-Abstract.html
- **Huang et al., "LLMs Cannot Self-Correct Reasoning Yet"**: intrinsic self-correction without external feedback
  doesn't reliably help; external feedback does. [S] https://arxiv.org/pdf/2310.01798
- **CRITIC** (ICLR 2024): verify with tools, then revise; consistent gains; time overhead linear in iterations;
  one iteration already brings most of the benefit. [S] https://arxiv.org/pdf/2305.11738
  -> The binding checker *is* the external tool here; the research favours "deterministic checker feedback" over
  "ask the model to reflect". [I]
- Reflexion (verbal feedback memory across episodes) https://arxiv.org/abs/2303.11366 ; process reward models
  ("Let's Verify Step by Step", step-level verifiers) https://arxiv.org/abs/2305.20050 — both need many
  trajectories/labels; overkill here. [I]
- **Apple M5 vs M4 (MLX)**: TTFT 3.3-4.1x faster (neural accelerators); generation ~1.2-1.27x (bandwidth 153 vs
  120 GB/s). Qwen3-1.7B bf16: 3.57x TTFT, 1.27x gen; 4096-token prompt. [S]
  https://machinelearning.apple.com/research/exploring-llms-mlx-m5
- M4 (base, 16 GB) community: Qwen2.5-1.5B-Instruct ~1,000 prompt tok/s, 31-95 gen tok/s (two runs disagree 3x). [S,
  weak] https://omlx.ai/benchmarks/v1ev5911
- **Estimate for base M5 [I]**: 1.5-1.7B 4-bit: ~3,000+ prompt tok/s, ~40-120 gen tok/s. A repair call with
  ~800 tokens of context (stage facts + beat + plan line) and ~25-40 output tokens (insert 1-2 lines):
  ~0.25 s prefill + ~0.3-0.8 s decode = **~0.5-1 s**. A 7-8B 4-bit: roughly 3-4x slower decode -> 1.5-3 s.
  **Key trick [I]:** the number is spoken at a specific word, typically 1-3 s into the sentence. Play the beat
  immediately and deliver the repair as an `@cue` line keyed to that word: if it lands before the word is spoken,
  the added latency is ~0. If it lands late, drop it.
- LoRA on a Mac with mlx-lm: feasible; 4-bit 7B LoRA fits in ~8 GB; no clean 1.5B timing found. [S]
  https://insiderllm.com/guides/fine-tuning-mac-lora-mlx/

## 4. Grounding / entailment models

- **NLI cross-encoders**: nli-deberta-v3-xsmall 87.8% MNLI, -base 90.0% [S]; MS MARCO MiniLM-L6 cross-encoder
  ~1,800 pairs/s (hardware unstated) [S]. https://www.sbert.net/docs/cross_encoder/pretrained_models.html
- **MiniCheck** (EMNLP 2024): 355M/770M fact checkers ≈ GPT-4 on LLM-AggreFact (74.7 vs 75.3 BAcc), ~400x
  cheaper; trained on ~14K synthetic examples made by **deleting a supporting sentence to create "unsupported"**
  (same trick as corrupting beats). [S] https://arxiv.org/html/2404.10774v2
- **HHEM-2.1-open**: flan-t5-base (~0.1B), ~1.5 s per 2k tokens on CPU, <600 MB. [S]
  https://huggingface.co/vectara/hallucination_evaluation_model
- **But NLI models are bad at numbers**: EQUATE — 9 NLI models on quantitative NLI averaged no better than
  majority class; a symbolic quantity reasoner beat them by 24 pts on numerics. [S]
  https://aclanthology.org/K19-1033/ , https://www.arxiv.org/pdf/1901.03735
  ALCE: NLI-based citation checking misses *partial* support (high false positives). [S] https://arxiv.org/pdf/2305.14627
  -> For **numbers**, a symbolic matcher (what binding.cpp already does) beats NLI. NLI/cross-encoder only adds
  value for the *non-numeric* claims ("they share weights", "this one gets bigger") — and stage-as-text facts must
  include relations (same colour, equal labels, which arrow grew) for that to work. Expect noisy precision (MiniCheck-
  class ~73-75% BAcc on in-domain-ish data [S]; out of domain lower [I]). Too noisy to block a beat; OK as a hint.
- Text-to-scene-graph: FACTUAL (ACL Findings 2023) T5 parsers turn a sentence into (entity, attribute, relation)
  triples; could map "the bigger weight goes to the closer word" into a check. [S]
  https://arxiv.org/pdf/2305.17497 ; package https://github.com/coder-drinker/FactualSceneGraph
- **Attribution framing** [I]: binding = "every claim cites a stage element". ALCE found even strong models lack
  full citation support ~50% of the time on ELI5 [S] (https://arxiv.org/pdf/2305.14627) — consistent with the
  observed 30/44: free-form generation drops citations; requiring a citation slot in the format raises it.

## 5. "NN in the engine's backbone" options

(a) **Engine derives the picture from the speech (text-to-visual).** A model maps each sentence's claims to stage
    ops. Honest assessment: deciding *what* to draw is the judgement the prior report says only a frontier model does
    well; a small model trained on 335 scenes learns templates (see prior report). It *can* work for a narrow slice:
    spoken number -> label/value on the right element; "X equals Y" -> value on X. Data: a few thousand
    (sentence, stage, op) triples (synthesisable, §6). Latency ~0.5-1 s on M5 1.5B, hidden by cue timing. Failure
    modes: wrong target element (a label on the wrong box is worse than none), clutter, labelling metaphors.
    Deterministic version first: number in Say not on stage -> attach to the element the beat just touched / the
    `focus` target. [I]
(b) **Learned critic** (cross-encoder/NLI over stage facts): fast (ms-10s of ms), but weak on numbers (EQUATE) and
    noisy on relations; the current lint already disagrees with the judge (−0.20). Needs labelled data the user
    hasn't produced. Use only as a ranking hint. [I]
(c) **Learned binding repair** (1.5-3B LoRA, input: plan line + stage facts + beat + checker complaint; output: 1-2
    inserted lines). Most plausible NN. Data: 2-5K pairs (§6). Cost: $0 local LoRA on M5 (hours), or $20-60 cloud
    GPU. Latency 0.5-1 s, off critical path via `@cue`. Failure: picks wrong element, invents ids (validate against
    engine — reject unknown ids), adds labels for incidental numbers. Quality ceiling [I]: good on numbers (most of
    the checker's catches), poor on steps/processes ("the blend step has no weighted output shown").
(d) **Distil Claude into a local scene writer**: rejected by prior report (lose breadth/accuracy; slower on base M5;
    small fine-tuned Manim models trail frontier pipelines). Nothing found in 2026 changes that; the best small-model
    result is a 30B with GRPO. [S: 2604.18364] Not viable on a base M5 at interactive latency. [I]
(e) **(Not NN) Claude-in-the-loop per beat via a tool** — beyond the obvious: expose a `beat` MCP tool via
    `--mcp-config`; Claude emits each beat as a tool call; the tool handler runs `binding.cpp` and returns
    `is_error` with the complaint, so Claude repairs *that beat* before it plays — the CRITIC pattern with the
    existing checker as the tool, which is exactly how Claude Code's own StructuredOutput retry works [S, §2].
    Tool input streams with `--include-partial-messages` (input_json_delta), so the stage can still start early
    [S/I]. Cost: every tool call is a new model request; even with prompt caching expect ~1-3 s per round trip [I,
    unmeasured]; reject only on failure. Risk: changes the streaming protocol; thinking may restart per call.
    **Measure before adopting.**

## 6. Training-data generation

- Precedents: GEC pretraining on synthetic corruptions (PIE: 0-4 random errors per sentence over 1BW; GECToR:
  synthetic pretrain then real fine-tune) [S] https://arxiv.org/pdf/1910.02893 , https://arxiv.org/pdf/2005.12592 ;
  but synthetic corruptions differ from real human errors and cause odd edits [S]
  https://aclanthology.org/2023.bea-1.38.pdf . **Break-It-Fix-It** (ICML 2021): a critic (compiler) verifies the
  fixer's outputs on *real* bad inputs and a learned breaker makes realistic bad inputs; iterate. [S]
  https://arxiv.org/pdf/2106.06600 . MiniCheck's "delete the supporting sentence" synthesis [S, §4].
- Recipe for this project [I]:
  1. Positives: every beat in all ~335 scenes where binding.cpp passes and the number is shown (not only the 36
     all-good scenes) — likely 1-2K beats.
  2. Corrupt: delete the `mark`/label/`\val`/`@cue` lines that carry a spoken number (the checker knows which) ->
     (broken beat, complaint) -> target = deleted lines. Also move a label to the wrong id for negatives.
  3. Real failures (BIFI step): the ~41-54 real binding lints per 44-scene run; have Claude (subscription, offline)
     write the fix, keep only fixes the engine parses and the checker passes. This anchors the distribution to
     Claude's actual misses, which synthetic deletion won't reproduce (those are *never-written*, not deleted).
  4. Hold out by question (not by beat) — 44 distinct questions is the real sample size.
  Caveat: the checker defines the label, so the model can only learn what the checker already knows; it adds
  *where/how* to show, not *whether*. For "what to show", the plan block is the best spec available.

## 7. Ranked options

| # | Option | Fixes | Latency/beat | Cost | Data | Worth it when |
|---|---|---|---|---|---|---|
| 1 | **Language: speech reads the stage** (`{id}` refs in Say resolve to stage values; bare digits in Say = parse lint; engine shows the referenced value when spoken) | Numbers by construction (the largest checkable share) | 0 | engine work, days | none | Now. It is the "can't be wrong" foundation; no NN or grammar needed. |
| 2 | **Per-beat check + deterministic auto-bind** (run binding.cpp as each beat arrives; spoken number not on stage -> attach label to the beat's focus/last-touched element as an `@cue` on that word; plan "show:" vs beat diff) | Numbers Claude still writes bare; plan drift flagged | ~0 (ms) | days | none | Now, alongside #1. |
| 3 | **Beat-as-tool with checker feedback** (MCP tool rejects failing beats, Claude repairs in-turn) | Numbers + anything the checker can phrase; uses Claude's judgement for *what* | +1-3 s only on failing beats (unmeasured) | engine/server work; tokens on subscription | none | If #1-2 leave binding <3.5 and a 1-3 s hiccup on bad beats is acceptable. Prototype + measure. |
| 4 | **Small local repairer** (1.5-3B LoRA on MLX, inserts 1-2 lines, ids validated, delivered on the cue word) | Where to place a missing number/value; tolerant of Claude's phrasing variants | ~0.5-1 s, hidden behind speech | $0 local / $20-60 cloud; ~1-2 days | 2-5K pairs via §6 | Only if #2's placement heuristic is visibly wrong often (measure: % auto-binds judged misplaced). |
| 5 | **NLI/cross-encoder critic for non-numeric claims** | "share", "bigger", "same" claims — as hints | 10-50 ms | free models | a few hundred user labels to calibrate | Only after the user labels some scenes; never as a blocker (noisy, bad at numbers). |
| 6 | **API + constrained decoding** (Say as segments, digits only via refs) | Same as #1 but enforced at decode time | grammar compile first use; risk to reasoning quality | leave subscription: pay per token | none | Only if Claude ignores #1's rule often and the API cost is acceptable. Probably never. |

Won't work / not worth it: distilling Claude into a local scene writer (d); a grammar for semantic claims; NLI as a
number checker; a learned "picture from speech" model for anything beyond numbers/values on existing elements.
