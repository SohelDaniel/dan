# D7 (engineer A): trails across a turn boundary

Worktree: `/Users/soheldanial/code/server/testing/visulaizations/tutor/.claude/worktrees/agent-a80a990da1d665390`
(branch `worktree-agent-a80a990da1d665390`, base commit 6038197). Nothing committed. Scenes, frames and helper
scripts referred to below are in this folder (`scenes/`, `frames/`, `rt.sh`, `fold.sh`, `append-diff.sh`).

## Problem

A dot with `trail=all` draws where it has been, recomputed every frame from the recorded history of every number
that changed (`hist_`, markup/trails.cpp). The chat keeps the stage between turns as the model-only readback
(`chat/server.mjs` `readback()`: `--readback` with the `#` lines dropped, written to `state.dan`), and the readback
wrote a dot as its current `x= y=` only. After the fold the path was gone: in `<last_frame>` (a snapshot of
state + turn), in the background `--check`, after a voice change or restart (`live.dan = state.dan`), and in
`/api/replay-turn`. Only the live window, which is never reloaded, still had it.

Reproduced with the unchanged worktree binary (`rt.sh TUTOR scene old`: `--check`, the model-only readback, a
`--snapshot --at end` of the original and of its readback, stage area cropped, `compare -metric AE`):

| scene | what it is | `--check` | stage px differing (orig vs its readback) |
|---|---|---|---|
| `scenes/hurricane.dan` (audit-6) | two morphs with `with` | decodable: yes | 369 |
| `scenes/trails-descent.dan` (= scripts/descent.dan) | 5 re-marks, zig-zag, then `camera zoom=2 on=p` | decodable: yes | 19344 |
| `scenes/he-init.dan` (chat 20261020-171259, as Claude wrote it) | 3 dots on `x={clock}`, one `morph clock.value 0 -> 10` | decodable: yes | 1767 |
| `scenes/orbit.dan` (chat 20261021-003930) | `x={-2.236+3*cos(clock)}`, one morph round the ellipse | decodable: yes | 750 |

Frames: `frames/<scene>.old.orig.png` vs `frames/<scene>.old.rb.png` (montages: `frames/<scene>.montage.png`,
left the original, middle the old fold, right the new fold). he-init: the three curves (0.7^x falling, 1.3^x
capped, the flat line at 1) are the whole lesson; the old fold keeps three dots at x=10. The old readback:

```
mark clock param value=10
mark small dot x={clock} y={pow(0.7,clock)} trail=all color=blue label="weights too small"
```

What the real chats do (grep of chat/sessions, read-only): 11 distinct `trail=` lines in 6 sessions; 9 are
`trail=all`, and 7 of those are driven by a `param` clock through `{expr}` (`x={clock}`, a projectile on a clock
`s`, an orbit, a stress-strain dot on two params through a path call), 2 by re-marks (gradient descent). Two are
`trail=1.5s` (a fading tail, moot after a turn). So the carry has to work for expression-driven paths, not only
for literal `x= y=` steps, and "re-derive from the script" would mean replaying the lesson's own animation.

Why the fold cannot simply sample by time: the fold is an instant run (`main.cpp` sets `opt.instant`;
`runToEnd()` calls `update(0)`), so `clock_` never advances; every history segment is recorded at `t0 = 0` with
`dur = 0`, `histValue` always answers with the final value, and `trailRuns` finds no span (`t1 - t0 < 1e-3`).
The history still holds every change in order (before, from, to, ease, final), which is what the carry uses.

## Designs considered

1. **Confess.** The readback writes `trail=all` with a `# trail: lost across a fold` comment and the prompt says
   a trail does not survive a turn. Cheap (10 lines), honest, but the learner's picture after a restart, the
   `<last_frame>` Claude reasons about and the `--check` score all still lack the path; axiom 1 (a thing's
   history is part of it) stays bent and axiom 4 is not even reachable (a re-drawn trail cannot morph in from a
   path that is gone). Also the `#` lines never reach Claude (chat/server.mjs:223 drops them), so the confession
   would reach only humans. Rejected.
