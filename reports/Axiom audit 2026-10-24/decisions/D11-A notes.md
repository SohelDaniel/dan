# D11-A: the camera on timelines and number lines

Worktree: `/Users/soheldanial/code/server/testing/visulaizations/tutor/.claude/worktrees/agent-a33c4596c97d07376`
(branch `worktree-agent-a33c4596c97d07376`, base commit 6038197). Nothing committed; `patch.diff` is `git diff`
(with `git add -N scripts/geology.dan` so the new script appears), its `--stat`, and `git status --short`. The
untracked `tools/tex/node_modules` is the MathJax symlink the brief asked for. Scenes and frames referred to below
are in this folder (`scenes/`, `frames/`); all commands were run from the worktree root.

## Problem

Reproduced on the unmodified engine (built at 6038197; baseline `make test` ok, `make check` 124 ok, `make golden`
0 WORSE at mean 97.5, `make append` 152 identical / 4 different: the known space-downhill / space-saddle pair).

1. The geology timeline (the auditor's `r14-geology-camera.dan`, copied to `scenes/`):
   ```
   ./tutor scenes/r14-geology-camera.dan --check
   lint: camera: line 10 needs an axes or plane scene (this one is timeline)
   layout score: 96/100
   ```
   The camera line is refused, so the lesson's second half ("zoom in to the last ten thousand years") never
   happens: farming (-10000) and now (0) stay on one dot (`# overlapping: d e` in the readback). The frame
   (`frames/old-r14_end.png`) also shows the float ranges: tick labels `-4499999744 … -499999744`, and the tick
   that should read `0` reads `256` (the drawing loop accumulates `t += st` in float, and -4.5e9 is not a float:
   the nearest is -4499999744).
2. A number line from 0 to a million, then the interval [0, 1] (`scenes/nl-million.dan`):
   ```
   ./tutor scenes/nl-million.dan --check
   lint: camera: line 8 needs an axes or plane scene (this one is numberline)
     layout: dot 'b' overlaps name of 'a'
   layout score: 74/100
   ```
   `frames/old-nl_end.png`: zero and one are one dot under one another's names; `camera fit=a,b` does nothing.

Cause: `Engine::doCamera` (markup/camera.cpp:19) accepted only `axes`/`plane` (and handed `space` to its own
camera). `camApply` was written against `axes_` (a view override of the axes' ranges); the two one-dimensional
schemas map data to the screen through `rm::NumberLine nl_` (`layoutNumberline`, `layoutTimeline`), which the
camera never touched. Their ranges (`nlMin_/nlMax_`, `tlMin_/tlMax_`, the targets, `DataCtx`, and
`rm::NumberLine::min/max`) were `float`; the timeline's ticks were `niceStep` + `fmtNum` with a float accumulator
(render.cpp:807-809), the number line's `NumberLine::draw` the same (raymanim.cpp:540-550).

In the 63 real chat turns Claude wrote 2 camera lines (both `camera spin`), 0 timelines and 1 number line; the
prompt said "Only on `axes` and `plane`" (chat/prompt.md:120) and the toolbox's zoom move said the same, so the
language itself had been telling it not to.

## Designs considered

**A. Re-range the line (let the camera set `nlMin_/nlMax_`, the schema's own range).** The smallest patch: a camera
line on a line schema would write new targets for the eased range. Rejected: it is exactly what axiom 2 forbids
(the schema's scale is the prescan's and never moves); the readback would print a `range=` the author never wrote,
and `camera reset` would have nothing to return to except by remembering the old range: the camera would become a
second owner of the structure instead of a window onto it.

**B. A separate 1D camera (`camApplyLine`), parallel to `camApply`.** Clear separation, about 60 lines repeating
the resolve / path / follow / release logic of the 2D one with one coordinate. Rejected for duplication: the move
logic (van Wijk path in base pixels, automatic duration, critically damped follow with a dead zone, the release
back to the base) is identical; only three things differ (where the base view comes from, where a thing is, where
the view is written back). Two copies would drift.

**C. One camera; a line is a view with one coordinate (chosen).** `camApply` already works in "base pixels"
through `toPx`/`fromPx` and only reads the schema's view at the top and writes it back at the bottom. On a line
the base view is the line's range with a unit y range nobody uses; every thing sits at y = 0; the follow dead zone
then reduces to its x part on its own, and the path's aspect only matters for an in-view test on y that is always
true. The differences are confined to: the base (`nl_.min/max` vs `axes_`), `camPoint`/`camBox` (a thing's value
on the line), the fit (only the width counts; two things frame however close they are, one thing gets a third of
the line as on a plot), the zoom cap (1e9 on a line, which maps in doubles; the float axes keep 1000), the
writeback (`nl_.min/max`), and the two layout functions calling it and culling what leaves the window, as
`layoutAxes` does. New syntax: none. `zoom= on= x= fit= follow= reset` keep their 2D meanings; `y=` on a line is
linted as meaningless.

**Ticks under zoom.** Keep `niceStep` (1-2-5) and accept a pop whenever the step changes while the window moves,
or use the axes' `viz::tickLevels` (the dividing chain 10-5-1-0.5…, the finer level fading in by pixel spacing;
Heer & Robertson, as `docs/plan-camera-fields-trails.md` records). Chosen: `tickLevels`, so the number line and
timeline tick exactly like the x axis, before and during a move. Cost: a line that showed a 2-step shows 5s with
the 1s as unlabelled ticks instead, as the axes already do; I looked at the corpus frames (below).

**Numbers.** `double` for the one-dimensional ranges end to end (`nlMin_…tlMax_`, their targets, `DataCtx`,
`rm::NumberLine`), ticks as `k * step` (never an accumulator), labels through the axes' `tickText(v, step)`
(decimals from the step: a zoomed window reads 1914.5, 1914.6; the number line [0, 1] reads 0, 0.1 … 1.0). The 2D
axes stay float: their mapping (`rm::Axes`) is float end to end, no defect is observed there and its camera is
capped at 1000x, inside float; converting it is a wide mechanical change outside this decision (Limits).

## Chosen design

What a camera line means on a line schema (the world fixed, the window moves):
- `camera zoom=Z on=p`: the window is 1/Z of the schema's range, centred on p's value (`year`/`t`/`at` on a
  timeline, the line's property on a number line). `camera zoom=Z` alone zooms about the current centre;
  `camera zoom=Z x=1990` centres on a value.
- `camera on=q`: pan to q at the same zoom. `camera fit=a,b`: the window frames a and b with the same 56 px margin
  as a plot, however close they are; one thing (or two at one value) gets about a third of the line, as on a plot.
- `camera follow=p`: the window eases after p when it nears the window's edge (the same spring and dead zone).
- `camera reset`: back to the schema's whole range, and the override ends.
- Things outside the window are culled (not drawn, no labels, not in the checker, ignored by `fitStage`), as on a
  plot; the ticks re-tick from the visible window with the dividing chain, numbers with the decimals the step
  needs; the name labels (stacking on a number line, alternating ranks on a timeline) are computed from what is in
  view, so they re-pack as things leave and return, gliding on the placer's springs.
- Readback: the camera line is written back exactly as for a plot (`camera fit=[a, b]`,
  `camera zoom=100000 on=d`), and `--check` re-parses it (the gate that refused it is gone).

Axioms:
- 2 (Structure): kept by construction. The schema's range (`nlMin_/nlMax_`, from the prescan or `range=`) is
  never written by the camera; `camApply` reads it as the base and writes only the window (`nl_.min/max`),
  re-derived from the base every frame. `camera reset` returns to the base because the base never moved; the
  end frame of the geology script is pixel for pixel its first frame.
- 4 (Continuity): the move is the same eased `Active::CAM` and van Wijk path as on a plot (log-linear zoom, a
  pan that zooms out mid-flight when the move is long), with the automatic duration from the path length; the
  ticks cross-fade rather than pop.
- 6 (Decodability): the camera spec is read back and re-parsed (tested through `--check` and a unit test).
- 1, 3, 5: untouched (nothing is deleted; culling is the camera's view, as on a plot; colours and focus unchanged).
- Position comes from structure: nothing new names a pixel; `fit=`/`on=` name things, `zoom=` a ratio, `x=` a value.

Language: no new keys or verbs. README's camera section, chat/prompt.md:120 and the toolbox's zoom move stop
saying "axes and plane only", and the toolbox gains a move ("A long span, then the sliver that matters") that the
toolbox test plays. Before/after for Claude:
```
# before: impossible on a timeline (the camera line is refused, the lesson's second half never shows)
schema timeline
mark d event year=-10000 label="farming"
mark e event year=0 label="now"
Say: "Zoom |in|in to the last ten thousand years."
@in camera on=d zoom=100000        # lint: needs an axes or plane scene
# after: the same line works; or frame the two things directly
@in camera fit=d,e
```

## Implementation

- `markup/camera.cpp` (+40/-27): header comment; `kAxesZoom`/`kLineZoom` caps; `doCamera` accepts `isLine`
  schemas and lints `y=` on a line; `camPoint` and `camBox` return a thing's value on the line (y = 0);
  `lineFrame()` (the camera's frame on a line: as wide as the line and as tall, so a fit of one thing gets the
  same "a third of the view" a plot gives it); `camApply` takes its base from `nl_.min/max` with a unit y on a
  line, uses the right cap, fits by width only on a line (two distinct things frame however close), writes the
  view back into `nl_.min/max`; the follow spring now acts on the displacement to the goal (`v.cx - gx`), so a
  centre at -4.4e9 keeps its digits through the float spring (no behaviour change on plots: the same equation).
- `markup/layout.cpp` (+40/-33): `isDataSchema` via `isLine`; `listRange` as a template (floats for axes, doubles
  for lines); the number-line and timeline branches of `computeRanges` in doubles with 1e300 sentinels (the old
  `1e9f` sentinel would also have failed a timeline whose smallest year is above 1e9) and `std::lerp`;
  `layoutNumberline`/`layoutTimeline` call `camApply(lineFrame())` after setting the base and mark what leaves
  the window `culled` through `offLine`; `lineValue` (one definition of "a thing's place on the line", used by
  both layouts and the camera); `fitStage` skips culled things (otherwise a dot a million pixels off would shrink
  the stage).
- `markup/engine.hpp` (+14/-6): `isLine`; the 1D ranges, targets and `DataCtx` fields in `double`; declarations.
- `rm/raymanim.hpp/.cpp` (+66/-32): `niceStep`/`niceRange` in double with float wrappers; `rm::Tick`;
  `NumberLine` in double (`step` field dropped: nothing set it), `ticks(minPx = 90)` from `viz::tickLevels`
  (ticks are `k * step`, labels from `tickText`), `draw` through `ticks`.
- `markup/render.cpp` (+4/-5): the timeline's furniture draws `nl_.ticks()` with the fade.
- `tests/test_engine.cpp` (+63): four cases (below). `scripts/geology.dan` (new): the regression script for
  check/golden/append; its baseline added by hand to both `tests/golden/*.json` (no `--save`, so no other entry
  moved). `README.md`, `chat/prompt.md`, `chat/toolbox.md`, `docs/HANDOFF.md`, `docs/history.md`: the language,
  the map and the log.

About 320 lines changed in all, 100 of them documentation and tests.

## Tests

`tests/test_engine.cpp`, four new cases:
1. "camera on a number line: fit opens the window on the things named, the rest leaves the view, the range stays"
2. "camera on a timeline: zoom on a thing separates what sat on one dot; reset returns to the whole line"
3. "camera follow on a number line: the window goes after a thing that moves"
4. "a number line keeps its range exactly: a 4.5-billion-year timeline is not -4499999744" (the exact range, the
   exact position of 0, ticks as multiples of the step with the one at 0 reading "0"; the `ticks()` part was
   added with the implementation, since the method did not exist before)

On the old engine (tests appended, nothing else changed):
```
tests/test_engine.cpp:487: ERROR: CHECK( !refused ) is NOT correct!                values: CHECK( false )
tests/test_engine.cpp:492: ERROR: CHECK( b->box.x - a->box.x > 300 ) is NOT correct!  values: CHECK( 0.00102234 >  300 )
tests/test_engine.cpp:493: ERROR: CHECK( body(p, "c") == nullptr ) is NOT correct!  values: CHECK( 0xbd31d0120 == nullptr )
tests/test_engine.cpp:496: ERROR: CHECK( rb.find("camera fit=[a, b]") != std::string::npos ) is NOT correct!
tests/test_engine.cpp:508: ERROR: CHECK( now->box.x - d->box.x > 150 ) is NOT correct!  values: CHECK( 0.0020752 >  150 )
tests/test_engine.cpp:509: ERROR: CHECK( body(p, "a") == nullptr ) is NOT correct!
tests/test_engine.cpp:525: ERROR: CHECK( body(pic, "z") == nullptr ) is NOT correct!
tests/test_engine.cpp:532: ERROR: CHECK( nl.min == -4500000000.0 ) is NOT correct!  values: CHECK( -4.5e+09 == -4.5e+09 )
[doctest] test cases:  35 |  31 passed | 4 failed | 0 skipped
```
(the last line's "values" print the float -4499999744 rounded; the comparison is false.)

A first version of the fix still failed case 1 twice, usefully: the 2D fit's "at least a third of the view"
rule treated two things 0.001 px apart as one point (zoom 2.7), and then the fit's height term (a dummy on a line)
capped it at 2.7 again. Both are what the line-specific lines of the fit now handle.

After:
```
[doctest] test cases:  35 |  35 passed | 0 failed | 0 skipped
[doctest] assertions: 210 | 210 passed | 0 failed |
```

## Regressions

All on the final build (logs `new-*.log` in this folder; baseline logs `base-*.log`).

`make test` (all four binaries + compute.mjs):
```
[doctest] assertions: 35525 | 35525 passed | 0 failed |   (place)
[doctest] test cases:    14 |    14 passed | 0 failed    (viz)
[doctest] test cases:  8 |  8 passed | 0 failed          (script)
[doctest] test cases:  35 |  35 passed | 0 failed        (engine, 210 assertions)
exit 0
```
`make check`: 126 lines "ok", none "FAIL" (baseline 124: + `scripts/geology.dan`, + the new toolbox move);
`node tests/toolbox.mjs`: 32 moves ok, 0 failed.

`make golden` (both placers): no WORSE, no ERROR; 53 scripts (52 + geology):
```
scripts/geology.dan                96.0    96.0    97.7    0      0      0.50   0.16      (no baseline yet)
scripts/histogram.dan              100.0   100.0   100.0   0      0      0.38   0.12   same
scripts/mean.dan                   100.0   95.3    99.9    1      0      0.12   0.19   same
scripts/timeline.dan               96.0    96.0    96.0    0      0      0.52   0.16   same
mean layout score 99.6 over 53 scripts      (new placer; baseline 99.6 over 52)
mean layout score 97.5 over 53 scripts      (old placer; baseline 97.5 over 52)
```
The two 96.0 (geology, timeline) are the timeline layout's own far-reach category notes ("category of 'c' is
124px from dot 'c'"), present before this change. I then added geology's numbers to both golden files by hand
and re-ran `node tests/golden.mjs`:
```
scripts/geology.dan                96.0    96.0    97.7    0      0      0.50   0.16   same
mean layout score 99.6 over 53 scripts
exit 0
```

`make append`:
```
DIFFERS: scripts/space-downhill.dan in chunks of 2
DIFFERS: scripts/space-saddle.dan in chunks of 2
DIFFERS: scripts/space-downhill.dan in chunks of 3
DIFFERS: scripts/space-downhill.dan in chunks of 5
append equivalence: 155 identical, 4 different
```
The same four as the baseline (152 identical, 4 different; the three new identical ones are geology in chunks of
2, 3 and 5): the known stale space lint, unrelated.

A clean rebuild of the engine after `touch` on the changed files printed no warning in camera.cpp, layout.cpp,
render.cpp, raymanim.cpp/hpp or engine.hpp.

Decodability on the reproducers (new engine): `scenes/r14-geology-camera.dan` decodable, 0 axiom warnings,
100/100; `scenes/nl-million.dan` decodable, 100/100 (was 74 with "dot 'b' overlaps name of 'a'");
`scripts/geology.dan` decodable, 96/100 (the far-reach notes).

## Frames

`frames/`, all 1280x720 from `--snapshot` (offscreen, so not high-DPI; nothing here touches the drawing
transform). Old engine: `old-r14_end.png`, `old-nl_end.png`. New engine:
- `new-geo_1.png`: the whole line, ticks `-4500000000 … -500000000, 0` (exact; `0` is partly under the farming
  leader, as before), farming and now on one dot, as the sentence says.
- `new-geo_6.5.png`: the first sentence still up (estimated timing at 170 wpm, no voice): identical to 1 s.
- `new-geo_7.5.png`: mid-move, anchored on the target: the window about 750 million years wide, major ticks every
  100 million with the finer 50-million ticks fading in, Earth forms and first life culled (no dots, no labels),
  the three visible events re-ranked (now above, farming below), the caption "camera -> the view moves to what
  the voice is about (zoom is a pointer)".
- `new-geo_9.png`: landed: farming (-10000) and now (0) at the 56 px margins, ticks every 1000 years
  `-10000 … -1000, 0`, each label on its own side. This is the frame the lesson needed.
- `new-geo_end.png`: after `camera reset`, pixel for pixel the 1 s frame (the world never moved).
- `new-nl_3.6.png`: the first sentence still up: zero and one stacked on one spot, a million at the right,
  ticks every 100000 as before.
- `new-nl_end.png`: the window [0, 1]: zero at the left margin, one at the right, ticks `0, 0.1 … 1.0`, the
  million culled.
- Unchanged scripts, for the new tick chain: `new-timeline_end.png` (the road to 1914: yearly ticks 1905 … 1915,
  the same picture as before); `new-mean_end.png` (labels 10 … 90 every 10 as before, now with unlabelled 5s
  between them); `new-histogram_end.png` (a tick at every integer bin, labels at 5, 10, 15 where before every
  second integer was labelled). I judge the last two as no worse; the reviewer may prefer the old labelling on
  the histogram, in which case the number line could ask `ticks()` for a smaller `minPx`.

## Limits and risks

- Not in the checker: the tick numbers of number lines and timelines were never furniture for the placement
  checker (`furnitureHere` returns nothing for them) and still are not; a culled thing is correctly absent, but a
  name label could sit on a tick number without a lint. Listed in HANDOFF §9.
- The 2D axes' ranges (`axX0_…`, `rm::Axes`) stay `float`; only the lines are `double`. A plot with coordinates
  in the billions would still show float ticks. The camera's zoom cap of 1000 on plots is unchanged.
- Tick labels have no thousands separators (`-4500000000`), as before on the axes; at the full view the ten
  labels are 108 px apart and read, but only just.
- The follow spring's velocity is still a float in data units; the position is now computed as goal plus a
  displacement, so following far from zero is stable. Not exercised on a billion-year line beyond the unit test
  on a 0..100 line.
- Behaviour change on scenes without a camera: the 1D ticks use the dividing chain (1-5-10 with the finer level
  faded), so some lines show different tick labels than before (histogram: 5/10/15 instead of every 2). Golden
  scores do not see ticks; the frames above are the evidence. Scripts and chats need no migration: no syntax
  changed, and every existing line reads back the same (`make append` identical for all 1D scripts).
- The timeline's label ranks are recomputed from what is in view, so during a long pan labels can swap sides as
  things enter and leave; the placer's springs glide them, but a fast pan across many events will look busy. Not
  observed in the geology frames (two to three events in view at a time).
- `camera zoom=` without `on=` on a line zooms about the current centre, as on a plot; a first `camera zoom=10`
  on an untouched line therefore centres on the middle of the range, which may not be where the author looked.
  The README's examples all give `on=` or `fit=`.
- The real-chat evidence is thin (no timeline in 63 turns): whether Claude now writes these moves is a prompt
  question the toolbox move and README sentence address, not something I measured with an eval run.

## Verdict

Ship. The decision's four parts are done and tested: the camera works on number lines and timelines with the 2D
camera's own code and vocabulary (no new syntax), the window is a view override and the schema's range never
moves (axiom 2 by construction; reset returns to the identical frame), the move is the same eased van Wijk path
with cross-fading ticks (axiom 4), the spec reads back and re-parses (axiom 6), the 1D ranges and ticks are exact
in doubles, and every regression is green at the baseline's numbers. The one visible side effect on existing
scenes is the tick chain on number lines (now the same as the axes'), shown in the frames; if the reviewer wants
the old 1-2-5 labels on static lines, that is a one-parameter change in `NumberLine::ticks`.
