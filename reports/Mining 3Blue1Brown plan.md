# Mining 3Blue1Brown: plan

2026-10-22. Research, tests and an audit only; no project code was changed. The evidence is in
`research_notes/Mining 3Blue1Brown/`:
- `mining.md`, `idioms.tsv`: narration ↔ code;
- `translation.md`: real scenes converted into our markup;
- `prompt-size.md`: prompt size, measured;
- `positioning.md`: layout audit and a learned positioning model.

## What the tests found

**The material.** 3Blue1Brown's repo has 375 code files from 2015–2026, about 30k animation calls, and about 6k step comments
written in his own words ("# Show four points going to a square"). The captions repo has 144 videos with timed sentences; 124 map to code.

**The idiom table needs no alignment.** About 4.9k step comments sit directly above the animation code that does
them: the author's own "intent → animation" pairs. Counted per verb (top rows, `idioms.tsv`):
- highlight → Indicate, 5.9× more often than average;
- zoom → camera move;
- transform → TransformMatchingTex;
- count → ShowIncreasingSubsets;
- label → GrowFromCenter/GrowArrow;
- remove → fade;
- move → MoveToTarget/ValueTracker.

**Pairing spoken sentences with code is hard.** Simple matching was only about 60% right when spot-checked. A
Claude-based alignment using the step comments as a bridge should reach 85–90% (an estimate, not tested). It's
2–3 days of work, and optional.

**Translation into our markup works as a filter, not blindly.**
- All 12 real scenes parsed, and 10 of 12 were fully clean after one automatic repair round.
- Only about half kept 3Blue1Brown's idea: CLT, x², the Bayes area model and Fourier were good; spirals, Pi creatures and 3D failed.
- A vision judge must filter the results, and the source should be pre-filtered: no 3D, Pi creatures, images, or objects generated in loops.

**What our language lacks**, ranked by how many of the 12 scenes needed it:
1. choreography (transform one thing into another, staggered entrances);
2. generated sets (n points on a circle, thousands of primes);
3. things sliding against each other (a convolution window);
4. emphasis effects (flash, wiggle);
5. simulation;
6. faint ghosts colliding with new things;
7. vectors that ride a grid's matrix.

Engine bugs seen along the way:
- Unicode superscripts and subscripts render as "?";
- lines show their ids as names;
- an array cell with a quoted comma next to empty cells fails decodability.

**Prompt size is not the obstacle** (measured).
- Today's appended prompt is about 17.7k tokens, and Claude Code caches it, including across processes.
- Adding about 16k tokens of examples changed time-to-first-scene-line by +0.2 s, which is noise (n=15, paired). Claude's thinking dominates.
- Research says that at 5–20 examples, picking the relevant ones beats a fixed list by a lot. Very long prompts start to degrade at larger sizes than ours.

**A learned positioning model: no.**
- Of 104 layout complaints, about 30% are placement decisions a model could learn. About 35% are engine rules or bugs (leftovers, the formula band squeezing the axes, the subtitle zone), and about 35% are content (too many things, Claude's own coordinates).
- Learned graph drawing only ties classical layout, and learned label placement is about 5 s per layout.
- 3Blue1Brown places about 97% of things relative to something else (`next_to` 11k, `arrange` 2.8k, `to_edge` 5k), which is our "position from structure" rule again.
- What would help instead:
  - teach the geometric checker what the judge sees in the end frame (leftovers, things in the subtitle zone, lost tick labels, empty thirds of the stage, tiny text, labels detached from what they name);
  - fix the rules it finds;
  - later, if anything is learned: Code2Video's split, where a model picks named slots and the engine places them.

## The plan

**Phase 1: idiom library (2–3 days).**
1. Cluster the 4.9k step-comment blocks by intent (verb plus what it acts on).
2. Keep the 40–60 kinds that matter for a tutor (highlight a part, count, compare two quantities, transform a formula, zoom on a detail, slide/move, label, grow/shrink, step through a process, show two views…).
3. For each kind, take 3–5 real 3Blue1Brown blocks. Translate them with Claude, then run `--check` with one repair round, render them, and keep only those a vision judge scores at least 3.
4. Write each kept one as a short idiom: one line of when to use it, one narration line, and 3–8 lines of markup.

**Phase 2: teach it (1 day plus eval).**
- Up to about 50 idioms go in the system prompt, grouped by narration kind (about 10–15k tokens, no measured latency cost).
- A bigger library is retrieved instead: the chat server picks the 5–10 idioms closest to the question and the stage, and puts them in that turn's message, so the cached prompt never changes.
- A/B test on the 44 questions: nothing, 50 fixed, retrieved top-8. Measure per-sentence binding, the scene judge and latency.

**Phase 3: close the language gaps the mining exposes.** Turn the most-used idioms into engine verbs, so they are one
line for Claude and always laid out right. Fix the three engine bugs. Candidates:
- a sliding window;
- generated point sets;
- vectors riding the grid matrix;
- flash/wiggle emphasis.

**Phase 4 (optional): narration-aligned pairs.** Run the Claude alignment of captions to code (2–3 days) to get
whole 3Blue1Brown-style scenes with his real narration. These could become exemplars, or later training data if a small
model is ever wanted.

**Alongside: layout.** Extend the checker to see what the judge sees, then fix the rules. No positioning model.

## What success looks like
- Per-sentence "fully shown" rises clearly above the 50–57% band (two baseline runs differ by about 3 points).
- Layout, accuracy and teaching hold or improve.
- Time to first word unchanged.
- Judged by the user on real frames, not by scores alone.
