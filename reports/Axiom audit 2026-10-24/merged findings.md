# The audit, merged (2026-10-24)

Eight auditors (Fable, max effort), one per axiom plus the position rule and the language itself, read-only, each
with reproducers in `audit-N/`. Every "WRONG" entry below was re-run by me before it entered this list
(`verify/log.md`). Sizes are the auditors' estimates. Nothing in the repo changed.

## 1. Right: what holds, with the evidence

- **Identity (1):** one thing per id; nothing removed except by `scene`/reload; ids unique across marks, links,
  groups by construction; every morph starts from the real value and a wrong `from` is linted and corrected
  (session note 2 confirmed); the chat's fold cannot duplicate a thing (a real session's state replays
  byte-identical). (A1)
- **Structure (2) for literal values in a whole script:** baselines incl. negatives, one scale per property,
  `max=`/`min=` promises, literal morph targets on bars, axes, timelines, 3D boxes: right from frame one, never
  move. Streamed final readback == whole readback in every case. (A2)
- **Zero (3):** data coincidence lints in 2D and 3D; names in a crowded 3D scene stay clear; a category shared
  across views shares its colour; formulas, params, cards, arrays take no palette slot; a cell with seven touching
  shapes, a music staff and a grammar tree all fit the axiom. (A3)
- **Continuity (4) wherever a change goes through an Active:** numeric re-marks, entrances with the 0.15 s
  stagger, focus fades, band growth, graph re-flow, 2D camera chains, 3D spin to view, scene fade-out, short and
  `0s` morphs, trails across a cut. The one-second rest after a formula step works in all three cases. (A4)
- **Bounded focus (5) in the normal cases:** a cued focus lasts its sentence, an uncued one the next, the dim eases
  to 0.2 and back, groups and parts, cells, `focus none`, two focus lines in one sentence, a `scene`. (A5)
- **Decodability (6) as text:** the round trip never fails (0 of 80 eval scenes, 23 audit scenes, 75 matrix
  cells, a 6-turn lesson); synonyms canonicalise to one spelling; hidden things, interleaved views, the `main` view,
  space camera lines, learner-set params, apostrophes, auto axes ranges all read back faithfully. (A6)
- **Position from structure where it was built:** flow (chains, diamonds, 12-leaf trees, lanes, a node linked to
  a group, 5-party sequences), free layout, views, the checker: every structural case scored 100 and read well;
  no transient overlaps when a graph grows a node per beat. Claude never typed a formula or label at coordinates
  in 41 sessions; retired words appear only in sessions before their retirement. (A7)
- **The append failures** on `space-downhill`/`space-saddle` are an artefact: one stale `# lint: space:` comment in
  the streamed readback; the model lines are identical. (A6, verified)

## 2. Wrong: violations with reproducers (all re-run by me unless marked A-only)

