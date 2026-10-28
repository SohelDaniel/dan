# Audit A3: axiom 3, Zero

Auditor A3 of eight. Repo read-only; nothing built, nothing committed. Everything referenced below lives under
<scratchpad>/audit-3
(call it A3/): scenes in `A3/scenes/`, `--check`/`--readback` output in `A3/out/<scene>.check|.readback`, frames in
`A3/shots/`, the sweeps as `A3/keysweep2.sh`, `A3/dropmatrix.py`, `A3/scan_sessions.py`, `A3/scan_colorclash.py`
(results in `A3/out/keysweep2.txt`, `dropmatrix.txt`, `sessions_scan.txt`, `colorclash_scan.txt`). Repo paths
below are relative to <repo>. Commands were run from the repo root
as `./tutor A3/scenes/<name>.dan --check | --readback | --snapshot A3/shots/<name> --at end --mute`.

The axiom, as the README states it (README.md:364): "Unrelated things never share a color. Two things share ink only
when the script relates them (`of=`, `on=`/`in=`/`over=`, `at=`, a name and its thing, data placed on one plot) …
Where position is data (axes, number line), an overlap is reported as a lint, because coinciding reads as 'equal'."
The handoff's framing adds the mirror: "Nothing in the picture without a meaning."

## 1. Verdicts

Sub-rule by sub-rule. "Holds" means verified with the reproducer named; a scene name is a file in `A3/scenes/`.

1. **Unrelated things never share a colour, within one scene, up to ten kinds: holds.** `b2_ten.dan` (10 categories):
   no lint; `g3_music.dan` (pitch classes C/E/G), `g4_grammar.dan` (NP, Det, N twice each): same category, same hue,
   nothing else shares one (frames `A3/shots/g3_music_end.png`, `g4_grammar_end.png`).
2. **Above ten kinds: violated by construction, with a lint.** `b1_eleven.dan` (12 primes on a number line, no
   categories), `g1_elements.dan` (12 element kinds), `g5_history.dan` (12 events): the 11th and 12th things repeat
   hues 1 and 2 (P1 and P11 both blue; H and Cl both blue), `--check` prints `zero: more than 10 distinct kinds of
   thing, colors repeat`. Code: `markup/engine.cpp:719-727`, palette of 10 in `rm/raymanim.cpp:46-49`.
3. **`color=<name>` against a palette slot: violated silently.** `a8_colorclash.dan`: `mark a bar input v=3` takes
   slot 0 (blue); `mark z bar color=blue v=1 label="tax"` is the same blue; `--check`: "axiom warnings: 0"
   (frame `A3/shots/a8_colorclash_end.png`, crop `z_d4a8.png` right half). `colorOf` (`engine.cpp:735-740`) returns
   the named colour without consulting the slots; `related()` is never asked. The twelve colour names
   (README.md:125, "Colours by name") are the same hues as the ten palette slots plus white and grey, so every named
   colour can collide with a category. Real chats (`A3/scan_colorclash.py`, an approximation of slot order over the
   41 `live.*` scripts): a `color=` name equals a palette hue held by an unrelated thing in 8 of 41 sessions.
4. **A `label` kind takes a palette slot and is drawn in it: violated (meaning the author never asked for).**
   `a4_labels.dan`: one bar (category `score`, blue) and three `label` marks: the notes come out orange, green and
   purple (`A3/shots/z_top.png`). `b2_label.dan`: 10 categories + 1 note trips the 10-kind lint; `b2_tex`,
   `b2_param`, `b2_card`, `b2_array`, `b2_enclose`, `b2_link`, `b2_group`, `b2_of`, `b2_colorkw` do not. Code:
   `claimColor` (`engine.cpp:728-733`) exempts block kinds, `param`, `of=`, `color=`, not `label`; `drawLabels`
   (`render.cpp:504`) and `drawInfo` (`render.cpp:741`) paint the text with `colorOf(m)`. The README (293) promises
   this only for formulas ("doesn't use up a color"); a free-standing note is ink in the same sense.