2. **Re-derive: write the motion back as morphs.** The readback would print the dot at its first position
   followed by `morph`/re-mark lines. Rejected: the readback is the final scene, not a script (axiom 6's
   vocabulary is state); replaying motion on every restart and `--snapshot` is wrong; the chat's folds would
   accumulate a script; and 7 of the 9 real cases are param-driven, where the motion is `morph clock.value`,
   the lesson's animation itself.
3. **Carry the path as data, sampled by time in the live engine.** The window has the real history with time
   and could write the trail at finish (it already prints a readback nobody reads). Rejected: the server folds
   when Claude's reply ends, while the stage is still playing, with an instant run; the fold's engine must be
   able to write it on its own.
4. **Carry the path as data, read off the history in the order it happened (chosen).** A `was=[x,y, x,y, …]`
   key the readback writes and a load draws. The order of recorded changes is a timeline the instant engine
   does have; sampling each recorded change along its own parameter gives the path's geometry (which does not
   depend on speech timing, so the fold is the same whether the voice was fast or slow). Details below.

Within 4, two sub-choices:
- Overload `trail=` with the list (`trail=[…]` meaning "kept, and here is what it holds") vs a separate key.
  Separate key chosen: Claude often re-marks a dot from memory with `trail=all`, which would replace the list
  and lose the path; a separate `was=` survives any re-mark (props persist), and a fading `trail=1.5s` keeps
  its own meaning.
- Re-simplify the whole path every fold vs keep what was carried as it is. Chosen: what was carried in is kept
  verbatim (it was already reduced) and only what happened since is reduced, from the junction on; everything
  is reduced again, coarser, only when the list would exceed 160 points. Stable text turn after turn, no slow
  erosion of early detail, and it still reads back to itself.

## Chosen design