| # | what | axiom | evidence | hit in real chats? |
|---|---|---|---|---|
| W1 | A cue line placed one beat early runs at the **end of the previous sentence**, not on its word; `--check` is silent (the hook lint is global, not per beat). | 5 / sync | `verify/early.dan`: morph at 4.80 s, its word at 7.02 s | yes: a "dead end" cross 5.9 s before the voice said it (20261020-171259 t2). Corrects session note 4. |
| W2 | A plot-only kind under a schema that can't draw it is accepted, read back, scored 100 and **never drawn**: 15 kinds × 5 schemas = 75/75 cells. Same for annotations of a link or group, `part-of` a link or cell, `at=`/`on=` a formula part, an `angle` without `pts`. | 6, 3 | session finding 1; `verify/enc.dan` (enclose of a link: nothing drawn, 100) | not yet (Claude declares schemas); the trap is one typo away |
| W3 | Re-marking a thing to an **expression**, re-marking a **link**, re-`link`, re-`group`, a new property or `color=`/`label=`: instant, no easing. | 1 (no jump cuts), 4 | `verify/remark-expr.dan`: P teleports while Q eases | yes: 18 lines in 3 sessions (link labels re-marked today) |
| W4 | `morph f.fn -> {x^3}`: the **curve vanishes for the whole morph** and pops at the end; `fn -> "string"` removes it for good. | 4 | `verify/fn.dan`: no curve at 1.5 s of 3 | yes: a Fourier lesson lost its "sum" curve (20261020-184851) |
| W5 | The prescan reads **literals only**: a bar driven by `{expr}` slides the shared scale every frame; a dot at `x={k}` leaves inferred axes; a space point leaves the box. `--check` cannot see it (instant mode lays out the end state only: 100/100 where `--layout-report` says 84 and "runs 322 px off screen"). | 2 | `verify/r1-expr-bar.dan` (bar a: full height at 1 s, a sliver at 9 s, same value); `verify/r3c-expr-dot.dan` | yes: a gravity turn whose `pull` bar never changes height while the voice says it drops to a quarter (20261017-045709 t3) |
| W6 | Live mode re-prescans per chunk: a later beat re-scales what is on stage (bars ease to a quarter, axes 0-2 to 0-14, timeline 1990-2000 to 1990-2020, **dot radii jump in one frame**, the auto schema flips free→axes / axes→space). | 2 | A2 r2, r3, r13, r4, r5 (by code; both end states in frames) | not found: Claude writes ranges on all 33 real plots |
| W7 | A thing created on the same beat as `scene "…"` (with/cued) is **deleted** by the fade. | 1 | `audit-1/scenes/scene-with.dan`: readback has only `b` | not yet (0 of 37 chats cue a scene) |
| W8 | `color=blue` beside a category holding palette slot 0: unrelated things share a hue, no lint. | 3 | `verify/clash.dan`: `tax` and `input` the same blue | yes: about 8 of 41 sessions (approximate scan) |
| W9 | A category's colour **swaps across a `scene`** (slots reset and go by first appearance). | 3 | `verify/scenes.dan`: `input` blue then orange | plausible in any multi-scene lesson |
| W10 | A retired placement word is **not ignored**: `mark a bar region=2 score=5` draws a bar of 2 (region became the magnitude), lint says "ignored"; no lint on `link`/`group`; the readback keeps the words. | 2, position | `verify/region.dan` | only old sessions |
| W11 | A bare duration or number token on a mark becomes a **colour category** (`mark sec line … 5s`). | 3 | session finding 5 | not in 978 real mark lines |
| W12 | `label` kinds take a palette slot and are drawn in it; three plain notes come out orange, green, purple; 10 categories + 1 note trips the "more than 10" lint. | 3 | A3 a4_labels, b2 | likely |
| W13 | `dot on=f x=2` lands on the curve's bounding box; `on=` a line is off screen; `on=` a parametric path ignores `t=`; `dot on=S` in space sits on the floor with `{p.z}` empty. | position, 3 | session finding 7; A7 d1-d4 | no (nobody writes it yet; it is the feature the user asked for) |
| W14 | Pointing ahead points at a bar when the sentence **counts** ("all three bars" pulses the bar whose value is 3); plain holders beat formulas ("the slope is two" pulses a bar). | sync | A5 p1, p2 | plausible |
| W15 | `{a}` spoken after a long morph cued earlier in the same sentence says the end value seconds before the bar shows it. | sync | A5 e1 (documented behaviour, unwarned) | plausible |
| W16 | A `focus` with no sentence after it persists forever and reads back as `focus x`; `interrupt()` never clears focus; cells of a focused array snap to 0.3 instead of easing. | 5, 4 | A5 f3-last, f5 | latent |
| W17 | Chained `tex` morphs: the first glide snaps to its end when the second starts; a `tex` morph during the write-in reveals all glyphs; `\val{}` re-centres the formula 14–40 px several times a second. | 4 | A4 r07, r09, r12 | yes: every `\val{}` scene jiggles |
| W18 | A second view added halves the existing plot in one frame; a re-declared schema snaps its ranges; a link whose end arrives later pops in complete. | 4 | A4 | plausible |
| W19 | The readback is the truth about the **model**, not the stage: a bar morphed 10→2 reads back `v=2` and re-parses on a smaller scale; a morph adding `x=` re-parses as a different auto schema; trails are lost; the band's grown height is lost. `<last_frame>`, the background `--check` and a restart show a picture the learner did not see (live vs state: 21 k px differ). An empty view or a bare `schema left` at a turn's end is dropped by the fold, so the next turn's marks land in the wrong view. | 6 | A6 rt/*, turns/ev-*, rs-* | the view case: likely in any two-view lesson |
| W20 | The server strips every `#` line of the readback (no derived fact, overlap or history reaches Claude) and forwards only indented lines of `--check` (a `decodable: NO` would not reach Claude). | 6 (the loop) | chat/server.mjs 221-239, read | by design, but it means the readback's comments are dead weight |
| W21 | The checker's false alarms: nested groups ("box overlaps group", 35/100 for a correct picture); a label seated on a grouped member; marks before the first `schema <view>` line land inside a plot with a clean 100. | 3, position | A7 | plausible |
| W22 | Furniture in palette hues: pulses, new glyphs and the `code` highlight are palette yellow; the dictionary caption and key hints are drawn in the learner's window. | 3 | A3 e2 | always |

