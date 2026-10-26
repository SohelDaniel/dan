# D6-A: holding a subject (`focus a,b`, `focus x hold`) and the edges of axiom 5

Engineer D6-A. Worktree: `/Users/soheldanial/code/server/testing/visulaizations/tutor/.claude/worktrees/agent-a0ebb97c92abd4bdb`
(branch `worktree-agent-a0ebb97c92abd4bdb`, base commit 6038197). Scenes and frames referred to below are under this
folder (`scenes/`, `frames/`); `patch.diff` is the worktree's `git diff` with its stat and status.

## Problem

Reproduced on my own build of the base commit (`make tutor`; `./tests/test_engine`: 31 cases, 184 assertions pass,
no focus test among them, as the enforcement map says).

1. **Two subjects need a group they did not mean.** `scenes/cf1-music.dan` (the auditor's chorale): "Listen to the
   outer two, soprano and bass" with `@two focus sop`, then two sentences about the pair.
   `./tutor scenes/cf1-music.dan --snapshot frames/old-cf1 --at 6,11 --mute`: at 6 s only the soprano is lit (the
   bass, half the subject, is as dim as the inner voices); at 11 s, while sentence 3 discusses the pair, all four
   boxes are back at full strength. `scenes/cf2-music-group.dan` does it with `group outer part-of=[sop,bas]` and a
   `focus outer` before every sentence: `frames/old-cf2_6.png` shows an enclosure labelled "outer voices" and the
   four boxes re-laid into lanes (the boxes moved), ink and motion the lesson did not ask for.
2. **A subject cannot be held across sentences.** The only way is a focus line before every sentence (the user's
   real turn `chat/sessions/20261017-045709/turns/002.txt` writes `focus i` / Say / `focus det` / Say / `focus j` /
   Say / `focus det` / Say / `focus none`). 10 of the 24 focus lines in the user's chats are `focus none` written by
   hand: Claude already thinks in "open … close" brackets.
3. **A trailing focus never ends.** `scenes/f3-last.dan` ends on `focus b` after its last sentence.
   `./tutor scenes/f3-last.dan --readback` printed `focus b` as the scene's last line; `--snapshot --at end`
   (`frames/old-f3_end.png`) shows a and c still dim after the script finished. In the chat this dims the stage
   through Claude's thinking and the next turn's first sentence (state.dan carries the line).
4. **`interrupt()` keeps the focus.** `markup/engine.cpp:380-403` set `say_.active = false` without
   `closeSentence()`; the only clear was in `closeSentence`. Confirmed by my test on the old engine: after an
   interrupt 1.5 s into a sentence the readback still printed the focus line and the other box stayed at 0.2.
5. **Cells of a focused array snap.** `scenes/f5-cell.dan`, `--snapshot --at 0.5,0.95,3.0`: `frames/old-f5_0.95.png`,
   0.08 s after the cue, already shows the other cells at their dim level while the box "other" is still fading
   (`render.cpp:350-353` recomputed `dimmed` from `focus_` every frame, never through an eased value).

Contact sheet of the old engine: `frames/old-sheet.png` (cf1 at 6 and 11 s, cf2 at 6 and 11 s, f3 at the end, f5 at
0.95 s).

## Designs considered

**A. An explicit, bounded hold (the auditor's #10): `focus a,b` and `focus x hold` … `focus none`.** Chosen.
The author states the span as a bracket (open with `hold`, close with `none`), the engine keeps every bound
(none, a new scene, an interrupt, the end of the turn, a fifth sentence). Claude already writes the closing half by
hand (10/24). One new word, no new verb, decodable as written.

**B. The cheaper form: a focus lasts until the next focus line.** This is what the oldest scripts (`scripts/bank.dan`:
`focus alice` … `focus bob` … `focus none`) and the hand-written `focus none` lines suggest Claude believes
already. Rejected: it moves the bound from the engine to Claude's discipline. 13 of the 24 real focus lines are cued
inside a sentence with no `none` after them (`chat/sessions/20261017-045709/turns/005.txt` ends on `@going focus
ice`); under B they would leak into the next turn's sentences until Claude remembered to clear. Axiom 5 says the
engine enforces the bound.

**C. A group without a drawn enclosure.** A group also makes a lane (the layout re-flows, cf2's boxes moved), so it
would need a second flag to be inert: a structural word used as a free canvas, the invisible-plane loophole the
handoff already lists as a problem (§9). It also still needs a focus line per sentence. Rejected.

**D. A counted hold, `focus x for=3`.** The author states the bound as a number; no cap lint needed. Rejected: the
count goes stale when the paragraph is rewritten (Claude writes beats and revises), and a wrong count fails silently
by a sentence either way, while a bracket fails loudly (the cap lint) and reads as the subject's span.

**E. How a trailing focus should end (the auditor's #2).** Two options: clear when the stage idles and the subtitle
has faded (a timing heuristic), or an explicit end-of-turn signal. Chose the signal. The live stage plays beats while
Claude is still writing; an idle gap mid-turn (a python block, a slow beat) would end a focus that was set up for the
sentence about to arrive, and the stage would then differ from the readback. The signal is one line in the server
(`#!turn-end` after the last beat) and a flag in the engine; the headless modes end their turn at the end of the
file, which keeps streamed playback equal to whole playback (`make append`) and lets `--check` report the slip.

No analogue from outside software was needed; the bracket form is the obvious one.

## Chosen design

Syntax (README's verb table, in the prompt):

```
focus <id>[,<id>…] [hold] | none     what the voice is about: everything else dims, for one sentence;
                                     `hold` keeps it until `focus none` (four sentences at most)
```

Also accepted: `focus [sop, bas]` (the list form of `of=[a,b]`) and `focus a, b`; `hold` is a word anywhere after
the verb; `all`/`nothing` still mean `none`. Targets are what they were: things, cells and spans (`a[3]`, `a[1:3]`,
`M[1][:]`), groups, links. The readback prints `focus sop,bas` or `focus sop,bas hold` while a focus is on.

Semantics:
- A plain focus lasts one sentence, exactly as before (the one it is cued in, or the next one if it stands between
  sentences). Nothing changes for the 24 focus lines in the chats, the 40 in `scripts/`, or the exemplars.
- `hold`: the focus stays through the sentences that follow until `focus none` (or any new `focus` line), a
  `scene`, an interrupt, the end of the turn, or the start of its fifth sentence (`kHoldMax = 4`), where the engine
  ends it with the lint `focus: the hold on 'a' ran past 4 sentences and ended on its own; put focus none where
  the subject changes`. The count starts at the focus line, so a `focus none` placed between the fourth and fifth
  sentences closes it without a lint.
- End of turn: the server sends `#!turn-end` after the last beat; the stage ends any focus once it has finished
  playing (`turnDone_ && finished()`), eased. A focus that was never through a sentence (f3) is reported:
  `focus: 'b' is set after the last sentence, so no sentence is about it: it ended with the turn`. The headless modes
  (`--readback`, `--check`, `--snapshot`, `--layout-report`, the window on a file, `#!load`) declare the end of the
  file the end of the turn; `appendText` reopens it. So `state.dan` never carries a focus into the next turn, and the
  live stage and the readback agree. (The server's `readback()` keeps only model lines in `state.dan`; the two new
  lints reach Claude the way every lint does, through `--check` and `<last_scene_issues>`. The pause-and-ask window
  of `chat/explain.mjs` opens its explanation as a file, so it gets the file rule.)
- An interrupt ends the focus (nothing more is said).
- Cells: a focused array keeps an eased per-thing `cellDim` (7/s, the same ease as `lit`) and remembers which cells
  the focus lit while the dim fades out; the other cells go to 0.3, as before (parts of a lit thing stay nearer than
  an unrelated thing's 0.2; the dictionary row now says so). Area diagrams (`focus pop[1]`) get the same ease.

Axioms:
- **5 (bounded focus):** the default is unchanged; the hold is the author's explicit statement and the engine
  enforces five ends for it. "Then everything is at full strength again" now also holds after an interrupt and at
  the end of a turn, where it did not before.
- **4 (continuity):** the cells of a focused array and the parts of a focused area now ease like everything else
  (test: one frame after the focus the other cells are at 0.92, after a second at 0.3, back above 0.95 a second into
  the next sentence). Nothing snaps.
- **6 (decodability):** `focus a,b hold` reads back in the same words and `--check` re-parses it to the same scene
  (test). A finished turn reads back without a focus line, which is also what the stage shows.
- **3 (zero):** two subjects no longer need an enclosure and a lane the lesson did not mean (cf2). Focus draws no
  ink; it only dims.
- **1, 2:** untouched. A focus changes no value and no scale; `focus a,b` lights without re-laying out anything.

Markup before/after (the chorale, `scenes/cf1-music.dan` → `scenes/cf1-hold.dan`):

```
# before: half the subject, or a group                 # after
Say: "Listen to the |two|outer two, soprano and bass…" Say: "Listen to the |two|outer pair, soprano and bass…"
@two focus sop                                         @two focus sop,bas hold
Say: "When the soprano rises, the bass falls…"         Say: "When the soprano rises, the bass falls…"
Say: "And when the soprano falls, the bass rises…"     Say: "And when the soprano falls, the bass rises…"
                                                       focus none
Say: "Done."                                           Say: "And now all four voices come back together…"
```

What Claude has to learn: a comma, one word (`hold`) and the closing `focus none` it already writes. Said in three
places: the README's verb line (in the prompt), the "Signal" bullet of `chat/prompt.md`, and a new toolbox move
"Hold one subject while several sentences discuss it" (checked by `tests/toolbox.mjs` like the others).

## Implementation

348 insertions, 45 deletions over 15 files (`patch.diff`); engine code about 130 lines net, tests 177, text 30.

- `markup/script.hpp` (+4/−1): `Line::hold`; the header's `focus` line.
- `markup/script.cpp` (+13/−4): `readFocus` reads the targets into `members` (commas, an optional `[…]`), `hold`,
  `none`; `id` is the first target. The unused `focus a.prop` split is gone.
- `markup/engine.hpp` (+18/−2): `Obj::cellDim`, `Obj::cellFocus`; `focus_` is a vector; `focusHeld_`,
  `focusSaid_`, `kHoldMax`, `turnDone_`; `unfocus`, `closeTurn`, `focused`, `focusRef`, `focusText`, `cellLit`;
  public `litOf` (for tests) and `endTurn`.
- `markup/engine.cpp` (+90/−26): `analyze` (the never-marked lint per target); `reset`/`clearStage` → `unfocus`;
  `execLine` FOCUS; `startSay` (the cap at a fifth sentence); `closeSentence` (count, clear unless held);
  `interrupt` (clear); `appendText` (`turnDone_ = false`); `update` (the `lit` lambda over several targets with a
  named `inGroup` walk, the eased cell dim, `closeTurn` once the turn is done and the stage finished, before the
  readback is printed); `readback` (`focus a,b hold`); the new functions in one block.
- `markup/render.cpp` (+2/−5): `\textcolor{id}` glyphs lit through `focused`; array cells through `cellLit`.
- `markup/area.cpp` (+4/−3): the eased part dim.
- `markup/placement.cpp` (+2/−2): label and ink priority through `focusRef`.
- `main.cpp` (+8): `endTurn()` after every file load, after the headless chunk loop, and on `#!turn-end`.
- `chat/server.mjs` (+1): `stageSend('#!turn-end')` after the last beat has been sent.
- `README.md` (verb line, dictionary row, axiom 5), `chat/prompt.md` (the Signal bullet), `chat/toolbox.md` (the
  move), `docs/HANDOFF.md` (axiom 5 line).

## Tests

`tests/test_script.cpp`: "focus takes several things (a,b or [a,b]) and a hold; none clears".
`tests/test_engine.cpp` (live engines stepped at 60 fps, the sentence boundaries read off `timeline`):
1. "focus a,b lights both without a group; the rest dims; everything returns after the sentence"
2. "focus … hold keeps the subject through the sentences about it, until focus none; no lint when closed in time"
3. "a hold is bounded: after four sentences the engine ends it and says so"
4. "a hold ends with a scene"
5. "a hold ends with an interrupt: nothing more is said, so nothing is the subject"
6. "a focus set up after the last sentence ends with the turn, eased, and is reported; streamed, it waits for the
   turn's end" (a file, the chat's appended beat + `endTurn`, and the instant `--readback` path)
7. "focus a[3]: the array stays lit, its other cells dim like everything else, eased in and out"
8. "a held focus reads back as written and re-parses to the same scene; a target never marked is reported"
The instant helper `played()` now calls `endTurn()`, as `--readback` and `--check` do; all 31 old cases still pass
with it.

Failing first. To make the failures behavioural rather than compile errors, I added two observation hooks before the
run: `litOf()` with the old render maths for cells, and an empty `endTurn()`; `Line::hold` was declared but not
parsed. Old engine (`./tests/test_engine`, trimmed):

```
TEST CASE:  focus a,b lights both without a group; …
tests/test_engine.cpp:516: ERROR: CHECK( e.litOf("sop") > 0.99f )   values: CHECK( 0.200016 >  0.99 )
tests/test_engine.cpp:517: ERROR: CHECK( e.litOf("bas") > 0.99f )   values: CHECK( 0.200016 >  0.99 )
TEST CASE:  focus … hold keeps the subject …
tests/test_engine.cpp:533: ERROR: CHECK( e.litOf("alt") < 0.25f )   values: CHECK( 0.999422 <  0.25 )
tests/test_engine.cpp:535: ERROR: CHECK( e.readback(false).find("focus sop,bas hold\n") != npos )
TEST CASE:  a hold is bounded …
tests/test_engine.cpp:551: ERROR: CHECK( e.litOf("b") < 0.25f )     values: CHECK( 1 <  0.25 )
tests/test_engine.cpp:555: ERROR: CHECK( lintSaying(e, "focus:", "hold") )
TEST CASE:  a hold ends with an interrupt …
tests/test_engine.cpp:575: ERROR: CHECK( e.readback(false).find("focus a hold\n") != npos )
TEST CASE:  a focus set up after the last sentence ends with the turn …
tests/test_engine.cpp:590: ERROR: CHECK( e.litOf("a") > 0.95f )     values: CHECK( 0.20002 >  0.95 )
tests/test_engine.cpp:591: ERROR: CHECK( e.readback(false).find("focus") == npos )   values: CHECK( 81 == … )
tests/test_engine.cpp:592: ERROR: CHECK( lintSaying(e, "focus:", "turn") )
tests/test_engine.cpp:601: ERROR: CHECK( s.litOf("a") > 0.95f )     values: CHECK( 0.2 >  0.95 )
tests/test_engine.cpp:604: ERROR: CHECK( r.readback(false).find("focus") == npos )
TEST CASE:  focus a[3]: the array stays lit, its other cells dim like everything else, eased …
tests/test_engine.cpp:614: ERROR: CHECK( e.litOf("a[0]") > 0.8f )   values: CHECK( 0.3 >  0.8 )
TEST CASE:  a held focus reads back as written …
tests/test_engine.cpp:629: ERROR: CHECK( rb.find("focus a,b hold\n") != npos )
tests/test_engine.cpp:636: ERROR: CHECK( lintSaying(bad, "focus:", "'zz', which is never marked") )
[doctest] test cases:  39 |  32 passed |  7 failed | 0 skipped
[doctest] assertions: 254 | 237 passed | 17 failed |
```

and `./tests/test_script`: `REQUIRE( s.steps.size() == 5 )  values: REQUIRE( 4 == 5 )` (my own slip: the `with`
line joins the step before it; the expectation was corrected to 4 steps and the `with` line's `hold`/`members`;
on the old parser the case then fails on `members == {"sop","bas"}`, since it read one id `sop,bas`).

"A hold ends with a scene" passes on the old engine too (there, the focus ends at the sentence anyway); it guards
the new behaviour rather than proving a bug. Interrupt's first assertion (`litOf("b") < 0.25`) also passes on the
old engine; the readback one fails, as quoted.

New engine:

```
[doctest] test cases:  9 |  9 passed | 0 failed | 0 skipped        (test_script)
[doctest] assertions: 56 | 56 passed | 0 failed |
[doctest] test cases:  39 |  39 passed | 0 failed | 0 skipped      (test_engine)
[doctest] assertions: 254 | 254 passed | 0 failed |
```

## Regressions

`make test` (tails):
```
[doctest] test cases:    21 |    21 passed | 0 failed | 0 skipped      test_place
[doctest] assertions: 35525 | 35525 passed | 0 failed |
[doctest] test cases:    14 |    14 passed | 0 failed | 0 skipped      test_viz
[doctest] assertions: 18066 | 18066 passed | 0 failed |
[doctest] test cases:  9 |  9 passed | 0 failed | 0 skipped            test_script
[doctest] assertions: 56 | 56 passed | 0 failed |
[doctest] test cases:  39 |  39 passed | 0 failed | 0 skipped          test_engine
[doctest] assertions: 254 | 254 passed | 0 failed |
node tests/compute.mjs: 16 × ok
```

`make check`: 125 `ok`, 0 `FAIL` (every `scripts/*.dan`, `chat/examples/*.dan`, `chat/idioms/*.dan`), then
`node tests/toolbox.mjs`: 32 moves `ok`, including the new "Hold one subject while several sentences discuss it".

`make golden`: every one of the 52 scripts `same` with both placers; `mean layout score 99.6 over 52 scripts` (new
placer) and `97.4` (old placer). Nothing WORSE, nothing better: focus never touched layout, and still doesn't.

`make append`:
```
DIFFERS: scripts/space-downhill.dan in chunks of 2
DIFFERS: scripts/space-saddle.dan in chunks of 2
DIFFERS: scripts/space-downhill.dan in chunks of 3
DIFFERS: scripts/space-downhill.dan in chunks of 5
append equivalence: 152 identical, 4 different
```
The four are the pre-existing ones the brief names. I diffed space-downhill in chunks of 2 (`frames/sd-one.txt`
vs `frames/sd-live.txt`): the only difference is the comment line `# lint: space: a surface is shown but the camera
never moves; …` present in the whole run and absent in the streamed one; every model line is identical. Not focus.
My own streamed-vs-whole checks on the new scenes (chunks of 2 or 3 through `--stdin`): `cf1-hold`, `hold-overrun`
(its cap lint included) and `f3-last` are identical.

The auditor's chat copies (`audit-5/chats/*.dan`, 8 real turns) and 12 focus scenes under `--check`: no focus lint
anywhere except `f3-last` (the intended one) and `f11-late-beat` part 1 on its own, whose trailing `focus b` is set
up for its second chunk; streamed with part 2 as one turn there is no lint and no focus line (`scan-focus-lints.sh`).

`--check` on the new scenes: `cf1-hold` decodable, no lint; `hold-overrun` decodable with the cap lint; `f3-last`
decodable, readback without a focus line, the turn lint; `f5-cell` decodable, no lint.

## Frames

`frames/new-sheet.png` next to `frames/old-sheet.png` (same tiles: cf1 at 6, 11, 16, 22 s; f3 at the end; f5 at
0.95 s):
- cf1 with `focus sop,bas hold` (`scenes/cf1-hold.dan`, verbose log: the hold set at 3.38 s on "outer", sentences 3
  and 4 at 8.32 and 13.70 s, `focus none` at 19.20 s): at 6 s soprano **and** bass lit, alto and tenor dim (old:
  soprano alone); at 11 and 16 s still so, through the two sentences about the pair (old: all four back at 11 s);
  at 22 s all four back. No enclosure, no lane, no box moved (old cf2 moved them).
- f3 at the end: a, b and c all at full strength (old: a and c dim for ever).
- f5 at 0.95 s (0.08 s after the cue): the cells 2, 5, 8, 16, 23 and the box "other" are at the same partial
  brightness, both fading (old: the cells already at 0.3 while the box was still bright). At 3.0 s the cells sit at
  0.3 and the box at 0.2, as documented.
The checker's layout score is 100 for all of them, before and after: this change is about brightness, not boxes.

## Limits and risks

- The cap is a constant (four sentences). A longer paragraph on one subject sees the stage return at the fifth
  sentence, with the lint; Claude can re-issue `focus x hold`. Four matched the cases in hand (the chorale, a
  parse tree: three sentences); it is a taste constant and easy to change.
- No lint for a hold left open to the end of the turn (it did its job; the turn ends it). Only a focus that no
  sentence used is reported. If stale-looking turn ends appear in real chats, a second lint is five lines.
- Live, a hold or a trailing focus ends only once the server has sent `#!turn-end` **and** the stage has finished
  playing. If a turn errors before `finish()` or the server is restarted mid-turn, the stage keeps the focus until the
  next sentence closes it, as today. A stage window reopened mid-turn (`appendStage` → `spawnStage`) is loaded as a
  complete turn, so if it finishes replaying before the next beat lands a hold is closed early, once.
- The windowed live path (`#!turn-end` → `endTurn()`) is exercised by the unit test of `appendText` + `endTurn`
  and by the headless `--stdin` run, not by a real window (none allowed here). `tutor restart` and one real chat
  with a held subject is the check to make before trusting it.
- Still not solved (not in this decision): `focus f[part]` on a formula part is not a target; a count of a subset
  ("the outer two") still earns the binding lint; the `of=` notes of a focus sit at 0.7 undocumented.
- Timing in the tests is the engine's estimated word timing; with Inworld timestamps the cue fires at a different
  moment but the sentence boundaries that bound a focus are the same events.
- Whether Claude uses `hold` well is unmeasured (no Claude-in-the-loop eval run here). The language change is
  three short sentences in the prompt material and one checked toolbox move; a bad habit it could pick up is a
  hold on every paragraph, which the cap lint and the four-sentence bound limit.
- Migration: no existing script, exemplar, idiom or chat turn changes behaviour (all 125 check ok, golden same, no
  chat copy lints). An old `state.dan` ending on `focus x` (none in the user's sessions) would print the turn lint on
  load, then play normally. The `focus a.prop` form (never written anywhere) now warns instead of focusing `a`.

For whoever commits this: `docs/HANDOFF.md` §3 is updated in the patch; also bump "Last updated", the toolbox count
in §8 (27 → 28 moves), and add a `docs/history.md` entry (what the auditor found, the five designs, the numbers above).

## Verdict

**Ship with caveats.** The hold is worth the bend: it is explicit in the markup, bounded five ways by the engine, read
back in the same words, and it removes a group and a focus line per sentence that Claude was writing to fake it. The
edges of axiom 5 (a trailing focus, an interrupt) are now closed and tested, and the cells ease (axiom 4). Every
regression is green; the only `make append` differences are the known stale comment. Caveats: the live
`#!turn-end` path needs one real windowed chat to confirm (no window here), the four-sentence cap is a constant, and
Claude's uptake of `hold` is unmeasured.
