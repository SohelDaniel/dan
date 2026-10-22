# Fixing "spoken but not shown": plan

2026-10-22. Read-only: no project code was changed. Evidence comes from four sources:
- three fresh eval runs on the 44 broad questions;
- a new per-sentence measure;
- a code audit;
- research outside tutoring software, and on models.

Notes and scripts are in `research_notes/Spoken but not shown/`.

## The problem, measured properly

The scene judge sees 3 frames per scene, so it can't tell whether a sentence was shown *when it was said*. I built
a per-sentence measure (`experiments/sentjudge.mjs`): one frame at the end of every Say, and a judge that marks each
sentence's claim as shown, partly shown, or not shown. I checked several verdicts against the frames by eye. Today's
system (baseline, 44 scenes, 335 claims):

| claim kind | share of claims | fully shown | partly | not |
|---|---|---|---|---|
| numbers / quantities | 53% | 54% | 39% | 7% |
| processes ("add", "splits", "doubles") | 14% | 40% | 44% | 17% |
| same / different / each its own | 7% | 68% | 23% | 9% |
| comparisons | 2% | 17% | 50% | 33% |
| other (a thing pointed at, a rule) | 24% | 43% | 47% | 10% |
| **all** | | **50%** | **41%** | **10%** |

Half of what the tutor says is only partly shown, or not shown at all, at the moment it is said. Numbers are half of
all claims, so they are the biggest single target. Processes fail most often.

The judge's scene notes split the failures into five kinds:
1. numbers said but not on stage (about 10 scenes);
2. a process described but not animated (about 8);
3. the picture contradicting the words, e.g. "slot five" while slot 6 is checked (about 6);
4. leftovers contradicting the ending (about 5);
5. generic pictures (about 4).

## What was tested

| variant | per-sentence fully shown | not shown | scene judge: binding | layout | accuracy | time |
|---|---|---|---|---|---|---|
| baseline (scene judge run twice) | 50% | 10% | 3.14 / 3.11 | 3.48 / 3.41 | 3.61 / 3.59 | 20.5 s |
| A second model edits each beat before it plays (Sonnet) | **63%** | **4%** | 3.14 | **3.00** | 3.39 | +5–9 s per beat |
| Same editor, Haiku | — | — | 3.00 | 3.14 | 3.45 | +4–9 s per beat |
| **Storyboard plan** (prompt-only change) | **60%** | 8% | 3.23 | 3.39 | 3.48 | +1.6 s |

**The editor (a second model in the loop) does bind more, but it ruins the picture.** It adds labels and formulas
after the composition is decided. The layout engine then crams them in (cut off, overlapping), and sometimes the
editor breaks the scene: in the dice scene it re-marked the dots and the histogram fell apart.
- Layout dropped in 20 of 44 scenes; accuracy also dropped.
- A model cannot be bolted on afterwards; this includes a small local model, which would be a weaker version of the same editor.

**The storyboard plan works and costs almost nothing.** The plan's `show:` list becomes a two-column script, one row
per sentence: `gist || what changes on stage, on which word`, with "— aside" allowed.
- Per sentence: 27 scenes better, 14 worse (paired).
- Layout unchanged (−0.06, noise).
- About 1.6 s more per turn, because the plan doubles from about 400 to 770 characters.
- Watch: the scene judge's accuracy is −0.12 (17 scenes worse, 9 better). That is close to the noise, but run it twice before trusting it.

This matches the research. In Code2Video, removing the planner that pairs each line with its animation dropped scores
from 79 to 38; removing the critic dropped them only to 72. Two-column AV scripts in film work the same way: an empty
video cell is plain to see.

### Follow-up: latency and repeat runs (same day)

Measured as time to the first stage line, median of 15 questions per prompt, all run side by side with cold
`claude -p` starts. The differences matter more than the absolute times.

| prompt | first stage line | claims fully shown |
|---|---|---|
| current (two runs) | 13.5 s / 12.2 s | 50% / 53% |
| storyboard (plan rows) | +2.6 s | 60% |
| terse storyboard (rows replace shape/show) | +1.4 s | 55% |
| inline storyboard (a `# >` row before each Say) | +2 s | 56% (only 35/44 scenes wrote the rows) |

Two runs of the current prompt differ by 3 points, so the smaller gains are within chance. The full storyboard's
10 points cost 2.6 s before the tutor speaks. **Decision: the prompt stays as it is.** Go straight to step 2, which
costs no latency.

### Follow-up: `{expr}` in Say, built and tested (same day)

Built as planned, with the axioms checked:
- a shadow engine resolves values before the voice is requested;
- array cells and grids resolve;
- the learner's slider updates the shadow;
- the readback is unchanged, so Decodability holds;
- the scene is identical to typing the numbers, so Identity holds;
- tests are green: unit, check, append (126/126) and golden.

