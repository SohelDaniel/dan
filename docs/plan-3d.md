# Plan: 3D

Research: `research_notes/3D/explainers.md` (manim, 3Blue1Brown's measured habits, math tools, perception, LLM-written 3D).

The same principle as 2D: Claude says *what*, and the engine decides the pixels, the timing and the camera numbers.
Two rules from the user:
- **Parity.** Everything 2D has works in 3D: the verbs, notes, names, pointers, `{expr}`, pointing ahead, Python
  data, the toolbox moves.
- **Teaching need.** 3D is chosen only when the idea is spatial, never by default.

## Findings that shape the design

- **One mapping, on the CPU.** A 3D view projects every point through one function (orbit angles, a narrow
  perspective) to the screen, and everything is drawn as 2D strokes and filled triangles, as now.
  - The label placer, the overlap checker, `--check`, snapshots, `indicate`/`send`/notes and pointing then work on
    projected positions unchanged.
  - raylib's GPU 3D mode would break snapshots (render targets don't nest), drop the Retina/zoom transform, draw
    1 px lines on macOS and need a GL context for `--check`.
  - This is ManimCE's model too (painter's order with depth sorting).
- **Depth by sorting.** Surfaces are triangles. Curves, vectors and the mesh are cut into short pieces. All of them
  are sorted back to front each frame (about 7k items, well under a millisecond).
  - Surfaces are slightly translucent, so what is behind them shows faintly: occlusion reads, and nothing is lost
    (identity).
  - Light Lambert shading, as in 3Blue1Brown's surfaces: light fixed relative to the camera, so a turn doesn't flicker.
