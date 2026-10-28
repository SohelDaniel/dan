# Audit A7: the derived rule "position comes from structure"

Auditor A7 of eight. Read-only on the repo; nothing built, nothing committed. Everything I made is under
`<scratchpad>/audit-7/`:
`scenes/*.dan` (51 reproducers), `check-all.txt` (`--check` of the first 44), `readbacks.txt`, `layout-reports.txt`,
`shots/*.png` (frames), `sheets/s01..s16*.png` (contact sheets, 2x2 at 800x450). Commands below are relative to that
folder; `T` is `<repo>/tutor`. Line numbers are the working tree
of 2026-10-24.

The rule as the handoff states it (HANDOFF §3): placement words (`region below above left-of right-of near align pos`)
are retired and ignored with a lint; columns come from links, order from declaration, lanes from groups; `at=`, `on=`,
`in=`, `over=`, `of=` are the relations that place one thing against another; data sits where its data puts it; the
engine is the only author of pixels.

Short answer. The rule holds where it was built: the flow layout, the free layout, views and the checker all derive
position from structure and I could not make them take a position from the author. The holes are at the edges of the
vocabulary: (1) the retired-word lint covers `mark` lines only, and a *numeric* retired word is not ignored but becomes
the bar's magnitude; (2) the placing relations accept target kinds they cannot place (a link, a group, a formula part
for `at=`/`on=`, a curve or surface for `on=`) and fail silently: the thing lands at a bounding box, in the corner text
stack, or is not drawn, with `--check` saying 100; session finding 7 is one instance of this; (3) the language still
carries pixel numbers (`r=` on dots, `size=` on formulas, `bend=` on links) and coordinate-placed text (`label`/`tex`
with `x= y=`) that the checker treats as data; (4) marks written before the first `schema <view>` line are laid out
inside that view even when it is a plot. On creative freedom: anything topological or metric can be said today; what
cannot be said is a conventional, non-metric arrangement (a couple beside each other, an orchestra's fan, a 4-4-2 on a
pitch) without either inventing coordinates or accepting the flow's conventions. The design treats plane coordinates
as data and the prompt asks for exactly that for physical things, so inventing them is not a violation; it is the
cost the rule chose, and the eval's 12/44 coordinate errors are its price.

## 1. Verdicts

Each sub-rule: holds / violated / suspected, reproducer and command. "holds" means I tried to break it and could not.

1. **Retired words on a `mark` are ignored, with a lint.** Holds. `scenes/a1-retired-mark.dan`: all eight words on one
   bar; `$T scenes/a1-retired-mark.dan --check` prints eight `layout: 'a' <k>= is ignored: position comes from
   structure (…)` lines, decodable, layout 100; the frame is three plain bars in declaration order. (The readback
   keeps the words, see gap 2.1.)
2. **The same words on a `link` or a `group`.** Violated (no lint). `scenes/a2-retired-link-group.dan`:
   `link ab arrow a -> b region=top below=c pos=[0.5,0.5]` and `group g part-of=[a,b] region=bottom align=left
   pos=[…]`: `--check` prints no lint at all, decodable, 100. Nothing reads them, so the picture is right, but the
   rule's enforcement has a hole: engine.cpp:245 is `if (ln.verb == Verb::MARK)`.
3. **A numeric retired word is ignored.** Violated. `scenes/a3-retired-numeric.dan`: `mark a bar region=2 score=5`
   and `mark c bar below=4 score=1`. `--check` lints the words as ignored, but the frame
   (`shots/a3-retired-numeric_end.png`, sheet s01) shows bar a at **2** and bar c at **4**: `region`/`below` are not in
   `kPositional` (engine.cpp:16-24), so `magProp` (engine.cpp:462-468) takes the first numeric property. The readback
   has no `# derived` line because the three bars now have three magnitude properties (`readbacks.txt`).
4. **`pos=` is read by no layout code.** Holds. `grep -n '"pos"' markup/*.cpp markup/place/*.cpp` finds it only in
   `kPositional` (engine.cpp:19) and the lint list (engine.cpp:246).
5. **The toolbox, README, prompt and examples show no retired word.** Holds. `grep -n -E
   '\b(region|below|above|left-of|right-of|near|align|pos)=' README.md chat/prompt.md chat/toolbox.md
   chat/examples/*.dan scripts/*.dan` finds README:316 (the sentence that retires them) and prompt:116 (the sentence
   that forbids them) only. Real chats: the words appear in 7 of 41 sessions, all dated 2026-10-17 to 10-20 morning,
   before the retirement (history 8b); none since.
