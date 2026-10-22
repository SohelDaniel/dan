# Mining 3Blue1Brown: feasibility study

Everything here is reproducible from `scripts/` (run with `python3 -I`). Inputs are `../3b1b/videos` and `../3b1b/captions`, and the tutor repo was not touched.

## 1. Inventory

| | count |
|---|---|
| Code: .py files (2015 to 2026) | 375 (372 parse as py3) |
| Classes / classes defining `construct` | 7,440 / 5,816 (subclass-only config scenes come on top of these) |
| `self.play(` calls | 30,419 |
| Indented `#` comments (step-comment candidates) | 12,569; they are dense from 2018 on (1.1k to 1.7k per year) |
| Prose step comments inside scene methods (`blocks.jsonl`) | 6,108, of which 4,892 have play() and recognisable animations |
| Caption folders with a transcript | 144 (2015 to 2024). There are no captions for the 2025/26 code |
| Caption folders mapped to code | **124 clean** (100 auto by TF-IDF transcript-vs-code with score≥0.2 and ratio≥1.4, plus 24 checked by hand). About 14 are unclear (Lockdown Math livestreams, eulers-formula-dynamically, ...). About 6 have no code (talks, Q&A, some2) |
| Sentences in mapped videos | 15,546 (123 have sentence_timings.json) |

Per-year counts are in `inventory.tsv` and the mapping is in `mapping.tsv`. Caveat: series share one code folder (eola: 11 videos, nn: 4, transformers: 4, diffyq: 3, optics: 4). These need a second, file-level mapping (chapterN.py, partN/), which is mechanical.

## 2. Step comment -> animation idioms (`idioms.tsv`, `idioms_fine.tsv`)

Method: `extract.py` uses the AST to find every class method. Consecutive comment lines merge into one prose comment, and commented-out code and TODO/test notes are dropped. Each comment owns the code up to the next comment. For each block it collects the play() args (resolving LaggedStartMap(FadeIn, ...), `.animate.X`, frame/camera moves, ValueTracker changes and the old-style `self.play(mob.shift, UP)`), plus animation constructors assigned to variables in the block. `idioms.py` then classifies the comment's lead verb into 23 relations using a small lexicon (first match in the first 5 words).

FadeIn and FadeOut appear in 35 to 60% of every relation's blocks (setup and cleanup), so the table shows the top 3 *excluding* them. The "distinctive" column is lift against the global base rate, and it is the real idiom signal.

