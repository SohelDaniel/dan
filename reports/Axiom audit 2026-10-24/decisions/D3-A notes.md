# D3-A: colour stability across scenes and across turns

Engineer D3-A, in two hands: the first engineer designed and built most of it and was cut off by a usage limit while
reading the regression tails; a second engineer reviewed that diff, found and fixed the bugs listed under
"Implementation / changes relative to the predecessor", finished the regressions, the measurements and these notes.
Worktree: `<repo>/.claude/worktrees/agent-a6c78de2a8fc9f6e2`
(base commit 6038197, nothing committed). Everything referenced below is under
`<scratchpad>/dec-D3-A`
(called `D3A/`): scenes in `D3A/scenes/`, frames in `D3A/frames/`, logs `D3A/*.log`, measuring scripts in
`D3A/harness/`, an untouched copy of the base commit with its own binary in `D3A/old/` (verified equal to HEAD for every
tracked file under markup/, rm/, tests/, main.cpp, Makefile).

## Problem

Reproduced on the untouched engine (base 6038197):

1. **Across scenes.** `./tutor D3A/scenes/b4_scenes.dan --snapshot … --at 1,end --mute`: scene one draws `input` blue and
   `output` orange; scene two declares `output` first and draws it blue, `input` orange (`D3A/frames/final/sheet_b4.png`,
   top row). `--check`: decodable, 0 axiom warnings. Cause: `clearStage()` erased `colorSlot_`/`nextSlot_` at every
   `scene`, and slots are handed out in claim order. In the user's real lessons (`D3A/harness/scenescan.py` over copies of
   the 41 `chat/sessions/*/live.dan`): 7 category words recur in a later scene; the old engine draws 1 of them in another
   colour: lesson 20261024-101323, `op` is purple in scene 1 and orange in scenes 2-4, the orange that meant `step` (the
   hand-written backward pass) in scene 1 (`D3A/frames/final/sheet_op.png`, top row).
2. **Across the fold (the chat's state, `--check`'s re-parse).** `./tutor D3A/scenes/colour-slots.dan --readback` prints
   `mark a box z` first; the live engine claimed `z` fourth (purple), the re-parse claims it first (blue)
   (`D3A/frames/final/sheet_fold.png`, top row). `--check` still says decodable: it compares text only. The chat folds
   every turn through the readback (`chat/server.mjs:222`), renders `<last_frame>` from the folded text and restarts the
   stage on it, so the colours Claude sees and the colours after a restart can differ from the learner's window.
   Measured on the 13 real multi-turn chats (28 turns, `D3A/harness/foldscan.py`): no turn drifted on the old engine
   either; the drift needs a re-categorised kind (or, after this change, more than ten kinds) and has not yet happened in
   a real chat.
3. **A named colour beside a palette hue.** `./tutor D3A/scenes/a8_colorclash.dan --check`: `mark a bar input` takes blue;
   `mark z bar color=blue label="tax"` is the same blue; 0 axiom warnings. `colorOf` returns named colours without
   consulting the slots. Found by the new lint in 10 of the 41 real lessons (22 cases, all inspected, all genuine: e.g. a
   blue `k` vector inside a blue `grid space` lattice, orange attention arrows beside orange `mlp` boxes, a blue bucket
   around blue water), and in 7 of the project's own curated scenes (toolbox "A 3x3 matrix", `chat/examples/compare.dan`,
   `chat/examples/heart.dan`, `scripts/backprop|lanes|pointers|space-determinant.dan`).
4. **A learner's slider takes its colour while being drawn** (found in review; pre-existing). `drawControls` calls
   `colorOf` on a `param`, which `claimColor` skips, so the slot is claimed lazily at the first frame drawn: the window
   gives the slider blue and `input` orange, while the headless readback, `--check`, the fold and the `--at end` frame
   Claude receives give `input` blue (`D3A/scenes/probe/slider.dan`, `D3A/frames/probe/slider2_end.png` vs
   `slider_end.png`). 4 of the 41 real lessons have a learner slider.

## Designs considered

The slot table is the lesson's dictionary for the colour channel. The memory that keeps a kind's colour can live in
the engine (lost at the fold), in the word (a hash), or in the text (the readback).

- **A. Keep the table across `scene`, in the engine only.** Two lines. Fixes problem 1 in the window, but the fold then
  drifts more: a later scene that reuses earlier words in another order is coloured one way live and another from the
  folded state, and `--check`'s re-parse of a multi-scene readback differs. It also runs out of ten slots in a long
  lesson. Rejected alone.
