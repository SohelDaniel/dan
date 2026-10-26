# Audit A4: axiom 4, Continuity

"Every change is an eased morph; things grow in, links draw, focus fades." (README: "Every change is a morph with
easing. New marks grow in, links draw themselves, and focus fades.")

Method: read engine.cpp / layout.cpp / render.cpp / placement.cpp / camera.cpp / space.cpp / controls.cpp, then
reproduced every suspect with two `--snapshot` frames 0.1 s apart (the second frame already shows the change complete
= a jump; a frame in between shows motion = eased). Scenes in `audit-4/scenes/`, frames in `audit-4/shots/`, one
contact sheet per scene in `audit-4/sheets/` (frames left to right in time order), layout reports in `audit-4/flips/`.
Commands: `./tutor scenes/<s>.dan --snapshot shots/<prefix> --at t1,t2,…` (times given per item); cue times for
Say-driven scenes came from `./tutor scenes/<s>.dan --layout-report - --verbose`. All frames are from the muted,
offscreen renderer; nothing here depends on the voice.

Short answer: the eased machinery (NUM/STR Actives, entrance, focus, band, camera, 3D, ranges, label springs) is
real and works wherever a change goes through it. The stage jumps in the places where a change bypasses it, and
those places are not exotic: re-marking a link, adding a property, replacing an expression by a number, morphing a
function, chaining two formula morphs, a ticking `\val{}`, a second view arriving, a scene's new axes. Ten of the
jump paths fired in the user's real chats (counts below). No lint names any of them.

## 1. Verdicts

Sub-rule / case → verdict, reproducer (scene; snapshot times; what the frames show).

Eased morph of a change:
- Numeric re-mark of an existing literal: HOLDS. `r02b_remark_num_to_num.dan` (1.45,1.55,1.8,2.3): x 3 → 3.0 → 3.7 → 5 over 0.7 s.
- Re-mark of a prop the mark did not have yet (`r=`, `color=`, `opacity=`, `label=`): VIOLATED. `r42_new_prop_remark.dan` (1.45,1.55,1.8): the dot is r=7 then r=24 in one frame. `r47_remark_adds_color.dan` (1.45,1.55,1.9): blue → red in one frame; a category change (`r12b_category_remark.dan`, same times) cross-fades over 0.8 s. engine.cpp:957-960 (`else` branch: no `cur` → direct `setProp`). Real chats: 4 `color=` and 4 `colors=` added by re-mark (sessions 20261020-223156, 20261022-172953 ×3, 20261022-191811, 20261022-205428).
- Re-mark that swaps an expression for a literal (or a literal for an expression): VIOLATED. `r02_remark_expr_to_num.dan` (1.45,1.55,1.8,2.3): `mark q dot x={2+h}` then `mark q x=5`: x=3 → x=5 in one frame. Same `else` branch (NUM vs EXPR types differ). Real chats: 1 (20261023-022741 turn 2, `p` x={…} re-marked `x=2 y=3`).
- `mark` on a link (label, colour, value): VIOLATED. `r01_link_remark.dan` (1.45,1.55,1.8): white "w = 0.2" → red "w = 0.9" in one frame. engine.cpp:881-886 (every prop `setProp`). Real chats: 10 lines in 3 sessions (20261022-205428 `mark w21 label="dw = …"`, 20261022-211954 `mark w11 color=yellow`, 20261024-101323 `mark lz label="z̄ = 0.5" color=orange` ×4: today's session).
- Re-`link` to another end: VIOLATED. `r03_relink.dan` (1.45,1.55,1.8,2.5): the arrow is on C in the next frame while the boxes are still gliding. engine.cpp:976-979. Not seen in chats.
- Re-`group` with a new member: VIOLATED. `r04_regroup.dan` (1.45,1.55,1.8): the enclosure spans A..C in the next frame (box positions unchanged). engine.cpp:995-998; `groupBounds` has no state to ease. Not seen in chats.
- `morph <path>.fn -> {expr}`: VIOLATED, worse than a cut. `r13_fn_morph_expr.dan` (1.45,1.6,2.2,2.9,3.2): the curve and its name vanish for the whole 1.5 s and the new curve pops at the end. engine.cpp:1046-1049: an EXPR target takes the numeric path, `a.to = num(EXPR)` is NaN, `fn` is a NaN number until `finalVal` lands. `--check`: clean, no lint.
- `morph <path>.fn -> "expr"` (quoted): VIOLATED, permanent. `r13b_fn_morph_str.dan` (1.45,1.6,2.2,3.2): the curve never comes back (`fn` is now a string; `pathAt` needs an EXPR). No lint. Real chat: 20261020-184851 turn 1, line 48 `morph s.fn -> "sin(x)+sin(3*x)/3+sin(5*x)/5" 1.5s`: the "sum" curve of the Fourier scene disappeared for the rest of that lesson.
- Lists of a different length (a polygon gaining a vertex): VIOLATED (documented only by a parenthesis). `r44_list_length_change.dan` (1.45,1.55,2.0): square → pentagon in one frame. layout.cpp:213-226 eases only equal lengths; README §Lists: "(same length)". No lint.
- `morph … 0s`: a cut by request. `r43_morph_zero.dan` (1.45,1.55). Sanctioned (the author asked), undocumented, no lint. See §3.
- Chained `tex` morphs (the second starts while the first glides): VIOLATED. `r15b_tex_chain_tight.dan` (cues at 1.75 and 2.15; frames 2.1,2.17,2.3,2.6,3.7): the first morph's glide (ink box w=120 at 2.1) snaps to its finished state (w=301 at 2.17) when the second takes over; `settleActive` (engine.cpp:1004-1021) erases the blend. Real chats: 13 `morph .tex` lines within 3 lines of the formula's mark or previous morph (e.g. 20261017-075325 turn 1, lines 18 and 20).
- A `tex` morph while the formula is still writing in: VIOLATED. `r07_tex_morph_during_writein.dan` (0.25,0.32,0.4,0.6,0.9): at 0.25 s only "a² + b²" is written; at 0.32 s the whole old formula is visible at once (render.cpp:123-128 and 152-170: once a blend exists the write-in branch (135) is skipped and every old glyph is drawn at alpha 1-t).
- `\val{}` ticking inside a formula: VIOLATED (a per-frame re-centring). `r08_val_ticking.dan` (fine series 2.0…4.2 every 0.1 s, `magick … -trim` left edge): 182 → 182 → 196 → 196 → 182 → 196 → 182 … → 156 → 169 px. The formula is centred on the width of its current text (render.cpp:121 `originOf`, layout.cpp:147-151 `bodyBounds`), and `fmtNum` alternates "0.7"/"0.73", so the whole formula shifts 14 px several times a second and 26-40 px when a digit is lost. Not eased. Every scene with a live `\val{}` does this (the README's recommended way to show a value: "It updates live while the value morphs").

New marks grow in:
- Marks: HOLDS. `r17_stagger.dan` (0.1,0.25,0.4,0.6,1.5): five bars enter 0.15 s apart, each over 1 s.
- A block arriving in a band that must grow first: HOLDS (held at appear=0 until there is room, layout.cpp:643-644; `r19_band_push.dan` 1.4,1.7,2.0,3.2,4.5: the plot glides down, the formula fades in after).
- Dot radius when a streamed beat raises the magnitude max: VIOLATED by construction. `r09a_dots_prefix.dan` vs `r09b_dots_appended.dan` at 1.5 s: with `c value=36` in the script, a and b are r≈15/22 px instead of 29/44 px. `dotRadius` (layout.cpp:72) reads `maxAbs_` directly and `appendText → analyze → usePrescan` (engine.cpp:89, 291-295) replaces it when the beat arrives, so live every dot shrinks in one frame. (Not observable with the headless tools: `--stdin` chunks exist only in instant mode; the two frames plus the code are the evidence.)
- Furniture at a scene start: VIOLATED (a pop after the fade). `r11_scene_boundary.dan` (1.4,1.8,2.15,2.25,2.4,2.8): the old scene fades over 0.7 s, the stage is empty at 2.15, at 2.25 the new axes, grid and ticks are at full strength while q is at 5% of its entrance. render.cpp:800-820 drawFurniture: alpha is `stageFade_` only, which is 1 the frame after `clearStage`. The first scene's axes also pop at t=0 (nothing to ease from; acceptable).

Links draw themselves:
- When both ends exist: HOLDS (`r18_graph_arrival.dan` 1.45,1.6,1.8,2.2,3.5: the new "b" arrow draws from bias while the boxes glide; layout score 100 at every sample, 0 flips).
- When an end arrives later: VIOLATED. `r06_link_before_end.dan` (1.95,2.05,2.2,2.5,3.1): at 2.05 the arrow is complete while B is a faint 5%-size box. `appear` ticks from creation for everything in `order_` (engine.cpp:1458-1467), the link just had nothing to draw to; the `link:` lint (967) fires but the engine then pops the link. Same mechanism for a group whose members come later and for an `at=` pointer's arrow (`r46_pointer_before_target.dan` 1.95,2.05,2.2,2.6: the label glides in from the info corner, the arrow appears complete).

Focus fades: HOLDS. `r32_focus.dan` (focus cued at 1.82; 1.8,1.9,2.1,2.5,5.55,5.7,6.0): a and c dim over ~0.3 s, stay dim to the end of the sentence (5.6 s), return over ~0.3 s. engine.cpp:1466 `smoothApproach(…,7)`.

Layout easing:
- Positions, band, zoom, graph re-flow: HOLD (r18, r19; `smoothApproach(…,9)` layout.cpp:804-808, `bandH_` 641, `fitStage` 890-942).
- A derivation growing (`aligned`): eased but every earlier line moves at every step. `r14_aligned_alone.dan` (1.4,3.0,5.6): line 1's ink box y 215 → 180 → 146 and x 466 → 466 → 250 (the block is re-centred in both axes). `r14b_aligned_over_plot.dan` (same times): top-anchored in the band, so y holds, but x still re-centres when a longer line arrives. Not a jump; a mental-map cost (3Blue1Brown keeps earlier lines fixed: new lines `next_to` the old).
- A second view added: VIOLATED. `r28_view_added.dan` (1.45,1.55,1.8,2.5): the full-width plot is half-width in the next frame (view areas, layout.cpp:830-845, are assigned per frame with no easing, and plot marks are `direct`).
- The schema re-declared with wider ranges: VIOLATED. `r45_schema_redeclared.dan` (1.45,1.55,1.8): axes 0..6 → 0..12 in one frame (execLine SCHEMA sets `rangesInit_ = false`, engine.cpp:851/864, so computeRanges uses k=1, layout.cpp:446). Not seen in chats (0 re-declarations in 63 turns); the 3D counterpart eases (space.cpp:423, `v.rangesInit` is not reset).
- `direct` (data-placed) marks snapping to `tpos` (layout.cpp:806-807): correct when the data eases (r02b, camera moves), and the carrier of every jump above that changes the data instantly (r02, r42, r45).
- Label flips (`--layout-report`, `labelFlips`): 23 over scripts/*.dan (7 of 39 scripts). Worst `compound.dan`: 8 flips in 1.7 s on one label (`tan#0 2->1->7->1->-2->5->-2`, 47.9-49.6 s; frames `shots/cmp_*.png` 47.8…49.6: the "growth rate" label hops along the moving tangent line). Then showcase 5 (a planet's name while it orbits), descent 4 (`p#0` 0<->2, frames `shots/dsc_*`: the dot's name flips corner as it steps), space-downhill 2, space-cross 2, space-kinds 1, derivative 1. Each flip glides on a 0.08 s spring (placement.cpp:1101-1150), so no single frame jumps, but a label whose side flips 4 times a second is in constant motion.

Camera: HOLDS. `r30_camera_chain.dan` (zoom at 1.60, reset at 3.08 while the zoom path may still run; frames 1.55,2.2,3.0,3.15,3.3,3.6,4.5): the reset takes over from the current view (camera.cpp:36-58 `cam_.from = cam_.now`). 3D: `r31_space_spin_view.dan` (spin, then `view=top` at 1.5; 1.4,1.6,2.0,2.5): the turn eases from the spinning orientation (space.cpp:478-516).

Scene: fade-out HOLDS (0.7 s CLEAR Active, blocking, so the next lines wait); the pop-in of the new furniture is the violation above. A NUM/STR morph in flight is erased with the stage (engine.cpp:670-673); CAM/ORBIT/WAIT survive harmlessly.

interrupt() / skip(): deliberate cuts (by reading; not reproducible headless: `--stdin` chunks exist only in instant mode and ignore `#!interrupt`). After `interrupt()` (engine.cpp:380-403) every Active lands, `update(0)` runs with `instant=true`, so `layout(1.f)` converges positions (smoothApproach with dt=1), `bandH_`/`zoom_` snap to their targets and the label springs snap (`keepLabels`: `snap` when dt ≥ 0.5). Appended lines then start from that settled state: new marks enter over 1 s, morphs start from the real values. No second jump. The playground (tools/write.mjs:64-80) does `#!load PREFIX`, `#!interrupt`, then the edited tail + `#!chunk`: one deliberate cut to the prefix's end state, then normal play. Fine.

The learner's slider (controls.cpp:57-77, test_engine.cpp:103): continuous under the drag by design; it erases every Active on that id (a morph in flight gives way) and sets the value directly; dependents (fields, curves, positions) track the value instantly, which is right for a drag (the hand is the easing).

Cross-fades: category colour and category label HOLD (r12b; `strBlend` + `blendText`, placement.cpp:495 sets `blendKey` for the category only). `label=` text: VIOLATED. `r12_label_remark.dan` (1.45,1.55,1.9): "before" → "after" in one frame although a 0.8 s STR Active exists (engine.cpp:950-956): nothing reads `strBlend["label"]` (markLabelsAt builds the name with `displayName`, render.cpp drawMark `label`/`box` kinds use `text()`), so the Active is dead weight. Same for `name=`, `text=`.

Session findings in this area: #2 (a wrong `from` animates from the real value) confirmed for numbers and strings (engine.cpp:1033-1037, 1050-1058); the `fn` case above is the hole beside it: the type of the target, not the `from`, decides whether a morph is a morph. #1, #3-#8 are outside axiom 4.

## 2. Rule vs code gaps

- README "Axioms, enforced" 4: "Every change is a morph with easing. New marks grow in, links draw themselves" vs engine.cpp:881-886 (`mark` on a link: `setProp` for every prop), 957-960 (re-mark of a new prop or another type: `setProp`), 976-979 (re-link), 995-998 (re-group), 1046-1049 (an EXPR morph target becomes NaN), 1458-1467 (`appear` ticks from creation, so a link whose end comes later is complete when first drawn), render.cpp:800-820 (furniture alpha = `stageFade_`, 1 right after `clearStage`), layout.cpp:830-845 (view areas not eased), layout.cpp:446 + engine.cpp:851/864 (`rangesInit_ = false` on a re-declared schema → k=1).
- README language table line 11: "morph <id>.<prop> … a change; the only way anything moves" vs engine.cpp:932-956: a re-mark moves numbers (0.7 s), lists (0.9 s) and strings (0.8 s) too, and 957-960: a re-mark of a new prop moves nothing (it cuts). The prompt (chat/prompt.md:169) teaches "Re-`mark` to update, `morph` to animate", which Claude follows: 136 re-marks in 63 turns, 18 of them into the cutting branches (10 on links, 8 adding `color=`/`colors=`).
- README §formulas: "`\val{expr}` … updates live while the value morphs" (232) vs render.cpp:121 / layout.cpp:147-151: the formula is re-centred on its current width every frame (r08: 14-40 px shifts).
- README §formulas: "Changing a formula (`morph f.tex`, or re-marking with a new `tex=`) moves the symbols the two formulas share to their new places and fades the rest" (234) vs engine.cpp:1004-1021 (`settleActive` erases a running blend: the glide snaps, r15b) and render.cpp:123-128 (a blend abandons a write-in, r07).
- README §Lists: "glide number by number (same length)" (99) vs layout.cpp:213-226: a different length is a silent cut (r44), with no lint and a caption that says "change of category -> color morphs" (engine.cpp:1044, the STR/LIST caption).
- README `scene`: "fade the stage out and start a fresh one" (265): the fade is there; nothing says the new scene's axes appear without one (r11).
- The dictionary row "change | motion | `Active` morphs; categories cross-fade color" is exact: categories do, `color=` and names do not (r47, r12). The README's hide text ("fade it to a faint ghost") holds (0.6 s).
- The handoff §3 says the axioms "are enforced in code": for axiom 4 there is no enforcement at all, only the default path. No lint mentions a jump; `--check` passes every scene in this audit with "axiom warnings: 0", and `--layout-report` measures overlap and label flips, never displacement per frame. The enforcement map's "Jump cuts" table (883, 957-960, 978, 997, 1033, controls.cpp, layout.cpp:72) is confirmed line by line; add to it: 1046-1049 (fn/EXPR target), 1004-1021 (chain snap), render.cpp:121 and 123-128, 800-820, layout.cpp:213-226 and 830-845, engine.cpp:851/864.
- `fn` has no documented morph at all (README shows `morph s1.from -> [0,2]` for solutions and `morph G.m` for grids, never `fn`), yet nothing stops it and Claude reached for it in a real lesson. Either a function blend (§5 F2) or a lint; today it is the only morph that empties the stage.

## 3. Where this axiom limits creative freedom

The axiom has no word for a change that really is instantaneous. The language offers `0s` (works: r43, c02, c04), but it is undocumented, the README calls morph "the only way anything moves" and the prompt says "nothing jumps", so Claude never writes it (0 of the 221 morph lines in the 63 turn files carry `0s`), and when a cut does happen the engine gives no sign that it was meant.

- Chemistry / physics, a quantum jump (`c01_electron_jump.dan`; frames 3.1,3.5,3.7,4.3): "it jumps straight to level three; it is never in between" while the dot slides through n = 2 (frame 3.7 shows it sitting on level 2). The eased picture states a falsehood about the physics. The honest picture is a cut, or a fade out at 1 and a fade in at 3 (two opacity morphs, which the language can write but the prompt never suggests).
- History, a border redrawn by treaty (`c02_treaty_border.dan`, cue at 4.48; 4.4,4.55,4.8): with `0s` the border moves to the river on the word, correctly. With the default duration it would creep across for a second, which is not what a treaty does. And a border that gains a vertex (`r44`) cuts whatever the author writes, because only equal-length lists glide.
- Economics, a price gap (`c04_price_shock.dan`; 3.0,5.0,5.2,5.6): `morph p.y -> 8 0s` with `trail=all` leaves a true gap in the trail (no vertical line): the right picture, and nothing documents that it can be done.
- Music, a plucked note (`c03_note_attack.dan`; 1.9,1.97,2.1,4.5): a 0.05 s morph is a sharp attack and a 3 s linear morph the decay. The axiom costs nothing here: short morphs are allowed.
- Programming: `code … line=` "glides the highlight" (README 205): a program counter jumping from line 2 to line 7 sweeps through lines 3-6, which never run. Suspected (by README text and render.cpp:333 `num(m,"line")` eased), not reproduced.
- Mathematics, a derivation (r14): the author cannot pin the first line; every step re-centres the block. Position comes from structure, so there is no `anchor=`; the engine would have to choose top-left stability for growing blocks on its own.

Is a cut ever legitimate? Yes, in four cases, three of which the engine already does: after an interrupt (the learner asked), at a scene boundary after the fade, under the learner's hand on a slider, and for a discrete event in the subject matter that the voice names on a cue word. The honest rule is "no accidental cuts": a cut is a sentence-level event, drawn as one (a flash or an afterimage at the old place, as 3Blue1Brown pairs `Flash` with an instant change), never a by-product of a property's type, a missing feature, or a second view arriving. Today every cut on the list in §1 is accidental, and the one legitimate cut has no name.

## 4. Small model

Measured against `reports/Small model for tutor weak spots.md` (don't train a scene writer; small models fit narrow, label-rich, checkable jobs; local 7-8B is ~20 s per scene on this Mac, so only tens-of-tokens jobs fit live):

- Not a model job: every violation in §1. They are engine code paths with a deterministic fix and a deterministic test (a displacement meter, §5 F0). A model that "smooths" output would be a worse version of `smoothApproach`.
- Not a model job: durations and eases. The engine already derives camera timing from path length (van Wijk & Nuij) and formula rests from the change; the same can be done for morph durations (distance in data units → seconds). Claude's chosen durations are not failing in the data.
- A narrow job that would fit, if wanted later: a discrete-event tagger inside the binding pre-flight (the report's step 7 tagger). Input: one beat (the Say and its lines). Output: a few tokens: which morph, if any, describes a discrete event ("jumps", "instantly", "overnight", "snaps", "is signed", "teleports", "collapses") and so should be a cut (`0s` plus a flash) rather than an eased morph. Labels: cheap to collect by asking Claude to tag its own beats on a corpus (the 80 eval scenes + 63 turns ≈ 500 morphs), then checked by a human on a sample. How its output is checked: the beat is re-run; the lint "a discrete word with an eased morph" (or the reverse) disappears. Size: 1.5-3B LoRA, hours. But this starts as a 30-line regex lint on the cue word's sentence, and the regex should ship first; the model only earns its place if the regex misses too much.
- A taste critic for "calm vs jittery" (step 8 of the report): frames can't show motion; it would need the displacement meter's numbers, which already decide the matter without a model. Skip.

## 5. Proposed fixes, ranked by value over cost

F0. A continuity meter, first (the test for everything below). In `--layout-report`, per frame and per thing: the displacement of its drawn box, the change of its alpha and colour, and the change of a plot's area; report `maxJump` (px/frame, which id, when) and `jumps` (a list like `flips`), ignoring frames where a `0s` morph or an interrupt landed. Add it to the golden set (`tests/golden/*.json`) so a regression fails `make golden`. Touches no axiom; ~80 lines in main.cpp/layout.cpp, 2-3 h. Failing today: compound.dan, derivative.dan and r08 report jumps > 10 px/frame; after the fixes below they don't.

F1. One path for every change of an existing thing (engine.cpp:881-886, 957-960, 976-979, 995-998). Re-marking a link, a group or a mark uses the same NUM/STR Actives as a numeric re-mark; a new numeric prop eases from a natural start (`r`/`size`/`w` from the current drawn size, `opacity` from 1, `x`/`y` from the current position); `color=` cross-fades through `strBlend["color"]` read in `colorOf` (like a category); names (`label=`, `name=`, `text=`) get `blendKey` in `markLabelsAt` and `blendText` in the `label` and `box` draws; a re-group eases `groupBounds` by keeping the previous rectangle and approaching the new one. Axioms 1 and 4 (strengthens both). ~80 lines, 3 h. Tests (live engine, `update(1/60)` as test 231 does): after `mark e color=red` on a link, `colorFor`-equivalent of the link at +0.1 s is neither the old nor red, at +1 s red; after `mark a label="after"`, `markLabels(a)[name].blendKey` is set while the STR Active runs; after `mark q r=24`, `bodyBounds(q).width` at +0.1 s < 20 px and at +1 s = 48. Today all three fail.

F2a. Numbers behind expressions: in doMark and doMorph, when the current value or the target is an EXPR, ease between their numeric values and land on the expression (`finalVal` already keeps expressions). engine.cpp:932-941, 1046-1049. ~12 lines, 1 h. Test: r02 (`x={2+h}` → `x=5`): the dot's x at +0.1 s is between 3 and 5. Fails today (it is 5).
F2b. A function morph: `morph f.fn -> {g(x)}` blends `pathAt` between old and new over the duration (store the old expression in `strBlend["fn"]` as today, and have `pathAt`/`curvePoints` lerp the two values); `fn -> "string"` (and any STR target on `fn`, `vx`, `vy`, `xt`, `yt`) is a lint ("fn must be an expression {…}; a string draws nothing") that fails `--check`. Axioms 1, 4, 6. ~50 lines, 3 h. Tests: at the midpoint of the morph x² → x³, `pathAt(f, 1.5)` lies strictly between 2.25 and 3.375 (today it is NaN); `--check` on r13b prints the lint and returns 1 (today it returns 0). Both fail today.

F3. `appear` starts at the first drawable frame (engine.cpp:1458-1467): hold a link's `appear` at 0 while `linkGeometry` is not ok, a group's while no member is positioned, a pointer's/annotation's while its anchor is missing. ~15 lines, 1 h. Test: r06: at the frame b first exists, the link's `appear` < 0.2 (or, with F0, no jump of e). Fails today (appear = 1).

F4. Formula stability (render.cpp:121, 123-128; engine.cpp:1004-1021):
  a. `\val{}` width: reserve the width of the widest value the prop will take (the prescan has min/max; typeset the value with `\phantom` padding to the longest of fmtNum(min), fmtNum(max)), or left-anchor a formula that holds a `\val{}` at the glyph before it. ~30 lines, 2 h. Test: r08: `bodyBounds(f).x` constant (±1 px) over the 3 s morph. Fails today (±20 px).
  b. A blend that starts during a write-in keeps the write-in's gate for glyphs the old formula had not written yet (`gt` from `appear`, applied to old glyphs too). ~10 lines, 1 h. Test: r07 at 0.32 s: the glyphs of "= c²" have alpha < 0.3. Fails today (≈0.66).
  c. A chained tex morph continues from where the glyphs are: when `settleActive` replaces a running `tex` blend, record the current interpolated glyph positions as the start of the new one (a synthetic "old" layout: the shared glyphs at `lerp(pa,pb,t)`, the fading ones at their alpha). ~60 lines, 3 h. Test: r15b: no glyph moves more than 6 px between 2.1 and 2.17 s (F0 meter). Fails today (the box widens 120 → 301 px in one frame).

F5. Areas and furniture ease too: `View.area` and `axes_.area` approach their targets (`smoothApproach`, 9) so a second view squeezes the first over ~0.5 s (layout.cpp:839, 1251); a re-declared schema changes range targets without resetting `rangesInit_` (engine.cpp:851/864); furniture gets an entrance (`furnitureAlpha` ramping over 0.6 s from a scene start, render.cpp:800-820). Axiom 2 is untouched (the ranges end where they end). ~45 lines, 3 h. Tests: r28: the plot's width at +0.1 s is > 80% of its old width; r45: `axes_.xmax` at +0.1 s < 7; r11: furniture alpha at +0.05 s after `clearStage` < 0.3. All fail today.

F6. Lists of different lengths glide: resample the shorter polyline onto the longer one's count (insert points along edges) before interpolating in `listNums`; for `items`, the existing value-travel already handles length changes. ~40 lines, 2 h. Test: r44 at +0.5 s: the fifth vertex is between the old edge and (6,3). Fails today.

F7. Label-side hysteresis in time: the placer keeps the previous side unless the new one has been better by a margin for 0.3 s (placement.cpp:1101-1150 already has `crossedSince_` for the crossing case; generalise it). ~25 lines, 1.5 h. Test: `node tests/golden.mjs` with `labelFlips` added to the goldens: compound.dan ≤ 2 (8 today), descent.dan ≤ 2 (4 today), total ≤ 10 (23 today).

F8. Streaming scales: ease `maxAbs_` for dot areas as `barScale_` is eased (layout.cpp:72 → a per-prop eased `dotScale_`). Shared with A2. ~15 lines, 1 h. Test: append a beat with a value 4× the max; the radius of an existing dot at +1 frame is within 5% of before, at +1 s at its new value. Fails today.

F9. Name the cut. README and prompt: "`0s` is a cut: for a change that really is instantaneous (a quantum jump, a treaty, a price gap); say it on the word, and the engine marks the moment" (a short flash/afterimage at the old place, like 3Blue1Brown's `Flash`, drawn by the engine when a 0 s morph lands). Lint: a `0s` morph with no cue word ("a cut needs its word"). ~40 lines + docs, 2 h. Test: `--check` on a `0s` morph outside any sentence prints the lint; with F0, a flash is drawn (an extra ink item) at the landing frame.

F10. Growing blocks keep their top-left (r14): when a block's size grows, keep its top (already so in the band) and its left edge (centre only when its width shrinks or a new block arrives). Position still comes from structure; this is the structure's own stability rule. ~20 lines, 1.5 h. Test: r14 line 1's ink x,y unchanged after each added line (fails today: y 215 → 146, x 466 → 250).

Ranking by value/cost: F0 (enables all), F1 (18 real-chat hits), F2a+F2b (a real lesson lost its curve), F4a (every `\val{}` scene jiggles), F3, F5, F4b+c, F7, F6, F9, F10, F8.

## 6. Checked and fine

- Numeric re-mark of a literal eases 0.7 s (r02b). Category change cross-fades colour and label 0.8 s (r12b). Morph with a wrong `from` animates from the real value with the identity lint (session finding #2; engine.cpp:1050-1058).
- Entrances: 1 s, 0.15 s stagger, at most 1 s behind the first (r17); a block waits for the band to make room (r19); bars grow from zero, dots pop with `easeBack`, curves stroke in, formulas write in glyph by glyph (render.cpp, seen in r07's first frame and r13's first frame).
- Links draw themselves when both ends exist (r18); a new routed bend unfolds from the straight line (layout.cpp:810-819, by reading).
- Graph re-flow when boxes and links arrive: boxes glide, links follow, no transient overlap in r18 (layout score 100 at every 0.5 s sample, 0 flips). The handoff's "overlaps that last only during a transition" did not show in this 5-box case; denser graphs not tried.
- Band growth pushes the plot down smoothly; the plot keeps 40%; zoom eases (r19; layout.cpp:641, 890-942).
- Focus: dims over ~0.3 s, holds for the sentence, returns over ~0.3 s (r32). Bounded (axiom 5's territory, confirmed in passing).
- Camera: a new 2D move takes over from `cam_.now` (r30); `camera reset` eases back; `follow=` is a critically damped spring with a dead zone (camera.cpp:189-201, by reading). 3D: `spin` then `view=top` eases from the spinning orientation (r31); orbit/tilt/zoom chain from `now` (space.cpp:478-516); space ranges ease at k = dt·5 (space.cpp:423).
- Scene: 0.7 s fade, blocking, the new scene's marks enter normally (r11); NUM/STR morphs in flight are dropped with the stage.
- Short morphs: a 0.05 s morph reads as a sharp attack (c03). `0s` works as a cut and a trail shows a true gap across it (c02, c04).
- interrupt()/skip() and the playground's `#!load` + `#!interrupt`: one deliberate cut, then everything eases from the settled state (by reading: engine.cpp:380-403, 1368-1393; layout(1.f) converges; keepLabels snaps at dt ≥ 0.5).
- The slider: continuous under the drag, a morph in flight gives way (test_engine.cpp:103), dependents track instantly, which is right.
- `hide`/`show`: 0.6 s opacity morphs; a hidden block keeps its slot (layout.cpp:595); a link fades with its ends (render.cpp:699-702).
- Label springs: a side change glides (0.08 s critically damped), never jumps (placement.cpp:1111-1131); only the frequency of flips is the issue (§1).
- Ranges of a number line / axes / timeline widen with easing when a new mark arrives (layout.cpp:446, k = dt·5, by reading; not frame-tested).
- All 32 audit scenes pass `--check` ("decodable: yes", 0 axiom warnings), which is itself a finding: the checker is blind to every jump in §1.

## Appendix: working log (chronological, raw)

### Log 1: suspects from reading engine.cpp / layout.cpp / camera.cpp / controls.cpp / space.cpp / render.cpp
S1 doMark on an existing link (881-886): direct setProp. S2 re-mark `else` branch (957-960): new prop / type change / EXPR. S3 doLink existing (976-979). S4 doGroup existing (995-998). S5 dur 0. S6 appear ticks from creation (1458-1467). S7 drawTex abandons write-in. S8 drawTex origin centred on current width. S9 dotRadius maxAbs_. S10 direct marks (807). S11 furniture alpha = stageFade_. S12 strBlend["label"] unread. S13 fn morph: EXPR target → numeric path → NaN; STR target → never a curve again. S14 group bounds unstated. (Later: S15 chain snap via settleActive; S28 view areas; S44 list length; S45 schema re-declared.)

### Log 2: reproduced (see §1 for the frames)
Confirmed jumps: r01, r02, r03, r04, r06, r07, r08 (fine series 182/196/156/169), r11, r12, r13, r13b, r15b (box 120 → 301 px in one frame), r28, r42, r44, r45, r47. By construction: r09a/b. Eased: r02b, r12b, r17, r18, r19, r30, r31, r32, c03, c04. Label flips: 23 / 39 scripts, compound 8, showcase 5, descent 4.
Session scans (63 turn files): links re-marked 10; re-marks adding a new prop: colors 4, color 4, label 2, range 2, set 2; expr→literal re-mark 1; `morph .fn` 1 (the Fourier "sum" curve vanished); tex morphs within 3 lines of the previous change 13; schema re-declared in a scene 0; `0s` morphs 0.
Tool notes: macOS has no `timeout`; zsh needs `setopt shwordsplit` for montage file lists; `magick … -crop 1200x494+40+66 +repage -fuzz 25% -trim -format "%X %w"` measures an ink box; snapshots ran fine without caffeinate.
