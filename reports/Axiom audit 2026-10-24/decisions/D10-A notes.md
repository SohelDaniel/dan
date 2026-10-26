# D10-A: `morph f.tex += "row"`, a derivation step that does not re-emit the formula

Engineer D10-A, 2026-10-24, two sessions. The first wrote the syntax, the composition, the docs and a glyph-anchored
hold, and was cut off before its notes and checks were finished (its notes are kept in `b/notes-predecessor.md`).
The second (this file) reviewed that work, reproduced and fixed the bugs listed under Implementation, narrowed the
hold to derivations, re-ran every regression and measured. Worktree `.claude/worktrees/agent-a17670d2dc9974cc8` (branch
`worktree-agent-a17670d2dc9974cc8`, base 6038197), uncommitted. All paths below are under this folder (dec-D10-A/).
This session's logs are in `b/` (the top-level `*.log` files are the first session's); `patch.diff` is the final diff.

## Problem

Reproduced on the old engine (pristine export of HEAD in `old-src/`):
- **No append form.** `morph f.tex += "x^2 + 6x &= -2"` is not a parse error: `+=` is read as the `from` value, so each
  step *replaces* the formula with the bare row (a TeX error, "Misplaced &"), and `--check` still says "decodable:
  yes". The derivation never grows (`scenes/deriv-append.dan` on the old engine: 3 identity lints, 4 TeX errors;
  readback `tex="\textcolor{sq}{(x+3)^2} &= 7"`).
- **Every step re-emits the whole formula.** In the toolbox move the scene is 769 characters, 440 tokens; the same
  lesson with rows is 559 characters, 277 tokens (counted below). In the user's real chats (63 turn files, verified
  by re-running `steps.py` on a fresh copy): 87 formula-to-formula steps, of which 69 change a one-line formula,
  8 are pure row appends, 4 change the last row and add one (2 of those only add the `&` the previous row lacked),
  3 change only the last row, 2 change an earlier row, 1 changes an earlier row and adds one. The eval corpus the
  language auditor measured is derivation-heavy (351 aligned steps, 293 pure appends, 58 altering an earlier row);
  it is not in the worktree, so those numbers are quoted, not re-measured.
- **Earlier rows move when a row arrives** (audit-4 r14). The band centres a block, so a derivation that grows by
  Δ moves every earlier row by Δ/2: on a free stage the first row climbs at every step (old engine, live timing:
  top 284 → 257 → 230 → 203 → 178 px over four rows), and a row wider on the left moves the `=` column (84.6 px
  for `2x &= 8` → `x^2 + 10x + 25 &= 33`, the same on a free stage and in the band over a plot). Frames
  (`frames3/glide-sheet.png`, top row, measured on the PNGs): the first row's top climbs 38 px during one step
  (169 → 131 px) and 95 px by the end of the lesson.

## Designs considered

**Syntax** (unchanged from the first session).
1. `morph f.tex += "row" 1.2s` (chosen): one new token on the verb that already means change; durations, `ease=`,
   `@cue` and `with` apply as to any morph; it cannot be mistaken for `->`; the auditor proposed the same form.
2. A verb (`step f "row"`): a seventh verb for one property of one kind; everything downstream (rests, pacing,
   `since:`, readback, binding) already treats a morph of `tex` as a step. Rejected.
3. A property (`mark f row="…"`, `morph f.tex add="…"`): a `row=` that is not stored breaks "a re-mark updates the
   props it names" and never reads back; `add=` makes a key=value carry the target. Rejected.
4. Content sniffing (`-> "\\ x &= 4"` means a new row): no new token, but a meaning hidden in two characters and no
   way to lint a slip. Rejected.

**Replacing the last row.** 3 of 18 aligned steps in the real chats change only the last row; a virtual
`morph f.last -> "row"` would be a property that is never a property (no readback vocabulary for it). Not a form:
the whole-formula `->` covers it and the docs say so.

**Where the row is composed.** At execution (`doMorph`), from the formula the thing has then (the parser never sees
it: it may come from an earlier turn of the live chat), as `from` is checked there; the TeX warm-up (`analyze`)
follows each formula through the script to typeset the composed formula ahead.