| relation | blocks | top-2 overall | top-3 excl. FadeIn/Out | distinctive (lift) |
|---|---|---|---|---|
| show | 1377 | FadeIn 57%, FadeOut 44% | ShowCreation 29%, Write 27%, frame.animate.* 18% | CountInFrom x2.0, animate.arrange x1.9, GrowFromEdge x1.8 |
| other | 939 | FadeIn 54%, FadeOut 44% | ShowCreation 26%, Write 25%, MoveToTarget 14% | FadeOutToPoint x2.9, animate.change_mode x2.6, FlashUnder x2.5 |
| add | 489 | FadeIn 54%, FadeOut 29% | ShowCreation 25%, Write 25%, frame.animate.* 18% | SwitchOn x2.6, VFadeIn x1.6, GrowFromPoint x1.5 |
| highlight | 315 | FadeOut 46%, FadeIn 36% | ShowCreation 31%, LaggedStartMap 17%, Write 16% | Indicate x5.9, animate.center x2.9, animate.fade x2.8 |
| move | 314 | FadeOut 45%, FadeIn 34% | frame.animate.* 22%, MoveToTarget 18%, ValueTracker.set_value 16% | animate.move_source_to x13.0, FlipCoin x3.6, animate.set_x x3.0 |
| talk | 263 | FadeOut 48%, FadeIn 47% | Write 40%, ShowCreation 37%, PiCreature.change 16% | PiCreature.change x4.0, Blink x3.7, animate.surround x3.3 |
| transform | 243 | FadeOut 47%, FadeIn 41% | LaggedStart 18%, ValueTracker.set_value 18%, Write 16% | TransformMatchingTex x2.6, TransformMatchingShapes x2.1, Transform x1.7 |
| flow | 155 | FadeOut 36%, FadeIn 32% | ValueTracker.set_value 19%, ShowCreation 18%, Write 14% | VFadeOut x3.2, UpdateFromFunc x2.3, ValueTracker.set_value x1.9 |
| label | 120 | FadeIn 61%, Write 48% | Write 48%, ShowCreation 28%, LaggedStartMap 16% | GrowFromCenter x2.7, Write x1.9, GrowArrow x1.9 |
| count | 100 | FadeIn 60%, FadeOut 47% | Write 39%, ShowCreation 22%, TransformFromCopy 21% | TransformMatchingShapes x5.0, ShowIncreasingSubsets x4.0, AnimationGroup x3.6 |
| zoom | 97 | frame.animate.* 71%, FadeOut 36% | frame.animate.* 71%, animate.scale 14%, animate.set_opacity 13% | frame.animate.* x4.4, animate.set_stroke x3.0, animate.scale x2.6 |
| remove | 79 | FadeOut 77%, FadeIn 38% | LaggedStartMap 25%, Write 11%, ShowCreation 11% | animate.fade x3.5, FadeOut x1.8, LaggedStartMap x1.8 |
| write | 75 | FadeIn 61%, Write 57% | Write 57%, ShowCreation 17%, TransformFromCopy 16% | TransformMatchingShapes x3.3, Write x2.3, animate.next_to x1.6 |
| compare | 49 | FadeIn 59%, FadeOut 51% | ShowCreation 24%, Write 20%, MoveToTarget 18% | GrowFromCenter x2.1, AnimationGroup x1.9, Transform x1.6 |
| example | 45 | FadeOut 56%, FadeIn 44% | Write 29%, ShowCreation 24%, TransformFromCopy 16% | ShowIncreasingSubsets x4.8, Restore x2.1, GrowArrow x1.6 |
| restore | 45 | FadeOut 53%, FadeIn 44% | LaggedStartMap 18%, MoveToTarget 18%, Restore 16% | Restore x2.5, animate.set_opacity x2.0 |
| define | 44 | FadeIn 61%, FadeOut 41% | Write 36%, ShowCreation 25%, ValueTracker.set_value 25% | Blink x3.0, PiCreature.change x2.9, ValueTracker.set_value x2.4 |
| grow | 42 | FadeIn 55%, FadeOut 43% | MoveToTarget 33%, TransformFromCopy 31%, ShowCreation 24% | TransformMatchingShapes x9.5, MoveToTarget x2.5, animate.shift x2.5 |
| shrink | 35 | FadeOut 40%, MoveToTarget 29% | MoveToTarget 29%, frame.animate.* 29%, LaggedStart 26% | animate.scale x2.6, Transform x2.2, MoveToTarget x2.2 |
| split | 27 | FadeIn 56%, FadeOut 33% | Write 30%, MoveToTarget 26%, LaggedStart 22% | MoveToTarget x2.0, LaggedStart x1.7 |
| combine | 20 | FadeOut 50%, FadeIn 45% | Write 30%, MoveToTarget 25%, ShowCreation 20% | MoveToTarget x1.9 |
| swap | 18 | ShowCreation 39%, MoveToTarget 39% | ShowCreation 39%, MoveToTarget 39%, LaggedStartMap 33% | Transform x3.6, MoveToTarget x3.0, LaggedStartMap x2.3 |
| connect | 1 | MaintainPositionRelativeTo 100%, animate.body 100% | MaintainPositionRelativeTo 100%, animate.body 100%, animate.move_to 100% |  |

Readings: zoom means camera `frame.animate.*` (71%). Highlight means ShowCreation of a rect, LaggedStartMap(Indicate/FlashAround), and dimming others (`set_opacity`/`fade`). Transform means TransformMatchingTex/Shapes. Flow/vary means ValueTracker `.animate.set_value` with updaters. Count means ShowIncreasingSubsets/CountInFrom. Talk means PiCreature change/Blink plus a speech-bubble Write. Label means Write plus GrowFromCenter(brace)/GrowArrow. Remove means FadeOut/`fade`. Restore means `Restore`/`animate.restore`. Split, combine and swap mean MoveToTarget/LaggedStart.

Limits: "show" is 28% of blocks and says nothing (its object carries the meaning). 19% of blocks have no verb (noun-only comments like "# Axes"). connect and compare are rare.

## 3. Narration alignment (3 videos)

Videos: 2023 **clt** (4 files, 68 scenes, 221 sentences), 2023 **convolutions2** (4 files, 79 scenes, 201 sentences) and 2020 **hamming-codes** (1 file, 66 scenes, 156 sentences, part 1 of 2 sharing the file).

`align.py` methods:
- **(a) Text overlap.** Stemmed TF-IDF between each sentence (±1 neighbour at half weight) and the scene's strings, comments, identifiers and class name, z-scored per scene. Taking the argmax alone is noisy.
- **(b) Monotonic DP.** The first try was a single Viterbi over all scenes in file order. It failed because the files are parallel tracks: wordy_scenes.py (pi-creature/text overlays) interleaves with main.py, and the overlays drowned out the text-poor animation scenes. The final version runs a monotone alignment with gaps per file (each file is in narration order), then picks the best track per sentence. A sentence counts as confident when its segment has 2 or more lexical anchors.