6. **Links make columns; a back-link arcs; a link that skips columns is routed through gaps.** Holds.
   `scenes/e1-cycle3.dan` (c -> a arcs under the row), `e2-diamond.dan`, `e5-link-to-group.dan` (source -> [a,b,c] ->
   output, links drawn to the lane box), `e7-grow.dan` (a -> e skipping b's column routed below): all 100, frames in
   sheets s06-s09; `tests/test_place.cpp:232-345` cover the same.
7. **Order from declaration.** Holds. `scenes/f1-free15.dan` (15 bars in two rows, left to right, top to bottom),
   `i5-football.dan` (within a category column, declaration order top to bottom).
8. **Groups are lanes, stages line up across lanes.** Holds. `scenes/e4-lanes-unequal.dan`: a 3-stage lane above a
   5-stage lane fed by one input; A step k sits above B step k (sheet s06).
9. **A hierarchy reads top-down.** Holds for trees (`e3-tree12.dan`: 16 nodes, 12 leaves in one row; `i7-grammar.dan`),
   and the engine says so only when every thing has at most one parent (flow.cpp:345-355). A family tree
   (`i6-family.dan`, two parents per child) therefore reads left to right, and an undirected `--` link still makes a
   column (`i6c-undirected-ranks.dan`: grandma one column right of grandpa, sheet s16). By design, but see §3.
10. **Categories become columns when there are no links.** Holds. `scenes/i5-football.dan`: GK | 4 defenders | 4
    midfielders | 2 strikers with no coordinates (sheet s11).
11. **`at=` places a pointer at its target.** Holds for a mark, a cell, a span, a dot on axes, a curve (its visible end,
    as documented), a 3D point (`c-at.dan`, `c-at-axes.dan`, `c-at-space.dan`, sheet s02). Violated, silently, for a
    formula part (`at=f[w]` points at the whole formula: `anchorBox`, layout.cpp:415-437, has no tex-part branch), a
    link and a group (`at=ab`, `at=g`: `anchorBox` returns false because `mark()` is null, the label is never
    positioned and falls into the corner text stack, placement.cpp:257-269). No lint for any of the three; `--check`
    says decodable. Two pointers at `arr[2]` and `arr[1:3]` overlap each other (layout.cpp:674-677 and 710 key the
    spread by the exact `at=` string).
12. **`on=`/`in=`/`over=` seat a thing on its target.** Holds for a box, a cell, a span, an array on an array's cells,
    a dot (`c-on.dan`, `c-in-over.dan`, `a4-at-bar-on-dot.dan`, `tests/test_engine.cpp:289`). Violated for a curve, a
    line, a parametric path and a surface: **session finding 7 confirmed** (`d1`, `d2`, `d3`, `d5`, `d6`, sheets s04,
    s05, s13; details in §5 fix 1): the dot lands on the curve's end box, 16 px off screen, at the end of the orbit
    with `t=` ignored, or on the floor of the space; `{q.y}` and `{p.z}` have no value; `morph q.x` changes the
    model but not the picture. Also violated for a formula part (`on=f[w]` seats on the whole formula) and for a
    link or a group (corner stack). A label `on=` a bar sits on the bar's value (`a5-at-bar-free.dan`, 76/100).
13. **`of=` places an annotation about its target.** Holds for a mark, a cell, a span, a formula part, a list of
    marks, a dot on axes, a 3D point (`c-of.dan`, `c-of-axes.dan`, `c-at-space.dan`). Violated, silently, for a link
    and a group: `mark n5 enclose of=ab` and `mark n6 brace of=g` draw **nothing** (`annotationTarget`,
    annotate.cpp:38-60, resolves marks only), `--check`: decodable, 0 lints, 100 (sheet s03). A `brace of=<curve>` is
    a 12 px brace at the curve's end: drawn but meaningless.
14. **`part-of` makes lanes and enclosures.** Holds for marks and nested groups (`c-partof.dan`: g1 inside g2 drawn).
    A link or a cell as a member (`group g3 part-of=[ab]`, `group g4 part-of=[arr[1]]`) is accepted, draws nothing,
    no lint. The checker flags the nested case: "box 'a' overlaps group 'g2'" (`ink.members` holds direct members
    only, placement.cpp:273-277), 35.5/100 for a correct picture.
15. **Data sits where its data puts it.** Holds (`b1-tex-at-data.dan`, `h1-coincide.dan`, `i1-europe.dan`,
    `i4-molecule-water.dan`). It also holds for text placed by coordinates: `scenes/b4-label-xy-on-plot.dan` puts two
    `label … x=2 y=1` on top of a dot at (2,1); the frame shows three things on one point and `--check` says 100
    (any kind with `x=` and `y=` is `direct`, layout.cpp:1287-1291; data bodies may coincide, placement.cpp:290-294).
    A loophole of the same family as the axis-less plane (history §9). Claude has not used it in 41 sessions
    (`label`/`tex` with `x=`: 0 hits).
16. **The engine is the only author of pixels.** Mostly holds. The vocabulary still carries four pixel numbers: `r=`
    on a dot is a radius in px (layout.cpp:66) while `r=` on a circle is world units; `size=` on a formula is px
    (README:288); `bend=` is a px bulge (README:273) honoured only on links the flow did not route
    (`b3-bend.dan`: `bend=80` on the forward link does nothing, placement.cpp:816-823); `extend=` is a flag (fine).
    In 41 sessions Claude typed `r=` 7 times (5-9 px on dots) and `size=` 3 times (28-36). `side=left|right` on a
    vector (6 uses, all forces on carts) is the point of application: data.
17. **Views: side by side in declaration order; stacked when one is a number line or timeline; the band above all;
    a third view; `schema net` re-selects.** Holds (`g1-three-views.dan` stacked, `g4-three-side-by-side.dan` three
    columns, `g3-reselect.dan`; sheets s07, s08, s16). Violated for marks written before the first view:
    `scenes/g2-view-after-marks.dan` lays the earlier boxes a -> b **inside the loss plot** (layout.cpp:30 "marks from
    before the first view belong to it"; the axes layout sends non-data marks to a 340x150 free layout in the plot's
    corner, layout.cpp:1295); `--check` 100 because the frame is the ground of its own view's things
    (`shots/g2-view-after-marks_end.png`).
18. **Free layout: two rows when crowded, a `layout:` crowd lint.** Rows hold (`f1`, `h2`: eight long-named bars split
    cleanly). The crowd lint did not fire in any scene I built; `f3-crowd-group.dan` (12 bars in one group, so no
    row split) scores 73 with six name-on-name overlaps and no crowd lint: the lint needs squeeze < 0.6
    (layout.cpp:1052) and names collide well before that. The checker reports the overlaps, so the turn's
    `<last_scene_issues>` still carries them.
19. **The checker tells forced coincidence from avoidable overlap.** Holds. `h1-coincide.dan` (two dots at one data
    point): `zero: p and q coincide on the axes but are unrelated; coinciding reads as 'equal'`;
    `d1-dot-on-curve.dan`: `zero: p and q overlap by accident in the axes layout` plus `layout: dot 'p' overlaps dot
    'q'`. zeroCheck (layout.cpp:1352-1378) covers dots, bars and boxes only; the layout checker covers all ink; the
    two never consult each other (enforcement map gap 4), so a pointer-on-pointer overlap is `layout:` only.
20. **Streaming: overlaps during a transition, the flicker between turns.** Overlaps: not reproduced. `e7-grow.dan`
    (six nodes one per beat, a new source column at 3.85 s): `--layout-report -` worst 100, 0 label flips; frames at
    3.9/4.1/4.3/4.6 s (sheet s14) show the row gliding right while d enters on the left, nothing overlapping. The
    flicker: reproduced once, by design. `e8-grow-tree.dan`: f(4) -> f(3) is a chain (left to right) until the second
    child arrives at 3.67 s; by 3.8 s the picture has turned top-down and f(3) has glided from beside f(4) to below it
    (sheet s15b). Eased (axiom 4 kept), but the mental map is broken once per tree; `FlowMemo` is cleared on the
    orientation change (flow.cpp:367). In the chat this happens whenever a tree's first two links come in separate
    beats.

Session findings in my area: **7 confirmed** in every form (fn path, line, parametric, surface, and a vector riding
such a dot). **1's family extended**: besides a curve under the wrong schema, these also vanish or misplace with a
clean `--check`: `of=` a link or group, `part-of` a link or cell, `at=`/`on=`/`in=` a link or group, `dot on=S` in
space. Enforcement-map §9 (position-from-structure has no test for the lint): confirmed, and the gap is wider than
the lint (verdicts 2, 3, 11-14).

## 2. Rule vs code gaps

1. README.md:316 "`region= … pos=` are retired: old scripts still play, the words are ignored, and `--check` says
   so." Code: engine.cpp:244-249 lints them under `if (ln.verb == Verb::MARK)` only; a `link`/`group` with them is
   silent (verdict 2). And "ignored" is false for a number: `kPositional` (engine.cpp:16-24) does not contain them,
   so `magProp` (462-468) makes `region=2` the magnitude (verdict 3). `isPlacementKey` (engine.hpp:41-44) keeps them
   out of categories only. The readback (engine.cpp:1575 prints every prop) carries them into the server's
   `state.dan`, so the same lint repeats every turn of that chat.
2. README.md:298 "You never say where a thing goes. Position is a channel of the dictionary, like color." vs the
   same reference: README:273 "`r=` (fixed dot radius)" and chat/examples/heart.dan:12 `mark blood dot x=1.4 y=0.35 r=9` (r in px on a dot: layout.cpp:66 `return
   std::max(2.0, num(*r))`), README:288 "`size=` is the font size in px" and "On axes, `x= y=` puts its centre at a
   data point" for a formula, README:273 "`bend=40` on links". Four numbers the author types that are pixels, not
   data. Claude uses `r=` and `size=` (verdict 16).
3. README.md:311 "touching on purpose: `on=` (on its top edge), `in=` / `over=` (on its centre)" and prompt.md:116
   "`on=` / `in=` / `over=` touch it on purpose" vs layout.cpp:687-699: the seat is the target's `anchorBox`; for a
   curve that is `curveEnd` (layout.cpp:430-433, a 12x12 box at the visible end), for a line a 0x0 rect at a `tpos`
   that is never set (bodyBounds falls through to `{p.x,p.y,0,0}`, layout.cpp:157; a line gets `tpos` only with
   `x= y=`, layout.cpp:1279). Nothing in the reference says a dot can ride a curve, but nothing says it can't, and
   the prompt's wording invites it (finding 7).
4. README.md:310 "`at=` a pointer naming a thing (or a cell, or a span a[1:3])" vs README:107 where annotation
   targets include "a named part of a formula (`f[id]`)". The parser accepts `at=f[w]` and `on=f[w]`; `anchorBox`
   has no tex-part branch (annotate.cpp:24-35 `texPartBox` is used by `of=` only), so both land on the whole formula
   without a word (verdicts 11, 12).
5. README.md:94-106 (annotations) and :62-69 (`group … part-of`): the targets are "a thing, an array cell or span,
   or a named part of a formula". Code: `annotationTarget` (annotate.cpp:38-60) and `groupBounds` (layout.cpp:177-199)
   silently skip anything that is not a mark; `analyze`'s `need()` (engine.cpp:238-253) checks only that the id is
   *defined*, and a link or group id is defined. So `of=ab`, `part-of=[ab]`, `part-of=[arr[1]]` pass every check and
   draw nothing (verdicts 13, 14).
6. prompt.md:116 "`<last_scene_issues>` lists any overlaps" vs the checker's blind spots and false alarms: nothing
   listed for the undrawn `of=ab` (c-of) or for boxes inside a plot they "belong to" (g2, verdict 17); listed
   though correct: a nested group's grandchildren (`c-partof`, placement.cpp:273-277 direct members only) and a
   label seated `on=` a member of a group (`c-on` "label 't1' overlaps group 'g'": `intended()` in
   place/check.cpp:10-48 knows group-member and label-owner, not label-on-member-of-group).
7. README.md:328 "Name a schema and it becomes a view: the marks after it are laid out together" vs layout.cpp:30
   and :48 "marks from before the first view belong to it": with a plot as first view, earlier boxes are drawn on
   the plot (verdict 17). The prompt's "declare the schema first" is the only guard.
8. README.md:301 "A hierarchy (every thing has at most one parent, and something branches …) reads top-down" vs
   flow.cpp:345-355: also requires no lanes and two thirds of the parents branching (documented in the code, not the
   README), and README:66 "`line` (connected)" vs flow.hpp:18 (`FlowEdge` has no direction), layout.cpp:1144 (every link becomes an edge) and flow.cpp:363 (every edge ranks): an undirected `--` link is an edge
   a -> b for ranking, so "connected" things sit in successive columns (i6c). The README never promises "beside",
   but a reader of "connected" expects it.
9. docs/HANDOFF.md:85 "Placement words (`region`, `below`, `left-of`, `pos`, …) are retired and ignored with a lint"
   and enforcement-map "Derived rule" row 1: both describe the lint as covering the words; neither mentions that it
   covers `mark` lines only or that the words still pass through `magProp`. The map's row for layout.cpp:1052 (the
   crowd lint) describes a lint that, in practice, rarely fires before names collide (verdict 18).

