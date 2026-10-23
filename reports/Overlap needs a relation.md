# Overlap needs a relation

2026-10-23. Sources: `research_notes/Overlap/sources.md`.

## What the learner saw
A pause-and-ask answer showed four overlaps (`chat/sessions/20261022-211954/explain/1`):
- a matrix formula drawn over a plane view;
- the name of row 2's box sitting on row 1, and row 1's on row 2;
- a faint red cross left over the corrected formula;
- a brace name running off the left edge.

The scene was written by Fable at max effort, the same model and setting as the lesson. The system's promise is that overlap can't happen, so why did it?

## Why it happened
The overlap checker and the layout code each had a blind spot:

1. **The checker couldn't see views.** A view's plot, axis lines, title and dividing line were not ink, so a formula on a plot scored 79/100 with no complaint.
2. **The formula band could spill onto a plot.** Above a plot the band was capped at 60% of the height, and the rule that shrinks the stage switched itself off for plots. A taller band just ran into the plot.
3. **Permissions were too coarse.** A box around one row may touch that row, but its *name* was allowed to touch the whole formula, so it landed on another row.
4. **Hidden notes stayed as ghosts** (12% opacity), so a cross over a formula that had since been corrected still said "wrong".
5. **A graph squeezed into half the stage** left no room for the brace beside its column of inputs.

## The rule (from linguistics, cartography and typesetting)
- Engelhardt's syntax of graphics treats touching, containment and superimposition as grammar: they are always read as relations.
- Mackinlay requires a picture to encode only the facts.
- Marks & Reiter call unintended visual relations false implicatures.
- Map labelling (Imhof; Christensen et al.) lets a name belong to one feature only.
- TeX lets boxes overlap only by explicit opt-in, and reports content that doesn't fit instead of drawing it over its neighbours.

So: **two things share ink only when the script relates them**, through `of=`, `on=`/`in=`/`over=`, `at=`, a name and its thing, or data on one plot. A relation to a part covers that part only. This is the founding rule, "position comes from structure", applied to overlap. Axiom 3 in the README now says it.

## What changed (engine)
- **The checker sees every view's frame.** Each piece of ink belongs to a region (the band, or one view), and a frame is ground only for its own region (`Ink::Frame`, `addFrames`, `regionOf`).
- **The band can't spill onto a plot.** The stage is drawn smaller instead, uniformly, so scales and baselines hold.
  - The room the band needs only grows within a scene. Letting it shrink back made blocks flow between rows and column every frame, an oscillation found while testing.
  - With no plot on stage nothing changes. A first version capped every scene's scale and made unrelated scenes smaller; the audit caught it.
- **A note's name stays off the rest of a formula it names a row of.** An upper row's name goes above the formula and a lower row's below, and the band keeps the room. Arrays are unchanged, because their cell notes already sit off the strip.
- **A hidden note fades out completely.** The readback still says `opacity=0.12`, so the scene re-parses the same.
- **A graph leaves room for a named brace on a column,** on the side away from its links.

## Measured
- **The screenshot's scene:**
  - 79/100 with the old checker;
  - 10/100 with the new checker on the old layout;
  - 97/100 worst moment after the fixes, with no label side changes.
- **309 recorded lessons** (worst moment of each, `--layout-report`): 92.2 before, 92.3 after.
  - It's neutral because two views and named row boxes are rare in recorded lessons.
  - 3 lessons scored worse, and none has a new lasting overlap:
    - in one, a note's name crosses a formula during its 0.6 s fade, where it used to stay as a ghost;
    - in one, the same overlaps as before are scored a little higher;
    - in one, a single half-second sample at the end of a transition catches a different moment.
- **Golden layouts (46 scripts, both placers):** unchanged. Tests: 4 new engine cases and 1 checker case, each failing on the old engine.

## Not fixed
- **Names crossing the outlines of shapes and lines** in geometry and plots. This is the largest class, about 110 lessons. Making a crossing cost more in the label placer changed nothing; most of those names are fixed or boxed in.
- **Overlaps that last only during a transition** (a block growing as it pulses, things easing past each other).
- **Last resorts the research recommends:** leader lines, moving a name into the caption when no spot is free, and solving a whole transition at once (Been et al.).
