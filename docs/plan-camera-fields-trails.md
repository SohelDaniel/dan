# Plan: camera, fields, trails (2D, before 3D)

Research notes are summarised here; sources at the end. Principle as before: Claude says *what* (zoom on this,
a field of this, trail behind that); the engine decides pixels, timing curves, densities and colours.

## Findings that shape the design

- **One mapping.** Every world→screen conversion goes through `rm::Axes axes_` (set in `layoutAxes`). A camera is a
  *view-range override* there, not a raylib `Camera2D`: labels, the placer and the checker then agree with the pixels,
  and strokes/text stay screen-size for free (manimgl's model: strokes in screen units, labels fixed in frame).
  Manim CE and Motion Canvas animate width/scale linearly, which crawls then lurches; animate **log(width)**.
- **Zoom transitions:** van Wijk & Nuij (2003), as in d3 `interpolateZoom`, ρ = √2; duration `clamp(S/0.9, 0.6, 2.0)` s
  (their users preferred V ≈ 0.9). Short moves: eased lerp of centre and log w. Following: critically damped spring
  (Holden) with a dead zone (Cinemachine). Ticks re-tick from the visible range and **cross-fade** between nice steps
  by pixel spacing (Heer & Robertson: gridlines should move and fade, not pop). Curves are re-sampled over the visible
  range, so "zoom until it's straight" is real, not an artefact.
- **Fields:** Laidlaw et al. 2001: streamlines beat arrow grids for every task; sign/direction and critical points
  must be visible. manimgl arrow length `L·tanh(|v|/L)` (zero vectors vanish, critical points stay visible) — not
  manim CE's sigmoid. Even spacing: Jobard & Lefer 1997 (d_test = 0.5 d_sep). Flow: "passing flash" along
  precomputed streamlines (cheap) rather than framebuffer fading (smears labels, frame-rate dependent). RK4, not
  Euler (orbits spiral). Directions/lengths in **screen space** (unequal axis scales).
- **Contours:** marching squares with linear edge interpolation (d3-contour), saddles by centre value (no flicker
  while animating), nice levels (~8, every 5th bold), levels **locked** during a morph. Labels: matplotlib
  `locate_label` (straightest block, upright, gap in the line).
- **Heat maps:** perceptually uniform maps only (Crameri 2020: rainbow distorts by >7%): viridis for f ≥ 0,
  a dark-centred diverging map (berlin-like) when f changes sign; 60–75% opacity so text keeps contrast.
  CPU grid → texture, bilinear; 160×90 while animating, 320×180 static.
- **Expressions are the bottleneck:** tree walk with string lookups per node. Fields need a **compiled** form with
  slots for x, y (and t), engine names folded to constants once per frame.
- **Trails:** manim/Unity accumulate per frame → frame-rate dependent, wrong after skip, empty in snapshot/instant
  runs. Desmos/GeoGebra/orbit sims **recompute** from the motion. Our motion is morphs (known start, duration, ease)
  and expressions of params, so we record each numeric animation as a segment and evaluate a mark's position at any
  past time s exactly; sample on an absolute time grid (no shimmer) plus segment boundaries (exact corners).
  Render as our own triangle strip with per-vertex width/alpha, limited miters (≤2) and a 1 px feather for AA
  (raylib's thick lines have gaps/spikes and a single colour; snapshots have no MSAA).

## Milestones (each: tests, snapshot inspection, `make test/check/golden/append` green, commit)

0. **Foundations.** `expr` compiler to a flat stack program (slots x,y,t; constants folded; falls back to the tree
   when an engine call can't be folded). `rm::strip()` — tapered, fading, feathered polyline. Pure algorithms in a
   raylib-free module `markup/viz/` (zoom path, spring, nice steps, marching squares, RK4, streamline placement,
   colormaps) with doctest unit tests.
1. **Camera** (axes/plane). `camera zoom=<n> on=<id> [x=<data>]`, `camera on=<id>` (pan), `camera fit[=a,b]`,
   `camera follow=<id>|none`, `camera reset`, optional duration. View override in `layoutAxes`; tick cross-fade;
   off-view marks and labels culled; readback emits the camera; `--check` round-trips. Exit: derivative zoom scene
   reads well at start/mid/end frames; no label overlaps during the move (layout report sampled through it).
2. **Fields.** Kinds `field` (`vx= vy=`, `style=arrows|lines|flow`), `slope` (`fn=` dy/dx, `solutions=`),
   `heat` (`fn=`, `cmap=auto|viridis|diverging`), `contour` (`fn=`, `levels=auto|n|log`, labels), and `grad(f)`
   helpers. Fields morph through params. Exit: ODE, potential and loss-landscape scenes at 60 fps
   (bench), snapshots inspected, decodable.
3. **Trails.** `trail=1.5s` on any moving mark on axes/plane, `mark tr trace of=p` (persistent), deterministic
   (snapshot at t equals live), sampled per research constants. Exit: orbit, projectile and gradient-descent scenes;
   snapshot at the same t is identical whether reached in one run or after appends.
4. **Language & teaching.** README language section (feeds the prompt), prompt.md picture table (fields for ODEs /
   physics, contour + trail for optimisation, camera for "zoom in until it's straight"), one exemplar, scripts added
   to `make check`, golden baselines, an eval round on relevant prompts.

## Sources

van Wijk & Nuij, *Smooth and efficient zooming and panning* (2003); d3-interpolate `zoom.js`; d3-array `ticks.js`;
Heckbert *Nice numbers*; Talbot, Lin, Hanrahan (2010); manim CE `moving_camera.py`, manimgl `camera_frame.py`,
`stroke.wgsl`; Motion Canvas `Camera.ts`; Unity Cinemachine composer/group framing; Holden *Spring-It-On*;
Heer & Robertson, *Animated transitions in statistical data graphics* (2007); Bret Victor, *Up and down the ladder
of abstraction*; Laidlaw et al., *Quantitative comparative evaluation of 2D vector field visualization methods* (2001);
Jobard & Lefer (1997); manimgl/manim CE `vector_field.py`; mapbox webgl-wind; cambecc earth; d3-contour;
matplotlib `contour.py`; Crameri et al., *The misuse of colour in science communication* (2020); Distill,
*Why momentum really works*; Li et al., *Visualizing the loss landscape* (2018); manim `TracedPath`/`TracingTail`;
Unity TrailRenderer; Godot Line2D; raylib `rshapes.c`; DesLauriers, *Drawing lines is hard*;
Yuksel et al. (centripetal Catmull-Rom); Cutting, *Representing motion in a static image* (2002).

## Status (2026-10-19)

All four milestones done. Tests: `make test` (placement + viz/expr unit tests), `make check`, `make golden`
(99.9 new placer), `make append` (63/63). Eval on six prompts that call for the new tools (tools/eval/prompts-2d.json):
6/6 decodable, 0 axiom warnings, geometric layout 98.6–100, and Claude chose the intended tool each time
(slope+solution for cooling, camera for the derivative, heat+contour+trail for descent, field for charges,
trails for orbit and projectile). Fixes found by testing: anchored zoom (the target stays in view), a dividing tick
chain (no popping), fixed items never separated (axis bands crossing at a centred origin), overlapping re-marks of
one value in instant mode, plates under formulas and legends over fields.

Not done: the vision judge on these frames; contour labels and field legends are not yet in the layout checker;
camera on free/graph/timeline schemas (pan along long arrays) — only axes and plane.
