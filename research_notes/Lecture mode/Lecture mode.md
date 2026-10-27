# Lecture mode: a long lecture prepared from the learner's files

The user's ask (2026-10-27): give the tutor files (PDF, markdown, JSON, any text) and it prepares a lecture of 30
minutes or more, scaled to the material, split into parts. Claude plans it, builds it part by part with as much time
as it needs, and the result plays in the stage window (not an mp4), where the learner can pause and ask.

Decided with the user: a question is answered **on the stage** (an aside, then resume where it paused); each part
ends with a **short check**; building is **quality first** (hours in the background are fine).

## 1. How others generate long lessons

| system | input -> output | pipeline | lesson |
|---|---|---|---|
| PresentAgent (2025) | long document -> narrated slide video | parse, compose slides, plan narration, sync audio; a VLM judge (PresentEval) | segments of ~30-150 s each, one slide each |
| TheoremExplainAgent (2025) | theorem -> Manim video, > 5 min | planner agent + coding agent, optional RAG over Manim docs | 93.8% of videos render with o3-mini, but **most have minor layout problems** |
| Code2Video (2025) | topic -> Manim lecture | planner (lecture flow), coder (with auto-debug), **critic that looks at frames** (visual anchors) | the critic exists because the coder cannot see its own layout |
| AutoLectures (2025) | slide deck -> narrated video | narration per slide + aligning spoken phrases to slide regions (F1 > 92% with an LLM) | under $1 per hour of video |

What carries over: **plan the whole lecture first, then write each segment, then look at the frames.** A writer
alone gets most scenes to run but not to look right. We already have the pieces: `--check` lints, `--snapshot`,
`--layout-report`, and the judge rubrics in `tools/eval/`.

## 2. What the learning research says

- **Segmenting** (Mayer): learner-paced segments beat one continuous presentation; in the lightning study a 2.5 min
  animation was split into 16 segments with a continue button. -> Parts of 2-4 min, one idea each.
- **Video length**: across 6.9 M edX sessions, engagement dropped steeply past ~6 min per video (Guo, Kim, Rubin
  2014; the 6 min figure is from summaries of the paper, not checked against the PDF). -> No part longer than ~5 min.
- **Pretraining** (Mayer): knowing the names and characteristics of the main concepts before the mechanism helps
  transfer; supported in 13 of 16 tests, median d = 0.75 (one review gives 0.46, unchecked). -> Part 1 is a map of
  the lecture and its vocabulary.
- **Interpolated tests** (Szpunar, Khan, Schacter 2013, PNAS 110:6313): short memory tests between segments of a
  21 min online statistics lecture halved mind wandering and improved retention; breaking the lecture up with
  restudy instead did not. (The "half" and "40%" are from an interview with the authors.) -> A check at the end of
  each part, a question to answer, not a summary to reread.
- **Worked examples**: where the source has a procedure, show a worked case before the general rule. (Not checked
  against a source in this note.)

## 3. What follows for the design

1. **Ingest** everything as text (and the PDF itself, so figures are seen). Length target from size, ~150 spoken
   words per minute; the user can change it.
2. **Plan**: one call reads all sources and writes the outline (parts, each with a goal, the frame, the claims and
   how each is shown, the check, minutes, source pages) and a **bible** (notation, names, running examples, colour
   meanings) that every part follows. The user sees and approves the outline before building.
3. **Build** part by part, each a fresh call with the bible, the outline, the part's source excerpt and a summary of
   what earlier parts said. Then `--check` -> fix; snapshots + layout report -> critic -> fix. A finished part is a
   checkpoint.
4. **Package**: one `lecture.dan` (parts are `scene "Part N: ..."`), an index, and the voice warmed into the disk
   cache, so the lecture plays offline and instantly.
5. **Play**: jump between parts, a `check` that waits for an answer, `?question` answered as an aside on the stage,
   then back to the paused moment.

## Sources

- PresentAgent: https://arxiv.org/html/2507.04036v1
- TheoremExplainAgent: https://www.emergentmind.com/papers/2502.19400
- Code2Video: arXiv 2510.01174 (read through a summary only)
- Narrated lecture videos from slides: https://arxiv.org/html/2505.02966v1
- Mayer, segmenting principle: https://www.cambridge.org/core/books/multimedia-learning/segmenting-principle/37240877DDA0362355ADB39936027982
- Mayer, pretraining principle: https://www.cambridge.org/core/books/abs/multimedia-learning/pretraining-principle/01791D57F5D4164251269E6DF56A8BF1
- Guo, Kim, Rubin 2014: https://up.csail.mit.edu/other-pubs/las2014-pguo-engagement.pdf
- Szpunar, Khan, Schacter 2013: https://pmc.ncbi.nlm.nih.gov/articles/PMC3631699/