## 3. Where this rule limits creative freedom

The question: can the AI draw any field's picture while never giving pixels? The non-maths arrangements, each tried.

| case | scene | what the engine did | said structurally? | verdict |
|---|---|---|---|---|
| map of Europe, countries where they are | `i1-europe.dan`: `schema plane … axes=false`, nine `rect country x= y= w= h=` | nine boxes in their real arrangement (sheet s10), 100 | no: nine coordinate pairs Claude must know | the design (plane coordinates are data; prompt: "a physical system … drawn as its parts in their real arrangement on a `plane`"). The cost is Claude's geography and arithmetic (eval: 12/44 scenes with coordinate errors). |
| heart with four chambers and vessels | `chat/examples/heart.dan` | the shipped example: chambers as `rect`s at coordinates, blood as a dot morphing between them | no (coordinates) | the design; vessels are not drawn as things (a `line` per vessel would need more coordinates) |
| orchestra seating | `i2-orchestra.dan` (groups, free) vs `i2b-orchestra-plane.dan` (circles at coordinates) | groups: three upright columns of boxes side by side, not a fan (sheet s10); plane: the fan around the conductor | the fan cannot be said; only "sections" can | a gap: a semicircle of rows is a conventional, non-metric arrangement the rule has no word for; coordinates must be invented |
| chessboard | `i3-chessboard.dan`: `array cols=8 items=[64 glyphs]`, `enclose of=B[7][4]` | the board with the pieces, the king boxed and pulsed; 100 | yes, entirely | holds; the best case for the rule |
| molecule geometry (water) | `i4-molecule-water.dan`: circles, two `line` bonds, `angle pts=` | correct bent shape with 104.5° (sheet s11); each bond took its own colour and shows its id ("B1", "B2") because the lines had no category or label | geometry no (coordinates); topology yes (`i4b`: O -- H, O -- H as a graph, drawn as a tree) | the design; 3D molecules go the same way with `schema space` spheres and `polygon` faces. Note for the zero axiom: two bonds in two colours is what "unrelated things never share a colour" produces unless the author gives them one category word |
| football formation (4-4-2) | `i5-football.dan`: `schema graph`, boxes in four categories, no links | four category columns: GK, 4, 4, 2, rotated 90° to the pitch (sheet s11); 100, no coordinates | yes, as columns | holds, with a twist: the formation reads sideways and there is no pitch; a pitch would need the plane and 11 coordinate pairs |
| family tree with marriages | `i6-family.dan` (`--` for marriages, arrows to children) and `i6b` (couples as groups) | i6: left-to-right flow, grandma a column right of grandpa, mum floating mid-column (sheet s11); i6b: two lanes feeding the children, left to right (s12) | partly: parent -> child yes; "married" (beside, same generation) no | a gap: two things that are peers and adjacent cannot be said; `--` ranks (i6c); two parents make `isTree` false, so generations run sideways |
| sentence parse tree | `i7-grammar.dan` | a proper top-down tree | yes | holds |
| orbit, forces on a cart (physics, from real chats) | sessions 20261017-075325, 20261020-184851 | `axes=false` planes, `param` clocks, `vector at=k side=right` | coordinates for the setup, structure for the motion | the design |