**Keeping earlier rows still.** Three designs, the last two built and measured:
1. Keep a growing block's top-left (audit-4 F10). A wider left-hand side would still move the `=` column.
2. The first session's: when *any* band formula's typesetting changes, offset it so the glyphs it kept stay where
   they were (pair the k-th glyph of a character with the k-th, take their mean drift: 3Blue1Brown's
   TransformMatchingTex with the anchor on the matched part), vertically per formula. Measured on the 35 formulas
   that change in the user's real chats (probe replays, old vs this design): 15 ended 42 to 274 px further off
   centre than today (one-line equations that grow to the right, "1/2 + 1/3 = 3/6 + 2/6 = 5/6", keep their left edge
   and walk right), 4 left the stage for a moment; in a lone stack a block above a derivation re-centred upward
   while the derivation held, opening a gap of 14 → 41 → 67 px; and a stale per-frame flag made a formula that
   left the band jump 131 px on its next change. Rejected as built.
3. **Chosen:** the same kept-glyph anchor, but only for a *derivation* (a formula that is one block of rows,
   `\begin{E} … \end{E}`) and only sideways; vertically the stack keeps its top. One-line formulas stay centred as
   today. On the same 35 real formulas, 29 move as today (equal to the run-to-run noise, see Frames) and the other
   6 are the 5 real derivations, held on purpose, plus one noisy `\val{}` formula.

## Chosen design

**Syntax.** `morph <id>.tex += "<row>" [1.2s] [ease=…]`, with `@cue` and `with` as any morph. A `+=` glued to its
neighbours (`f.tex+= "…"`, `+="…"`) is still `+=`.

**Composition** (`texWithRow`, run at execution on the formula the thing has then):
- a formula that is one block of rows (`\begin{aligned} … \end{aligned}`, also `array`, a matrix alone, …) gets the
  row as its last row;
- any other formula becomes `\begin{aligned} <it, its first = outside braces and environments preceded by &> \\
  row \end{aligned}` (unless it has an `&` of its own); `A = \begin{pmatrix}…\end{pmatrix}` is such a plain formula,
  so its matrix is not reopened;
- an empty formula starts a block with the row (and a lint says it had none);
- a stray `\\` before or after the row is dropped (the decision's own example wrote `+= "\\ x &= 4"`, which would
  have made an empty row); an empty row adds nothing;
- a "row" that is the formula's own kind of block again (`\begin{aligned} … \end{aligned}` re-emitted, the likely
  slip of a model trained on the old form) is taken as the whole formula, with a lint. A row may start with a
  matrix (`\begin{pmatrix}…\end{pmatrix} &= v`).
From there it is an ordinary change of the whole formula: the same `Active` and `strBlend` glide, the same rest
after a step, the same `since:` log and timeline events (`tex f` at the same times as the `->` form), the same TeX
warm-up and error reporting (at the `+=` line).

**Readback (axiom 6).** `mark f tex tex="<the full current formula>"`, the model, not the source form (as for
`hide`, re-marks and link kinds). `--check` re-parses it to the same scene. The `+=` scene and the whole-formula
scene read back byte-identical (unit test, streamed too), and so do the 5 real lessons rewritten with `+=`.