- **B. Colour as a hash of the word.** The fold cannot drift except at collisions, which are resolved by order and so
  drift as today; with 10 hues two of four words collide half the time; it throws away the palette's order (the first
  two kinds would no longer get the strong blue/orange pair) and changes every existing frame. Rejected.
- **C. `color=` written into the readback.** Turns a category into a fixed colour in the state: a later re-mark of the
  category keeps the stale `color=`, and Claude learns from `<stage>` that colours are given by `color=`. Rejected.
- **D (the first engineer's). The table is the lesson's, and the readback carries it in one line,**
  `palette input output - [z]` (position = slot, `-` an empty slot, `[id]` an uncategorised thing). Eviction past ten:
  the kind *claimed* longest ago. Named colours: a new kind skips hues named on stage; a later clash is linted.
- **D' (shipped): D, with each entry naming its colour and the entries in eviction order:**
  `palette step=orange input=blue loss=green [k]=purple`. Two reasons found in review: (1) D's line loses the recency
  that eviction uses, so past ten kinds the fold evicts a different word than the window (reproduced:
  `D3A/scenes/probe/lru_*.dan`, window `x`=orange, fold `x`=blue); (2) Claude cannot read hues off D's line: the palette
  order (blue, orange, green, purple, yellow, teal, pink, red, brown, lime) is written nowhere in the prompt, and the
  README lists the colour names in another order, so the new lint's advice "use another colour name" could not be
  followed with knowledge. Recency had to change meaning to make the line a fixed point of `--check`: "claimed longest
  ago" depends on claim order inside the current scene, which a re-parse cannot see; "left the stage longest ago" (at a
  scene cut, or when its last thing is re-categorised) is a property every re-parse reproduces.
- **Named colours, considered and rejected: a prescan of the scene's `color=` names** (the auditor's "better" option),
  so kinds avoid hues named later. The live window gets a turn beat by beat: a kind is claimed before a later beat's
  `color=` arrives, while the fold sees the whole turn, so the prescan would recreate exactly the window/fold drift this
  decision removes (and `make append` would differ). Moving a kind's colour when a name arrives breaks continuity.
  Changing the named colour breaks the author (the voice may say "the orange arrows"). So: avoid by construction what
  can be (a name already on stage), report the rest.

## Chosen design

- A category word holds its palette slot from its first appearance to the end of the lesson; `scene` releases only the
  slots of uncategorised things (a scene's ids start fresh). A new kind takes the lowest free slot, skipping every hue
  something on stage uses by name (`color=`, `colors=[…]`, `\textcolor{<colour>}`). Past ten, the kind that left the
  stage longest ago gives its slot up (ties: the lower slot), never one whose hue is named on stage; if none can, the
  "colors repeat" lint fires as before.
- A learner's slider (`param … set=learner`) holds a colour from the moment it is marked (before: from the first frame
  drawn, in the window only).
- The readback's second line is `palette <kind>=<colour> …`: kinds that left the stage first (oldest first), then those
  on stage (by slot), then any overflow. A palette line only gives a kind that has no colour a colour that is free, so a
  copy of it is a no-op and it can never move or duplicate a colour.
- A colour given by name on the hue of an unrelated kind on stage is linted as `zero:` (an axiom warning in `--check`,
  fed back to Claude by the chat), unless the thing is of that kind, or points at / sits on / notes a thing of it
  (`at= on= in= over= of=`).
- Prompt: README's language section says a word keeps its colour across `scene`, documents the `palette` line and the
  named-colour rule; the 3D grid example (README, prompt.md's table, toolbox move) becomes
  `mark G grid m=[…] color=grey` (the lattice is ground; `grid space` made `space` an accidental colour category that
  clashed with the conventional blue `k`, a pattern 2 of the 10 linted real lessons copied); two prompt examples and four
  scripts are fixed so they no longer teach clashes (compare: attention links `color=yellow`, the README's own colour
  for a highlighted weight; heart: the blood, coloured by name, is declared before the kinds, with a comment saying
  why).

Axioms:
- **3 Zero.** Strengthened: one word is one colour for the whole lesson, so a word never takes the colour another word
  had in an earlier scene; a kind never lands on a hue named on stage; the remaining clash (name after kind) is reported.
- **6 Decodability.** The `palette` line makes the re-parse, the chat's fold, `<last_frame>` and a restarted stage colour
  as the window does, including which kind gives its slot up next; `--check`'s text comparison is a fixed point (tested).
- **1 Identity.** A category is one thing across the lesson; an uncategorised thing of Part 1 does not hold a slot in
  Part 2 (its id is gone).
- **2 Structure / 4 Continuity.** No colour jumps: a slot is never reassigned while its holder is on stage, a palette line
  never moves a colour, and a re-categorised mark still cross-fades.
- **Position from structure:** untouched.

Claude's markup: unchanged, except the 3D grid idiom (`grid space m=[…]` → `grid m=[…] color=grey`). What Claude reads
changes: `<stage>` gains one line. Before (scene two of a two-scene lesson on stage):
```
title "Two"
schema free
mark c bar output v=4
mark d bar input v=1
```
After:
```
title "Two"
palette input=blue output=orange
schema free
mark c bar output v=4
mark d bar input v=1
```

## Outside the field

A map legend. Cartography states symbol → meaning in a legend so the reader never needs the order in which the map was
drawn; a symbol stays fixed for the whole atlas, and a new meaning gets a new symbol, not a retired one's. This shaped
D': entries name their colour (`input=blue`, the language's own `color=blue` vocabulary) instead of meaning "the i-th
slot", and a word keeps its colour for the lesson. The recycling rule (the least recently shown meaning gives up its
symbol first) is a cache's LRU, made reproducible by recording departures instead of uses.