## 3. To change, ranked (value over cost; each has a failing-then-passing test in the auditor's notes)

Tier 1, the silent real-chat breaks, each small:
1. **Per-beat cue lints** and no cross-sentence fallbacks (W1). ~40 lines. A5 #1.
2. **A lint for a kind the schema can't draw, and for relations that can't resolve** (W2). ~60 lines + an ink-census test over every script. A3 #1, A6 #2, A7 #2.
3. **One eased path for every re-mark, re-link, re-group**, including expressions, `color=`, `label=` (W3). ~40 lines. A1 #1, A4 F1.
4. **Function morphs blend; numbers behind expressions ease** (W4). A4 F2a+F2b.
5. **Prescan by evaluation** (the shadow engine already runs the script ahead) and **`--check` lays out the start state and each sentence** (W5). Half a day + 2 h. A2 #1, #2.
6. **No delete during a scene fade** (W7). ~15 lines. A1 #2.
7. **Finish retiring the placement words** (lint on link/group, strip them, never a magnitude) (W10). ~15 lines. A7 #3.
8. **A bare duration/number token on `mark` is a parse error** (W11). ~15 lines. A3 #2.

Tier 2, colour (axiom 3):
9. Named colours join the slot table, or a lint on the clash (W8). ~25 lines. A3 #5.
10. `label` kinds are ink, not categories (W12). ~8 lines. A3 #3.
11. One relation predicate for `zeroCheck` and the checker; `in=`/`over=` stop being "accidental" (gap 4). ~40 lines. A3 #4.
12. Category colours kept across scenes, or colour as a function of the category name (W9; also the "flicker" item). Design choice, see §5. A3 #7, A6 #6.

Tier 3, decodability and the loop:
13. Readback keeps empty views and the current view (W19 view case). ~8 lines. A6 #1.
14. Readback carries the bar/dot scale as `max=`; pins the auto schema as a model line (W19). ~20 lines. A6 #3, #4.
15. A scene digest next to the text comparison in `--check` and `append.sh` (clears the 3D append artefact too). A day. A6 #5.
16. Server: forward `decodable: NO`; let the window write the true end-of-turn frame, or reword prompt.md:180. 5 lines. A6 #10.
17. Drop ignored placement words from the readback; remove the unused `.readback` writer. ~10 lines. A6 #7, #11.