Assessment. Three kinds of picture, and the rule serves two of them:
- **Topological** (graphs, trees, lanes, sequences, grids, arrays, categories): fully structural, and the engine's
  layouts are good (every flow case I built scored 100 and read well). This is also where the eval says Claude is
  strongest (CS topics 4.0-4.25).
- **Metric** (maps, molecules, mechanisms, orbits, geometry): coordinates, which the project defines as data. Not a
  violation; the rule's text ("Where position is data … an overlap is reported as a lint") and the prompt both say so.
  The freedom is there; the cost is that Claude must compute positions, which the small-model report measured as the
  largest mechanical failure class (12/44). The report's step 1 (geometric references: intersections, peaks, tangents)
  is the right answer and is itself an instance of this rule: name the fact, let the engine compute the pixel.
- **Conventional but not metric** (a seating fan, couples beside each other, a formation on a pitch, a periodic table,
  a timeline with lanes, a courtroom): here the retired words were the only vocabulary (`left-of`, `below`), and their
  removal was a deliberate trade against two layout systems fighting (history 8b). What is missing is small and
  structural, not positional: a relation that means "beside, same rank" (an undirected link could mean exactly that,
  verdict 9) and perhaps "rows" (a group of groups laid as rows). Everything else in this class can be said as a
  grid, a plane, or lanes.

