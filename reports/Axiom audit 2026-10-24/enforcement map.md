# Where each axiom is enforced (mapped 2026-10-24 from the working tree; paths relative to the repo root)

Main finding of the mapping: several code paths still change things instantly or move scales mid-script, contrary to axioms 2 and 4. They are marked **GAP** and collected in "Gaps worth knowing" at the end.

## Axiom 1: Identity (no delete, re-mark in place, wrong `from` animates from the real value)

| file:line | function | what it does |
|---|---|---|
| `markup/script.hpp:49` | `enum class Verb` | The verb set has no delete or remove. |
| `markup/script.cpp:562-568` | `parseLine` | `re-mark`/`remark` become `mark`. Any other verb, including `delete`, fails with `"unknown verb '…'"`. |
| `markup/script.cpp:424-437` | `readHideShow` | `hide`/`show` become a `MORPH` of `opacity` to `kGhostOpacity` (0.12) or 1 over 0.6s. "It stays on stage (identity)". |
| `markup/script.hpp:24` | `kGhostOpacity` | 0.12. |
| `markup/render.cpp:14-24` | `opacityOf` | A hidden note (`isAnnotationKind`) is rescaled to 0 so it leaves no ghost. The mark stays in the model. |
| `markup/render.cpp:25-30` | `ghost` | Opacity below 0.3 counts as a ghost. A link ghosts when either end does. |
| `markup/engine.cpp:881-886` | `doMark` | `mark` on an existing link updates its props in place. **GAP:** direct `setProp`, no Active. |
| `markup/engine.cpp:887-890` | `doMark` | Warns that ids must be unique if the id is a link or group. |
| `markup/engine.cpp:920-962` | `doMark`, re-mark branch | Updates in place. Category change: `strBlend` + STR Active (922-929). Numbers: NUM Active 0.7s (932-941). Lists glide (942-949). Strings cross-fade (950-956). Everything else: direct `setProp` at 957-960. |
| `markup/engine.cpp:965-984` | `doLink` | Existing id updated in place (976-979). Warns `"identity: '<id>' is already used"`. |
| `markup/engine.cpp:986-1001` | `doGroup` | Same pattern. |
| `markup/engine.cpp:1004-1021` | `settleActive` | A new change to the same `id.prop` takes over from the one in flight; a finished Active lands its final value first. |
| `markup/engine.cpp:1033-1037` | `doMorph`, string/list | Wrong `from` warns `"identity: line N morphs X from 'A' but it is 'B'; morphing from the real value"`. |
| `markup/engine.cpp:1050-1058` | `doMorph`, numeric | `start` is the real current value. Wrong `from` warns `"... (no jump cuts)"`. |
| `markup/engine.cpp:654-697` | `clearStage` | Erases marks, links, groups, order, colour slots, scales, camera, history. Runs only from the `CLEAR` Active. |
| `markup/engine.cpp:867-874` | `execLine` `SCENE` | Pushes `Active::CLEAR` (0.7s). |
| `markup/engine.cpp:1329-1337` | `tickActives` `CLEAR` | `stageFade_ = 1 - e`, then `clearStage()` at k >= 1. |
| `markup/engine.cpp:297-354` | `reset` | Full model wipe on load. Runtime `identity`/`zero` lints dropped (352-353). |
| `markup/engine.cpp:241, 243` | `analyze`, `need()` | `morph`/`focus` lines referring to an id never marked raise a lint. |
| `markup/engine.cpp:1025` | `doMorph` | `"morph: no thing called '<id>' (line N)"`. |
| `main.cpp:189` | `--check` | `identity` lints count as axiom warnings and fail the check. |
| `chat/prompt.md:119, 169`; `chat/toolbox.md:383` | prompt | `hide` dims what's done, "nothing jumps"; reuse what's on stage, re-`mark` to update. |

## Axiom 2: Structure (prescan fixes shared baselines and scales; they never jump)