- **Text is never in the 3D plane.** Names, numbers and formulas are upright screen text at projected points, with a
  dark outline (3b1b's `set_backstroke`, 328 uses). Formulas stay in the band (`fix_in_frame`, 331 uses).
- **The camera is said in words.** Views: `default`, `top`, `front`, `side`. Turns: `orbit=<degrees>`. Slow ambient
  rotation: `spin` / `spin=off`. Plus the 2D verbs (`on=`, `zoom=`, `fit=`, `follow=`, `reset`).
  - No angles in the script: LLMs mix up conventions, and their camera paths were occluded half the time (CinemaTraj).
  - Default view: tilt φ = 70° (3b1b's mean is 72.6°), azimuth 30° so x comes toward the viewer on the left, y goes
    right, z is up (right-handed).
  - Spin is 1.5°/s, eased in and out, kept through narration as 3b1b does. Explicit turns take 2–4 s, about 15°/s
    for big ones.
  - Never below the floor, never edge-on to the floor.
- **Shape reads from motion** (the kinetic depth effect; Ware). A static 3D surface is ambiguous, so a lint asks for
  `camera spin` or an `orbit` when a surface is shown and the camera never moves.
- **Height reads from drop lines** (Ware): `drop` on a dot draws a dashed line to the floor and a shadow.
- **Fitting is rotation-invariant.** The scale comes from the bounding sphere of the range box, so an orbit never
  makes the scene breathe.
- **Units:**
  - `space` is a world: equal units, so spheres are round and right angles are right, which linear algebra needs.
  - When the z range is far from x/y (a loss surface), each axis is scaled to a cube like Desmos's box, and the axes
    are numbered so the reader knows.
  - `units=equal|box` overrides the choice.

## The language (README; it feeds the prompt)

```
schema space x=[-3,3] y=[-3,3] z=[-2,2]                 a 3D world, z up (grid=false, axes=false, units=box)
mark p dot x=1 y=2 z=1 drop label="p"                    a point; drop: a dashed line to the floor and its shadow
mark v vector x=0 y=0 z=0 dx=1 dy=2 dz=2 label="v"       cone-tipped; to=[x,y,z] | to=<id>; at= rides along
mark L line pts=[0,0,0, 2,1,3]                           a segment (arrow: with a head)
mark h path xt={cos(t)} yt={sin(t)} zt={t/4} t=[0,12]     a curve in space
mark S surface fn={x^2 - y^2} label="saddle"              z = f(x, y) over the x, y range
mark T surface xt={...} yt={...} zt={...} u=[0,6.28] v=[0,3.14]   a parametric surface (sphere, torus, ...)
mark P plane normal=[1,1,1] d=1                           ax + by + cz = d, clipped to the box
mark B sphere x=0 y=0 z=0 r=1
mark F polygon pts=[0,0,0, 1,0,0, 0,1,0]                  a flat face (x,y,z triples): tetrahedra, prisms
mark G grid space m=[1,0,0, 0,1,0, 0,0,1]                 the unit cube and lattice under a 3x3 matrix; morph m
mark E field vx={-y} vy={x} vz={0.3}                      arrows in space (cones), sparse
camera view=top | front | side | default                 named views
camera orbit=90                                           turn around the vertical axis
camera spin | camera spin=off                             slow ambient turn
```

Everything else is unchanged and works in space:
- `morph p.z -> 3`; lists morph (`pts`, `m`, `to`);
- `trail=` (3D history), param clocks, `{expr}`, Python data;
- `indicate`/`send`/`focus`/`hide`, notes (`enclose of=p`, `brace of=[a,b]`), `at=` pointers, `[id]` in a Say;
- views side by side (a surface beside its contour plot);
- `heat`/`contour` on a `plane` view next to the `space` view.

## Teaching (prompt and toolbox)

- **A row in the picture table:** use `space` when the idea is spatial. Examples: surfaces z = f(x, y), gradients
  and tangent planes, cross product, determinant as volume, 3D linear maps, rotations in space, solids, molecules,
  crystal lattices, loss surfaces of two weights.
- **Otherwise stay flat:** a 2D picture is clearer for anything that is not spatial (Munzner: no unjustified 3D).
- **Toolbox moves:**
  - a surface with a point riding it (gradient descent);
  - a cross product with its parallelogram;
  - determinant as volume (a grid morph);
  - a plane and its normal;
  - a parametric curve drawn by a moving point;
  - surface + contour views side by side.

## Milestones (each: tests, snapshots inspected, `make test/check/golden` green, commit)

1. **Projection core.** `viz::Space` (pure, unit tested): orbit angles, perspective, rotation-invariant fit,
   project → screen + depth, unproject for picking. Named views, eased transitions.
2. **Schema plumbing.**
   - `space` in the parser.
   - `isSpace`; data views and side-by-side views; ranges with z (sampling surfaces and 3D curves).
   - `z dz zt …` are positional, never categories or magnitudes.
   - Auto schema when z is used.
   - `clearStage`, readback (the camera's spec).
3. **Things.** dot, vector, line/arrow, path, surface (fn and parametric), plane, sphere, polygon, grid (3x3), drop
   lines, trails. Depth-sorted drawing, shading, mesh lines, the axes box with ticks and names.
4. **Camera.** `view=`, `orbit=`, `spin`, and the 2D verbs projected; the instant/shadow engine resolves them at
   once; spin lint.
5. **Parity with 2D.**
   - Checker: a space view's frame, projected boxes, surfaces as ground.
   - Label placer: projected anchors and strokes.
   - `zeroCheck` and readback overlaps use 3D distance.
   - Binding: z coordinates and the "z" symbol.
   - Pointing ahead, notes, `send`, `focus` on 3D things.
6. **Fields in space** (sparse cones, `solution of=` in 3D).
7. **Teaching.**
   - README, prompt row, toolbox moves (checked by `tests/toolbox.mjs`), a script in `make check`.
   - Eval on `tools/eval/prompts-3d.json` against the baseline: does Claude choose 3D for the 24 spatial questions
     and stay flat for the 12 others; claims shown; judge scores; frames read by eye.