**What changes.** A kept trail (`trail=all` on a plot or in a space) is part of its thing: the readback writes
the path it has traced as `was=[x,y, x,y, …]` (`x,y,z` triples in a space), the points it went through before
where it is now (the mark's `x= y=` states that), with `-` where it jumped (a teleport leaves no line). A mark
loaded with `was=` draws that path first and the live history continues it, so the trail does not pop and the
checker and the placer see it as ink.

How the path is read off the history (`Engine::trailPath`, trails.cpp):
- every recorded change carries the script step that made it and its sequence number; a mark remembers how
  many changes had happened when it appeared;
- the model is put back to before anything changed (each key to its first segment's `before`), the changes are
  walked forward in sequence; the changes one step made (a re-mark's x and y, a morph and its `with` lines)
  move together, sampled at 128 points along their eases (a dot on `x={clock}` traces the curve the clock drove
  it along, not a chord); `set` changes jump; changes before the mark appeared only move the model; when the
  position is literal numbers, only the mark's own changes are walked; then the model is restored and the
  current position is the path's last point, whatever the history missed;
- a `set` whose jump is more than 5 % of the path's extent breaks the path (what the live engine's 40 px teleport
  rule does, in data terms);
- each run is reduced with Ramer–Douglas–Peucker (`viz::simplify`, pure, unit-tested) to the fewest points within
  0.2 % of the path's extent, axis by axis (so a tall thin plot keeps its corners too), at most 160 points in all;
  the points are rounded to the 6 significant digits the readback prints *before* reduction, so the written path
  reads back to itself.

**Axioms.**
- 6 Decodability: strengthened. The readback now holds what the stage draws; `--check`'s round trip holds on
  every scene with a trail (the written list is a fixed point: the same input simplified again changes nothing,
  rounding happens before reduction, and the carried part is never re-reduced unless the list is too long). The
  new key is documented in README's language section, so it is in the vocabulary Claude reads and writes.
- 1 Identity: the path is written and reloaded with the thing; nothing is deleted, a re-mark keeps `was=`.
- 4 Continuity: after a load the carried path is drawn with the dot's own entrance alpha (`alphaOf`, which eases
  `appear`), so it grows in with the dot instead of popping; the live path continues from its last point.
- 2 Structure: untouched (no scale or range is read from `was=`; the prescan ignores lists).
- 3 Zero: the carried path is ink of its own dot in the checker (`storm~trail`), as the live path already was.
- Position from structure: `was=` is data in the plot's own units, like `x=`; it is not a placement word.

**Claude's markup before/after.** Before, a turn ended and `<stage>` said
`mark small dot x={clock} y={pow(0.7,clock)} trail=all color=blue label="weights too small"`; the next turn's
frame had no curve. After:

```
mark small dot x={clock} y={pow(0.7,clock)} trail=all color=blue label="weights too small" was=[0, 1, 0.390625, 0.869944, 0.78125, 0.756803, …, 8.90625, 0.0417258]
mark he dot x={clock} y=1 trail=all color=green label=He was=[0, 1]
```

Claude writes nothing new; it may write `was=` itself for a journey already made (a python block's
trajectory), which README now says in one sentence. Writing `-` in the list marks a jump.

## Outside the field

Only the polyline simplifier is borrowed: Ramer–Douglas–Peucker from cartography (generalising a coastline to the
fewest vertices within a tolerance; the standard in GIS and in vector tile pipelines). It transfers directly: a
trail is a line to be kept within a visual tolerance with corners exact, and RDP keeps both ends and every point
that mattered, and is idempotent, which the round trip needs. Rolled by hand (30 lines) rather than taken from a
library, in `markup/viz/` where the project keeps its pure, tested maths.

## Implementation

- `markup/viz/space.hpp`, `space.cpp` (+35): `viz::simplify(pts, eps)` → indices to keep (RDP, iterative).
- `markup/engine.hpp` (+11): `HistSeg` gains `id`, `prop`, `step`, `seq`; `bornSeq_`, `histSeq_`, `curStep_`;
  declarations of `keepsTrail`, `trailSeed`, `trailPath`, `trailCarried`.
- `markup/engine.cpp` (+16/−4): `was` added to `kPositional`/`kTextKeys` (so a stray string value never becomes a
  colour category); `execStep` records the current step; `appendText` attributes a late `with` line to its step;
  `doMark` records the change count at birth; `reset`/`clearStage` clear the new state; `readback` writes `was=`
  last, as it is now, in place of a stored one.
- `markup/trails.cpp` (+200): the two record functions fill the new fields; `trailRuns` draws the carried path
  first (projected like the live path, same width and alpha); `keepsTrail`, `trailSeed` (parses `was=`, `-` is a
  gap), `trailPath` (the walk, the breaks, the reduction), `trailCarried` (the list as written).
- `README.md` (+2): one paragraph in "Trails: where something has been" (part of the prompt).
- `tests/test_viz.cpp` (+25), `tests/test_engine.cpp` (+76): see Tests.
- `chat/prompt.md`, `chat/toolbox.md`, `chat/server.mjs`: unchanged (no new verb; the fold needs no change).

About 370 lines in all, 140 of them tests.

## Tests

New, all failing on the old engine (built and run before the change; `tests/test_engine`, 36 cases):

```
TEST CASE:  a kept trail survives the fold: the readback carries the path, and a scene loaded from it draws it
tests/test_engine.cpp:501: ERROR: CHECK( rb.find("mark storm dot x=-89 y=29.5 trail=all label=Katrina was=[-75, 24, -82, 25]") != std::string::npos ) is NOT correct!
tests/test_engine.cpp:506: ERROR: CHECK( trail->line.size() >= 3 ) is NOT correct!   values: CHECK( 0 >= 3 )
TEST CASE:  a path driven by a param clock is carried as the curve it traced, not as a chord
tests/test_engine.cpp:516: FATAL ERROR: REQUIRE( v.size() >= 12 ) is NOT correct!   values: REQUIRE( 0 >= 12 )
TEST CASE:  a carried path continues: a dot loaded with was= that moves again writes back the old path plus the new
tests/test_engine.cpp:528: ERROR: CHECK( e.readback(false).find("was=[0, 0, 1, 0, 1, 1]") != std::string::npos ) is NOT correct!
TEST CASE:  a jump leaves no line: a set that teleports the dot breaks the carried path
tests/test_engine.cpp:535: ERROR: CHECK( rb.find("was=[0, 0, 1, 1, -]") != std::string::npos ) is NOT correct!
TEST CASE:  in a space the carried path has three numbers a point, and follows the surface it was tied to
tests/test_engine.cpp:547: FATAL ERROR: REQUIRE( v.size() >= 9 ) is NOT correct!   values: REQUIRE( 0 >= 9 )
[doctest] test cases:  36 |  31 passed | 5 failed | 0 skipped
```

(`tests/test_viz.cpp`: "simplify: points on a straight line go, corners stay, and simplifying twice changes
nothing" does not compile on the old engine, the function being new.)

After the change:

```
tests/test_viz    [doctest] test cases:    15 |    15 passed | 0 failed   (18190 assertions)
tests/test_engine [doctest] test cases:  36 |  36 passed | 0 failed   (231 assertions)
```

Each engine case also loads the readback into a second engine and checks `again.readback(false) == rb`, which is
exactly what `--check` does. One iteration during the work: the first version wrote the walked end and the
current position both (`was=[0, 0, 1, 1, -, 5, 1]`); a point identical to the previous one is now not pushed.

## Regressions

All run on the final binary (logs: `test.log`, `check.log`, `golden.log` in this folder).

`make test`:
```
[doctest] test cases:    21 |    21 passed | 0 failed | 0 skipped    (test_place)
[doctest] test cases:    15 |    15 passed | 0 failed | 0 skipped    (test_viz, was 14)
[doctest] test cases:  8 |  8 passed | 0 failed | 0 skipped          (test_script)
[doctest] test cases:  36 |  36 passed | 0 failed | 0 skipped        (test_engine, was 31)
ok … (20 compute checks)                                              exit 0
```

`make check` (scripts, chat/examples, chat/idioms through `--check`, then `node tests/toolbox.mjs`): exit 0,
124 lines `ok`, no FAIL. The toolbox moves with trails ("Repeat until it settles", "Motion and physical systems",
"A surface, and a point that moves on it (3D)") now write `was=` in their readback and still round-trip.

`make golden`: exit 0; every script `same` for both placers (104 `same` lines); means 99.6 (new placer) and
97.4 (old; 97.5 on the first run, within the ±2 tolerance the script has for its time-based sampling). No script
has `was=`, so nothing could change here.

`make append`:
```
DIFFERS: scripts/space-downhill.dan in chunks of 2
DIFFERS: scripts/space-saddle.dan in chunks of 2
DIFFERS: scripts/space-downhill.dan in chunks of 3
DIFFERS: scripts/space-downhill.dan in chunks of 5
append equivalence: 152 identical, 4 different
```
The four known pre-existing ones. Because space-downhill has a kept 3D trail, I diffed what append.sh compares
(`append-diff.sh`, for each of the four): the only line differing is the stale
`# lint: space: a surface is shown but the camera never moves…` comment on the streamed side; the model lines,
including `mark p dot x=0 y=0 z={0.4*(p.x^2 + p.y^2)} trail=all drop=1 label=you was=[-2, -2, 3.2, …,
-0.127985, -0.127985, 0.013104]`, are identical whole vs streamed ("model identical" for all four). So the walk
gives the same path whether the script arrived in one piece or in chunks of 2, 3 or 5 lines.

The folded states themselves: `./tutor scenes/<scene>.new.rb.dan --check` exits 0 for hurricane, descent,
he-init and orbit (decodable: yes, axiom warnings 0, no new lint).

The chat's chained fold (`fold.sh`: fold(fold(t1)+t2) vs fold(t1+t2), with a second turn written for two
lessons): identical for the descent (corners), different text for he-init's curve (the whole fold reduces the
curve with the extent of both turns and picks other sample points; both lists are within tolerance of the same
curve). Noted under Limits; nothing in the chat compares the two.

