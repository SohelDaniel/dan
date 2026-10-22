# Semantic layer + visual compiler: prior art (researched 2026-10-22)

Question: should Claude declare the SEMANTICS (entities, quantities, relations, operations) and a
deterministic compiler compose the visual, instead of Claude imitating example markup?

## 1. Data vis: semantics -> encodings, compositionally
- Mackinlay APT (1986): data relations typed (nominal/ordinal/quantitative); graphical languages
  ranked by expressiveness (shows exactly the facts) + effectiveness (perceptual ranking of
  channels: position > length > angle > ... > color). Composition = merge of single-relation
  designs sharing an axis. https://doi.org/10.1145/22949.22950
- Show Me (Mackinlay, Hanrahan, Stolte 2007, Tableau): rule table field-types -> chart; ~in
  production for ~20 years. Works because the semantic vocabulary is tiny (dimension/measure x
  count). https://doi.org/10.1109/TVCG.2007.70594
- Vega-Lite (Satyanarayan et al. 2017): spec = data + mark + encoding channels + composition
  operators (layer, concat, facet, repeat) + selections; compiler fills ALL defaults (scales,
  axes, legends). Novelty comes from the composition operators. https://vega.github.io/vega-lite/
- Draco (Moritz et al. InfoVis 2019) / Draco 2 (2023): designs = logical facts; hard constraints
  = validity, soft constraints = preferences with weights (hand-set or learned from ranked
  pairs of designs from perception experiments); cost = weighted sum of violated soft
  constraints; solver (ASP/clingo) returns the cheapest completion of a partial spec.
  Draco 2 notes v1 was hard to use/extend (little tooling). https://idl.uw.edu/papers/draco ,
  https://arxiv.org/abs/2308.14247
  -> Lesson: "partial spec + solver completes the rest" is the right shape; weights learned
  from pairwise judgments of rendered output (we could do this with our own eval screenshots).

