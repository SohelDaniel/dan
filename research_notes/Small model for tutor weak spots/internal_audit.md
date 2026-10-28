# Internal audit: what Claude gets wrong in the live visual tutor (local data only)

Scope: only local data. That means 308 judged eval scenes (7 runs × 44 broad questions) plus 24 from eval1/eval2 and 3 from show2, 39 assistant turns in real chats (23 sessions), and 38 Claude Code transcripts of those chats. All scores use the v2 judge rubric (Sonnet, `--effort low`, scale 1-5). The judge reads the script, the engine's `check.txt` and 2-3 frames. Analysis scripts are in `<scratchpad>/audit/` (agg.py, lints.py, cats.py, all.json). No project files were modified.

Source shorthand:
- SP = `<scratchpad>`
- REPO = `<repo>`

Runs:
- latA = current prompt.
- latB = "don't deliberate" variant.
- latC = no ```plan block.
- broad..broad4 = earlier iterations of the prompt and engine on the same 44 questions.

## Which rubric dimensions are lowest, and on which subjects and question types?

### Takeaway
**Binding** (narration states something the picture doesn't show) is the lowest dimension in every run: 3.06 mean over 308 scenes. It is followed by layout (3.43), picture (3.48), teaching (3.62) and accuracy (3.63). Fit (3.98) and natural (3.93) are effectively solved.

By subject:
- CS algorithms are strongest: recursion, dijkstra, tcp, bayes and passive all average about 4.0-4.25 over 7 runs.
- **Chemistry, biology and physics "draw the physical thing" questions** are weakest: bonds 2.98, refraction 3.04, circuits 3.08, ph 3.22, dna 3.24, heart 3.27.
- Geometric proofs and plots are also weak: pythagoras 3.12, stddev 3.29, catalyst 3.31, pendulum 3.33.
- "Why" questions are the weakest question type in latA.

### Cited Findings
- latA (44 questions), mean scores: fit 4.00, binding 3.18, natural 3.86, picture 3.50, layout 3.52, accuracy 3.70, teaching 3.68. checkOk 41/44, 41 binding lints, mean generation time 19.0 s, 8.0 Say lines. — [SP/latA/summary.json](file://SP/latA/summary.json)
- Score distribution in latA:
  - binding scored exactly 3 in **32/44** scenes and ≤2 in 2;
  - picture: 20 threes, 2 at ≤2;
  - layout: 14 threes, 4 at ≤2;
  - accuracy: 17 threes, 2 at ≤2.

  — [SP/audit/agg.py output over SP/latA/full/*/score.json](file://SP/audit/agg.py)
- Pooled over all 308 broad scenes (7 runs): fit 3.98, binding 3.06, natural 3.93, picture 3.48, layout 3.43, accuracy 3.63, teaching 3.62. — [SP/audit/all.json](file://SP/audit/all.json)
- latA by subject:

  | Subject | n | picture | binding | accuracy | teaching | layout |
  |---|---|---|---|---|---|---|
  | Biology | 5 | **2.80** | 2.80 | 3.20 | 3.40 | |
  | Chemistry | 4 | 3.25 | | 3.25 | | |
  | Physics | 7 | 3.29 | 3.00 | 3.43 | 3.43 | |
  | Math | 10 | | 3.00 | | | **3.10** (lowest) |
  | CS (non-ML) | 7 | 4.14 | | 4.43 | 4.14 | |

  — [SP/audit/agg.py](file://SP/audit/agg.py)
- latA by question type (n=11 "why" questions): fit 3.73, binding 2.91, picture 3.27, accuracy 3.27, teaching 3.27, all lowest among the types. The 16 "how" questions do best (teaching 4.00). — [SP/audit/agg.py](file://SP/audit/agg.py)
- Per-question mean of all 7 dimensions, averaged over 7 runs.
  - Worst 12: bonds 2.98, refraction 3.04, circuits 3.08, pythagoras 3.12, ph 3.22, dna 3.24, heart 3.27, selection 3.29, stddev 3.29, catalyst 3.31, pendulum 3.33, quicksort 3.33.
  - Best: recursion 4.24, bayes 4.10, tcp / passive / dijkstra 4.02, interference 3.96.

  — [SP/audit/all.json](file://SP/audit/all.json)
- Only **36/308 (11.7%)** broad scenes score ≥4 on every dimension. 79/308 (25.6%) have at least one dimension ≤2. In latA only 4/44 are all ≥4 (dijkstra, git, hashtable, recursion). — [SP/audit/agg.py](file://SP/audit/agg.py)
- Some failures recur on the same question across runs. These are stable weak spots, not judge noise:
  - heart: "four boxes with no vessels, valves or arrows" (latA). "Just labeled boxes with a dot… ignores real anatomy" (broad4). "A crude grid of boxes" (latC).
  - vaccine: "generic box-and-arrow flow" in latA, latC and broad4.
  - overfit: the wiggly curve doesn't miss the new point enough in latA, latC and broad4.

  — [SP/latA/full/heart/score.json](file://SP/latA/full/heart/score.json), [SP/broad4/full/heart/score.json](file://SP/broad4/full/heart/score.json), [SP/latC/full/heart/score.json](file://SP/latC/full/heart/score.json)

### Inferences
- The strong areas are abstract, discrete structures: graphs, trees, sequences and arrays. The engine's graph/flow layout handles these well, and Claude only has to name nodes and links.
- The weak areas are continuous or physical depictions: anatomy, molecules, optics, circuits and geometric rearrangements. Claude has to invent a spatial depiction out of primitive dots, lines and polygons, and the vocabulary has no domain idioms for these (no heart, molecule, ray or circuit templates).

### Gaps
- n per subject is small: 1-10 questions per subject per run, and history, language and music have 1 question each. Subject rankings outside math, physics, CS and biology are not reliable.
- The judge's noise is about ±0.15 per mean (HANDOFF §5). Subject differences under ~0.3 should not be trusted.

## Failure taxonomy: counts across the 44 latA questions, and who can fix each

### Takeaway
I hand-coded all 44 latA judge notes (multi-label). The most common failure is **"spoken but not shown"**: a step, claim or number is narrated and never drawn. It appears in 30/44 scenes.

The next most common:
- overlap/label placement: 20/44, mostly the engine's job;
- generic or weak depiction: 17/44 (10 strictly "boxes instead of the thing");
- Claude's own **numeric/spatial computation errors**: 12/44;
- leftover clutter at the end: 12/44;
- colour binding: 11/44;
- cue/order timing: 8/44;
- spoken factual errors: 4-6/44.

A keyword classifier over the other 6 runs gives the same ranking.

### Cited Findings
Hand-coded categories (latA, n=44, a scene can carry several). Quotes are from [SP/latA/full/<q>/score.json](file://SP/latA/full) unless another run is given.

1. **Spoken but not shown (binding): 30/44.**
   - Spoken numbers specifically: about 10 scenes (bayes, entropy, primes, projectile, compoundint, ww1, kmeans, bonds, tcp, neuron).
   - A process or step narrated but not animated: about 20.
   - Quotes:
     - projectile: "Time (2.9 s) and gravity (9.8) are narrated but never shown."
     - git: "The conflict explanation is narration-only, with nothing drawn on stage."
     - log: "The doublings are never shown as steps (no ×2 arrows between cells)."
   - Who fixes it: partly **(a)**. The `binding:` lint already exists:
     - number-word lints: 23 in 13 questions;
     - "nothing on stage changes" lints: 10 in 10;
     - "same/shared but no shared colour" lints: 8 in 5.

     But it only feeds the *next* turn (HANDOFF §9), and its correlation with the judge's binding score is weak (r = −0.20 over 308 scenes). Deciding *what* to draw for a process ("show the doubling") is **(b)**.
2. **Overlap, label placement, detached labels, cramped or clipped: 20/44.**
   - Quotes:
     - eigen: "The 'stretched ×3' and 'stretched ×2' labels sit off their arrows… Pointer arrows… clash with the axis numbers."
     - stddev: "A's dots stack and their labels collide."
     - kmeans: "The formula slide shrinks and shifts the plot and axes."
   - Who fixes it: mostly **(c) engine**. Claude gives no coordinates for labels.
     - The checker flags some: `layout:` overlap lints, 15 in 7 latA questions.
     - Crowding lints (>6 things): 4.
     - The geometric layout score correlates only r = 0.36 with the judge's layout score (mean geometric score 97.4/100 vs judge 3.43/5).
3. **Generic or weak depiction: 17/44 broadly, 10 strictly** (balancing, bonds, circuits, dijkstra, dna, heart, ph, selection, vaccine, ww1).
   - Quotes:
     - heart: "The picture is four boxes with no vessels, valves or arrows."
     - vaccine: "a generic box-and-arrow flow with no germ, antibody or lock-and-key imagery."
     - ph: "three boxes in a row, not a ladder."
   - Who fixes it: **(b)**. This needs domain knowledge and choosing a metaphor. An engine-side library of domain idioms (heart/vessel, atom/electron shell, ray diagram, circuit loop) would turn part of it into **(a)/(c)**: the model only has to pick the idiom.
4. **Numeric/spatial computation errors by Claude: 12/44.**
   - Plot or curve coordinates (7): catalyst, integral, neuron, overfit, supply, bigo, chords.
   - Geometry or vectors (4): pendulum, pythagoras, refraction, falling.
   - A physics value (1): circuits.
   - Quotes:
     - supply: "At price 5 the shifted supply curve gives 2 apples, not 3.5." Claude hand-wrote `mark gap line x1=3.5 y1=5 x2=5 y2=5` (supply/played.txt line 16).
     - catalyst: "The green 'smaller Eₐ' bar sits at x=6.5, away from the peak." Hand-written `x1=6.5` (catalyst/played.txt line 13).
     - pendulum: "The pull vectors are not along the path (tangent)."
     - integral: "The sliver sits at x=5 to 5.4 while A stays fixed at 4."
   - Who fixes it: **(a)**. A program can compute intersections, peaks, tangents and plot ranges if the markup lets Claude *refer* to them (`at=intersect(d,sup,y=5)`, `peak of f`, `tangent of path at t`) instead of typing numbers.
5. **Leftover clutter or a muddled final frame: 12/44.**
   - Quotes:
     - pythagoras: "The final frame is a muddle: faded leftover shapes and overlapping labels."
     - quicksort: "The end frame still shows leftover '3: smaller' and 'pivot' labels."
     - fourier: "Hidden components still show as faint ghosts."
   - Who fixes it: mostly **(a)**. Auto-expire annotations at the end of their sentence, cap ghosts, add a lint for "annotation never hidden". Partly **(c)**: ghost rendering is an engine choice.
6. **Colour binding (formula symbols not coloured like their things, colours without meaning): 11/44.**
   - Quotes:
     - attention: "The formula's w_i is coloured orange but nothing in the picture matches it."
     - log: "The formula isn't colour-bound to the cells."
     - stddev: "Mean arrows for A and B are different colours."
   - Who fixes it: **(a)**. Deterministic: `\textcolor{id}` names can be cross-checked against things on stage (`--check` already validates the names), and things can be auto-coloured by shared identity.
7. **Cue/order timing (drawn before or after it is spoken): 8/44.**
   - Quotes:
     - neuron: "The curve is drawn in full before the narration reaches it."
     - quadratic: "The parabola sits on stage before the algebra."
     - kmeans: "The narration says the blue centre moves first, but the orange c1 moves first."
   - Who fixes it: **(a)**, partly.
     - `hook:` lints exist: in latA, 6 "tag never spoken" and 5 "spoken but no line tagged", in 10 questions.
     - Claude naming a colour the engine assigned ("blue" when the engine drew orange) is an interface problem. Claude doesn't pick most colours, so a lint could match colour words against assigned colours: **(c)/(a)**.
8. **Spoken factual or scientific inaccuracy: 4 clear cases (circuits, ph, primes, quadratic), 6 counting looser claims (bigo, quicksort).**
   - Quotes:
     - ph: "'Acid strength' is wrong (it's H⁺ concentration)."
     - quadratic: "'Minus two and minus three' is mis-stated."
     - circuits: "Series current is wrong… the numbers mislead."
   - Who fixes it: **(b)**, and it needs broad knowledge. Note that most accuracy deductions (19/44 scenes at accuracy ≤3) come from *visual* errors (category 4), not from wrong words.
9. **Stock or ritual ending: 4/44** (catalyst, chords, passive, quadratic): "The closing question is a stock ritual."
   - Who fixes it: **(a)/(b)**. A cheap lint for phrases like "Want to try", "drag yourself"; whether a question helps is a judgement call.
10. **Markup misuse or parse errors: 1/44 scene broken** (selection: 4 × "unknown verb 'check'/'cross'").
    - Claude wrote `@tall check of=g3`, but the language is `mark k check of=answer` (README line 96-97). The judge: "The check and cross verbs failed, so the key steps… never appear."
    - In the other runs, parse errors appear in only latB (5 in 2 questions) and latA (4 in 1). Examples: unbalanced expression `{-55 + 100*sin(t*pi/1.2)...}: missing )`, and `unexpected 'link' in link`.
    - Who fixes it: **(a)**. A forgiving parser could accept a bare annotation verb as `mark auto <verb>`. Expression brackets can be checked before the beat plays.
11. **Incomplete answer: 1-2/44** (clt: "Only the setup is delivered… with no offer to continue"; 3 Say lines, 29.4 s). — [SP/latA.log](file://SP/latA.log)
    - Who fixes it: **(a)** to detect (Say count vs plan), **(b)** to fix.
12. **Engine or harness rendering bugs: 3/44.**
    - catalyst: "The Eₐ labels render as 'E?'". The font lacks U+2090; Claude wrote `label="Eₐ"`.
    - interference: "The final formula shows -7.35e-06". `\val{2*cos(phi/2)}` is not rounded.
    - git: "Debug caption text appears at the bottom of the frames."
    - Who fixes it: **(c)**.
13. **Annotation on the wrong target: 2/44** (balancing: "red highlight… sits on the header row"; fractions: "The 'bigger piece' checkmark sits over the second third"). Also an engine size issue: fractions bars drawn at different widths, from `area parts=[1,1,1]` vs `[1,1,1,1]` (fractions/played.txt line 2-3).
    - Who fixes it: **(a)/(c)**.

Cross-run comparison (regex classifier over judge notes, approximate; hand-coded latA is the reference):

| category | latA | latC | broad4 | latB | broad | broad2 | broad3 |
|---|---|---|---|---|---|---|---|
| spoken not shown | 26 | 27 | 24 | 24 | 24 | 25 | 21 |
| overlap/placement | 21 | 19 | 19 | 13 | 16 | 14 | 13 |
| clutter/leftover | 12 | 13 | 9 | 11 | 14 | 13 | 13 |
| colour binding | 11 | 11 | 10 | 6 | 8 | 6 | 12 |
| generic picture | 10 | 10 | 5 | 6 | 10 | 9 | 7 |
| numeric/spatial | 17* | 8 | 9 | 10 | 11 | 10 | 12 |
| factual | 9* | 5 | 4 | 6 | 5 | 5 | 3 |

\*The regex over-counts against the hand coding (12 numeric, 4-6 factual). — [SP/audit/cats.py](file://SP/audit/cats.py)

- Representative notes from other runs:
  - Numeric errors: broad4/projectile "The vector arrow is mis-scaled to 20 m/s"; latC/overfit "The new point sits close to the curve at x=2.25, which undercuts 'way off'."
  - Spatial: broad4/bonds "the shared electrons sit on the outer edges of the H circles, not between the nuclei."
  - Factual: latC/bayes "'99 vs 999' is about 10 to 1, not 'about nine percent'."

  — [SP/broad4/full](file://SP/broad4/full), [SP/latC/full](file://SP/latC/full)

### Inferences
- Roughly, by who can fix the failure instances:
  - About **half are deterministic** (a/c): overlap, clutter, colour binding, cue/hook, parse, render bugs, coordinate computation if the markup offers geometric references.
  - About **a third need judgement** (b): choosing the depiction, deciding *how* to show a narrated process, factual care.
  - The remainder is mixed: spoken numbers are detectable deterministically, but placing them well is a judgement.
- The "spoken not shown" failure is the biggest single target. Detection is already deterministic, but it arrives too late (after the scene has played). A fast checker that runs **per beat, before the beat plays**, could flag it in time, or a small model could insert the missing `label=`/`\val{}`. The lint text even gives the fix ("put the number where it happens").

### Gaps
- Labels are hand-coded by one reader from judge notes, not from frames. I did not re-inspect frames for each case.
- The judge sees `check.txt` and sometimes echoes the lints ("so the engine's lint flags it", hashtable). That inflates agreement between lints and the judge for the binding and hook categories.

## Which failures involve Claude's numeric/spatial computation vs choice of picture vs language?

### Takeaway
- In latA, **18/44 scenes contain hand-typed numeric coordinates** (`x=`, `y1=`, `points=[…]`), and 12 of the 44 have a judge-noted numeric or spatial error.
- Scenes with hand coordinates have **lower accuracy in 6 of 7 runs** (by 0.09-0.46). broad is the exception (3.75 vs 3.68).
- The worst latA "hand coordinate" scenes are kmeans (10 coordinate attributes), refraction (9), overfit, heart, bonds (7 each) and pendulum (6). Most of these are among the weakest questions overall.

Split of the latA failure instances:
- **Computation**: about 12 scenes.
- **Picture/metaphor choice**: about 17.
- **Language**: factual 4-6, ritual 4, mis-named colour or order 1-2.
- **Binding** (what to draw for a spoken claim): 30. This is a planning failure, neither pure computation nor pure language.

### Cited Findings
- latA: 18/44 played.txt files have numeric coordinate attributes; 12 use `fn={…}` expressions (the engine computes those); 23 use `schema axes` or `plane`. — [SP/latA/full/*/played.txt](file://SP/latA/full)
- Accuracy, scenes with hand coordinates vs without:

  | Run | with | without |
  |---|---|---|
  | latA | 3.61 | 3.77 |
  | latC | 3.47 | 3.81 |
  | broad4 | 3.69 | 3.79 |
  | broad | 3.75 | 3.68 |
  | broad2 | 3.48 | 3.57 |
  | broad3 | 3.53 | 3.80 |
  | latB | 3.06 | 3.52 |

  — [SP/audit (inline script over */full/*/played.txt + score.json)](file://SP/audit)
- In latB, picture and teaching are also lower with hand coordinates (2.94 vs 3.52, 2.88 vs 3.74).
- Concrete numeric failures where Claude computed values by hand:
  - supply: shortage drawn at x1=3.5 instead of 2.
  - catalyst: Eₐ bar at x=6.5, away from the peak.
  - neuron: threshold ~−53 mV vs a −50 tick, from the hand-written expression `{-70 + 10*exp(...)}`.
  - interference: an unrounded `\val` showing −7.35e-06.
  - overfit (3 runs): the "wiggly" curve coefficients don't produce a big miss.

  — [SP/latA/full/supply/played.txt](file://SP/latA/full/supply/played.txt), [SP/latA/full/catalyst/played.txt](file://SP/latA/full/catalyst/played.txt)
- Even with thinking on, Claude's own plan shows it intends correct bindings. attention/plan.txt says `"softmax, sums to 1" -> links morph to weights; "mix of values" -> send from words to it`, yet the judge says "the blend step has no weighted output shown." The gap is between plan and execution, not missing intent. — [SP/latA/full/attention/plan.txt](file://SP/latA/full/attention/plan.txt)

### Inferences
- Numeric and spatial failures are the best fit for deterministic code: expose geometric queries (intersection, extremum, tangent, foot of perpendicular, landing point, auto plot range) so Claude names relations instead of computing numbers. This matches the HANDOFF research note that "LLM-chosen coordinates fail, solvers and relations work."
- Picture-choice failures are concentrated in a known handful of physical topics (heart, vaccine, bonds, dna, circuits, refraction, ph). Two fixes fit: exemplars or idiom templates for these, or a retrieval step that hands Claude a known-good scene skeleton.
- Plan-vs-execution binding drift could be checked deterministically. Parse the `show:` line of the plan into claim→encoding pairs and verify that each encoding appears on its cue.

### Gaps
- I didn't verify every judge claim about wrong coordinates against the played markup. I spot-checked supply, catalyst, interference and integral, and they were consistent.

## How often do scenes have parse errors, lints, checker issues?

### Takeaway
- Scenes almost always decode: checkOk is 39-43/44 per run.
- True parse errors are rare: at most 2 questions per run, and only in latA and latB.
- **Binding lints** are endemic: 38-54 per 44 scenes, in 13-19 questions.
- Overlap lints vary a lot by engine version: 4-32 per run.

Real chats look similar: 32/36 staged turns ok, and 26 binding, 23 layout, 8 hook, 7 parse and 2 zero lints over 39 assistant turns.

### Cited Findings
- Lint counts per run (indented check.txt lines, deduplicated), shown as count (number of questions):

  | Run | checkOk | bind-number | layout-overlap | bind-no-change | bind-shared | hook | parse |
  |---|---|---|---|---|---|---|---|
  | latA | 41/44 | 23 (13) | 15 (7) | 10 (10) | 8 (5) | 11 (10) | 4 (1) |
  | latB | 39/44 | 29 (11) | 8 (7) | 10 (8) | 9 (8) | 5 | 5 (2) |
  | latC | 41/44 | 32 (19) | 16 (11) | 16 (13) | 6 (6) | 3 | 0 |
  | broad4 | 43/44 | 29 (13) | 23 (10) | 12 (12) | 7 (6) | 0 | 0 |
  | broad2 | — | 32 (17) | 32 (12) | | | | |

  Other lints per run:
  - latA: crowd>6 in 4 questions;
  - latB: crowd>6 in 7 questions, plus one each of `say:` pointing at a never-marked thing, `indicate:` on a never-appeared target, and a `value:` NaN;
  - latC: crowd>6 in 6 questions;
  - broad: 41 `zero:` colour-reuse lints in 3 questions (an old engine colour bug).

  — [SP/audit/lints.py](file://SP/audit/lints.py)
- Summary bindingLints totals: latA 41, latB 49, latC 54, broad 38, broad2 49, broad3 46, broad4 50. — [SP/*/summary.json](file://SP)
- Real chats: 39 assistant turns across 23 sessions; 36 have a stage, 32 with stage.ok; 3 turns errored with "You've hit your session limit". — [REPO/chat/sessions/*/messages.jsonl](file://REPO/chat/sessions)
- Real-chat hook lints are all the same pattern: a tag with no matching spoken word (`@score`, `@go` ×3, `@gen`, `@x`, `@follow`, `@higher`). — [REPO/chat/sessions/*/messages.jsonl](file://REPO/chat/sessions)
- **Server/engine bug**: in session 20261020-223156, the previous turn's lint text was parsed as scene markup:
  - `parse: line 1: unknown verb 'lint:' | lint: binding: line 6 says "hundred"…`;
  - then a nested `unknown verb 'parse:'` on the next turn.

  So lint feedback leaked into the stage input, or Claude echoed it into its scene. Category (c), or a prompt/server interface issue. — [REPO/chat/sessions/20261020-223156/messages.jsonl](file://REPO/chat/sessions/20261020-223156/messages.jsonl)

### Inferences
- Syntax is not the problem. Claude writes valid markup more than 95% of the time. The deterministic lints already catch the commonest *semantic* problem (binding) but don't change the output in time.
- The hook lint "tag never spoken" is purely mechanical. The cue pre-flight already glues some cases (HANDOFF §8d), and extending that fuzzy matching would remove most of them.

### Gaps
- I did not count `layout:` lints across real sessions per turn type, and I did not run `audit-sessions.mjs` (read-only constraint; it renders frames).

## Latency: thinking vs plan vs scene

### Takeaway
In real chats:
- the first spoken sentence arrives at a median of **7.9 s** (mean 12 s);
- drawing starts at a median of 9.5 s;
- the turn finishes at a median of 18.3 s.

Transcripts show thinking blocks in 46/66 turns (70%). The thinking block is logged a median of 6.5 s after the user message, and the text a further 9.7 s later.

Prompt variants:
- removing deliberation (latB) cuts mean generation from 19.0 s to 15.1 s but drops accuracy by 0.36 and binding by 0.38;
- removing the plan (latC) saves about 2.3 s at roughly equal judge scores but raises binding lints from 41 to 54.

### Cited Findings
- Server meta over 36-39 turns (seconds, median / mean / max):
  - first: 7.85 / 12 / 126.4;
  - stage: 9.5 / 14 / 128.4;
  - done: 18.3 / 21 / 142.9.

  — [REPO/chat/sessions/*/messages.jsonl](file://REPO/chat/sessions)
- Transcripts (38 files, 71 user turns, 66 with text). Assistant message models: Sonnet 64, Opus 59, Haiku 8. Median time from user message to the completed text message:
  - Opus: 17.3 s (n=26);
  - Sonnet: 8.2 s (n=33);
  - Haiku: 13.3 s (n=4).

  Thinking content is empty in all logged blocks (redacted), so thinking tokens can't be measured directly. Median output tokens are 1189 per turn. — [~/.claude/projects/*chat-sessions-*/*.jsonl](file://~/.claude/projects)
- Eval generation time (secs, median / mean):
  - latA: 18.3 / 19.0;
  - latB: 13.4 / 15.1;
  - latC: 16.3 / 16.7.

  Line and Say counts are equal across variants (about 25 lines, 8 Say). — [SP/lat*/full/*/meta.json](file://SP)
- HANDOFF reports the "don't deliberate" prompt: first sentence ~4 s instead of ~7.5 s. Effects on scores, rejected:
  - accuracy 3.70 → 3.34;
  - teaching 3.68 → 3.41;
  - binding 3.18 → 2.80.

  The no-plan variant saves ~1.5 s with spoken-but-not-shown lints 41 → 54, and was kept. — [REPO/docs/HANDOFF.md §8d](file://REPO/docs/HANDOFF.md)

### Inferences
- Thinking is the bulk of the time to first word, and it buys accuracy and binding. A small model can't replace that thinking for open-ended questions.
- A small model could instead *post-process* each beat in parallel with streaming (add missing number labels, fix cue tags, compute coordinates) without touching Claude's first-word latency.

### Gaps
- Transcript timestamps are per completed message, so streaming time within the text can't be separated from the logs. Plan vs scene token split per turn was not measured.

## How much usable training data exists?

### Takeaway
About **335 judged (question → scene markup) pairs** exist in the eval scratchpad: 308 broad + 24 types + 3 showcase. That is 44 unique broad questions × 7 variants plus 12 type questions × 2.

- Only **36 of the 308 broad pairs (11.7%)** score ≥4 on every dimension. 150 have teaching ≥4 and accuracy ≥4.
- There are 39 real-chat turns (unjudged, real user questions), 9 exemplars and 33 hand scripts.

That is enough for few-shot retrieval or a lint-trained classifier. It is far too little to fine-tune a scene-generating model.

### Cited Findings
- 338 played.txt and 335 score.json across SP run directories. Broken down: broad, broad2, broad3, broad4, latA, latB, latC with 44 each; eval1 and eval2 with 12 each; show2 with 3. — [SP](file://SP)
- All-≥4 pairs per run:
  - latA 4;
  - latB 3;
  - latC 7;
  - broad 1;
  - broad2 8;
  - broad3 8;
  - broad4 5.

  Questions that are all-≥4 most often: recursion, passive, tcp, dijkstra, bayes, interference. — [SP/audit/agg.py](file://SP/audit/agg.py)
- Each pair includes:
  - the question;
  - plan.txt;
  - scene.txt and played.txt;
  - reply.txt;
  - check.txt (lints);
  - layout.json (geometric score);
  - 2-3 frame PNGs;
  - score.json (7 scores + a ≤40-word critique).

  The critiques make the data usable as labelled error data (failure → note). — [SP/latA/full/attention](file://SP/latA/full/attention)
- Real chats: 40 user messages and 39 assistant turns in [REPO/chat/sessions](file://REPO/chat/sessions), plus 39 turns/*.txt files.
  - Repeat asks suggest failures the user felt. "he initialization" was asked 7+ times across 5 sessions; the plan says "5th time asking: new angle".
  - Two explicit "I don't get it" / "I didn't get that" follow-ups.

### Inferences
- The best negative/positive pairs come from the same question rendered by 7 variants: score deltas on identical prompts. They suit a reranker or critic, such as "pick the best of N beats", or the planned "taste scorer".
- A small specialised model is plausible for narrow, label-rich tasks:
  - spoken number → which mark gets the label (lints give automatic labels in 13-19 questions per run);
  - cue tag repair;
  - choosing a slot or annotation target.
- It is not plausible for depiction choice or factual content. Those need Claude's breadth, and the data has only 44 distinct questions.

### Gaps
- No human (user) ratings exist on these scenes. All labels are from a Sonnet judge at low effort, which is noisy (±0.15 per mean) and sees the lint output. Real-chat turns have no judge scores.