## Implementation

18 files, +379/-54 (`D3A/patch.diff`). Engine ~170 lines, tests ~155, docs/scenes ~50.

- `markup/engine.cpp` (colours section, `clearStage`, `reset`, `doMark` re-mark path, `doMorph`, `doLink`, `doGroup`,
  `execLine`, `readback`):
  - `colorKey(m)`: the key a mark holds ("cat:word", "id:id", or "" for ink/borrowed colour); learner params hold one.
  - `colorKeyOnStage`, `noteLeft(key)` (stamps `leftAt_` when nothing on stage carries a kind any more).
  - `namedHues()` / `namedHuesIn` / `hueOf`.
  - `freeSlot()`: the lowest free slot not named on stage, else evict the off-stage, non-named kind with the oldest
    `(leftAt, slot)`, else warn and overflow.
  - `claimColor`.
  - `doPalette`.
  - `lintNamedColors` (`zero:` lint, with the relation exemption and advice by kind).
  - `clearStage`: stamps the kinds on stage as leaving and erases only `id:` slots.
  - Re-mark: claims the new key and stamps the old one when a mark's key changes.
  - `readback`: the `palette` line.
- `markup/engine.hpp`: declarations; `slotUse_`/`useSeq_` (predecessor) → `leftAt_`/`leaveSeq_`; `learnerSets` const.
- `markup/controls.cpp`: `learnerSets` const (1 line).
- `markup/script.cpp|hpp`: `Verb::PALETTE`, `readPalette` (entries `kind=colour`, stored in `ln.props`; the parser
  checks the shape, the engine the colour, as for `\textcolor` names).
- `rm/raymanim.cpp|hpp` (predecessor): the named-colour table shared by `colorByName` and a new `colorName`.
- Tests: `tests/test_engine.cpp` (10 cases), `tests/test_script.cpp` (1 case).
- Docs/scenes: `README.md` (language section: colour names rule, `scene`/`palette` setup lines, grid example; the
  dictionary row; axioms 3 and 6; sample readback), `chat/prompt.md` (one table cell), `chat/toolbox.md` (the 3x3 move),
  `chat/examples/compare.dan`, `chat/examples/heart.dan`, `scripts/backprop|lanes|pointers|space-determinant.dan`.

**Changes relative to the predecessor's uncommitted diff (8 files, +268/-30):**
1. `make check` was red: the new lint failed the toolbox move "A 3x3 matrix" and 6 curated scenes (the predecessor saw
   it at 17:56 and had started on an engine change, see 7). Fixed by correcting the scenes (real clashes, checked by eye) and
   the 3D grid idiom in the prompt.
2. README (part of the prompt) still said `scene` resets colours and did not mention `palette`; the planned prompt
   sentence was never written. Written.
3. Eviction past ten kinds did not survive the fold (line without recency): new line format + "left the stage" recency.
4. Learner sliders were coloured at draw time (window ≠ readback): they now claim at mark time.
5. `freeSlot`'s second eviction pass could silently give a new kind a hue named on stage: removed (the lint fires
   instead).
6. Lint advice "or give 'X' that category" was wrong for links, groups and formulas: now only for marks other than
   formulas; and a thing related by `at= on= in= over= of=` to the kind's thing is no clash.
7. `col::lattice` was declared in `rm/raymanim.hpp` and never defined or used (an unfinished step): removed.
8. My own bug, caught by the real-lesson scan before shipping: a param handed to the learner by a *re-mark* was not
   claimed, so 1 real lesson (20261022-211954) became `decodable: NO`. Fixed (a re-mark claims whenever the mark's key
   changes), with a test.

