# 3b1b Manim -> tutor markup: translation test (2026-10-22)

Everything is under this directory:
- `snippets/NN-*.py`: the Manim block. `NN-*.narr.txt`: the caption sentences said during it.
- `system.txt`: the system prompt (the README language section, all 9 exemplars, and the output rules, including `# NOT EXPRESSIBLE:`).
- `tools/translate.sh`: `claude -p --model sonnet`, no tools. `tools/repair.sh`: one round that feeds back `--check`. `tools/cap.py`: caption lookup.
- `out/NN-*.txt`: first translation, `.check.txt` its check output. `out/NN-*.r1.txt` and `.r1.check.txt`: after one repair round.
- `frames/NN_{5,15,end}.png`, `frames/sheet_NN.png` (contact sheet), `sheet_NNr1.png` (repaired versions).
- Frames were taken at 5s, 15s and the end (`--at 5,15,end`). At 3s the stage was usually still empty.

## Results

| # | scene (video, class) | topic | check rc / lints (1st) | layout 1st -> r1 | fidelity 1st | r1 | NE lines |
|---|---|---|---|---|---|---|---|
| 01 | clt BuildUpGaussian (e^x -> e^-x^2 -> e^-cx^2) | probability | 0 / clean | 100 -> - | **4** | - | 5 |
| 02 | eola ch3 TrackBasisVectorsExample | lin alg | 0 / 1 binding | 100 -> 100 | **3** (j-hat lands at (1,0), not (3,0); v doesn't ride the grid) | 3 | 4 |
| 03 | eoc ch3 NudgeSideLengthOfSquare | calculus | 0 / 1 layout | 70 -> 100 | **4** | - | 6 |
| 04 | nn part1 IntroduceWeights | neural net | 0 / clean | 100 | **3** (no pixel grid, 4 boxes for 784 neurons) | - | 5 |
| 05 | convolutions MovingAverageExample | convolution | 0 / 2 binding | 100 -> 100 | **2** (arrays instead of bars, the window never slides, result shown in full from the start) | 2 | 6 |
| 06 | moser_reboot ExplainNChoose2 | combinatorics | 1 / colour axiom | 0 -> 58 | **2** (chord ids drawn as labels, array over the circle) | 2 | 4 |
| 07 | clacks BlocksAndWall (mass 100) | physics | 0 / 1 binding | 92 -> 100 | **2** (hand-scripted bounces, blocks overlap, count jumps 5->31) | 2 | 1 |
| 08 | spirals IntroducePrimePatterns | number theory | 1 / zero x2 | 100 -> 100 | **1** (9 hand-placed primes: no spirals, no rays) | 1 | 4 |
| 09 | bayes HeartOfBayesTheorem | probability | 0 / 1 binding | 100 -> 100 | **4** (area + by= is a natural fit) | - | 5 |
| 10 | bayes StateGoal (Pi creature) | Pi creature | 1 / morph-unknown, zero | 100 -> 100 | **1** (a dot called "you") | 2 | 3 |
| 11 | shadows IntroduceShadow (3D cube) | 3D | 1 / identity x2 | 100 -> 100 | **1** (two flat rectangles) | 1 | 8 |
| 12 | fourier WrapCosineGraphAroundCircle | Fourier | 0 / 4 layout | 83 -> 100 | **3** (wound graph overlaps the signal) | **4** (two views, clean) | 4 |

- **Parsing:** 12/12 decodable (readback round-trips).
- **First pass:** 9/12 have no axiom warnings, but only 2/12 are free of all lints. Mean layout score is 87.
- **After one repair round:** 10/12 are fully clean and the mean layout score is 96.5. 05 and 07 each keep one lint, and 06 has 6 overlaps.
- **Fidelity (judged by me, 1-5):** mean 2.5 on the first pass and 2.67 after repair. 6/12 score 3 or more (7/12 after repair). 3 scenes are failures (08, 10, 11).
- Repair fixes lints, not meaning. It improved 12 (overlap -> two views) and 10 (cleaner). It did not touch 02's wrong matrix, 07's overlapping blocks, or 05's non-sliding window.

## Manim features with no equivalent (scenes affected; the impact on the idea)

1. **Animation choreography** (8/12): Transform / ReplacementTransform / TransformFromCopy between different objects, DrawBorderThenFill, GrowFromCenter, Write, LaggedStart, path_arc, directional FadeIn. Mostly cosmetic, but losing "this copy flies into that slot" hurts 04 and 09. `send` covers part of it.
2. **Procedural generation / loops** (5/12): 21 chords and pair labels, 25k primes, 784 pixels, a 29-step convolution march, 31 clacks. Where it is the core idea it is fatal (08), and it is costly in 05, 06 and 07. The LLM hand-computes trig coordinates and typing 21 lines is verbose.
3. **Free relative motion of groups** (4/12): the next_to/shift family (sliding a window along a list, braces moving outward, a copy placed beside the original). It is core in 05.
4. **Emphasis effects** (4/12): Flash, FlashAround, wiggle, decay ripples, ShowIncreasingSubsets boxes. Low impact; `indicate` and `enclose` stand in.
5. **Updaters / simulation** (3/12): the collision physics updater, always_redraw fractions, the shadow following the projection. A `param` clock covers the closed-form cases (01's c tracker, 12's rotating vector), but not event-driven ones (07).
6. **Partial-opacity staging** (3/12): fade the others to 0.25 while one pair is lit, keep a faded copy. `focus` and `hide` are close, but hidden ghosts collide with new marks (05, 04).
7. **Text styling** (3/12): underlined title, background rectangle, fix_in_frame, per-word colours in Text. Cosmetic.
8. **Bar sequences** (2/12): dist_to_bars, a list drawn as bars with heights. Our `array` shows numbers, not shape.
9. **Custom axes / NumberLine with tip and labels** (2/12): partly covered by views.
10. **Rate functions** (2/12): there_and_back, wiggle.
11. **Pi creatures, bubbles, SVG icons** (1-2/12): fatal for 10. Teacher/student scenes would all fail.
12. **3D** (1/12): fatal (11).
13. **Matrix applied to vectors as well as the grid** (1/12): `G.m` moves only the grid. The LLM must hand-morph every vector, and it got j-hat wrong. There are also no "Transformed î" labels.
14. **Images** (ImageMobject pixels, 1/12).

## Engine and checker issues found along the way

- Unicode superscripts and subscripts in `label=` render as "?": `eˣ` and `aₙ` (01, 04). The font has no glyph.
- Lines in 06 show their ids (c12, c37...) as names, which clutters the circle.
- `--check` does not catch semantic errors. It misses solid rects overlapping (07's blocks), a wrong matrix (02), and a hidden ghost under a new card (05).
- The `zero:` colour axiom fires on scenes with many chords (06): every line takes a category colour.

## Viability for a few thousand pairs

The engine loop is viable as a **syntax and layout filter**. Every scene parses, and one round of check feedback converges on most of them. It is not a fidelity filter. Add a vision-judge pass on frames (tools/eval/judge.mjs) and keep scores of 3 or more.

Yield: about 50-60% usable, concentrated in 2D diagram scenes (graphs of functions, area models, grids, formulas, flows). Prefilter the source before spending calls:
- Drop classes based on ThreeDScene, PiCreatureScene or TeacherStudentsScene, and those using ImageMobject or SVGMobject.
- Drop heavy `for` loops that generate mobjects.

The real bottleneck is snippet extraction, not translation. Pairing a code block with the right caption sentences was manual: I matched on-screen text against the transcript with a regex. The code has no timing, so automating this needs a heuristic: match tokens from Tex/Text strings and step comments, then take the sentences around that point.

Cost is 1-2 sonnet calls per pair at about 30-60s each, run in parallel, so a few thousand pairs is feasible. Keep the `# NOT EXPRESSIBLE` comments as labels; aggregated, they form a ready-made gap ranking. The most valuable language additions this data suggests:
1. Generated sets: points from a rule or range, n points on a circle.
2. Sliding one array or bars relative to another, and bar-shaped arrays.
3. Vectors that ride a grid matrix.