Time: the readback of the real stress-strain turn (64 lines, three params morphed 12 times, a dot through a path
call) takes 0.13 s wall in all (`/usr/bin/time`), so the walk is not measurable against the engine's start-up.

## Frames

`rt.sh` with the new binary (same procedure as the reproduction; montages in `frames/`):

| scene | old fold, px differing | new fold, px differing | what the frames show |
|---|---|---|---|
| hurricane | 369 | 226 | the two legs of the track are back (`frames/hurricane.montage.png`) |
| he-init | 1767 | 252 | the three curves are back; the residue is sub-pixel along the curves and the "He" label a few px over (`frames/he-init.new.diff.png`) |
| orbit | 750 | 102 | the ellipse is back |
| trails-descent | 19344 | 19528 | the zig-zag is back; the residue is the camera: `camera zoom=2 on=p` re-parsed from the folded state settles on a different view (x from −2.6 instead of −3.1) and the contour labels move with it. Not a trail matter; see Limits |
| space-downhill (3D) | not measured before | 3 | the orange path down the bowl round-trips (`frames/space-downhill.montage.png`) |
| stress (real chat) | not measured before | 9300 | the yellow trail (a vertical segment at ε≈2.6) is in both; the residue is the two views swapping sides in the folded state (the readback declares a view where its first mark is), pre-existing and outside D7 |

