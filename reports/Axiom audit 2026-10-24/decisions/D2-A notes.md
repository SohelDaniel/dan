# D2, engineer A: name the cut

Worktree: `/Users/soheldanial/code/server/testing/visulaizations/tutor/.claude/worktrees/agent-adf4caed0898df485`
(branch `worktree-agent-adf4caed0898df485`, base commit 6038197). Nothing committed. Scenes in `scenes/`, frames
in `frames/`, regression logs `make-test.log`, `make-check.log`, `golden-new.log`, `golden-old.log`,
`make-append.log`, the failing test run `test-old-engine.log`, the passing one `test-new-engine.log`, the patch
`patch.diff`. `old/` is an export of the base commit (`git archive HEAD`) with the new test and a two-line shim
so the test compiles there: that is where the fail-first run came from.

## Problem

Reproduced on the base commit, with the continuity auditor's scenes copied to `scenes/`:

- `./tutor scenes/c01_electron_jump.dan --layout-report - --verbose`: the cue `|jump|` fires at 3.18 s and runs
  `@jump morph e.level -> 3 1s`. `./tutor scenes/c01_electron_jump.dan --snapshot frames/old_c01 --at
  3.1,3.5,3.7,4.3`: at 3.7 s (`frames/old_c01_3.7.png`) the electron sits at level ≈ 2.3, between n = 2 and
  n = 3, while the subtitle reads "jumps straight to level three; it is never in between". The eased picture states
  a falsehood about the physics, and nothing in the language told Claude it could do otherwise.
- `./tutor scenes/r43_morph_zero.dan --check` (an uncued `morph q.x -> 5 0s`): `decodable: yes`, `axiom
  warnings: 0`, no lint, exit 0. `--readback` ends `# since: morph q.x 1 -> 5 done`: the history does not say it
  was a cut.
- `./tutor scenes/c02_treaty_border.dan --check` (a cued `0s`): also clean. So `0s` works (the auditor's c02 and
  c04 frames: the border moves on the word; the price trail keeps a true gap), but it is undocumented, the README
  calls morph "the only way anything moves", the prompt says nothing, and the engine draws a cut as a silent
  jump: no sign on stage that it was meant.