One more freedom cost worth naming: the `axes=false` plane is the only way to draw anything metric, and the checker
treats everything on it as data that may coincide (verdict 15). That is right for a molecule and wrong for a text
placed by hand. The loophole is real but, in 41 sessions, unused.

## 4. Small model

Measured against `reports/Small model for tutor weak spots.md` (its conclusion: don't train a scene writer; small
models fit narrow, label-rich, checkable jobs; code first).

Would help, within this rule's area:
- **A layout-taste critic for offline evals** (the report's step 8). The exact checker scores overlap, off-canvas and
  composition; it cannot say that `f3` (73) is unreadable while `h2` (100) is fine, or that the user once saw a bad
  layout at 99.9 (HANDOFF §5). Labels that exist: 308-335 judge "layout" scores (mean 3.43, noise ±0.15), the
  `--layout-report` series, and (once logged, report step 6) the user's redo/reject clicks. Output: a rank among
  variants of one scene; checked against the judge and, cheaply, against held-out user picks. A small scorer on
  frozen image embeddings or on the checker's ink list (boxes, kinds, owners) trains in minutes. Not in the live loop:
  the stage must stay deterministic (append.sh, golden.mjs, the readback contract all assume it).
- **Coordinate plausibility for metric scenes**, as a tagger not a placer: given a plane scene's marks ("heart":
  rects named atrium/ventricle/lungs/body), flag an implausible arrangement (lungs below the body, a right atrium on
  the right of a left atrium) before it plays. Labels could be made by Claude at scale and filtered by a second
  frontier pass; output is a few tokens; checked by whether the human agrees. Value is uncertain: the report's
  table says the generic-picture failures (17/44) are judgement, and this tagger does not pick the picture, only
  catches a wrong arrangement.