| file:line | function | what it does |
|---|---|---|
| `markup/engine.cpp:116-227` | `analyze(fresh)` | The prescan. `see()` (120-129) records per-property `maxAbs`/`minVal`/`maxVal`/`isInt`. |
| `markup/engine.cpp:143-155` | `closeScene` lambda | One `Prescan` per scene (`scenePrescan_`, 151). Picks the scene's auto schema once (147-150). |
| `markup/engine.cpp:158-168` | `analyze`, MARK | Feeds every mark literal to `see()`; magnitudes via `kPositional`. |
| `markup/engine.cpp:207-210` | `analyze`, MORPH | Morph `from`/`to` values join the prescan. |
| `markup/engine.cpp:291-295` | `usePrescan` | Copies the scene's prescan into `maxAbs_`, `minVal_`, `maxVal_`, `propIsInt_`. Called from `analyze` (224), `reset` (316), `clearStage` (695). |
| `markup/engine.cpp:68-89` | `appendText` | A streamed beat calls `analyze(false)` (89): re-prescans the whole script so far and calls `usePrescan()`. |
| `markup/engine.hpp:483-488` | `struct Prescan` | Storage. |
| `markup/engine.cpp:462-468` | `magProp` | The magnitude property: first numeric prop not in `kPositional`/`kTextKeys`. One scale per property. |
| `markup/layout.cpp:1013-1035` | `layoutFree` | Bars on a shared zero, one scale per property; extents from `maxVal_`/`minVal_` plus the live value; `max=`/`min=` are the author's promise (1026-1027). |
| `markup/layout.cpp:1036-1041` | `layoutFree` | Baseline from `negFrac`. |
| `markup/layout.cpp:1060-1072` | `layoutFree`, `assign` | `barScale_[mp]` created once, then eased `smoothApproach(...,9)`: a changing scale moves smoothly, never snaps. |
| `markup/layout.cpp:441-582` | `computeRanges` | Number line (453-477), axes (479-554), timeline (556-578) ranges start from the prescan, widened by live values; recomputed only when the signature `schema|marks_.size()` changes (443-445), then eased `lerpf` k = dt*5. |
| `markup/layout.cpp:63-75` | `dotRadius` | Dot area scaled to `maxAbs_[k]`. **GAP:** no easing on `maxAbs_`. |
| `markup/space.cpp:~380-425` | space ranges | Snap to nice ends, eased k = lastDt_*5. |
| `markup/space.cpp:531` | lint | `"space: '<id>' reaches x = V, outside the box (x=[lo, hi]): widen the schema's x=[…] so it stays in view"`. |
| `markup/layout.cpp:587-649` | `layoutBand` | `bandNeed_` only grows within a scene (639); `bandH_` eased (641); a newcomer waits for room (643-644); the plot keeps at least 40% (647). |
| `markup/layout.cpp:658-662` | `bandFit` | Largest scale at which the band fits above a plot (0.6 to 1). |
| `markup/layout.cpp:890-942` | `fitStage` | Plots always fit (891-897); otherwise the zoom follows content for its first moments (`zoomSettle_`, 925-931), then only pulls back (937-941); eased. |
| `markup/engine.cpp:675-683`, `297-354` | `clearStage`, `reset` | Scales, band, zoom, ranges reset only at a scene or reload. |

When a streamed beat brings a value outside the prescanned range:
1. `appendText` (`engine.cpp:89`) re-runs `analyze(false)` and `usePrescan()`, so `minVal_`/`maxVal_`/`maxAbs_` grow.
2. Bars: `layoutFree` reads the new `maxVal_` every frame and `barScale_` eases to the new target (`layout.cpp:1066-1069`). Smooth, but no longer fixed for the whole script.
3. Axes, number line, timeline: the range recomputes only when `rangeSig_` changes (mark count), so a morph-only beat does not widen the range until the next new mark; the value can run outside the plot until then.
4. Dots: `dotRadius` uses `maxAbs_` directly (`layout.cpp:72`): every dot's radius jumps when an appended beat raises the max. **GAP.**
5. `tests/append.sh` guards only the final readback, not that scales stayed put.