## Tests

New/changed (all in `make test`):
- `test_engine`: "a category keeps its colour across scenes, whatever order the next scene declares them in" ·
  "a scene's ids start fresh: an uncategorised thing's own slot is free again after the cut" · "the readback re-parses
  to the same colours: a category changed by re-mark, and a reordered scene" (incl. the `--check` fixed point) · "the
  chat's fold keeps the palette: state(turn 1) + turn 2 colours like turn 1 + turn 2" · "a long lesson does not run out
  of colours: a kind that left the stage gives its slot up, ten on stage never do" · "a palette line never moves a colour
  already given" · "past ten kinds the one that left the stage longest ago gives its colour up, in the window and after
  the fold" · "a learner's slider holds its colour from the moment it is marked, as the window draws it" (incl. a
  param handed over by re-mark: fixed point) · "ten hues in use on stage: an eleventh kind is reported, never given a
  colour named by hand" · "a named colour is never shared with an unrelated kind: the palette skips it, or the clash is
  linted" (incl. links, formulas, and a pointer `at=` its target).
- `test_script`: "a palette line (the readback's) gives each kind its colour: words, [ids] and quoted words".

On the old engine (the final tests compiled against `D3A/oldtests`, a copy of HEAD; the same result came from restoring
HEAD in the worktree with a saved patch, `D3A/oldengine-newtests.log`; `git stash` was avoided because `refs/stash` is
shared with the main repo and other worktrees), `D3A/oldengine-finaltests.log`:
```
tests/test_script.cpp:104:25: error: no member named 'PALETTE' in 'mk::Verb'
TEST CASE:  a category keeps its colour across scenes, whatever order the next scene declares them in
tests/test_engine.cpp:490: ERROR: CHECK( sameColor(e.colorFor("d"), rm::categorical(0)) ) is NOT correct!
...
TEST CASE:  past ten kinds the one that left the stage longest ago gives its colour up, in the window and after the fold
tests/test_engine.cpp:564: ERROR: CHECK( sameColor(whole.colorFor("x"), rm::categorical(1)) ) is NOT correct!
TEST CASE:  a learner's slider holds its colour from the moment it is marked, as the window draws it
tests/test_engine.cpp:576: ERROR: CHECK( sameColor(e.colorFor("a"), rm::categorical(1)) ) is NOT correct!
...
[doctest] test cases:  41 |  32 passed |  9 failed | 0 skipped
[doctest] assertions: 251 | 236 passed | 15 failed |
```
The one palette case that passes on the old engine is the long-lesson guard (it protects the new design: the old engine
resets per scene). The re-mark slider and `at=` exemption assertions guard new code; each was watched failing on the
intermediate engine before its fix (`D3A/pred-newtests.log`: on the predecessor's engine the new parser case and 4
engine cases, 6 assertions, failed: fold eviction, slider, eleventh kind, link advice).
After: `[doctest] test cases: 41 | 41 passed`, `assertions: 253 | 253 passed`; `test_script` 9/9, 55 assertions.

## Regressions

Logs: `D3A/final-{test,check,golden,append,toolbox}.log` (final binary). Baselines: `D3A/baseline-*.log` (predecessor,
old engine, 17:45) and `D3A/baseline2-golden.log` (old engine `D3A/old/tutor`, rerun today).

- `make test`: place 21/21 (35525), viz 14/14 (18066), script 9/9 (55), engine 41/41 (253). `exit 0`.
- `make check`: 124 ok, 0 FAIL (93 scripts/examples/idioms + 31 toolbox moves), `exit 0`; baseline 124 ok, 0 FAIL.
- `make golden`: `mean layout score 99.6 over 52 scripts` / `97.4 over 52 scripts` (old placer), no WORSE, `exit 0`.
  Against the old engine run today, the only difference is `chat/examples/heart.dan` (edited here: `blood` is now
  declared first): mean 98.7 → 98.6, label flips 9 → 10, and the one overlap at 14 s names the pair in the other order
  (`name of 'blood' overlaps name of 'la'`), in both placer runs. Against the predecessor's 17:45 baseline,
  `chat/exemplars/descent.dan` (fill 0.20 → 0.23; old placer 87.4 → 85.9) and `photosynthesis.dan` (fill 0.83 → 0.84) also
  differ, but the old binary gives exactly the new numbers today, twice (environment drift, not this change).
- `make append`: identical to baseline: `append equivalence: 152 identical, 4 different` (space-downhill ×3,
  space-saddle ×1: the known pre-existing DIFFERS).
- `node tests/toolbox.mjs`: 31/31 ok, `exit 0`.