Would not help:
- **A learned placer** (positions from a model instead of `place/flow` and the label placer). It would break
  decodability (the same text must give the same scene), the memo-based mental map, the goldens, and the
  checker-as-ground-truth; and the exact layouts are already at 100 on every structural case here. The report's
  ALGOGEN/SGA citations argue the same: deterministic layout beats model layout.
- **Resolving "a point on the curve", intersections, tangents, auto ranges**: code (this audit's fix 1 and the
  report's step 1). Deterministic, exact, testable.
- **Choosing between a plane with coordinates and a graph**: judgement, already Claude's, already right in the
  sessions I read (every `axes=false` plane held a physical arrangement).

## 5. Proposed fixes, ranked by value over cost

### Fix 1 (finding 7): a thing that rides a curve. `on=<path|line|solution|surface>` derives the dependent coordinate

Spec Claude could follow:
- **Who can ride:** `dot`, `label` (seated at the point), a `vector`'s tail (`at=<rider>` already follows; also allow
  `vector on=f x=2` directly). Anything else `on=` a curve lints: `relation: 'k' on=f: only a dot, a label or a
  vector can ride a curve; an array seats on an array`.
- **What can be ridden, and the free coordinate the author gives:** a `path` with `fn=` and a `line` (`x1 y1 x2 y2`
  or `x y slope`): `x=`, y derived; a parametric `path` (`xt yt`) and a 3D `path`: `t=`, both coordinates derived; a
  `solution`: `x=`, y from the sampled curve (nearest sample, interpolated); a `surface` (`fn=`): `x= y=`, z derived.
  A missing free coordinate lints (`'q' on=f needs x=`; `on=orbit needs t=`); a free coordinate outside the domain
  or where the function is not finite falls through to the existing `value:` lint.
- **The model:** no derived property is stored. The readback stays `mark q dot on=f x=2 label=Q` (decodable by
  construction). `{q.y}`, `{p.z}` and `q.y` inside `\val{}` and other `{expr}` read the derived value:
  `Engine::member` (engine.cpp:571-577) consults the attachment when the property is absent. `morph q.x -> 2 2s`,
  `re-mark x=`, `trail=`, `camera follow=q` all work because position is derived per frame. The shadow engine
  (values spoken ahead) needs the same `member` path, no layout.
- **Layout:** in `layoutAxes` (layout.cpp:1250-1295) a rider is placed before the generic `hx && hy` branch:
  `tpos = axes_.toScreen(x, y_of(target, x))`, `direct = true`, `positioned = true`, culled like a dot; the attachment
  branch of `layoutPointers` (layout.cpp:687-699) skips targets of curve kinds. In space (`spaceStroke`,
  space.cpp:183-196) a dot `on=` a surface takes `z = fn(x, y)`; `spacePoint` likewise, so labels, `enclose`, `drop=`
  and `camera zoom on=p` follow.
- **The checker:** `picture()` already allows `on=` pairs to touch (placement.cpp:295-302), so the dot may sit on the
  curve's stroke; two riders at one `x` are data-placed and `zeroCheck` reports "coincide … reads as equal", which
  is correct.