## 2. Diagrams from math semantics
- Penrose (Ye et al. SIGGRAPH 2020): Domain (types, predicates) / Substance (instance:
  `Set A, B; IsSubset(B, A)`) / Style (selector rules: for every IsSubset(x,y) ->
  ensure contains(y.shape, x.shape), encourage ...). Style compiles to constrained numeric
  optimization. Same Substance under different Style = different diagrams; new combinations of
  predicates compose for free because each rule is local. https://penrose.cs.cmu.edu/siggraph20
  Limits: writing a Style is heavy up-front work; layout quality = local minima; when
  constraints are unsatisfiable it still returns a (misleading) diagram (critique in
  Cope-and-Drag paper https://arxiv.org/abs/2412.03310); per-domain Style libraries are the
  coverage bottleneck; static only (no time/animation).
- Edgeworth (CMU 2023): mutates Substance programs to produce diagram problem variants; usable
  options in the first 10 variants for 87% of problems (geometry, chemistry, discrete math).
  https://par.nsf.gov/biblio/10542035 -- evidence that the semantic layer generalizes WITHIN a
  domain whose Style exists.
- Bluefish (Pollock et al. UIST 2024): diagrams from declarative relations (Align, Distribute,
  Stack, Arrow, Background, Ref) that, unlike UI components, can share children; compound
  graph scenegraph. Gallery spans math/physics/CS/cooking. Sweet spot between low-level toolkit
  and fixed chart typology. https://vis.mit.edu/pubs/bluefish , https://arxiv.org/abs/2307.00146
  -> Closest analogue to our current markup + "on=" seating: relations, not coordinates.
- GoTree (Li et al. 2020): declarative grammar for tree layouts (element / layout / link
  components) - shows a narrow structure type can be fully compositional. (Not re-verified.)
- "Diagrammar": not found under that name; skipped.

## 3. Algorithm animation from semantics
- Balsa (Brown & Sedgewick 1984): annotate the algorithm with "interesting events"
  (Compare(i,j), Swap(i,j)); views subscribe to events and render them. Semantics = the event
  stream, rendering = view. The same events drive multiple views.
- Tango/XTango (Stasko 1990): path-transition paradigm - images, locations, paths, transitions
  composed with sequence/concurrent operators; algorithm events map to animation scenes.
- JSAV (Karavirta & Shaffer 2013), VisuAlgo: per-structure APIs (array.swap, highlight,
  pointer) that compile to fixed visuals - very consistent, but each new structure = new code.
  https://jsav.io
- Effectiveness: Hundhausen, Douglas & Stasko 2002 meta-study (24 studies): HOW learners
  engage matters more than WHAT is shown; ~40%+ found no significant effect.
  https://www.academia.edu/15636659/A_Meta_Study_of_Algorithm_Visualization_Effectiveness
- Heer & Robertson 2007 "Animated Transitions in Statistical Data Graphics": staging (one
  change type per stage), maintain object constancy, avoid ambiguity; congruence/apprehension
  (Tversky 2002). These are exactly rule-able. https://doi.org/10.1109/TVCG.2007.70539
- Gemini (Kim & Heer VIS 2021): grammar of transition steps over marks/axes/legends +
  sync/concat; recommender enumerates staged designs, ranks with a perceptually informed cost
  (Draco for time). 9/11 expert designs replicable. Gemini^2 recommends intermediate keyframes.
  https://idl.uw.edu/papers/gemini
- Canis (Ge et al. EuroVis 2020): select marks, partition into units by data attribute, apply
  effects with timing (stagger-like); compiles to Lottie.
  https://www.microsoft.com/en-us/research/publication/canis-a-high-level-language-for-data-driven-chart-animations/
- Animated Vega-Lite (Zong et al. VIS 2022/23): time as encoding channel + timer-driven
  selections; deliberately trades expressiveness for unification.
  https://vis.mit.edu/pubs/animated-vega-lite
- Manim: Mobject + Animation (Transform, Indicate, LaggedStart) - low-level, imperative; no
  semantic layer, which is why LLM Manim pipelines fight layout.

## 4. LLM writes semantic spec, engine renders
- Penrose lead (Sunshine, informal): LLMs are "much better at generating our domain-specific
  code than SVGs", despite it barely being in training data.
  https://linen.futureofcoding.org/t/23092211/
- FEYNMAN (arXiv 2603.12597, 2026): LLM writes ONLY Substance; Penrose default Style renders;
  10,693 programs / ~107k diagrams cheaply; 82.5% compile+yield in 2.6 rounds with planning.
  Argues TikZ mixes knowledge with drawing (AutomaTikZ compiled 3/5). Failure modes: Style
  variety capped at defaults; quality decays in knowledge-sparse domains.
  https://arxiv.org/html/2603.12597
- DiagrammerGPT (Zala et al. COLM 2024): LLM writes a diagram PLAN (entities, relations,
  boxes), auditor loop refines, then render -> beats direct text-to-image.
  https://arxiv.org/abs/2310.12128
- TheoremExplainAgent (Ku et al. 2025): LLM writes Manim directly; 93.8% completion but "most
  videos have minor visual layout issues". https://arxiv.org/abs/2502.19400
- Code2Video (Chen et al. 2025): Manim code + VLM critic with "visual anchor prompts" (a grid
  of named positions) to fix layout; "40% over direct code generation".
  https://arxiv.org/abs/2510.01174  -> even code-first pipelines retreat to a semantic-ish
  placement layer.
- LIDA (Dibia ACL 2023): grammar-agnostic, LLM writes plotting code + self-repair loop
  (no semantic layer; relies on library defaults). https://aclanthology.org/2023.acl-demo.11
- NL2VIS: no clean head-to-head Vega-Lite vs matplotlib found. VisPlotBench has both tracks
  (https://huggingface.co/datasets/TIGER-Lab/VisPlotBench); LLMs fine on common charts, weak on
  complex/rare ones in both (https://arxiv.org/abs/2507.22890). Treat the "spec beats code"
  claim as strongly suggested, not proven.

## 5. Pedagogy as rules
- Mayer multimedia principles that are mechanically checkable: temporal contiguity (visual
  event lands with its |cue| word), spatial contiguity (label next to thing), signaling
  (indicate), segmenting (one change per beat), coherence (no decoration), pre-training
  (name parts before the process).
- Explanation patterns as templates: process (stepwise state change), cause-effect (A then B,
  arrow), compare/contrast (side by side, aligned axes), worked example (general rule, then
  instance), part-whole. Concept maps give entities + labeled relations (= Penrose Substance).
- Tversky et al. 2002 "Animation: can it facilitate?": animation only helps when it matches
  the conceptual change (congruence) and is perceivable (apprehension) - argues for compiling
  only changes that carry meaning.

## Proposed semantic layer for our markup (sketch)
Entities: `quantity x = 6`, `sequence xs = [..]`, `grid`, `set`, `function f`, `object`.
Operations (each = Balsa-style interesting event -> compiler rule -> existing primitives):
1. slide(window w over seq s, step k, each: combine) -> seat w on= s[i:i+n], per stop
   send stagger= covered cells -> output cell, land result on cue. (= current conv idiom)
2. map(f, s -> t)       -> t laid parallel to s, send cell i -> t[i], staggered.
3. reduce(s -> v, op)   -> send stagger= all cells into v, v updates per arrival.
4. cause(A, B)          -> A indicated, link A->B, B changes after link lands.
5. equal(a, b) / compare(a, b) -> align a,b on a shared baseline; morph one into other or
   indicate the equal sign; contrast = same layout, differing part highlighted.
6. scale(q, factor)     -> morph q's length/size; ghost of old value kept for comparison.
7. transform(state1 -> state2) -> morph with object constancy, staged per Heer&Robertson.
8. partOf / contains    -> grouping/background box (Bluefish Background, Penrose contains).
Soft rules (Draco-style, weights tuned from our screenshot evals): one change per beat,
visual event at its cue word, nothing moves that the narration doesn't mention, labels
adjacent, reuse positions across beats (constancy).
Escape hatch: raw markup allowed inside any op (Bluefish/Vega -> Vega pattern).

## Failure modes seen in the literature
- Coverage gaps: every system is strong only where its rule library exists (Show Me charts,
  Penrose per-domain Style, JSAV structures). Unfamiliar topic -> no rule -> fallback to raw
  primitives, i.e. back to imitation.
- Rigidity / sameness: FEYNMAN's diagrams limited to default Style; Show Me charts all look
  alike; Animated Vega-Lite trades expressiveness for unification.
- Ugly or misleading defaults: Penrose local minima and silent unsatisfiable layouts; Draco
  weights need data.
- Semantic misdeclaration: the LLM still has to pick the right abstraction ("slide" vs "map");
  errors move upstream and are harder to spot visually.
- Authoring cost: Penrose Style, Draco 1 tooling complaints.
- Pedagogical payoff is not automatic (Hundhausen meta-study).