The golden baseline was re-saved for one script: bsearch's `lo` pointer glides through `mid` for about 0.1 s in the
old version too. Only the sampling moved, because "16" is a shorter word than "sixteen".

**The result: Claude barely used it.** Only 10 of 363 sentences had a `{…}`.
- Claims fully shown: 57%, against 50% and 53% for the two baselines, which is within chance.
- Number claims fully shown: 64%, against 54% and 60%.
- Layout, accuracy and teaching are unchanged.

Looking at the number claims that still fail, most aren't typed numbers that should have been read off the stage.
They're things or relations that were never drawn:
- "two computers agree to talk";
- "the heart is two pumps";
- "ten times harder to move";
- "a is three, b is four", with the sides never labelled.

A reference can only bind a number that is already on stage.

So `{expr}` guarantees that a spoken value never contradicts the picture, which it does correctly, but on its own it
doesn't move binding much. What would:
1. Getting Claude to use it: more exemplars that do (only bsearch does now).
2. Having the engine make the value readable when a ref's value isn't shown. The plan's "becomes the thing's label",
   not built yet.
3. Most of all, Claude drawing the things it talks about. That is a scene-design choice, not a language construct.

## The plan, in order

### 0. Measure what matters (1 day)
- Move `sentjudge.mjs` into `tools/eval/`.
- Report "% of claims fully shown" and "% not shown" next to the scene judge. The scene judge stays for layout and accuracy.
- The lint count is no measure: it fell from 48 to 30 with the editor while layout collapsed.
- Show the user 10 before/after sentence frames at each step. Their eyes decide.

### 1. Storyboard plan (prompt only, half a day)
Adopt `experiments/storyboard.md`'s plan change in `chat/prompt.md`:
- `board:` one row per Say, the picture's half first;
- numbers, steps, sameness and comparisons are never asides.

Then the server can check it mechanically, because the board is structured now:
- the number of rows should equal the number of Says;
- each non-aside row's beat should carry a cue or pointer.

These checks feed `<last_scene_issues>` like today. Re-run twice and confirm accuracy holds.

### 2. Speech reads the stage: numbers bound by construction (2–4 days)
This is the "Claude's input can't be wrong" fix, for the half of claims that are numbers.

**What changes for Claude:**
- A Say can speak a stage value: `Say: "That gives {h}, so cat goes in slot {h}."`, or `{expr}` like properties already allow.
- The engine speaks the value as words (12.5 → "twelve point five").
- On that word, the thing holding the value pulses (like an `[id]` pointer, so bounded focus holds).
- If the value isn't readable on stage yet, it becomes that thing's label: an ordinary re-mark that eases in and stays.
  This respects axiom 1 (Identity: no delete). An earlier draft had a tag that faded after the sentence, which would
  break that axiom.
- Values never become colour categories (axiom 3, Zero).
- The readback keeps the `{h}` reference, not the spoken number, so re-parsing rebuilds the same scene (axiom 6, Decodability).
- A bare numeral or number word in a Say becomes a lint. The prompt says to write numbers as references.

**What this rules out:**
- A spoken number with nothing behind it.
- A spoken number that contradicts the picture (the "slot five / slot 6" class): the words *are* the stage.

**Where, from the code audit:**
- `parseSay` (`markup/script.cpp:185-232`) parses `{…}` next to `[id]` and `|cue|`.
- The spoken text is fixed when the voice is queued (`engine.cpp:1530`), so the value is resolved from the model state at the start of that beat.
- The server's `#!voice` prefetch (`chat/server.mjs:341`) must send resolved text, or let the engine resolve it, so the TTS cache key (`tts.cpp:103`) is the real sentence.
- `\val{}`/`texSource` (`engine.cpp:1376`) is the existing resolver to reuse.

**Failure modes to test for:**
- Claude dodges with "about half". Lint the vague number words too.
- Values with no thing to live on, like "a million items". These are allowed as `{=1000000}` literals that create a tag; still shown, never invisible.
- Clutter. Tags expire.

**Verify:** number claims fully shown, from 54% to at least 85%; layout not worse; append test and golden tests green.

### 3. Per-beat gate in the server, deterministic (2–3 days)
Claude writes a beat in about 1.2 s, and it plays for about 6 s, so after beat 1 there is time to check each beat
before it plays (36 logged turns: a scene is written in about 9 s and played in about 50 s).
- Keep a shadow headless engine fed the same lines.
- Run the binding checks on each complete beat before it is sent.
- On a miss, only zero-clutter repairs:
  - a spoken value that an object already holds gets that object pulsed on that word (a late `@cue` line works, `engine.cpp:84-96`);
  - nothing else is added.
- Everything else is logged for the next turn, as today. No model editor: tested above, it costs layout.