I looked at every frame named above. The carried path is drawn with the same width and alpha as the live one,
and the placer keeps names off it (the "He" label moved to clear the carried line, as it would the live one).

## Limits and risks

- **Chained fold ≠ one-shot fold on curves.** The chat only ever folds chain-wise, and the carried part is kept
  verbatim, so the text is stable from turn to turn; but a test of the form `readback(fold(t1)+t2) ==
  readback(t1+t2)` (audit-6 proposed one for views) would fail on a curved trail. Corners (descent, hurricane)
  are identical either way.
- **A whole motion is sampled at 128 points.** One morph that sweeps several turns of an orbit gets coarse (25
  points per turn for ten turns, then the 160-point bound). One turn per morph, the real cases, is smooth
  (50 points for the ellipse at 0.2 %).
- **Changes with no recording are not walked**: a learner dragging a slider (`controls.cpp` sets the value
  directly) moves the dot without a history segment, in the live trail too; the carried path then ends with a
  straight segment from the walked end to the current position.
- **Timing inside a step is not known to the walk**: two morphs started by one step with different durations
  trace a curve live and a straight line in the carry (they are sampled on one parameter). Morphs of different
  steps are sequential in the carry; live they are sequential too when they block (plain `morph`) and
  simultaneous only for consecutive uncued re-marks, which is rare and would show as an L instead of a diagonal.
- **Teleport threshold** is 5 % of the path's extent, the live rule is 40 px: a small `set` jump draws as a line
  in both, a large one as a gap in both, and the two can disagree in between.
- **Text size**: a long trail adds up to about 3 KB to `<stage>` per trailed mark (160 points); typical real
  cases are 2 to 50 points (40 B to 1 KB).
- **Migration**: old sessions' `state.dan` files have no `was=` and load as before (no path, as today); the next
  fold of a turn that moves the dot starts carrying from there. Scripts are unchanged; `make golden` and `make
  append` baselines need no re-save. No change to the server.
- **Found on the way, not fixed (outside D7)**: `e` in an expression is Euler's number before any param is
  consulted (`markup/expr.cpp:184`), so the real stress-strain lesson's param `e` was shadowed and its dot sat
  at ε≈2.59 the whole time (`# value: p.x = 2.59`), with no lint. The carry is faithful to that. Worth a lint
  ("a param named e/pi/tau is shadowed") in whoever owns expressions. Also the descent's `camera zoom=2 on=p`
  does not settle on the same view after a fold, and a folded state declares a view where its first mark is,
  which can swap two views' sides (audit-6's view findings).

## Verdict

**Ship.** The carry makes the readback hold what the stage draws for every kept trail, in 2D and 3D, for literal
steps and for expression-driven paths; `--check`'s round trip holds on every scene with a trail (scripts,
examples, the 31 toolbox moves, the four reproducers and a real chat turn); whole and streamed runs write the
same path; nothing else moved (golden all `same`, append's four diffs are the known stale comment with identical
model lines). The language gains one key, documented in the one paragraph of README Claude reads. The limits
above are approximations inside the drawn tolerance or pre-existing issues named for their owners.