**Lints.** Parse (the line is dropped and said): `+=` on anything but `.tex` ("+= adds a row to a formula …; p.x is
not one"), `+=` with a `from`. Play: "append: line N: 'b' has no formula to add a row to; the row starts one";
"append: line N: += takes the new row only, not the whole formula; this one replaced it". Existing checks apply to
the row (`\textcolor{name}`, TeX errors on the composed formula, `morph: no thing called …`).

**The glyph morph and the layout.**
- Pairing unchanged (the k-th glyph of a character with the k-th), so the new row's glyphs are the new ones: lit
  yellow, settling down, exactly as a `->` to the longer formula (`frames3/glide-sheet.png`).
- A derivation in the band keeps the rows it had where they were (`holdFormula`): when its typesetting changes, its
  centre is moved by what the new typesetting moved the kept glyphs (`tex::drift`), applied to the drawn position at
  once (nothing drawn moves: `drawTex` starts the old formula where it was) and kept as a sideways offset from its
  slot, so a wider row extends the block around the `=` column. The offset is clamped to the stage (eased back when
  a row would leave it), lasts while the band holds the formula each frame, and goes when a change keeps no glyph.
- A lone stack (blocks only, on a free or graph stage) is centred when a block arrives and keeps its top while its
  blocks only grow (a derivation's new row, a note under it): it grows down, and slides up (eased) only as far as
  the stage needs (`loneTop_`, `loneBlocks_`). The band over a plot already kept its top.
- Everything else is as today: one-line formulas are centred, formulas at data points or flowed into rows are not
  held.

**Axioms.**
- 1 Identity: the same thing updated in place; `from` still checked against the real formula; no new delete path.
- 2 Structure: no scale or baseline changes; the band's need is computed as before (the hold is sideways only).
- 3 Zero: no new ink, colour or relation (`tex+=` can no longer become a junk colour category).
- 4 Continuity: the append is the same eased glide as the whole-formula morph (readback-identical scenes play the
  same `Active`; the unit test checks the settled positions agree within 0.5 px); the hold's jump moves nothing drawn;
  the slide when the stage is full is eased; a formula leaving the band no longer jumps.
- 5 Bounded focus: untouched.
- 6 Decodability: readback prints the full formula; `--check` round-trips; `+=` is a source form, like `hide`.
- Position from structure: the hold is the structure's own stability rule (the rows of a derivation and the order
  of a stack), not a placement word; Claude says nothing about pixels.

**Claude's markup, before / after** (the toolbox move):

```
mark f tex tex="\begin{aligned} x^2 + 6x + 2 &= 0 \end{aligned}"
Say: "Move the two to the other |side|side."
@side morph f.tex -> "\begin{aligned} x^2 + 6x + 2 &= 0 \\ x^2 + 6x &= -2 \end{aligned}" 1.2s              # before
@side morph f.tex += "x^2 + 6x &= -2" 1.2s                                                             # after
Say: "Take half of the six and square it, and add that nine to |both|both sides."
@both morph f.tex += "x^2 + 6x + 9 &= 7" 1.2s
Say: "Now the left side is a perfect |sq|square."
@sq morph f.tex += "\textcolor{sq}{(x+3)^2} &= 7" 1.2s
```

**Prompt and toolbox** (first session, kept): README "The language" (one grammar line under `morph`, the derivation
bullet rewritten around `+=`, "to change a row already there, morph the whole formula with `->`"), `chat/prompt.md`
(the derivation rule), `chat/toolbox.md` (the derivation move and one phrase in "A formula as the summary"),
`tools/write.html` (the cheat sheet). Token counts, with Claude's tokenizer: `claude -p --output-format json` on
`claude-sonnet-5-5` (the chat's default model), each text sent as the prompt, its count = reported input tokens minus
a 618-token baseline; deterministic (no noise); `claude-opus-5-5` gave the same counts on the two scene comparisons:

| text | before | after | change |
|---|---|---|---|
| the tutor's whole system prompt (as `chat/system-prompt.mjs` builds it) | 26,227 | 26,264 | +37 tokens per chat (+0.14%) |
| the toolbox move "A derivation that grows" | 620 | 473 | −147 (−24%) |
| the move's scene alone | 440 | 277 | −163 (−37%) |
| the 5 real lessons with pure appends, whole turn files, rewritten with `+=` (7 steps) | 9,274 | 8,532 | −742 (−8.0%) |

TeX is dense (about 2 characters per token), so chars/4 understated the saving (it gave −12% for the move). The
prompt pays 37 tokens once per chat; the toolbox's three-step derivation saves 163, so one derivation repays it.

## Outside the field

Not used.

## Implementation

| file | what | lines |
|---|---|---|
| `markup/script.hpp/.cpp` | `Line::append`; `readMorph`: `+=` and its two refusals, `ungluedAppend` (a glued `+=`); `topLevel`, `rowBlock`, `wholeFormula`, `texWithRow` | +82 −1 |
| `markup/engine.cpp` | `doMorph`: the row composed into `to`, the two lints; `analyze`: the TeX warm-up follows each formula (`texOf`); `loneBlocks_` reset with the band | +22 −9 |
| `markup/engine.hpp` | `Mark::texSeen/texShift/texHeld`, `loneTop_/loneBlocks_`, `holdFormula` | +6 |
| `markup/layout.cpp` | `layout`: the per-frame reset of the hold; `layoutBand`: the lone stack's held top, the `holdFormula` call; `holdFormula` | +37 −2 |
| `markup/render.cpp` | `drawTex`: the old formula starts where its kept glyphs are, when held | +5 |
| `markup/tex.hpp/.cpp` | `tex::drift(from, to, &pairs)` | +22 |
| docs | README, `chat/prompt.md`, `chat/toolbox.md`, `tools/write.html` | +13 −10 |
| `tests/test_script.cpp`, `tests/test_engine.cpp` | 5 + 6 cases | +225 |

174 lines of engine, 225 of tests, 13 of docs (`git diff --stat`: 14 files, +412 −22).

**Changes to the first session's work** (it was uncommitted and its tests had passed, but):
1. **Bug: one-line formulas walked off centre.** The hold applied to every band formula; on real chats 15 of 35
   changing formulas ended 42 to 274 px off centre and 4 briefly left the stage. Now only a derivation
   (`rowBlock`) is held; one-line formulas move exactly as today.
2. **Bug: a gap opened above a growing derivation.** The hold was vertical per formula while a lone stack still
   re-centred, so a formula above a derivation moved up as the derivation held (gap 14 → 41 → 67 px). Replaced by
   one rule for the stack (`loneTop_`): it keeps its top while its blocks grow and re-centres when one arrives. This
   also replaced the first session's `notes` centring (a note under a derivation now hangs below by the same rule)
   and its re-derived band height; `layoutBand`'s stacking is back to the original lines.
3. **Bug: a jump when a formula leaves the band.** `texHeld` was reset only for band blocks, so a formula moved to a
   data point kept the band's drawing rule and jumped 131 px when it next changed. Now reset with the other
   per-frame flags in `layout()`, and a hold restarts after any frame it was not held.
4. **Composition bugs:** a stray `\\` in the row made an empty row; `+=` on an empty formula left a red TeX error
   ("Misplaced &"); any row starting with `\begin{` (a matrix) was taken as the whole formula and wiped the
   derivation; a formula ending in a matrix got the new row inside the matrix; `rowBlock` was fooled by two
   matrices side by side. All fixed, each with a test case.
5. **Bug: a glued `+=`.** `morph f.tex+= "…"` became a property `tex+=` (an unknown string property: a colour
   category), `morph f.tex +="…"` replaced the formula with garbage. Now read as `+=`.
6. **Tests:** the layout tests played only `+=`, which the old engine cannot parse, so they failed there for the
   wrong reason and the note test passed there; they now play the whole-formula form too (and compare the two), the
   note test uses `->`, and three cases were added (the lone stack, the band over a plot, one-line formulas stay
   centred).

## Tests

New cases (all pass on the final engine: `make test`, 13 script cases / 89 assertions, 36 engine cases / 260):
- `tests/test_script.cpp`: "+= adds a row: a morph of the formula whose target is the new row only"; "+= is for
  formulas: on another property, or with a from, the line is refused and says so"; "+= written without spaces is
  still +=, not a property named tex+="; "texWithRow: …" (13 compositions); "rowBlock: a formula that is one block of
  rows, a derivation".
- `tests/test_engine.cpp`: "a derivation written with += reads back as the same scene as the whole-formula form, with
  no lint" (also streamed in two chunks); "+= makes a plain formula an aligned block …: it says so"; "a derivation's
  rows stay where they are as rows are added …" (five rows on a free stage, both forms, the slide when full; a wider
  left side; the same over a plot); "a lone stack keeps its top while its blocks grow: a block above a derivation
  stays, a note hangs below"; "only a derivation keeps its place: a one-line formula that grows stays centred".

**On the old engine** (the new tests compiled against `old-src/` with a test-local copy of `texWithRow`, since the
old engine has none, and without the `Line::append` assertions; `b/old-test-script.log`, `b/old-test-engine.log`):
3 of 3 parse cases fail on behaviour (`CHECK( !ln.hasFrom )`; `morph p.x += 3` accepted; `prop == "tex+="`, target
`+="2x &= 8"`); 4 of 5 engine cases fail:

```
CHECK( rows.readback(false) == whole.readback(false) )   values: mark f tex tex="\textcolor{sq}{(x+3)^2} &= 7" ...
logged: step 1: top 284.456 -> 257.21 ... step 4: top 284.456 -> 178.022       (rows re-centre)
logged: step 1: = at 711.201 -> 696.419                                         (the = column moves)
CHECK( std::abs(w[1].eqX - w[0].eqX) < 1.5f )   values: CHECK( 84.6146 < 1.5 )  (free stage; same over the plot)
logged: step 1: the block above 259.104 -> 231.858                              (the lone stack re-centres)
test cases: 5 | 1 passed | 4 failed     assertions: 82 | 43 passed | 39 failed
```

The fifth ("a one-line formula that grows stays centred") passes on the old engine by design: it guards today's
behaviour, and it fails on the first session's engine (`b/pred-test-engine.log`: off centre by 84 and 127 px), as
does the lone-stack case (the block above moves 27 px). The texWithRow/rowBlock cases test new functions and have no
old-engine counterpart.

## Regressions

All on the final code (`b/final-*.log`):
- `make test`: test_place 21/21 (35,525 assertions), test_viz 14/14 (18,066), test_script 13/13 (89), test_engine
  36/36 (260), `node tests/compute.mjs` all ok; exit 0. (Once, under load average 5, "a runaway block is stopped"
  failed with "took longer than 5 s" instead of "stopped": wall-clock timing in the python sandbox, files untouched
  by this change; it passed on every later run.)
- `make check`: 124 lines, all `ok` (scripts, examples, idioms, and `node tests/toolbox.mjs`'s 29 moves, including
  "A derivation that grows, with a beat after the step" and "A formula as the summary of what they saw").
- `make golden`: `same` for all 52 scripts under both placers (104 rows); mean layout 99.6 and 97.4, unchanged. Only
  5 of the 52 scripts change a formula at all, so golden says little about the hold; the real-chat replays below do.
- `make append`: 152 identical, 4 different: space-downhill (chunks of 2, 3, 5) and space-saddle (chunks of 2). The
  pristine HEAD build gives exactly the same 4 (`b/old-append.log`): pre-existing (the stale lint comment the brief
  names), not this change.
- Real chats: all 63 turn files of `chat/sessions` (copied read-only to `tokens/orig/`) read back byte-identical on
  the old and new engines; the 5 lessons rewritten with `+=` (`tokens/rows/`) read back byte-identical to their
  originals, with no `append:`, `tex:` or `parse:` lint.

## Frames

Snapshots with a warm TeX cache, both engines run from the same directory (`--snapshot` steps time faster than the
typesetter, so a cold cache shows formulas late; the first frames I took had that artefact and were redone).
Positions below are measured on the 1280x720 PNGs (brightness threshold, then the ink's bounding box).
- `frames3/glide-sheet.png` (the toolbox move, before / during / after its second step, old engine with `->` above,
  `+=` on this engine below). Top of the ink at 8.1 / 8.4 / 8.8 / 9.2 / 9.6 s / end: old engine 169 / 168 / 148 /
  132 / 131 / 74 px (38 px during the step, 95 px by the end); here 207 px at every moment of the step, while the
  new row fades in lit yellow beneath the others and settles, and 157 px at the end (a 50 px eased slide once four
  rows and the note fill the lone stage, which is zoomed to 1.4x).
- `frames3/stack-sheet.png` (a formula above a growing derivation; old, first session, this engine): old re-centres
  everything; the first session's opens a widening blank band; here the gap stays 14.0 px (probe: 14 / 14 / 14,
  against 14 / 41 / 67) and the stack slides up as one when full.
- `frames3/real3-sheet.png` (a real lesson, 20261024-101323 turn 3, a two-row block whose second row is rewritten
  longer). The left edge of the first row "forward : (w, x, b) ↦ z" at 8 / 11.5 / 57.5 / 79 s spans 187 px on the
  old engine (336, 370, 183, 209) and 18 px here (336, 336, 319, 337; the dip is the step that rewrites row 2, where
  the alignment column narrows and the least-squares anchor settles between the rows). The block extends right and
  ends off centre over the graph, on stage.
- `frames3/leave-sheet.png`, `frames3/leaverows-sheet.png` (a formula, then a derivation, moved from the band to a data
  point, then changed): on the first session's engine the left edge jumps 368 → 237 px (131 px) between 3.9 and 4.1 s;
  on this one and the old one it glides (368, 368, 366, 363 px at 3.9, 4.1, 4.15, 4.25 s).
- `frames/old-full_*.png`, `frames/new-append_*.png` (first session): the same story for the full lesson.

Real chats, replayed headless (`probe/probe.cpp`: live timing, every body box every 0.1 s; `probe/compare.py`;
`b/real-compare-*.txt`, `b/noise-*.txt`). Noise: the old engine against itself differs on 7 of 35 formulas when the
TeX cache state differs, because `\val{}` formulas are typeset asynchronously at their live values; with the same
cache order, old vs this engine differ on 6: the 5 real derivations (held on purpose: the path their top-left travels
drops 308 → 113, 189 → 74, 269 → 247, 173 → 126, 433 → 410 px; 4 of them end 42 to 97 px off centre, the fifth sits
where it did) and one `\val{}` formula that also differs between two runs of the same engine.

## Limits and risks

- **Derivations end off centre.** Holding the rows means a block whose later rows are wider extends to that side;
  4 of the 5 real derivations end 42 to 97 px off centre (on stage). That is the decision's trade (old rows stay put), but
  the user judges by the frame: if a lopsided derivation over a plot looks wrong to them, a gentle re-centre between
  sentences (not during a step) is the knob.
- **A row wider than the room left** is drawn past the stage edge for the ~0.3 s the block takes to slide back
  (the drawn position jumps with the kept glyphs, then eases to the clamped target). Rare (rows narrower than the
  stage); seen once in the first session's version on a one-line formula, which is no longer held.
- **A lone stack now keeps its top while any of its blocks grows**, not only derivations: a one-line formula that
  gets taller (a fraction) in a lone stack grows down instead of about its centre (a few px). Arrivals re-centre as
  before.
- **The `\val{}` jiggle (audit-4 F4a) is not addressed**: one-line formulas are re-centred on every value tick as
  before. Holding them too would fix it but moves 15 of 35 real formulas off centre; a separate decision.
- **Changing a row already there** still means re-emitting the whole formula with `->`: 8 of the 18 aligned steps in
  the real chats did (2 of the 4 that changed the last row and added one only added the `&` a plain formula lacked,
  which `+=` now does itself); `+=` does not prevent copy slips there. The glyph pairing is by character order, so a change that
  reorders symbols holds the least-squares position of the kept glyphs, not each glyph.
- **Content test for "derivation":** a formula counts as one if it is one `\begin{E} … \end{E}` block; a derivation
  written as `\displaystyle\begin{aligned}…` or two aligned blocks side by side is centred as before.
- **Not measured:** whether Claude writes `+=` reliably once the prompt teaches it (no eval run: it would need the
  eval corpus and a pre-registered bar, `reports/Idioms A-B preregistration.md`); token savings on the auditor's eval
  corpus (estimated 15% of scene characters there, not re-measured).
- **Migration:** none needed. No existing script or real chat has `+=` on a `morph` line (in the 63 real turn files it
  occurs only inside quoted code, a label and table cells), and all 63 read back byte-identical on the old and new
  engines; whole-formula derivations keep working and are held the same way; the readback never emits `+=`.

## Verdict

**Ship with caveats.** The syntax, composition, readback equality, lints and docs are right and tested; the old
engine fails the new tests on behaviour; every regression is clean (golden 104/104 same, append's 4 differences
pre-existing, toolbox 29/29), and on the user's real chats only derivations move differently from today, in the
intended way. The caveats: derivations whose later rows are wider end off centre (42 to 97 px on 4 of the 5 real ones),
which the user should see on the stage before it is final, and nobody has yet measured whether Claude writes `+=`
reliably. The first session's version should not ship as it was: it moved one-line formulas off centre in 15 of 35
real cases, opened gaps above derivations and could jump a formula that left the band.