- Corpus (read-only, the main repo's `chat/sessions`): 220 morph lines in 63 turn files, 0 with `0s`, 0 under
  0.4 s. Discrete events do occur: "it |snap|snaps" (a stress-strain curve; written as 0.4 s and 0.6 s morphs),
  "lo jumps |again|again" (a binary-search pointer; a re-mark of `at=`, which slides), "the plane collapses onto a
  line" (a 2.5 s linear morph, which is right: that collapse is the end of a continuous squeeze).

The decision: the form of a cut, what the readback says, whether it leaves a trace, how it is drawn, what the
prompt says about when it is right, and a lint for a cut not cued on a word.

## Designs considered

1. **A duration of `0s`** (exists; document it, draw it as an event, lint it when uncued). No new syntax: a cut
   is a morph whose duration is zero, which is the honest statement, and it is the end of a continuum the
   language already has (`0.05s` is a plucked string's attack, the auditor's c03). The readback's history can say
   `0s` in the same vocabulary. Claude learns one fact ("0s is a cut, rarely right"), not a form.
2. **A verb, `cut e.level -> 3`.** The intent is explicit in the word. Against: a seventh verb for a rare move;
   once `0s` exists there are two spellings of one thing (the language's principle 7, one way per thing) unless
   `0s` is made an error, which is a new rule for no gain; a cut has nothing of its own to read back (the readback
   prints state and history, and the history already speaks morph).
3. **An ease, `morph … 1s ease=step`** (CSS `step-end`: hold, then jump at the end of the duration). A real rate
   function, no parser change either. Against: it is a wait plus a cut, which the language already writes (a
   cue, or `wait`, then the change); the duration stops meaning what it means everywhere else; and the line reads
   as motion to anyone skimming it.
4. **A property on the thing, `mark e dot level=1 discrete=true`**, so every change of it cuts (a quantum
   number, a program counter). Against: it hides the cut from the line that performs it, it is a type claim the
   engine cannot check, and `code … line=` is documented as gliding.
5. **No cut; teach fade-out then fade-in** (two opacity morphs). Against: identity (one thing, read as a
   disappearance or as two things), three lines for one event, and it still cannot say "the border is now here".

Chosen: 1. The others add syntax or hide intent; `0s` is already the language's word for "no time".

How to draw the event, within design 1:
- a. **A flash at the new place** (3Blue1Brown's `Flash`/`FlashAround`; the mined corpus has emphasis effects
  under 2 per 100 steps, `docs/3b1b-habits.md`). Our dictionary already spends the pulse ring on "the voice
  points at it"; a cut drawn as a pulse is indistinguishable from `indicate`. And the new place needs no extra
  signal: an abrupt onset captures attention on its own (Yantis & Jonides 1984; from memory, not re-checked
  here).
- b. **An afterimage at the old place**: the thing as it was, drawn under the stage's things, fading over 0.6 s
  (the `hide`/`show` fade). It is a trail of zero length, so it has the meaning a trail already has, "where it was
  a moment ago", and no other. Chosen.
- c. Both. More ink for one event, and the pulse's meaning blurs. No.

## Chosen design

**Form.** `0s` on a morph is the cut. Nothing new to parse; `isDuration("0s")` already yields 0. The engine
reads the author's intent off the `Line` (`ln.dur <= 0`), never off the `Active` (in instant mode every Active
has duration 0, so that would call every morph a cut). Applies to numeric, list (`pts`, `m`, `to`) and string
(`tex`, a category) morphs alike.

**Drawn as an event.** When a cut lands on a shape (dot, bar, line, arrow, path, region, circle, rect,
polygon, angle, vector, grid), the engine keeps a copy of the mark as it was (its old properties and screen
position) and draws it for 0.6 s at strength `0.55 · (1 − t/0.6)^1.5` of the thing's own alpha, without its
names, under the shapes, so the thing itself stays crisp at its new place and its name goes with it. The copy
is drawn through `drawMark`, the same path `send` uses for a travelling copy, so every kind looks like itself.
Text (formulas, blocks, boxes, notes, names) and invisible drivers (`param`) leave none: two texts on one spot
read as neither, and a space draws its things by depth in `drawSpace`, not through `drawMark`. Nothing happens
in instant mode (`--check`, `--readback`, the shadow).

**Trace in the history.** The `since:` line names it: `# since: morph e.level 1 -> 3 0s done`. Same vocabulary,
and the line re-parses (axiom 6); `--readback` of the model is unchanged.

**Lint.** A `0s` morph on a step with no `@cue` (its `with` lines included) is `continuity: line N cuts id.prop
(0s) with no cue word; a cut is an event the voice names: cue it on its word (@word morph id.prop -> v 0s), or
let it morph`. `--check` counts it with `zero`, `identity` and `tex` as an axiom warning, so it exits 1; the chat's
background check forwards it in `<last_scene_issues>`; the toolbox test fails on it. A `@cue` whose `|cue|` is
never spoken is already the `hook:` lint; the parser's cue-gluing (a `@jump` line glues to "jumps") means a
tagged cut is cued even without the bars. Streamed, a cut arriving in a later chunk than its Say keeps its hook
(checked with `--stdin --readback`: no lint, `0s` in the history).

**The prompt.** README (part of Claude's prompt): the morph line gains "(0s: a cut, below)" and a Modifiers
bullet says what a cut is, when it is right (a quantum jump, a border redrawn by treaty, a price gap at the
open, a collision), that it is always on its word, that uncued it is linted, and that a quick change is a short
morph (`0.05s`), not a cut. `chat/prompt.md`: a "Show what you say" row for "a change that takes no time" and a
pitfall (the electron sliding through n = 2). `chat/toolbox.md`: a move, "A change that takes no time (a cut)",
checked by the toolbox test. Caption on stage: "cut -> e.level changes at once (0s): its afterimage marks where
it was (the one break in continuity, said on its word)".

**Axioms.**
- 4, Continuity: bent openly, as the decision asks. The README's axiom text now states the exception and its
  four conditions: stated in the line (`0s`), said on its word (else linted, and `--check` fails), named in the
  history, drawn as an event (the afterimage). Every other change still morphs; the default duration is untouched.
- 1, Identity: untouched. The afterimage is a drawing of a copy, never a thing: no id, not in `order_`, not in
  the model, not in the readback, not in `picture()`; a cut on a wrong `from` still starts from the real value
  (the identity lint path is unchanged, the cut only shortens the Active).
- 6, Decodability: untouched; the `since:` comment gains ` 0s`, the model lines are byte-identical (`make append`
  as before).
- 3, Zero: the afterimage shares ink only with the thing it is a trace of (a name and its thing is an allowed
  relation); it is a transient drawing like the pulse ring, outside the checker, for 0.6 s.
- 2, Structure, 5, Bounded focus, position from structure: not touched.

**Markup before/after** (the language does not change; what Claude writes changes by two characters):

```
Say: "It absorbs a photon and |jump|jumps straight to level three; it is never in between."
@jump morph e.level -> 3 1s        # before: slides through n = 2 while the voice denies it
@jump morph e.level -> 3 0s        # after: on "jumps" the electron is on n = 3, a fading dot marks n = 1
```

## Outside the field

Used, briefly, for the drawing question only. Film cuts a shot when the story jumps and dissolves when time
flows; comics and animation draw a teleport as the figure at its new place with a fading "ghost" at the old one
(animation's multiples), which is what the afterimage is. Perception: two stimuli flashed in succession fuse
into apparent motion only within roughly a tenth of a second (Korte's laws; from memory), so an afterimage that
lasts 600 ms is read as a trace, not as motion; and an abrupt onset captures attention by itself (Yantis &
Jonides 1984; from memory), so the new place needs no flash. Neither claim was checked against a source in this
session; the design does not depend on their exact numbers, only on "a fading copy reads as 'was here'", which the
frames confirm.

## Implementation

`git diff --stat`: 10 files, +152 / −18. Code: ~70 lines; docs and tests: the rest.

- `markup/engine.hpp`: `Obj::afterimage` (a float: > 0 marks a copy as a cut's trace, at that strength);
  `Engine::afterimage(id)` (public, for tests, beside `pulsing`); `Afterimage {Mark was; float t}`,
  `afterimages_`, `kAfterimage = 0.6f`, `afterimageStrength(t)`, `leaveAfterimage`, `drawAfterimages`.
- `markup/engine.cpp`: `doMorph` computes `cut = ln.dur <= 0`, leaves the afterimage before the value lands
  (both the string/list branch and the numeric one; only when the value really changes), appends ` 0s` to the
  history log, and captions the cut (`said` lambda); `leaveAfterimage` (the kind filter); `afterimage(id)`; the
  tick and expiry in `update`; `afterimages_.clear()` in the load reset, `clearStage` and `interrupt`; the
  `continuity:` lint in `analyze`'s static-lint block, over steps (so `with` lines inherit the step's hook).
- `markup/render.cpp`: `alphaOf` multiplies by the afterimage strength; `drawLabels` returns at once for a
  trace; `drawAfterimages` (a copy per trace through `drawMark`); the call in `draw()` before the grids and world
  shapes, so traces lie under shapes, links, trails and data marks.
- `main.cpp`: `continuity` counted as an axiom warning in `--check`.
- `README.md`: the morph line, a Modifiers bullet, the dictionary's change row, axiom 4's stated exception.
- `chat/prompt.md`: a "Show what you say" row, a pitfall. `chat/toolbox.md`: the move.
- `docs/HANDOFF.md` (map row, a gotcha, the moves count, Last updated) and `docs/history.md` (entry 8m), for
  whoever commits this; drop them if the other solution is chosen.
- `tests/test_engine.cpp`: one test case, below.

## Tests

`tests/test_engine.cpp`: **"a cut (morph … 0s) is the one stated break in continuity: it needs its word, the
history says so, and it leaves an afterimage"**. Three parts: an uncued cut is linted and the readback's history
carries `0s`; a cut cued on its word gives no `continuity:` lint; live (`update(1/60)`), one frame after a `0s`
morph the afterimage of `q` is > 0.3 while a 1 s morph on `p` leaves none, and 0.75 s later it is gone.

On the old engine (`old/`, base commit plus a shim `float afterimage(const std::string &) { return 0; }` so the
test compiles; `test-old-engine.log`):

```
tests/test_engine.cpp:485: ERROR: CHECK( linted ) is NOT correct!
  values: CHECK( false )
tests/test_engine.cpp:486: ERROR: CHECK( e.readback(false).find("morph q.x 1 -> 5 0s done") != std::string::npos ) is NOT correct!
tests/test_engine.cpp:496: ERROR: CHECK( live.afterimage("q") > 0.3f ) is NOT correct!
  values: CHECK( 0 >  0.3 )
[doctest] assertions: 8 | 5 passed | 3 failed |
[doctest] Status: FAILURE!
```

(The readback check was then changed to read the commented readback, `e.readback()`, since `readback(false)`
is the model without `#` lines; the old engine fails it either way, the history had no `0s`.)

On the new engine (`test-new-engine.log`):

```
[doctest] test cases: 1 | 1 passed | 0 failed | 31 skipped
[doctest] assertions: 8 | 8 passed | 0 failed |
[doctest] Status: SUCCESS!
```

Also checked by hand (not unit tests): `--check` on `scenes/c01_electron_cut.dan` (cued) → `axiom warnings:
0`, exit 0; on `scenes/r43_morph_zero.dan` (uncued) → `axiom warnings: 1`, the lint, exit 1; the cut streamed
in a later chunk than its Say (`--stdin --readback`) → no lint, `# since: morph e.level 1 -> 3 0s done`.

## Regressions

`make test` (`make-test.log`), exit 0:

```
./tests/test_place    [doctest] test cases:    21 |    21 passed | 0 failed   assertions: 35525 | 35525 passed
./tests/test_viz      [doctest] test cases:    14 |    14 passed | 0 failed   assertions: 18066 | 18066 passed
./tests/test_script   [doctest] test cases:     8 |     8 passed | 0 failed   assertions:    42 |    42 passed
./tests/test_engine   [doctest] test cases:    32 |    32 passed | 0 failed   assertions:   192 |   192 passed
node tests/compute.mjs   19 ok
```

`make check` (`make-check.log`), exit 0: 93 scenes (`scripts/`, `chat/examples/`, `chat/idioms/`) all `ok`, no
`FAIL`; `node tests/toolbox.mjs` 32 moves ok, including the new "A change that takes no time (a cut)".

`make golden` (`golden-new.log`, `golden-old.log`): new placer 52/52 `same`, mean 99.6; old placer 52/52 `same`,
mean 97.4; 0 WORSE, 0 better, both exit 0. (No repo scene has a `0s` morph, and the afterimage is not in
`picture()`, so nothing could move.)

`make append` (`make-append.log`): `append equivalence: 152 identical, 4 different`, the four being
`space-downhill` in chunks of 2, 3, 5 and `space-saddle` in chunks of 2: the known pre-existing 3D artefact (a
stale `space:` lint comment, model identical), same count as `docs/history.md` 8k records. No new difference.

## Frames

All from `--snapshot` (offscreen), viewed as PNGs; sheets built with `magick +append/-append`.

- `frames/old_c01_3.7.png` (base commit, the auditor's scene): the electron between n = 2 and n = 3 under
  "never in between". The reproduction.
- `frames/sheet_c01cut.png` (`scenes/c01_electron_cut.dan`, `0s`; 3.1, 3.2, 3.3, 3.5, 3.7, 4.3 s; cue at 3.18):
  at 3.1 the electron is on n = 1; at 3.2 it is on n = 3 with its name, and a dimmer blue dot sits on n = 1; at 3.3
  and 3.5 that dot is fainter; at 3.7 and 4.3 it is gone. `frames/new_c01cut_3.2.png` at full size: the trace is
  clearly secondary (about half strength) and has no name; it reads "it was here".
- `frames/sheet_c02.png` (treaty, 4.4, 4.55, 4.7, 5.0 s; cue at 4.48): on "moves" the border is at the river;
  the old border line and the ceded strip (the old East's fill) linger as a fading trace, nearly gone at 5.0.
- `frames/sheet_c04.png` (price gap, 4.9, 5.1, 5.3, 5.8 s): the dot drifts along its trail, then on "gaps" it is
  at 8 with a fading dot at the trail's end; at 5.8 only the true gap in the trail remains.
- `frames/new_fncut_3.05.png` (`scenes/fn_cut.dan`, `morph f.fn -> {x^3} 0s`): the cube at full strength over
  the fading square. A `0s` function change also lands correctly, where an eased `fn` morph empties the stage
  (the auditor's W4), because the cut sets the expression at once.
- `frames/new_c01eased_3.7.png`: the auditor's unchanged 1 s scene on the new engine, still eased: nothing
  changed for scripts that do not ask for a cut.

What I did not look at: a real (Retina) window; the drawing goes through `drawMark` and `alphaOf` only, with no
new transform, so the `--snapshot`/window difference the handoff warns about does not apply.

## Limits and risks

- **No trace for text or 3D.** A `0s` on `tex`, a card, a box's value, a note, a `param`, or anything in a
  `schema space` cuts without an afterimage. A param that drives shapes through `{expr}` makes them jump with no
  trace (the cut is on the driver). The lint and the history still apply. Space could get one later through
  `drawSpace`.
- **The afterimage is not in `picture()`**, like the pulse ring, so the checker never sees it and `--layout-report`
  never scores it. A 0.6 s trace can sit under a label; it is faint and brief, and it is a copy of a thing that
  was there a moment ago.
- **A ghost under a shape that shrinks in place**: a bar cut to a smaller value shows its old top fading above
  the new bar (right), but a circle cut to a smaller radius shows a fading ring around it, which could be read as
  a glow. Brief; not seen in a real lesson yet.
- **The 0.6 s and 0.55 are judged by eye** on three scenes, not measured with learners. Easy to tune in one place
  (`kAfterimage`, `afterimageStrength`).
- **Over-use by Claude.** The prompt says rare and names the cases; there is no measurement yet of how often
  Claude now writes `0s` or whether it reaches for it where a short morph is right (an eval on a discrete-event
  question set would tell; not done). An uncued cut is caught; a cued but wrong cut is a taste question.
- **`--check` now fails on an uncued `0s`.** No repo scene, example, idiom or real chat (0 of 220 morphs) has
  one, so nothing existing changes; an old script that did would print the lint and exit 1, and still play.
- **Migration:** none for scripts or chats; the readback gains ` 0s` in a comment only.
- The HANDOFF/history edits assume this solution is the one merged; drop them otherwise.

## Verdict

**Ship.** The decision's four questions have one small, local answer each (the form is `0s`, already in the
language; the history says so; the engine draws the event as an afterimage; an uncued cut is linted and fails
`--check`), the axioms are kept with the exception stated in the README's own words, the test fails on the old
engine on exactly the three behaviours and passes now, every regression is unchanged (192/192, 93 + 32 ok,
52/52 same on both placers, append as before), and the frames show the electron, the treaty and the price gap
doing what the voice says. The caveats are scope (no trace for text or 3D) and an unmeasured prompt effect, not
risks to what exists.