Real lessons (copies of the user's 41 `live.dan` and turns in `D3A/sessions/`, read-only):
- `--check` decodable: 41/41 new, 41/41 old (`D3A/harness/lintscan.py`, `D3A/decodable.txt`).
- Fold (`foldscan.py`): 13 multi-turn chats, 28 turns: colours of `state + turn` = colours of the whole lesson in 28/28
  turns (old: 28/28); every folded turn decodable (old: same).
- Scenes (`scenescan.py`): 7 recurring words, 0 recoloured (old: 1).
- Named-colour lint: 10/41 lessons, 22 lints (old: none, it was silent); no "colors repeat"; the longest palette line
  has 6 kinds, so eviction has not yet been needed in a real lesson.

## Frames

All in `D3A/frames/final/` (old engine = `D3A/old/tutor`):
- `sheet_b4.png`: scene two keeps `input` blue / `output` orange (old swaps them).
- `sheet_op.png`: the real lesson 20261024-101323: `op` stays purple in scene 2 (old: orange, the colour of `step`);
  the new kind `leaf` takes yellow instead of `input`'s blue.
- `sheet_fold.png`: colour-slots whole vs its re-parsed readback: old A purple/blue, new purple/purple; the eviction case
  whole vs folded: identical.
- `sheet_examples.png`: compare (attention links yellow, no longer the RNN boxes' orange); heart (organs orange,
  chambers green, the blue blood no longer the organs' blue).
- `sheet_matrix_backprop.png`, `matrix3_new_end.png`: the 3D lattice grey, `k` (blue) and `i` (green) stand clear (old:
  `k` merges into the blue lattice's axis line); backprop's gradient arrows yellow, apart from the orange `process` box.
- `sheet_lanes_pointers.png`, `pointers_new_end.png`: lanes (fixed W teal, attention yellow, apart from every kind);
  pointers (score link and α_j yellow; the `key` labels took green, purple, teal: the named yellow was skipped).
- `D3A/frames/probe/slider_sheet.png`: the slider scene, drawn early and drawn only at the end, now agrees with the readback.
Nothing looked worse. The heart's extra label flip was not visible in the frames looked at (3.6 s, end).

## Limits and risks

- **The lint will be heard.** It fires in about a quarter of real lessons (10/41), so the chat will feed "use another
  colour name" back to Claude more often. Every case inspected was a real clash, but the prompt changes that should
  lower the rate were not measured with `tools/eval` (no pre-registered A/B). Prompt edits: 2 README sentences plus the
  `palette` and `scene` lines, the grid idiom in 3 places, 2 examples.
- **A name given after the kind still clashes on stage** (only reported); avoiding it needs foresight the live window
  does not have.
- **Overflow** (more than ten kinds on stage at once, already an error the lint reports) is not guaranteed to survive a
  fold exactly: an overflow entry whose base colour is held is left to its mark to claim.
- **Draw-time claims that remain** (pre-existing, not seen in real lessons): `\textcolor{<param>}` with a non-learner
  param, a dangling `of=`, an unknown colour name. These still take a slot only in the window.
- **Colours of existing lessons change** where they should: later scenes keep earlier words' colours; a kind declared
  after a named colour skips it; scenes with a learner slider shift by one hue headless (now matching the window).
  Replaying an old chat shows the new colours.
- **Migration:** existing `state.dan` files have no `palette` line; the first fold after the upgrade derives the
  palette from the state's marks as today, then carries it. User data in `chat/` was not touched.
- **New language surface:** `palette` is parseable; if Claude writes one, it pre-assigns kinds' colours (only free ones,
  never moving one). The README calls it the engine's line.
- Left alone: the 2D `grid space` example (README:154: no `k`, no clash); `tools/write.html`'s highlighter does not
  know `palette` (cosmetic); `docs/HANDOFF.md` / `docs/history.md` not updated because nothing was committed. The open
  item "the flicker" should be updated with the commit.

## Verdict

**Ship with caveats.** It solves the decision:
- A category keeps its colour across scenes: tested, and the real `op` case is fixed.
- Across turns, the fold, `--check`, `<last_frame>` and a restart colour like the window, including eviction and
  sliders: tested, 28/28 real turns.
- A named colour can no longer silently share an unrelated kind's hue: avoided by construction when named first,
  reported otherwise.

All regressions are green, and every difference is explained. The caveats:
- The new lint adds feedback traffic in about a quarter of lessons, and the small prompt edits meant to reduce it are
  not eval-measured.
- Overflow past ten kinds on stage is only approximately preserved by a fold.

Run a `prompts-broad` A/B on the prompt part before or soon after committing, and update the HANDOFF with the commit.