Tier 4, continuity and voice polish:
18. `\val{}` width reserved; write-in and chained-morph continuity for formulas (W17). A4 F4.
19. Eased view areas and axes ranges; `appear` from the first drawable frame; dot radius eased when a stream raises the max (W6, W18). A4 F3, F5, F8.
20. Focus ends with the voice (idle, interrupt); cells ease; `interrupt()` keeps stage and readback in step (W16). A5 #2, #4, #6.
21. No auto-pointer for counts/ordinals; a lint when the voice would overtake a morph (W14, W15). A5 #3, #5.
22. Label-side hysteresis in time (23 flips over 39 scripts). A4 F7.
23. Checker: recursive group membership, seated-on-member, marks before the first view (W21). A7 #4.
24. Furniture off the palette; caption and hints off in the live window (W22). A3 #9, #10.
25. Small: reversed spans, `glueCue` exact-first, refuse `@cue Say:`, `# derived` honesty, empty `group part-of=[]`, array travel on a length change, docs (`from` optional, ids global).

## 4. Features and decisions for the user

- **D1. A point that rides a curve** (`dot on=f x=2`, y derived; `on=S x= y=`, z derived; `on=orbit t=`): session finding 7; A7 wrote the spec with 9 tests; half a day to a day. Removes the written-twice function from the derivative example and the 3D toolbox move. Recommended.
- **D2. Name the cut.** `0s` already works but is undocumented and Claude never writes it (0 of 221 morphs); A4 proposes documenting it, drawing it as an event (a flash at the old place), and linting a cut with no cue word. Recommended: small.
- **D3. Colour stability across scenes and turns**: keep a lesson-wide slot map, or make colour a function of the category name (a stable hash). Fixes W9 and the fold's colour drift; changes the dictionary row "every uncategorised thing gets its own slot". Decide.
- **D4. Uncategorised data on one plot shares one ink** (12 elements, 12 primes, 12 events no longer repeat hues). Bends the dictionary; decide.
- **D5. Live-mode widening policy** for a scale a later beat outgrows: A2 recommends "widen once, eased, at a sentence boundary, with a `structure:` lint fed back to Claude", keeping author ranges primary; no new syntax. Decide.
- **D6. `focus a,b` and `focus x hold`** (ended by `focus none`, a scene, an interrupt, the turn; lint past four sentences): bends axiom 5 openly for music/grammar cases. Medium value.
- **D7. Trails across a turn boundary:** carry (a `pts=` history key) or confess (prompt text). Decide.
- **D8. Rename `mark` → `thing`:** no principle decides it; measurable by A/B on check-ok and lints; helps the human reader; defer or do as a one-hour mechanical change. (A8 may add.)
- **D9. `path from=a to=b` as a synonym for a link:** principle 7 says no; the concept exists as `link`/`send`. Recommend: no; teach the word that exists. (A8 may add.)
- **D10. A `morph f.tex += "\\ …"` to append a derivation line** instead of re-emitting the whole formula each step (token cost grows quadratically). (A8 measures it.)
- **D11. Camera on timelines and number lines** (the only axiom-compatible way to change scale on a 4.5-billion-year line). A day.

## 5. The small-model question, merged

