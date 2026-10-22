# Should we train a positioning model? (audit + research, 2026-10-22)

Data: judge notes and scores for eval/full and eval-refs/full (88 scenes). eval-base2/full has **no score.json** (it was never judged), so only its layout.json and frames were used.
Raw dump: positioning/all.txt. 3b1b code concatenated: positioning/all3b1b.txt.

## 1. Layout complaints in judge notes (104 in 88 scenes)
| # | category | n | owner |
|---|---|---|---|
| A | label/mark overlaps another label, line or axis | 27 | ~19 placement (a/b); ~8 caused by leftovers or stacked data (c) |
| C | clutter, too many things, stray labels | 15 | c |
| B | leftovers: faded or hidden things still visible, stale marks | 14 | b (hide not clearing: fourier, balancing-refs) + c |
| D | cramped or tiny (labels, vectors, end frame) | 12 | ~4 a (ww1 7-box single row, dijkstra parallel edges, bayes arrows, recursion code box); rest b/c |
| H | Claude's coordinates make objects coincide (H atoms, triangles, ball on spot) | 7 | c |
| E | formula band pushes plot down, squeezes axes, ticks vanish | 6 | b |
| K | label detached from its anchor (Q off dot, link labels) | 6 | a/b |
| J | arrangement doesn't show structure (DNA strands not paired, bars not stacked, sentence scattered) | 6 | c (representation choice) |
| F | clipped or running into caption | 5 | b |
| I | glides, tree shift, axis rescale | 3 | b |
| G | arrow through things / doubled arrowheads | 2 | a/b |
| L | boxes drawn over an axis grid (wrong view) | 1 | b |
Total that a positioning model is aimed at (a): about 31 of 104 (30%). Engine rules/bugs (b): about 35%. Content (c): about 35%.

Frames read (17): eval stddev, pythagoras, balancing, refraction, recursion, kmeans, chords, complex, projectile, dijkstra, vaccine@6; eval-refs primes, ww1, falling, complex, integral, quicksort; eval-base2 dna, heart@14.
Notes from the frames:
- stddev: dots at near-equal values stack and their names collide (A1/A2/A4 unreadable). Label placer has no fallback (leader lines, or dropping names).
- dna (base2): brace labels "new" drawn on top of boxes because there is no gap between rows.
- complex: two "90°" labels overlap at the origin.
- projectile: "ball", "lands here", "Ground" and the tick "45" all pile up at the right edge.
- ww1 (refs): a 7-box chain in one row, so text is tiny and link labels sit on box edges. It should wrap or snake.
- vaccine@6: 4 unlinked boxes spread across the full width (they are columns before the links arrive).
- falling (refs): everything sits in the bottom third, the force vector runs through the caption, and 2/3 of the stage is empty.
- integral/kmeans: the y axis keeps only one tick label ("5") once the formula band grows.
- balancing/primes/quicksort/complex: leftover crosses, enclosures and faded vectors. This is a lifecycle issue, not position.
- pythagoras/refraction: Claude's own coordinates overlap. These are geometry the model wrote.

## 2. Judge layout vs geometric checker (n=88)
- Spearman correlation of judge layout with checker `final` 0.47, `mean` 0.35, `worst` 0.27, maxOverlaps -0.24, labelFlips 0.09, clutter note -0.23.
- Any final-frame issue: judge 2.96 (n=25) vs 3.62 (n=63). final<100: 2.67 (n=15) vs 3.59 (n=73). Clutter note: 3.07 vs 3.51.
- Transient issues (worst, flips) barely predict the judge. The end frame is what gets judged.
- The checker misses most of it: 4 of the 11 scenes judged ≤2, and 22 of the 28 judged 3, have final=100. It does not see leftovers (faded or ghost), things running into the caption, squeezed axes, empty composition, or tiny text.