Fix the checker bugs the audit found:
- chunked mode treats every Say as the last one, so number checks never run per beat;
- "seven hundred eighty-four" reads as 700, and "zero point one four" reads as "four";
- "half", "twice" and "doubles" are not recognised;
- at most 2 lints per sentence.

### 4. Processes (after 1–3, measured)
Processes are 14% of claims and fail most often (17% not shown).
- First see how far the storyboard gets them.
- Then consider a few verbs that animate themselves, like `send`, which already exists: e.g. `combine a b -> s` (values travel and merge on the word) and `split s -> a b`.
- Add only the ones the eval shows missing.

### 5. Models: not now, and why
- **Training a small model:** it would learn a weaker version of the editor that just failed on layout. The data is thin: 44 questions, all labels from a Sonnet judge.
- **A neural net "in the backbone" that makes Claude unable to be wrong:** no network can guarantee that. A language can, like markdown can't produce a broken heading: step 2 is that guarantee for numbers.
- **Constrained decoding / grammars:** they can't express "this number refers to a stage value", and `claude -p --json-schema` checks only at the end of the turn.
- **NLI checkers:** at chance on numeric reasoning (EQUATE).
- **Revisit when** steps 1–3 leave a narrow, labelled job. One example: where a value tag should go, if the deterministic placement is judged wrong often. That needs the user's own labels first, so log redo/reject in the chat.

## Ideas from other fields that shaped this
- **Data-to-text generation:** report writers (Wordsmith, AP earnings stories) fill sentences from data slots, so they can't state what the data lacks. Free-writing neural generators stay grounded only about 60% of the time. This shaped step 2.
- **Typst references fail to compile** when the target is missing; LaTeX prints "??", which is our current lint. This shaped step 2's hard lint.
- **Film's two-column AV script, EPUB read-aloud pairs, manim-voiceover blocks:** every line of audio owns a visual. This shaped step 1.
- **Aviation callouts:** every callout is checked against an instrument. But checking that becomes routine gets answered "without looking", so repairs must be cheap and few. This shaped step 3.
- **McNeill's gesture research:** speakers gesture on the focal idea, not on every word. So "— aside" is legitimate, and the target is not 100%.

## Caveats
- One generation run per variant and one per-sentence judging. The scene judge's noise is about ±0.2 per scene and about ±0.05 on a 44-scene mean.
- All scores come from Sonnet judges, not from the user.
- The per-sentence judge was validated by eye on a handful of frames only.
- Latency numbers for the editor are cold `claude -p` starts; a warm process would be faster, but it wasn't worth pursuing given the layout cost.

## Follow-up: a translator model between Claude and the stage (2026-10-22, evening)

**The idea tested** (the user's): Claude writes only the sentences, plus a plain-words `show:` line for each. A
translator model turns each beat into markup. A small local model trained for this would do the heavy lifting.

**The test.** All 44 questions, with two translators run on the exact same Claude output:
- Sonnet, the best a trained small model could hope to reach;
- the local Qwen3 8B, untrained (few-shot).

The scripts are in `research_notes/Spoken but not shown/experiments/` (intent.mjs and intent-writer.md).

| | claims fully shown | not shown | layout | picture | teaching | accuracy |
|---|---|---|---|---|---|---|
| today (two runs) | 50% / 53% | 10% / 12% | 3.48 | 3.59 | 3.73 | 3.61 |
| Claude + Sonnet translator | 59% | 7% | **2.55** | **3.09** | **3.18** | 3.20 |
| Claude + local Qwen 8B, untrained (17 scenes; the run hit a timeout) | 13% | 63% | 2.53 | 1.59 | 1.76 | 2.82 |

**Findings:**
- Claude did not write less: the scenes had a median of 1,590 characters against 1,530. Describing the picture in English takes as many words as the markup.
- Even a perfect translator makes the pictures much worse. It sees one beat at a time, never the whole design, so things land on the wrong view, pile up, and lose their arrangement (bonds, heart, DNA, neuron, pH all scored 1 on layout).
  - This is the same lesson as the beat editor: whoever designs the picture must also write it.
  - A trained small model can at best approach the Sonnet translator, so it inherits this ceiling.
- Untrained local Qwen 8B produced valid markup for only 53% of beats, and 8 s per beat. Training would fix validity (research: a few thousand engine-checked examples take small models from about 50% to over 90% on their own format), but not the design problem above.

**Verdict:** don't put a model between Claude and the stage.

Where machine learning can still help:
- **A fast learned critic.** A small model trained on the per-sentence judge's verdicts could flag "said but not shown" sentences in real time, as feedback for Claude.
- **Mining 3Blue1Brown** (153 videos of sentence-timed captions, about 8,500 step comments in his code) into a table of verb → visual idioms. These become engine verbs that Claude writes directly, keeping design and writing in one place.