- **Afterwards, the prompt:** `chat/examples/derivative.dan` becomes `mark p dot on=f x=2`, `mark q dot on=f x=4`,
  `@slide morph q.x -> 2.02 3s`, slope `\val{(q.y - p.y)/(q.x - p.x)}`; the 3D toolbox move drops the written-twice
  `z={0.4*(p.x^2 + p.y^2)}` (chat/toolbox.md "A surface, and a point that moves on it") and README:242's
  `value={p.z}` keeps working.
- **Tests that fail today and pass after:** (1) `dot on=f x=2` body centre within 1 px of `toScreen(2, f(2))`
  (today: the curve's end); (2) after `morph q.x -> 3`, centre at `toScreen(3, f(3))`; (3) `spokenLines()` for
  `"height {q.y}"` contains `f(q.x)` (today the `say:` lint "has no value"); (4) `dot on=L x=3` on a line at the
  line's y (today 16 px off screen); (5) `dot on=orbit t=1` at `(2cos 1, sin 1)` (today at the end of the orbit);
  (6) space: `dot on=S x=1 y=1` has `p.z == S(1,1)` and is drawn (today on the floor, `{p.z}` empty); (7) `array
  on=f` lints; (8) `--check` of `scenes/d1-dot-on-curve.dan` has no `zero:`/`say:` lint and the readback re-parses;
  (9) `examples/derivative.dan` rewritten with `on=f` gives the same P/Q centres as the expression version.
- **Axioms touched:** 3 (the rider shares the curve's ink by a declared relation: the axiom's own clause), 6 (the
  readback keeps the relation and no derived number: stronger decodability), 1 and 4 untouched; the derived rule is
  strengthened (y from structure instead of a second copy of the function). Size: ~150-250 lines across layout.cpp,
  engine.cpp (`member`, lints in `analyze`), space.cpp, plus tests and the example/toolbox edits; half a day to a day.

### Fix 2: relations that cannot resolve must say so (silent no-shows)

A load-time lint in `analyze` for `at=`, `on=`, `in=`, `over=`, `of=` and `part-of` members whose target is a link,
a group, or a kind the relation cannot place; and `at=f[part]`/`on=f[part]` either resolved through `texPartBox`
(the better fix, ~20 lines in `anchorBox`) or linted. Message in the project's voice: `relation: 'n5' of=ab points
at a link; notes point at things, cells and formula parts (or name the link in the Say)`. Tests: `scenes/c-of.dan`,
`c-at.dan`, `c-on.dan`, `c-partof.dan` each gain the lint and fail `--check` (today 100, 70, 40, 40 with no word about
the missing things); `at=f[w]` body sits over the `w` glyphs. Axioms: 6 (a readback that lists things the stage does
not show is the same hole as session finding 1), 3 (a note with no ink). ~40-60 lines, 2-3 hours.

### Fix 3: finish retiring the words

(a) Lint them on `link` and `group` lines too (engine.cpp:245: drop the verb condition or add LINK/GROUP); (b) strip
them from the props at parse or load so the readback stops carrying them and so a numeric one cannot become a
magnitude (or add the eight words to `kPositional`). Tests: `a2` lints; `a3`'s bars read 5, 3, 1 and the readback has
its `# derived score` line; `a1`'s readback has no `region=`. Axioms: 2 (a retired word must not pick the scale),
6. ~15 lines, under an hour.

### Fix 4: the checker's two false alarms and one blind spot

`intended()` (place/check.cpp:10-48): a group covers its members' members (collect recursively when building
`ink.members`, placement.cpp:273-277), and a label seated `on=`/`in=` a member of a group is inside the group. Blind
spot: marks before the first view land inside a plot (verdict 17): lint `view: 'a' comes before the first schema
line; declare the view first` and, in `addFrames`, let a plot frame be the ground only of data-placed things, so
boxes inside it are reported. Tests: `c-partof` 100 unchanged picture; `c-on` without the `t1`/`g` message; `g2`
fails `--check` with the lint and a "box 'a' overlaps the plot 'loss'" message. ~40 lines, 2 hours. Axiom 3.

### Fix 5: the flicker at a tree's first branch

Decide a scene's orientation once from the prescan when the whole script is known (`analyze` already walks every
line per scene to pick the auto schema, engine.cpp:135-150): if the scene's links will form a tree, lay the first
link top-down. In the chat the whole script is not known; the `plan` block's `frame:` line could carry `tree`, or the
server could pass a hint when the prompt's "hierarchies" row is chosen. Test: `scenes/e8-grow-tree.dan` snapshot at
2.5 s already vertical; `tests/test_place.cpp` streaming case: a chain that later branches keeps f(3)'s slot. ~30-50
lines engine side; the prompt/server hint is a design question for the user. Axiom 4 (continuity of the mental map).