Seven of eight auditors found **no job** for a small trained model in their area: every question is a lookup or a
deterministic path the engine already holds (prescan, relations, easing, readback). The two narrow jobs that fit
the report's criteria (narrow output, labels exist, a checker to verify) are both in the voice layer and both
belong **after** the deterministic fixes: (a) placing a cue when the author wrote the line but not the marker
(labels: the hook lint and the engine's own word times), (b) auto-pointer disambiguation when two things show the
number said (labels: the pointing report). A layout-taste critic for offline evals is the only layout-side
candidate; a learned placer would break decodability and the exact checker. Nothing changes the earlier
report's conclusion: don't train a scene writer; code first.

## 6. Session notes, final status

1 confirmed and widened (W2) · 2 confirmed, works as designed · 3 open decision (D8) · 4 corrected: the
convention is load-bearing after the first sentence (W1) · 5 confirmed (W11) · 6 fine · 7 confirmed as a trap and
specified (W13, D1) · 8 decided: keep one rule · 9 confirmed in detail (W5, W6, W3, W16).

## 7. The language against the nine principles (A8; corpus: 80 eval scenes + 63 real turns, 5,351 lines)

Claude's syntax is near-perfect: 3 dropped lines in 5,351, 0 TeX errors, 0 broken cues in 23,438 spoken words, cue
pairing 99.6%, no decay late in a scene (longest turn ~1,260 tokens). The language's costs are elsewhere.

| principle | verdict | the evidence, and the cheapest move |
|---|---|---|
| 1 forward-only | partly | ranges, `\textcolor{part}` before its annotation, `|cue|` before `@cue`, `max=` promises are real lookaheads; the prescan and arming tolerate most of them. No cheap change; D5 covers ranges. |
| 2 line-start structure | met | – |
| 3 no matching at a distance | met (parser) | quotes and braces balance within a line; aligned has a named closer. |
| 4 the parser counts | partly | ids, `a[4]`, `cols=`, `pts=` arity, `m=[9]` are the author's; `{expr}`/`\val{}`/python already move the rest to the engine. |
| 5 near-zero escaping | partly | `"` inside `code` text impossible; prose collisions in a Say: `[a]`, `[sic]`, `|x|`, `||v||` eaten from the voice (two of three lint, `{1,2,3}` is silent). Move: resolve unknown `[word]`/`|word|`/`{…}` in favour of literal speech. |
| 6 fail soft | partly, silent | **W23**: a second kind word (`mark A rect area …`: area is a kind) is "kind given twice" and the whole line and thing vanish; all 3 real parse failures are this (product_rule lost its base rectangle). A typo'd schema name becomes an empty view; a typo'd/capitalised key becomes a colour and the name disappears; a `with` chain behind a failed line joins the previous step and runs at the wrong moment; `mark a box red` is not red. Moves: second kind word → category (~6 lines); typo'd schema → error; drop `with` lines behind a failed line; lints for the silent slips. |
| 7 one way per thing | partly | synonyms canonicalise in the readback (fine); the harmful context-dependence is the bare word (kind or category by list membership). `path from= to=`: Claude never wrote it (0/143); recommend fail-soft acceptance as a link, not a documented synonym (D9). |
| 8 training distribution | met | borrowed shapes carry it; the invented forms show no error signature. Principle 8 leans `mark` over `thing` (D8). |
| 9 cheap tokens | partly | **derivations re-emit the whole formula per step: 55% of all TeX characters and 15% of all scene characters are copies**; 58 of 351 steps also alter an earlier row (some are copy slips the engine glides over silently). Move: `morph f.tex += "…"` (D10, about −15% scene characters). `\textcolor{name}` naming nothing paints nothing while failing `--check`: 174 lint lines, 21/80 scenes `ok=false` (move: an unknown name becomes a named part with its own slot). |

Field matrix (14 scenes, all decodable, frames viewed): possible: grammar, anatomy, economics, programming,
probability, pendulum. With workarounds: chemistry (no bond kind, no two-headed arrow), music (rects for notes, staff
lines need `label=""`), geography (schematic only), circuits (piecewise motion by clamps; no `mod`, no path-following).
Impossible today: era spans on `schema timeline`, a fillable Venn intersection, `"` inside `code` text.

The voice: the author controls words, cues and `{expr}` only. Missing for a tutor: emphasis, a pause inside a
sentence, question intonation, reading a formula aloud (today linted "say it in words"; a deterministic
formula-to-speech pass should come before any model).

Small model at the language level: nothing fits; the error rates are too low to learn from.

Added to the lists above: W23 (kind given twice) joins Tier 1 as fix 8b (~6 lines); the `\textcolor` named-part
change joins Tier 2; `+=` is D10; timeline era spans and two-headed links are reach items after D1.