Results: 223 of 578 sentences (39%) are confident. The rest go to NULL or low (footage, generic talk, scenes with no text).

**Spot check: 20 random confident pairs, judged by reading the scene code.** 12 are correct, 6 wrong and 2 uncertain, so **about 60% precision (70% lenient)**. The errors are mostly boundary slips of ±1 sentence next to short pi-creature reaction scenes ("Well, almost", "We can be more elegant"), plus generic sentences pulled into a text-heavy scene. File order is not always narration order either: in clt, HowVarianceAdds is defined after BuildUpGaussian but narrated before it. Separately I read about 25 hamming pairs: about 85% are right in the opening sections (old-style code with one scene per beat and many on-screen strings), and the last three wrongly land on part-2 code. Block level (sentence -> step comment within the scene) is weaker. For example, clt #138 "As we tweak sigma" lands on the "Tweak the spread" block for `c`: the idiom is right but the beat is wrong. About half of the confident pairs land in TeacherStudentsScene/text scenes, which are useless as animation exemplars.

Ten example pairs (sentence -> scene [block] -> the main animation in that block):
1. hamming #1 "The scratch really does affect the 1s and 0s..." -> DiskOfBits [Show scratch] -> `LaggedStartMap(MoveToTarget, flipped_bits), ShowCreationThenDestruction(scratch_line)`
2. hamming #5 "...store three copies of each bit." -> TripleRedundancy [Show redundant copies] -> `LaggedStart(*[TransformFromCopy(bits, c) for c in copies])` plus a "Redundant copies" label
3. hamming #7 "...using two thirds of your space for redundancy." -> TripleRedundancy [Show space division (256,247)] -> 16x16 bit grid, SurroundingRectangle on the ecc bits, labels
4. hamming #44 "total number of 1s is 7, that's odd, so the sender needs to flip..." -> ParityChecks [Show need to flip] -> `GrowArrow` + "Want this to be even", `LaggedStartMap(Indicate, one_rects)`
5. hamming #47 "if any bit gets flipped... even to odd" -> ChangeAnywhereToOneBit [Random changes] -> loop of `toggle_bit_anim(random bit)` with an updated parity rect
6. conv2 #22 "simulating two weighted dice... recording the sum" -> dice.py ConvolveDiscreteDistributions [Die rectangles] -> SurroundingRectangles on the sampled die and value
7. conv2 #134 "The product on the bottom looks like a copy of the top graph, limited to a window" -> MovingAverageAsConvolution [Show area] -> updater-driven window rect + "Area = 1" + Arrow
8. conv2 #183 "That step is longer by a factor of sqrt 2." -> AnalyzeStepAlongDiagonalLine [Show equation] -> `FadeTransform(dx_label.copy(), ...)`, `Write("= sqrt2 ·")`
9. clt #22 "we have a random variable... each outcome associated with some number" -> HighLevelCLTDescription [Random variables] -> TexText "Start with a random variable: X" + example boxes
10. clt #201 "Is the way a ball bounces off one peg independent of the next?" -> galton_board NotIdenticallyDistributed [Arrows to show distributions] -> per-peg FillArrow fans (bounce distributions)

## 4. Full-pass estimate

- **Lexical/DP as built.** 15.5k sentences x 39% confident x ~60% precision gives about 3.5k correct scene-level pairs. Dropping pi-creature/text scenes leaves about 2k visual pairs, and about 1 to 1.5k at step-block resolution. About a day to generalise (file-level mapping for series, run all). Precision is too low for training without review.
- **Recommended: LLM-assisted alignment, with step comments as the pivot.** Per video, give the model the timed transcript plus a compact scene outline (file order, class names, step comments, on-screen strings; about 10 to 40k tokens) and ask it to assign sentence spans to scene + step block, with a confidence and "none". Use the DP output as a prior or cross-check, and keep only pairs where the DP and the LLM agree or the LLM is highly confident. Projection, not yet tested: 50 to 60% coverage at about 85 to 90% precision, which is about 4 to 6k visual sentence->block pairs from 124 videos. The cost is roughly 3 to 5M input tokens, and the effort is 2 to 3 days including a 100-pair human audit.
- **Cheapest high-value product now:** the 4.9k (step comment -> animation calls) blocks need no alignment. The comments are the author's own intent labels ("Highlight subproblem", "Zoom in", "Swap out partial_x for partial_y"), which makes them a ready narration-kind -> idiom corpus. Use idioms.tsv as the table, and pick exemplars from blocks.jsonl by verb plus a distinctive animation.

Caveats: 3b1b code is manimgl (InteractiveScene, `frame.animate.reorient`, `TransformMatchingTex`), so the animation names need a translation layer to the tutor's verbs. Pre-2019 files use the old API (ApplyMethod, OldTex). Captions stop at 2024.
