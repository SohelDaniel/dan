# 3Blue1Brown's layout and pacing habits, measured

Mined on 2026-10-20 from the `3b1b/videos` source: 432 files, about 516k lines, parsed with Python `ast` and not run.
The numbers were verified four ways:

- a second, independent text-search count, which matched exactly once commented-out code was excluded;
- the spacing constants checked against the manim versions each era used;
- 40 random call sites checked by hand, all 40 correct;
- manimgl 1.7.2 installed and run, measuring real gaps.

Units: the frame is 8 units tall, so **1 unit = 90 px at 720p** (measured). Every gap is edge to edge.

## Placement

- **Relational, not absolute.** About 15.3k relational calls (`next_to`, `arrange`, `align_to`) against about 10.2k
  absolute ones (`move_to`, `shift`).
- **`next_to`** (n = 10,966):
  - Direction: below 30%, right 24%, up 23%, left 12%, diagonals 7%.
  - Gap: 0.25 units (22 px) is the default and the median; 0.1 (9 px) in 20% of calls; 0.5 to 1.0 (45 to 90 px)
    to separate blocks.
- **Screen edges and corners:** 0.5 units (45 px) from the edge, 73 to 87% of calls. `to_edge(UP)` is half of all
  edge placements, and 75% of what goes there is text (titles, equations). The bottom corners mostly hold characters.
- **`arrange`:** rows 57%, columns 40%. The gap is 22 px, with 68 to 90 px when spreading things out. Lists go
  down, left-aligned.
- **Labels:**

  | label on | where | gap |
  |---|---|---|
  | dot | up-right | about 9 px |
  | arrow | above or at the tip | 9 to 22 px |
  | box | outside, above or below | 22 px |
  | brace | outside, above or below | 22 px |

## Sizes

- **Fonts:** median `font_size` 36. Tex is about 42, axis numbers 16 to 18, headlines 60 to 72.
  - Measured: `font_size` F gives a digit about 0.65F px tall at 720p.
  - In our pixels: labels 17 to 22 px and formulas 38 px already match. A headline tier of about 60 to 65 px and
    axis numbers of about 14 to 16 px are missing.
- **Strokes:** lines about 1.8 px, arrows about 4.5 px, the default stroke 3.6 px.
- **Dots:** radius 7 px (84% use the default).
- **Axes:** set to 50 to 75% of the frame height, or the default that fills the frame.

## Pacing

- **A typical step** is about 1 s, then a 1 s hold. When he slows down it's to 2 or 3 s. Waiting is 38% of
  scene time.
- **Durations by animation:**

  | animation | usual length |
  |---|---|
  | `FadeIn` | 1 to 2 s |
  | `Write` | 1 s (2 s when long) |
  | `FadeOut` | 0.6 to 1 s |
  | `Transform`, `TransformFromCopy` | 2 s |
  | equation morphs (`TransformMatchingTex`) | 1 s |

- **Entrances:**
  - 30 to 40% of `FadeIn`s (2019 on) rise into place from below, by a median of 1 unit (90 px); the 25th
    percentile is 0.5 units (45 px).
  - Group entrances are staggered: lag 0.1 for fades, 0.2 to 0.25 for distinct items, 0.5 for one after another.
  - About 12% of steps are staggered.
- **Easing:** smooth in 98% of steps.
- **What he uses most** (2023 on, per 100 steps):
  - moving things (`.animate`) 52, `FadeIn` 32, `FadeOut` 28, `Write` 14, `LaggedStart` 14, `TransformFromCopy` 9;
  - emphasis effects (`Indicate`, `FlashAround`) under 2.

## Clutter

- A scene is short (median about 8 s, 3 steps) and ends with about **3 to 6 things on screen**.
- He clears by cutting to a new scene; `clear()` appears in only 3% of scenes.

## Colour

- **Usage share:** yellow 21%, blue 17%, white 14%, red 13%, green 9%, grey 8%, teal 5%, pink 3%.
- **Roles:**
  - yellow = emphasis
  - blue = the main object
  - red and green = the contrast pair
  - grey = de-emphasis
- **Formula symbols coloured like their objects** (`t2c`, `set_color_by_tex`): about 4 per file in every era.