### Fix 6: an undirected link means "beside"

In `assignRanks` (flow.cpp:50-96) an undirected edge (`--`, `Link.directed == false`) contributes no rank and instead
asks `orderRanks` to keep the two adjacent in one column (same column as the earlier-declared one). `i6-family.dan`
then puts grandpa and grandma in one column, dad and mum in one, the children in the third: still sideways (two
parents), but generations hold. Together with a lane per couple (`i6b`) the family tree reads. Test: `i6c` grandma's
box x within 1 px of grandpa's; `e1`-`e7` goldens unchanged. ~30 lines, 2-3 hours. Touches the derived rule only
(order from a relation).

### Fix 7, small ones

- Pointers at different spans of one array share the spread (layout.cpp:674-677, 710 keyed by the base id): `c-at`
  loses "pointer 'l2' overlaps pointer 'l3'". 15 lines.
- A thing seated `on=` a bar clears the bar's value label (`seatGap`, layout.cpp:368-373): `a5` 76 -> 100. 5 lines.
- Retire the pixel numbers or declare them: `bend=` is already ignored where the flow routes (drop it from README:273
  or lint it as ignored like the placement words); `r=` on a dot and `size=` on a formula could become words
  (`size=small|large`), or stay and be documented as the two pixel knobs the language keeps. The README's "formula at
  a data point" (`tex x= y=`) and `label x= y=` on a plot should be a pointer (`at=`) or a rider (fix 1); a lint for
  text placed by coordinates on a plot closes the b4 loophole. Each 10-20 lines; a documentation decision first.
- The crowd lint (layout.cpp:1052) could test name collisions after the squeeze instead of the squeeze alone; low
  value since the checker already reports them.

## 6. Checked and fine

- The lint for all eight retired words on a `mark`, each spelled as the README lists it (a1); `pos=` read nowhere.
- Prompt, toolbox, examples and scripts carry no retired word; Claude stopped using them the day they were retired.
- Flow layout: a cycle of three (back-link arcs under), a diamond, a 16-node tree with 12 leaves on one row, two
  lanes with 3 and 5 stages aligned from one input, a link to a group (drawn to the lane box, columns from its
  members), a 5-party 12-message sequence, a graph growing one node per beat (a new source column inserted on the
  left, the row gliding right, no overlap at 3.9-4.6 s, 0 label flips), categories as columns with no links.
- Free layout: 15 bars in two rows, bars and dots and a card mixed on one baseline, 8 long-named bars in two rows;
  groups stay together.
- Views: three side by side; stacked with a number line; the band above all; `schema net` re-selecting; a dot on a
  curve in a re-selected view at `{L(2)}`.
- Relations where they are implemented: `at=` a mark/cell/span/curve end/dot/3D point; `on=` a box/cell/span/dot,
  an array on an array's cells; `in=`/`over=` on centres; `of=` a mark, a cell, a span, a formula part, a list, a dot,
  a 3D point; nested groups drawn; `vector at=` a dot that moves by expressions (the current idiom) rides along.
- Typed numbers that are data: `x= y=` on axes/plane/space, `x1 y1 x2 y2`, `pts=`, `w= h=` on rects, `side=` on a
  force (point of application), `cols=` (structure), `extend=` (a flag), `t=` on timelines.
- The checker's wording: "coincide … reads as 'equal'" for data-forced coincidence, "overlap by accident" otherwise.
- `--check` was decodable on all 51 scenes; the readback re-parsed every relation, including the ones the engine
  cannot place (which is the problem: decodability without a picture).

## Work log
- Read audit-brief-common.md, enforcement-map.md ("Derived rule" table), session-notes.md (finding 7 is mine).
- Read: README language section, chat/prompt.md, chat/toolbox.md, the 4 examples, layout.cpp (all), place/flow.*,
  placement.cpp picture()/curve/link geometry, annotate.cpp, space.cpp dot/vector/camera, engine.cpp key
  sets/lint/magProp/catProp/related/doMark/readback/member/pathAt, main.cpp modes, tests (test_engine 65/289,
  test_place flow cases), reports/Small model…, history 8b/9.
- Wrote 51 scenes under scenes/; `--check` of all (check-all.txt); readbacks (readbacks.txt); layout reports
  (layout-reports.txt); frames (shots/), contact sheets (sheets/s01-s16); viewed every sheet.
- Sessions read (read-only): all 41 under chat/sessions, turns/*.txt and *.dan, for property-key frequencies, retired
  words by date, `on=`/`at=`/`side=`/`size=`/`r=` uses, and the `axes=false` planes.