5. **The pointer pulse is a palette hue: suspected-minor, reproduced.** `drawPulses` (`render.cpp:423-433`) draws
   `col::yellow`, which is `kPalette[4]` (`raymanim.cpp:46`); so do the "lit" new glyphs of a changed formula
   (`render.cpp:133`) and the highlighted line of a `code` block (`render.cpp:336-337`). `e2_pulse.dan`: five categories, `indicate d1`: the ring
   around D1 is the hue of the fifth kind (`A3/shots/z_e2a.png`). The same applies to the `|cue|` pointing-ahead rings,
   which are `indicate` lines the engine adds (`binding.cpp:226-240`, `engine.cpp:1160-1165`).
6. **Same category across two views: holds** (`b3_views.dan`: input blue and output orange in both views, frame
   `b3_views_end.png`; slots are per engine, `colorSlot_`, not per view).
7. **The same category across scenes changes colour: holds as documented, wrong for the learner.** `b4_scenes.dan`:
   scene 1 `input` blue / `output` orange; scene 2 declares output first: `output` blue / `input` orange (pixels
   sampled at the bar centres: scene 1 input (81,152,233), scene 2 input (240,138,76)). `clearStage`
   (`engine.cpp:654-697`) erases `colorSlot_`; README.md:322 says "ids, colors, schema reset". The dictionary's channel
   "category = colour" therefore only holds inside one scene; the open item "the flicker: keep the mental map between
   turns" is this.
8. **Shared ink only when related: the rule exists three times and the three disagree: suspected, reproduced both
   ways.** (Section 2, gap 4 has the table.) `c1c_in.dan` / `c1d_over.dan`: a dot the author put `in=`/`over=` a box
   gets `zero: a and b overlap by accident in the free layout` from `zeroCheck` (`layout.cpp:1352-1378` via
   `related()`, `engine.cpp:520-536`, which knows nothing of `on=/in=/over=/at=`), while the checker allows it
   (`placement.cpp:295-302`). The other way: `c7b_coincide_cat.dan`, `c7c_coincide_color.dan`, `c7d_coincide_link.dan`,
   `c7e_coincide_group.dan`: two dots at one data point are silent when they share a category, a `color=`, a link or a
   group, although the README's list names none of these and says a data coincidence "reads as equal".
   "Same category" is not a relation in the checker (nothing in `check.cpp:8-48` or the `allow` calls in
   `placement.cpp` mentions it): the checker follows the README, `zeroCheck` does not. "Data on one plot" is a
   relation in the checker (`placement.cpp:278-294`, every data-placed pair allowed) and deliberately not in
   `related()` (the comment at 288-290 hands that case to `zeroCheck`): consistent with the axiom's last sentence.
9. **Data coincidence of unrelated things is linted: holds** in 2D (`c7_coincide.dan`: `zero: a and b coincide on
   the axes but are unrelated`) and in 3D (`f1_space_crowd.dan`: `a` and `h` 0.02 apart: `zero: a and h are at one
   point in space`; `spaceCoincide`, `space.cpp:636-645`, 1.5 % of the box diagonal). Projection-only overlaps of the
   other six crowded dots are not linted, as designed (`layout.cpp:1365-1368`).
10. **Names in space never sit on other things: holds in a crowded case.** `f1_space_crowd.dan` (8 dots within 0.5
    units, a vector, `camera spin`): `--check` layout 100, no "overlaps" message; the frame (`A3/shots/z_f1.png`) shows
    every name clear of every dot and name (some cross grid lines, which are ground). Test 418 confirmed.
