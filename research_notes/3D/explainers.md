# 3D in explainers: manim, 3Blue1Brown, math tools, perception (2026-10-23)

**[R]** marks claims recalled but not checked against the source.

## Manim and 3Blue1Brown (checked in the source code)

- **ManimCE.**
  - Default angled view: phi=70°, theta=−135°.
  - Ambient rotation: `begin_ambient_camera_rotation(rate=0.02)`, about 1.15°/s.
  - Labels: `add_fixed_in_frame_mobjects` gives screen-space text; `add_fixed_orientation_mobjects` gives billboards.
- **ManimGL.**
  - `frame.reorient(theta, phi, gamma)`, in degrees; the vertical field of view is 45°.
  - Ambient rotation: `add_ambient_rotation`, 1°/s by default.
  - Surfaces are depth-tested with shading (0.3, 0.2, 0.4); other objects are drawn in painter's order unless `apply_depth_test()` is called.
- **3b1b/videos, measured over 647 hardcoded camera reorientations.**
  - phi: mean 72.6°, mostly 60–89°.
  - theta: clusters at 0 and ±15–30°.
  - Ambient rotation: 0.5–4°/s, left running through narration (21 starts, 15 stops).
  - Explicit moves: usually 2–5 s; half turns over 10–12 s (about 15–18°/s).
  - Usage counts: `fix_in_frame` 331, `set_backstroke` 328 (dark outline behind text), GlowDot 249, Sphere 108, ThreeDAxes 92, Surface 78, SurfaceMesh 64, Cube 55.
- **Videos with heavy 3D use:**
  - Essence of Linear Algebra ch. 5, 6, 10, 11 (transformations, determinant as volume, cross product) [R];
  - shadows 2021, holograms 2024, inscribed_rect 2024, transformers/embedding 2024, quaternions 2018, sphere_area 2018, convolutions2 2023, laplace 2025.

## Other tools

- **Desmos 3D.**
  - Default view: a box from −5 to 5, rotated slightly so two sides show.
  - Rotation is never automatic; a drag released while moving keeps spinning.
  - "Default orientation" and "XY orientation" buttons; perspective can be turned down to orthographic.
- **GeoGebra 3D.** A reset-view button, `SetViewDirection` (look along a vector or at a plane), a choice of projection.
- **Plotly** [R]. Default eye at (1.25, 1.25, 1.25), z up, text billboarded.
- **MathBox (Wittens).** Operators are learned by watching graphs morph and fold; labels are screen-space text anchored at 3D points.

## Perception

- **Munzner, "No unjustified 3D".** 3D is justified only for shape perception of spatial data. Otherwise occlusion, perspective distortion and tilted text all cost the reader.
- **Ware** [R].
  - Motion parallax and the kinetic depth effect (structure seen from slow rotation) are among the strongest cues for structure.
  - Occlusion is the strongest ordering cue.
  - Shading shows the shape of surfaces; drop lines and shadows show height above a plane.
- **Hairy Slices II.** Arrows in 3D need 3D heads (cones), not flat ones.
- **Labels in 3D illustrations** (Götzelmann, Hartmann et al.; Bekos et al. 2019 survey).
  - Labels sit inside things when there's room, outside with leader lines otherwise.
  - The layout must stay coherent from frame to frame.

## LLMs writing 3D

- **TheoremExplainAgent.** Most videos have minor layout issues.
- **CinemaTraj.** Camera paths written by LLMs were occluded 50–58% of the time; small composable camera moves helped.
- **Typical failures:** mixing up angle conventions and units; edge-on or from-below views; labels hidden behind surfaces; text pasted flat onto planes; too much rotation.

## What we take

- **When to use it:** 3D only when the idea is spatial.
- **Data and camera:** positions come from data, and the camera is moved by named views, never by angles in the script.
- **Labels:**
  - text always faces the screen, with a dark outline behind it;
  - formulas stay in the band;
  - leader lines when a label's anchor is hidden.
- **Default view:** phi about 70°, about 25° around from the front, z up, a narrow field of view.
- **Rotation:** slow ambient rotation (about 1.5°/s) while a surface or field is the subject, eased; still while formulas are read.
- **Drawing:**
  - surfaces depth-tested with light shading, a mesh on top, and translucency when something sits behind them;
  - drop lines down to the floor plane give height for free.