## Axiom 3: Zero (unrelated things never share a colour; shared ink only when related)

| file:line | function | what it does |
|---|---|---|
| `markup/engine.cpp:16-27` | `kPositional`, `kTextKeys` | Never magnitudes, never categories. |
| `markup/engine.hpp:41-44` | `isPlacementKey` | `region below above left-of right-of near align on in over` are never categories. |
| `markup/engine.cpp:470-475` | `catProp` | **Where an unknown string becomes a category:** an explicit category word (`@category`), else the first STR prop not in `kTextKeys` and not a placement key. |
| `markup/engine.cpp:476-481` | `catValue` | Reads it. |
| `markup/engine.cpp:719-727` | `colorForKey` | One slot per key (`cat:<value>` or `id:<id>`) from `rm::categorical`; when slots run out: `"zero: more than N distinct kinds of thing, colors repeat"`. |
| `rm/raymanim.hpp:42-43` | `categorical` | Ten hues. |
| `markup/engine.cpp:728-733` | `claimColor` | Claims a slot at creation; skips `of=`/`color=` marks, uncategorised blocks, `param`. |
| `markup/engine.cpp:735-750` | `colorOf` | `color=` wins, then `of=` (parent's ink), then the category or own-id slot; category change cross-fades (746-749). |
| `markup/engine.cpp:925, 952, 1040` | `doMark`, `doMorph` | Only a category's new value claims a colour. |
| `markup/engine.cpp:520-536` | `related` | A link between them, the same group, the same category value, the same `color=`, or `of=` pointing at the other. |
| `markup/layout.cpp:1352-1378` | `zeroCheck` | Dots, bars, boxes: skips related pairs (1363); in 3D only a real meeting counts (1366); data-placed overlap warns "coincide" (1373); other overlap "overlap by accident" (1375). Called from `update` (1476) and `runToEnd` (1495). |
| `markup/place/check.hpp:1-6, 34-36` | `Picture`, `allowed` | Every drawn thing is "ink"; `allowed` holds intended overlaps. |
| `markup/place/check.cpp:8-48` | `intended` | Pairs that may touch: `allowed`, a label and its owner, a link and its ends, a group and its members, furniture with furniture, frames by region, links crossing links. |
| `markup/place/check.cpp:73+` | `check` | Overlap, OffCanvas, InZone, LabelFar, LinkThrough, LinksAlong, and a 0-100 score. |
| `markup/placement.cpp:114-322` | `picture` | Builds all ink. `at=` pointer ink is a `Label` owned by its target (117-120, ~137-140); a vector `at=` a thing is owned by it (188-192); annotation `of=` stroke and name allowed on the target, but a name may not sit on another part of a formula (210, 218); a mark's labels are owned by it (223-236); data on one plot may overlap (278-294); `on=`/`in=`/`over=` allow touching (295-302); `regionOf` (304-310, 57-67) keeps the band out of plots. |
| `markup/placement.cpp:324` | `layoutCheck` | Runs `check`; used by `--check` (`main.cpp:197-204`) and `--layout-report`. |
| `markup/binding.cpp:373-391` | `checkBinding` §2 | Sameness claimed, nothing shares a colour. |
| `markup/binding.cpp:404-423` | `checkBinding` §5 | A formula symbol not coloured like its thing. |
| `markup/engine.cpp:274-288` | `analyze` | `\textcolor{name}` checked against things, parts, colour names (`tex:` lint). |
| `markup/render.cpp:104-118` | `drawTex`, `glyphColor` | `\textcolor{id}` takes the thing's `colorOf`. |
| `markup/engine.cpp:1622-1640` | `readback` | `# overlapping: a b`. |
| `main.cpp:189` | `--check` | `zero`/`identity`/`tex` lints are axiom warnings. |
| `chat/prompt.md:87, 90, 94, 202`; `chat/toolbox.md:161-162` | prompt | Sameness = one shared colour; formula symbols take their thing's colour; >10 categories repeat. |

## Axiom 4: Continuity (eased morphs, grow in, links draw, focus fades)

| file:line | function | what it does |
|---|---|---|
| `markup/engine.hpp:134-145` | `struct Active` | NUM, STR, WAIT, PAUSE, CLEAR, PULSE, CAM, FLIGHT, ORBIT; default `ease = rm::rate::smooth`. |
| `markup/engine.cpp:1293-1366` | `tickActives` | `e = a.ease(k)`; NUM interpolates (1320), STR cross-fade (1322-1323), CAM/ORBIT (1324-1328), CLEAR fades (1329); lands `finalVal`, logs `since_`. |
| `markup/engine.cpp:1043, 1065` | `doMorph` | `a.ease = rateByName(ln.ease)`; unknown names fall back to smooth. |
| `markup/engine.cpp:756-760` | `entranceDelay` | 0.15 s apart, at most 1 s behind the first (used at 900, 972, 992). |
| `markup/engine.cpp:896-900` | `doMark` | New marks start at `appear = 0` with `enterIn`. |
| `markup/engine.cpp:1458-1467` | `update` | Entrance 1 s (`appearRate`); `o->lit` eased `smoothApproach(…,7)` (the focus fade). |
| `markup/render.cpp:32-34` | `alphaOf` | `easeOut(appear) * lit * stageFade_ * opacity`. |
| `markup/layout.cpp:114, 120` | `bodyBounds` | Bar height grows with `easeOut(appear)`. |
| `markup/render.cpp:568, 646, 670, 689, 135-145, 707, ~50-60, 121-133` | | Dots pop (`easeBack`), paths stroke in, vectors grow, formulas write in glyph by glyph, links draw (`frac = easeOut(appear*1.2)`), labels cross-fade, shared glyphs glide and new ones light. |
| `markup/layout.cpp:213-226` | `listNums` | Mid-morph lists ease from the old numbers. |
| `markup/layout.cpp:804-808, 810-819` | `layout` | Positions eased `smoothApproach(pos,tpos,dt,9)`; new link bends unfold. **Exception:** `direct`/data-placed marks set to `tpos` immediately (807). |
| `markup/camera.cpp:36-58` | `doCamera` | Takes over from `cam_.now`; CAM Active 1.2 s, smooth. |
| `markup/space.cpp:478-490` | `doSpaceCamera` | ORBIT from `now`, 1.5-12 s, smooth; spin rate eased (506). |
| `markup/engine.cpp:1408-1425`, `1080+` | `update` | The rest after a formula change (`kRest`). |
| `markup/engine.cpp:867-874, 1329-1337` | scene transition | Fades 0.7 s, then `clearStage()` (an instant cut at fade 0). |

Jump cuts and direct `setProp` with no Active:

| file:line | what happens |
|---|---|
| `markup/engine.cpp:883` | `mark` on a link: props change instantly. |
| `markup/engine.cpp:957-960` | Re-mark of a new prop or a type change: instant (`recordSet`). |
| `markup/engine.cpp:978, 997` | Re-`link` and re-`group` props: instant. |
| `markup/engine.cpp:1033` | A morph with `dur 0` is instant. |
| `markup/engine.cpp:380-403` | `interrupt()`: every Active jumps to its end (deliberate; `#!interrupt` from `chat/server.mjs:307, 312`). |
| `markup/engine.cpp:1368-1393` | `skip()`: same, on user skip. |
| `markup/controls.cpp:57, 77` | `pointer`: the learner's slider sets `value` directly (continuous under the drag). |
| `markup/layout.cpp:72` | `dotRadius` jumps on a `maxAbs_` change. |
| `markup/engine.cpp:1310` | `PULSE` waits up to 2 s for its target, else `"indicate: '<x>' never appeared"`. |

## Axiom 5: Bounded focus

| file:line | function | what it does |
|---|---|---|
| `markup/engine.cpp:776-781` | `execLine` `FOCUS` | Sets `focus_` (`none` clears). Warns `"focus: '<id>' does not exist yet"`. |
| `markup/engine.cpp:1190-1202` | `closeSentence` | Runs cue lines never fired, then `focus_.clear()` (axiom 5). |
| `markup/engine.cpp:1351-1362` (`tickSay`), `1150` (`startSay`, instant) | | The calls into `closeSentence`. |
| `markup/engine.cpp:1440-1458` | `update`, `lit` lambda | 1 for the focus and its base id (`a[3]` keeps the array lit), 1 for members of a focused group (recursive, depth 8), 0.7 for `of=` notes of the focus (1456), **0.2** for everything else (1457). |
| `markup/engine.cpp:1466` | `update` | `o->lit` eased: the dim fades in and out. |
| `markup/render.cpp:75-76, 107-111` | `drawTex` | `\textcolor{id}` glyphs forced to `lit = 1` when `focus_ == g.color`. |
| `markup/area.cpp:103` | `lit` | Same dimming for area diagrams. |
| `markup/engine.cpp:306, 663` | `reset`, `clearStage` | Clear `focus_`. |

Focus between sentences: no special code. A `focus` line between sentence N's close and N+1 sets `focus_`, cleared by N+1's `closeSentence`. If the script ends on a `focus` with no sentence after it, the focus persists and `readback` emits `focus <id>` (1598).

## Axiom 6: Decodability

| file:line | function | what it does |
|---|---|---|
| `markup/engine.cpp:1543-1650` | `readback(comments)` | `title`, `schema` (or per-view `schema <view> <kind> props`), `mark id kind [category] k=v…` with current values (expressions stay expressions), `link`, `group`, `focus`, the 2D camera spec (1599), the space camera lines in order (1600). |
| `markup/engine.cpp:1601` | | With `comments=false` returns here. |
| `markup/engine.cpp:1603-1649` | | Comments: `# value:`, `# derived <prop>: max min mean total order`, `# overlapping`, `# since:` (last 16), `# lint:` except `binding:` (1646-1647). Non-finite expression warns `value:` (1579-1580). |

Included: hidden marks keep `opacity=0.12` (test at `tests/test_engine.cpp:347`); annotations, params, learner-set values, camera specs. Excluded: `Say`, `wait`, `pause`, hooks/cues, `indicate`, `send`, morph durations and eases, timing, previous scenes, the `# schema (auto)` line in model mode.

| file:line | function | what it does |
|---|---|---|
| `main.cpp:157-206` | headless modes | `--readback` and `--check` run `instant=true`. |
| `main.cpp:166-174` | | `--stdin` headless chunks (`#!chunk`) appended with `appendText`. |
| `main.cpp:178-183` | `--check` | `model = readback(false)`, re-parsed into a second Engine, `runToEnd`, `model2 = readback(false)`, `same = model == model2`. |
| `main.cpp:186-205` | | Axiom warnings (`zero`, `identity`, `tex`); prints `decodable: yes/NO`, the counts, `pointingReport()`, lints, the first 6 layout messages, the layout score. Exit 0 only if `same && ok && zero == 0`. |
| `markup/engine.cpp:1486-1495` | `update`, `runToEnd` | A live run prints the readback when finished and writes `<script>.readback`. |
| `chat/server.mjs:221-224, 436-437, 534` | `readback()` | The server stores stage state as the folded readback in `state.dan`; turns build on it: the chat relies on decodability. |
| `chat/server.mjs:234-239, 434-436, 327` | `check()` | Background `--check`; lints go back to the model as `<last_scene_issues>`. |
| `tests/append.sh` | | Every script in chunks of 2, 3, 5 must read back identical to the whole. |
| `tests/golden.mjs` | | Layout-score regression (layout, not decodability). |

## Derived rule: position comes from structure

| file:line | function | what it does |
|---|---|---|
| `markup/engine.cpp:244-249` | `analyze` | `"layout: '<id>' <k>= is ignored: position comes from structure (links make a flow, groups make lanes, order is order)"`. |
| `markup/engine.hpp:41-44` | `isPlacementKey` | Placement keys are never content or category. |
| `markup/place/flow.hpp:1-8, 18-49`, `flow.cpp` | `layoutFlow` | Links make columns, declaration order within a column, top-level groups are lanes, a memo keeps the mental map while streaming. |
| `markup/layout.cpp:1088-1175` | `layoutGraph` | Lanes in group order (1112-1113), categories as columns when no links (1121), a link to a group stands for its members (1127-1130); re-laid only when the structure signature changes (1145). |
| `markup/layout.cpp:951-1086` | `layoutFree` | Left to right in declaration order, groups together, two rows when crowded; `"layout: N things crowd one row; group them, or use a table/array"` (1052). |
| `markup/layout.cpp:764-822` | `layout` | Skips marks with `on=`/`in=`/`over=` (`attachment`, 944-948) or `at=` (780): placed with the pointers. |
| `markup/layout.cpp:670+` | `layoutPointers` | `at=` labels below a cell or on the first free side. |
| `markup/layout.cpp:587-649` | `layoutBand` | Blocks with no place of their own stack at the top. |
| `markup/engine.cpp:143-150` | `analyze` | The auto schema per scene from structure. |
| `chat/prompt.md:116` | prompt | "Structure is layout. Never say where things go …". |

## Lint table (every `warn(`, by prefix)

| prefix | file:line | message |
|---|---|---|
| identity: | engine.cpp:888 | `identity: '<id>' is already a link/group; ids must be unique` |
| | engine.cpp:966, 987 | `identity: '<id>' is already used` |
| | engine.cpp:1036 | `identity: line N morphs X from '<from>' but it is '<old>'; morphing from the real value` |
| | engine.cpp:1056 | `identity: line N morphs X from F but it is S; morphing from the real value (no jump cuts)` |
| zero: | engine.cpp:723 | `zero: more than N distinct kinds of thing, colors repeat` |
| | layout.cpp:1366 | `zero: A and B are at one point in space but are unrelated; coinciding reads as 'equal'` |
| | layout.cpp:1373 | `zero: A and B coincide on the <schema> but are unrelated; coinciding reads as 'equal'` |
| | layout.cpp:1375 | `zero: A and B overlap by accident in the <schema> layout` |
| layout: | engine.cpp:248 | `layout: '<id>' <k>= is ignored: position comes from structure (…)` |
| | layout.cpp:1052 | `layout: N things crowd one row; group them, or use a table/array` |
| | main.cpp:204 | printed: `  layout: <checker message>` |
| tex: | engine.cpp:271-272 | `tex: line N: <TeX error>` |
| | engine.cpp:285 | `tex: line N: \textcolor{name} is neither a thing on stage, a part an annotation points at, nor a color name` |
| hook: | engine.cpp:233 | `hook: lines tagged @h have no |h| spoken, so they only run when their sentence ends` |
| | engine.cpp:235 | `hook: |h| is spoken but no line is tagged @h` |
| say: | engine.cpp:237 | `say: line N points at [id], which is never marked` |
| | engine.cpp:1741 | `say: {X} is a formula, which the voice can't read: say it in words` |
| | engine.cpp:1742 | `say: {X} has no value on stage when it is spoken, so nothing is said there` |
| morph:/focus:/indicate:/send:/send via:/link:/group: | engine.cpp:241 | `<what>: line N refers to '<id>', which is never marked` |
| morph: | engine.cpp:1025 | `morph: no thing called '<id>' (line N)` |
| focus: | engine.cpp:778 | `focus: '<id>' does not exist yet` |
| link: | engine.cpp:967 | `link: <id> connects 'a' and 'b' but one is not on stage yet` |
| indicate: | engine.cpp:1310 | `indicate: '<x>' never appeared` |
| list: | engine.cpp:182-183 | `list: line N: id.key has 'a' then 'b': a comma splits a number; write ab or quote it ("a,b")` |
| space: | engine.cpp:138-139 | `space: a surface is shown but the camera never moves; …add camera spin…` |
| | space.cpp:531-532 | `space: '<id>' reaches x = V, outside the box (x=[lo, hi]): widen the schema's x=[…] so it stays in view` |
| value: | engine.cpp:1580 | `value: id.key = {expr} is not a number (division by zero, a cyclic reference, or an unknown name)` |
| voice: | engine.cpp:1222, 1686 | audio unavailable / no API key: estimated timing |
| binding: (binding.cpp:305-307, at most 2 per sentence) | binding.cpp:358 | `says "<word>", but no V is on stage and nothing visible counts V (…)` |
| | binding.cpp:390 | `says things are the same / shared, but nothing it shows shares a colour (…)` |
| | binding.cpp:397 | `says each has its own, but none of the N arrows carries a name or value (…)` |
| | binding.cpp:403 | `says something <verb>, but nothing on stage changes during this sentence (show it happening, on that word)` |
| | binding.cpp:422 | `formula '<tid>' writes <sym> but doesn't colour it like '<id>' (\textcolor{<id>}{…})` |
| | binding.cpp:471 | `says "s" as a name, but no s is on stage (…)` |
| camera: | camera.cpp:20 | `camera: line N needs an axes or plane scene (this one is <s>)` |
| | camera.cpp:26, 34, 94, 154 | follow/on/fit not on stage; zoom should be positive |
| | space.cpp:460, 474 | `view=<name>` is not a view; `on='<x>'` is not on stage |
| stagger: | layout.cpp:396 | `stagger: '<ref>' is not a span of cells (a[0:3]), so it happens at once` |
| field:/slope:/heat:/contour: | fields.cpp:116, 241, 282, 326 | needs vx/vy or fn |
| pointing: | binding.cpp:246-250 | not a warn: `pointingReport()` printed by `--check` |

Bookkeeping: `warn` at `engine.cpp:405-407` de-duplicates in `lint_` and prints `lint: …` unless in the shadow engine. On append, `hook:`, "never marked" and `\textcolor{` lints are re-derived (228-231). `identity`/`zero` lints cleared on `reset` (352-353). `binding:` left out of the readback (1646-1647). Parse errors print `parse: …` (engine.cpp:59, 84).

## Tests in tests/test_engine.cpp, by axiom

1. Identity: `re-marking updates the props it names and keeps the rest` (41); `a hidden note leaves no ghost …` (347, also 6); `the learner's slider sets the param … a morph in flight gives way` (103).
2. Structure: `a band too tall for the room above a plot shrinks the stage …` (333). None test the prescan or scale stability.
3. Zero: `colour slots go to categories only …` (50); views apart (76); annotations don't land on each other (87); named enclose off the formula (355); brace beside a column (374); space not-equal (396); space names clear (418); curve named where it leaves (247); `indicate f[part]` is a named part (467); binding symbol as a name (185).
4. Continuity: the rest after a formula step (457); stagger pulses (230); stagger send (258); space point moves with the camera (384).
5. Bounded focus: **no test** exercises `focus`, `lit()` or `closeSentence`.
6. Decodability: `{expr}` changes only what is said (141); space camera lines read back in order (406); shadow reads values ahead, streamed or whole (127).
Position from structure: chain of links left to right (65); cells seated on= cells (289).
Voice and pointing: 117, 151, 159, 169, 201, 221, 271, 302.

## Gaps worth knowing

1. No tests for axiom 5, none for the `layout:` placement-word lint or the identity morph lint; axiom 2's prescan untested (no test that a scale stays fixed across a morph or an appended beat).
2. Streamed beats can move or break prescanned scales: dot radius jumps (`layout.cpp:72`); bar scale eases to a new value instead of staying fixed (`layout.cpp:1066-1069`); axes/number line/timeline ranges don't widen until a new mark arrives (`layout.cpp:443-445`).
3. Instant changes with no Active: `mark` on a link (883); re-mark of a new or type-changed prop (959); re-`link`/re-`group` props (978, 997); morphs with `dur 0` (1033); data-placed marks snap (layout.cpp:807); `interrupt()`/`skip()` (deliberate).
4. Zero has two separate relation checks: `zeroCheck` uses `related()`; the layout checker uses `picture()`'s `allow`/`intended`. Neither calls the other.