## 3. 3Blue1Brown positioning (432 .py files, 527k lines in scratchpad/3b1b/videos)
Calls: next_to 11067 (DOWN 3223, UP 2649, RIGHT 2362, LEFT 1227), move_to 5617 (only ~720 to literal coordinates; the rest move to another object), shift 4897 (nudges), to_edge 3742 (UP 1900), to_corner 1445, arrange 2589 (RIGHT 1254, DOWN 964), arrange_in_grid 211, c2p/n2p 2699, align_to/match_x/y 1343, Brace 1098, SurroundingRectangle 1576, updaters 2495.
About 97% of his placement is relational: relative to another object, an edge, or an axis. That is already the engine's model ("position from structure"). As training data it is poor:
- Positions exist only at runtime, under pinned ManimGL versions. Many old scenes no longer run.
- There are no semantic labels for what each object *is*.
- It is a few thousand scenes in one artist's style.
- The usable signal is the statistics above (label below or above; title to the top edge; rows and columns via arrange). Those are rules you can just write.

## 4. Research (sources)
- Learned graph drawing ties classical stress layout. Rome stress: CoRe-GD 233.2, (sgd)² 233.5, DeepGD 235.2, neato 244.2. These models work on undirected unlabeled graphs; none is layered, label-aware or size-aware. https://arxiv.org/html/2402.06706v1 ; DeepGD https://par.nsf.gov/servlets/purl/10328001 (49 ms/graph, no guarantee beyond the training sizes); SmartGD https://arxiv.org/abs/2206.06434v2 ; Tiezzi https://arxiv.org/abs/2109.10061 ; DeepDrawing https://arxiv.org/abs/1907.11040
- Learned label placement: Reinforced Labels (Bobák, TVCG 2024) places more labels but takes ~5 s against 136-343 ms for classical methods. The authors call it non-interactive. https://arxiv.org/pdf/2303.01388 ; LPGT https://aa.ieeevis.org/year/2024/program/paper_v-full-1218.html
- Content-aware layout generation still overlaps: on posters, LayoutPrompter 0.231 and LayoutDM 0.524. https://arxiv.org/pdf/2407.19787 ; LayoutGPT reports no overlap metric https://proceedings.neurips.cc/paper_files/paper/2023/hash/3a7f9e485845dac27423375c934cb4db-Abstract.html
- LLM layouts overlap and overflow (AutoPresent https://arxiv.org/pdf/2501.00912). DiagrammerGPT needs an auditor pass (https://arxiv.org/html/2310.12128v2). Code2Video: a 6x6 anchor grid scored 82.8 against 45.2 for free placement, and 8x8 added clutter (https://arxiv.org/pdf/2510.01174). Discrete slots plus a deterministic solver is the pattern that works.
- Temporal stability: classical dynamic layout uses movement penalties (https://ar5iv.arxiv.org/html/1202.6042). Mental-map studies: https://eprints.gla.ac.uk/78675 . No learned layout model has a frame-to-frame stability term.
- Cost: a small transformer or GNN takes ~4-5 ms on the Apple Neural Engine (M3 Max reports; DistilBERT Core ML https://github.com/anentropic/experiments-coreml-ane-distilbert). An M5 would be <10 ms for <20 nodes (estimate). The rule engine takes 0.1-2 ms per frame. Cost isn't the obstacle; the training data and the lack of guarantees are.
- I found no user study showing learned layout beats rule or constraint layout for small labeled diagrams.

## 5. Verdict
A trained positioning model targets about 30% of complaints (label collisions, detached labels, chain wrapping, edge ports). The classical fixes for those are known and cheap:
- label candidates with leader lines or drop-names for dense dots;
- annotation labels that need clearance;
- wrapping long chains;
- reserving the edge-label margin;
- port fanning.
It would not touch the other ~70%:
- leftovers and the hide lifecycle;
- the formula band squeezing axes and clipping into the caption;
- content density;
- Claude's own wrong coordinates and representation choices.
Those need engine rules and checker coverage on the end frame, plus prompt and lint work. Recommendation: don't train a model. First extend the checker to what the judge sees (leftover ghosts, caption intrusion, axis tick loss, fill and empty-thirds, minimum text size, labels off their anchor), so its score tracks the judge (currently rho 0.47). Then fix the rules it surfaces. If a learned piece is wanted later, use the Code2Video pattern: a model, or Claude, picks discrete slots or relations, and the deterministic engine places and verifies them.