11. **The mirror (a meaning the author asked for that the picture drops): violated, session finding 1 confirmed and
    widened.** `A3/dropmatrix.py`: for each of free, graph, numberline, timeline, sequence and each of path, line,
    arrow, region, vector, circle, rect, polygon, angle, field, slope, heat, contour, trail, tex-with-x/y (75 scenes):
    every one is `decodable: yes`, lint-free, layout 100, and the readback lists the thing (`A3/out/dropmatrix.txt`).
    Frames: path, line, vector, circle, angle and field under free, graph, timeline and sequence differ from the
    schema's base frame by **0 pixels** (`magick compare -metric AE`); under numberline by 297 px, which is the base
    dot moving 50x47 px (`A3/shots/z_nl.png`), nothing of `f` drawn. The gates: `render.cpp:184`
    (`if (!isAxes(schemaNow())) return; // linted at load`), `render.cpp:640` ("the remaining kinds live on axes /
    plane"), `render.cpp:924` (trails), `drawFieldMark` draws into `axes_.area` (`fields.cpp:95-101`). There is no such
    lint anywhere (`grep -n "needs axes\|only draw" markup/*.cpp` finds nothing); the prompt says it in words only
    (`chat/prompt.md:204`). A second member of the class: an `angle` without `pts=` on axes (`d4_angle_pts.dan`:
    `mark g angle x=1 y=1 from=0 to=60`) is accepted, read back and invisible (`render.cpp:262-264`,
    `if (p.size() < 6) return;`), while the documented `pts=[…]` form draws (`A3/shots/z_d4a8.png`, left).
12. **Session finding 5 (a duration on a mark becomes a category): confirmed.** `a1_dur.dan`: readback
    `mark sec line 5s x1=2 …` (`A3/out/a1_dur.readback`), 0 lints, decodable; the dictionary caption in the frame
    reads "thing -> a line · category '5s' -> color" (`A3/shots/z_caps.png`). Parser: `readMark`,
    `markup/script.cpp:286-299` (a bare token that is not a kind is the category; `isDuration` at 160 is only
    consulted by other verbs). In the user's real chats it has not happened: 978 mark lines, 0 duration tokens
    (`A3/out/sessions_scan.txt`).
13. **A string property the engine does not know becomes a category: confirmed, enumerated.** `A3/keysweep2.sh`
    (one bar with `<key>=w`; the checker's picture dump shows a "category of 'a'" label iff the key became the
    category): categories: `bin caption class d desc domain element extend from height hint index info key kind n
    opacity part-of phase prop pts role size state status step subtitle t tag type value via view w xlabel ylabel
    zlabel`; text: `at cmap color drop ease fn label levels name note of set shape side style text title to trail unit
    units`. So: every positional key given a string (`xlabel="hours"` on a mark, `w=wide`, `t=now`, `size=big`),
    `from=` (while `to=` is text: `kTextKeys`, `engine.cpp:25-27`, lists `to` and not `from`), `value="high"`, and
    any key from another field (`desc`, `caption`, `hint`, `role`, `phase`, `state`). `a7_strkeys.dan` reproduces
    `xlabel="hours"` and `value="high"` as two categories (two slots, two muted words under the dots). Real chats:
    0 of 978 mark lines take their category from a key; Claude always uses the bare word (53 x `input`, 26 x `op`,
    17 x `space`, …), so this is a trap for human authors and for other fields, not a current failure.
14. **A label-like word without `label=`: confirmed.** `a2_bare.dan`: `mark alice box "Alice Smith"` → category
    "Alice Smith" (its own slot; the box shows the id "Alice" large and "Alice Smith" small as the category word,
    `A3/shots/a2_bare_end.png`); `mark c box label="C" 3.5` → category "3.5". A quoted multi-word bare token is
    almost certainly a label; the parser takes it as a category without a word. Real chats: 0 such lines in 978;
    the only bare-token accidents are 4 lines `mark w box param label="w = 3"` (two kinds: a parse error, not a
    category; `A3/scenes/a5_kindtwice.dan`).

## 2. Rule vs code gaps

1. **README.md:269** "A word is a **category**. The bare word after the kind (`mark alice bar account …`) or the
   first text property (`status=overdue`) is drawn as color." vs `catProp` (`engine.cpp:470-475`): the first
   `Value::STR` prop not in `kTextKeys` and not a placement key. "Text property" in the README means a word meant
   as a kind; in the code it means any string the engine does not list as text, including positional keys given
   a string and `from=` (list in verdict 13). Also "drawn as color" vs the code: the word is also drawn as a muted
   label under bars, timeline dots and anchored dots (`placement.cpp:495`, called at 503, 508, 525, 547), inside boxes
   (`render.cpp` drawBlock/box rows), and announced in the dictionary caption. Not a violation; undocumented ink.
2. **README.md:293** "A formula is ink … It doesn't use up a color." Holds (`b2_tex`). Nothing says the same of
   `label`, `card`, `code`, `array`, `table`; the code exempts the block kinds (`isBlockKind`, `engine.hpp:55-57`)
   and not `label` (`claimColor`, `engine.cpp:728-733`). A note that the README calls "short text" (dictionary row
   "name, value | short text") is coloured like a kind of thing.
3. **`render.cpp:184`** `if (!isAxes(schemaNow())) return; // linted at load` vs the lint table: no lint exists for a
   world kind, curve, line, field or trail under a schema that cannot draw it (verdict 11). `--check` prints
   `decodable: yes`, `axiom warnings: 0`, `layout score: 100/100` for a scene whose named curve is never on stage;
   the readback vouches for it; the chat's `<last_scene_issues>` would stay silent (`chat/server.mjs:234-239`).
4. **Three relation lists.** README.md:364 lists: `of=`, `on=/in=/over=`, `at=`, a name and its thing, data on one
   plot. `Engine::related` (`engine.cpp:520-536`) accepts: a link between them, the same group, the same category
   value, the same `color=`, `of=` either way. The checker (`place/check.cpp:8-48` `intended`, plus `allow` from
   `placement.cpp:117-120` at= pointers, 188-192 vector at=, 210/218 annotation of=, 223-236 a mark's own labels,
   278-294 data strokes and data pairs, 295-302 on/in/over) accepts: label and owner, link and its ends, group and
   members, furniture with furniture, frames by region, link with link, plus the allowed pairs.

   | relation | README list | `related()` (zeroCheck) | checker `intended`/`allow` |
   |---|---|---|---|
   | `of=` | yes | yes | yes (stroke and name on the target; not on the rest of a formula) |
   | `on=` / `in=` / `over=` | yes | **no** (`c1c_in`, `c1d_over`: "overlap by accident") | yes |
   | `at=` | yes | **no** (no reproducer: a pointer dot is laid out beside its target, `c2_at`, `c2b_at_axes`) | yes (pointer label owned by target) |
   | a name and its thing | yes | n/a (labels are not bodies) | yes |
   | data on one plot | yes (and "coinciding reads as equal") | no, on purpose: it is the coincidence lint | yes (every data pair allowed) |
   | a link between them | no | yes (`c7d`: silent) | ends may touch the link, not each other |
   | the same group | no | yes (`c7e`: silent) | member and box; members not each other |
   | the same category | no | yes (`c7b`: silent) | **no** |
   | the same `color=` | no | yes (`c7c`: silent) | no |

   Neither predicate calls the other (enforcement map, gap 4). The practical effects: the author's own `in=` is
   linted as an accident; two "planet" dots or two linked dots at one data point pass without the "equal" warning.
5. **README.md:322** `scene "Part 2" # … (ids, colors, schema reset)` and the dictionary row README.md:351 "one palette
   slot per category": together they make a category's colour a per-scene fact (verdict 7). The prompt tells Claude
   that sameness is one shared colour (`chat/prompt.md:87-94`); across a `scene` the same word is a different colour.
6. **README.md:351** "every uncategorized thing gets its own slot" with ten slots means any eleven uncategorised
   things violate "unrelated things never share a colour" by construction (verdict 2). The axiom names "data placed
   on one plot" as a relation, so twelve dots on one number line are related in the axiom's own sense, yet the
   dictionary gives each its own hue and then lints the repeat. The two statements pull against each other; the
   frame (`b1_eleven_end.png`) shows which one the learner sees: twelve colours that mean nothing.
7. **docs/HANDOFF.md:147** gotcha "An unknown string property becomes a colour category" is accurate and is still
   the behaviour; it does not mention `from=` vs `to=`, positional keys given a string, or `label` kinds.
8. **`showDict_` default true (`engine.hpp:468`), `drawHud` unconditional (`render.cpp:944`), toggled only when
   `!live` (`main.cpp:429`).** The caption "dictionary  re-mark bob -> same mark, updated in place (identity)" and
   the hint line "space pause → skip R restart D dictionary S screenshot" are drawn in every frame
   (`A3/shots/z_caps.png`, `sheet1.png`); by the code the live `--stdin` stage draws them too, with keys (R, D, S)
   that `main.cpp:426-432` disables in live mode. Not verified in the live window (not allowed to open one).
9. **`drawPulses` colour (`render.cpp:423-433`) = `kPalette[4]` (`raymanim.cpp:46`).** The README's dictionary has no
   row for "pointed at", and the axiom says unrelated things never share a colour; the ring does.

## 3. Where this axiom limits creative freedom

- **Chemistry, twelve element kinds** (`g1_elements.dan`, frame `g1_elements_end.png`): twelve elements as twelve
  categories: Cl takes H's blue and Fe takes He's orange, with the lint. The author's only ways out are fewer kinds
  (group into metals / non-metals / noble gases: a real teaching move, but forced), `color=` names (twelve names,
  the same hues, no lint when they collide with a category: verdict 3), or shape as the second channel (seven
  shapes, README.md:273). A periodic-table lesson, a 20-amino-acid lesson, a map of 15 countries, an orchestra of 12
  instruments: all over the limit. Ten is a sound perceptual limit for *simultaneous* hues; the engine has no other
  channel for identity (a text label is not a slot) and no way to say "these twelve are all just elements, one ink".
- **Biology, a cell whose parts must touch without being "related"** (`g2_cell.dan`, `g2b_cell_in.dan`): membrane,
  nucleus, two mitochondria, ribosome, Golgi, ER as circles and rects on a plane. Both pass with layout 100 and no
  zero lint, with or without `in=cell`, because shapes are strokes to the checker (`placement.cpp:182`) and
  stroke on stroke is "a matter of taste" (`check.cpp:39`). So the axiom does not bite here; but it does not help
  either: `in=` on a circle is accepted and means nothing (no containment check, no shared ink, no layout); the
  mitochondria share green by category, which is right. The membrane's translucent fill covers every organelle:
  shared ink the author did mean. Fine.
- **History, twelve events on a timeline** (`g5_history.dan`): twelve hues for twelve events that are all the same
  kind of thing, then the lint. The honest fix is the author writing a category (`mark e1 dot event t=1440 …`),
  which the prompt does not suggest for timelines; the engine could treat uncategorised data on one plot as one
  ink (fix 6 below).
- **Music** (`g3_music.dan`) and **grammar** (`g4_grammar.dan`) fit the axiom well: pitch class and syntactic
  category are exactly "a word is a category", two NPs share orange, two C's share blue, and the tree reads
  top-down. No limit found.
- **Any field with its own vocabulary of keys** (verdict 13): `element=`, `phase=`, `role=`, `tense=`, `charge=`
  become categories (right, and documented as "the first text property"), but so do `desc=`, `caption=`, `hint=`,
  `info=`, `xlabel=` on a mark, `value="high"`: an author from outside maths gets a colour and a muted word for a
  property that was meant as text, with no message.
- **Explaining "these two are the same thing" in two scenes**: the category changes colour at the scene cut
  (verdict 7); a lesson that says "the input again" in part 2 shows it in a new hue.

## 4. Small model

Read `reports/Small model for tutor weak spots.md`: don't train a scene writer; small models fit narrow, label-rich,
checkable jobs; most failures are mechanical and code's to fix. In axiom 3's area that conclusion is even stronger:
every question above has a deterministic answer the engine already holds (which slot a thing has, which pairs
overlap, whether a key is in a set, whether a kind is drawn under a schema). Specifically:

- **"Is this overlap intended?"** No model. The three rule sets disagree (gap 4) because the *code* has three lists,
  not because the question is fuzzy; unify the predicate and let the author say the relation (`in=`, `on=`, `of=`)
  as the README already requires. Labels for a classifier do not exist: the 309 recorded lessons have checker
  diags but no human verdicts on them; `tools/eval` judges frames 1-5 overall, not per overlap. Collecting them
  (the user clicking "meant it / not meant" on `--layout-report` issues) would give a few hundred labels, enough
  for a threshold tuner, not a model, and the right output would still be "which relation word to add", which is
  a lookup.
- **"Is this bare token a category, a label or a duration?"** A regex (`\d+(\.\d+)?(ms|s)`, quotes, spaces, a
  number) and a one-line lint; no model.
- **"Is this unknown key text or a kind?"** A whitelist from the README's vocabulary and a lint for anything else.
  A model would guess `desc` is text and `phase` is a kind; so does a 20-line table, with a message.
- **"Should these twelve things share one ink?"** This is the author's call (a category word), and Claude already
  makes it in the 978 real mark lines (categories everywhere); the prompt can ask for it on timelines and number
  lines. Not a small-model job.
- **Where a small model could genuinely help, adjacent to this axiom:** a *colour-blind and contrast check of the
  final frame* is deterministic too (the palette is fixed), so no. The one label-rich job is the report's step 7,
  binding repair ("which thing does this spoken number name"), which decides what gets a pointer, i.e. what ink
  the engine adds: a wrong auto-pointer puts a false relation on stage. Today `holderOfValue`/`holderOfName`
  (`binding.cpp:156-225`) point only when exactly one holder exists, which is the safe choice; a tagger would
  extend coverage to the ambiguous cases, with the lint logs as free labels and the readback as the check. Worth
  it only after the deterministic fixes, as the report says.

## 5. Proposed fixes, ranked by value over cost

1. **Lint a thing the schema cannot draw** (session finding 1; verdict 11). At load (`analyze`), for every mark whose
   effective kind is a world kind, curve, line/arrow, region, field, or that has `trail=`, under a scene whose
   schema is not axes/plane/space: `layout: path 'f' needs axes or plane; schema free can't draw it`, counted as an
   axiom warning so `--check` fails and the chat feeds it back. Same for an `angle` without six `pts`, a `vector`
   without `dx/dy` (a missing-key variant). Test: `A3/scenes/drop/free_path.dan` fails `--check` on the new engine
   and passes today; `d4_angle_pts.dan` lints `g` and not `f`. Touches axioms 6 (the readback stops vouching for an
   invisible thing) and 3. ~40 lines in `engine.cpp` (analyze) + 2 tests, 2 h. Also fix the comment at
   `render.cpp:184`.
2. **A duration or number as a bare token on `mark` is an error; a quoted multi-word bare token gets a lint**
   (verdicts 12, 14). In `readMark` (`script.cpp:286-299`): `isDuration(tok)` → `fail("a mark takes no duration; to
   draw it over time morph a property")`; a bare token with a space → lint `mark: "Alice Smith" is a category, not
   a name: write label="Alice Smith"`. Test: `a1_dur.dan` is a parse error; `a2_bare.dan` lints. Axiom 3 and 6.
   ~15 lines + tests, 1 h.
3. **`label` kinds are ink, not kinds** (verdict 4): `claimColor` skips `label` without a category; `drawLabels`,
   `drawInfo` use `col::fg` when the label has no category, `color=` or `of=`, as `drawTex`/`drawBlock` already do
   (`render.cpp:102-103, 307-308`). Test: `b2_label.dan` no longer trips the 10-kind lint; `a4_labels.dan` frame has
   three notes in the foreground colour. Axiom 3 only. ~8 lines, 30 min. (Check `tests/golden` for label-colour
   dependence: none expected, the score ignores colour.)
4. **One relation predicate** (gap 4): make `Engine::related(a, b)` the README's list plus the structural ones the
   checker needs, and have `picture()` fill `allowed` from it, so `zeroCheck` and the checker agree. Concretely:
   add `on=/in=/over=/at=` (either way) to `related()`; drop "same category" and "same `color=`" from the data
   coincidence skip (keep link and group as a *weaker* relation that downgrades the message from "unrelated" to
   "linked things at one point"), and keep the checker's label/link/group/furniture structure as is. Tests:
   `c1c_in.dan` and `c1d_over.dan` produce no zero lint; `c7b_coincide_cat.dan` produces the coincide lint;
   `tests/test_engine` 50/76/87 unchanged. Axiom 3. ~40 lines, 3 h including a golden run.
5. **Named colours join the slot table** (verdict 3): when a mark says `color=blue` and `cat:`/`id:` slot 0 (blue)
   is held by something unrelated, warn `zero: 'tax' is blue by name and 'input' is blue by palette, but they are
   unrelated`; better, let `colorForKey` skip hues already taken by name in this scene (the prescan sees every
   `color=`), so categories avoid the named colours. Test: `a8_colorclash.dan` either lints or gives `input` a
   non-blue slot. Axiom 3. ~25 lines, 2 h. Real chats would hit it in ~8 of 41 sessions (approximation).
6. **Uncategorised data on one plot shares one ink** (gap 6, verdict 2): on numberline, timeline, axes/plane dots
   and space points with no category, `color=`, `of=`, draw them in one data ink (foreground or the first slot)
   instead of a slot each; a dictionary row change ("every uncategorized thing gets its own slot" becomes "… off a
   plot"). This bends the dictionary, so ask the user first. Test: `b1_eleven.dan` and `g5_history.dan` produce no
   10-kind lint and one hue. Axiom 3 (and the README's dictionary table). ~15 lines, 1 h + the decision.
7. **Keep a category's colour across scenes in one lesson** (verdict 7, the "flicker" open item): a
   `lessonSlots_` map (category word → slot) surviving `clearStage`, reset only by `reset()`/a new chat; a scene may
   still reuse the slots of categories it does not mention. Test: `b4_scenes.dan` scene 2 `input` is blue. Touches
   axiom 1 across scenes and README.md:322 ("colors reset" becomes "colors of the words you reuse are kept").
   ~20 lines, 1.5 h.
8. **Text keys, and a lint for unknown ones** (verdict 13): add `from`, `xlabel`, `ylabel`, `zlabel`, `view`, `caption`,
   `desc`, `hint`, `info`, `subtitle` to `kTextKeys` (or better: a key is a category only if it is *not* in any known
   set, and an unknown key gets `mark: '<id>' desc= is not a key the engine knows; its word "…" became a colour
   category`). Test: `a7_strkeys.dan` lints twice. Axiom 3. ~15 lines, 1 h.
9. **Pointer and focus furniture in a non-palette colour** (verdict 5): draw pulses in `col::fg` at low alpha, or
   remove yellow from the palette's first ten (there are twelve names). Test: `e2_pulse.dan` frame ring ≠ k5's hue
   (a pixel test, or a unit test that `col::yellow` is not in `kPalette`). ~3 lines, 20 min; check the goldens'
   frames by eye.
10. **The dictionary caption and key hints off in live mode** (gap 8): `setShowDictionary(false)` and no hint line
    when `live` (`main.cpp` stdin path). Verify in the real window after `tutor restart`. ~4 lines, 20 min.

## 6. Checked and fine

- A formula, a param, a card, an array, an enclose, a link, a group, an `of=` mark and a `color=` mark take no
  palette slot (`b2_*.dan`, only `b2_label` trips the lint). README.md:293 holds.
- Ten categories share nothing (`b2_ten`); the same category in two views shares one hue (`b3_views`).
- The 10-kind lint fires once, exactly at the 11th slot (`b1_eleven`, `g1_elements`, `g5_history`).
- Two unrelated dots at one point on axes: `zero: … coincide … reads as 'equal'` (`c7_coincide`); in space with a
  0.02 separation (`f1_space_crowd`); the checker leaves data coincidences to zeroCheck as its comment says.
- Pointer dots with `at=` on free and on axes: no zero lint, no layout message (`c2_at`, `c2b_at_axes`).
- A dot seated `on=` a box or a cell: the checker reports the dot's *name* on the host ("box 'a' overlaps name of
  'b'", "array 'a' overlaps name of 'p'"), which is right by the README ("a relation to a part covers that part
  only"); the layout should move the name (frames `c1_on_end.png`, `c1b_on_array_end.png`). Not an axiom-3 failure.
- Big dots in a free layout (`c3_bigdots_*`): the layout keeps them apart (they run off the screen instead): no
  overlap to disagree about.
- `tex` with `x= y=` on axes and on a plane is placed at the data point (`d2`, `d3` frames: E=mc² at (3,3)); under
  free, graph, numberline, timeline, sequence the `x= y=` are ignored and the formula sits in the band (drawn:
  3-4k px differ from the base). A `field` draws on axes and on a plane (`d2`, `d3`). An `angle` with `pts=` draws
  on axes (`d4`).
- A `label` with no `at=` in a graph goes to the quiet corner stack and is read back (`d1_label_graph`); coloured
  (verdict 4), but present.
- The cell scene (`g2_cell`, `g2b_cell_in`): seven touching shapes, layout 100, no zero lint, same-category
  mitochondria share green; `in=` accepted and inert on shapes.
- Names in a crowded 3D scene stay off dots and names (`f1_space_crowd`, frame `z_f1.png`); test 418's claim holds
  beyond its own scene.
- Real chats (`A3/out/sessions_scan.txt`, 104 files, 978 mark lines): categories are deliberate words (`input`,
  `op`, `output`, `point`, `force`, `leaf`, …); 0 duration tokens, 0 quoted multi-word bare tokens, 0 categories from
  a `key=`; 4 lines with two kinds (`mark w box param …`: a parse error the model got back).
- The ghost of a hidden thing (opacity 0.12, README.md:83), the title gliding to the corner (`render.cpp:842-845`),
  axis numbers and ticks (furniture the checker lets data sit on), the formula band, group boxes in the muted
  colour (`render.cpp:719-728`, no category hue) and link labels are all either the author's or documented
  furniture with no false meaning.
- `related()` is used only by `zeroCheck` (`layout.cpp:1363`); `intended`/`allow` only by the checker: the
  enforcement map's gap 4 is exact.
