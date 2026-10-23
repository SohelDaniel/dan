# When may two things overlap? Sources (2026-10-23)

Gathered for `reports/Overlap needs a relation.md`. How well each was checked:
- **Read in the source:** Engelhardt's glossary, the Bluefish paper, Penrose's docs, Been et al.'s rules.
- **From memory, not re-checked:** Mackinlay, Palmer, Kosslyn, Christensen et al., Imhof, Dwyer et al., TeX, CSS, the practitioner tools.
- **Abstract only:** Marks & Reiter.

## Graphics as a language
- **Engelhardt, *The Language of Graphics* (PhD thesis, U. Amsterdam, 2002)**, and **Engelhardt & Richards, "A Framework for Analyzing and Designing Diagrams and Graphics" (Diagrams 2018).**
  - The basic syntactic structures are positioning in a meaningful space, clustering, separation, lineup, linking by a connector, containment and **superimposition**.
  - A label is anchored to its object by clustering, containment, superimposition or a connector.
  - So an overlap is always read as one of these structures, which makes a label on the wrong part a parse error.
- **Mackinlay 1986, "Automating the Design of Graphical Presentations of Relational Information" (ACM TOG 5(2)).** Expressiveness: a picture encodes all the facts and *only* the facts. An undeclared overlap breaks "only".
- **Marks & Reiter 1990, "Avoiding Unwanted Conversational Implicatures in Text and Graphics" (AAAI-90).** Generated graphics must avoid unintended implicatures, the visual version of misleading wording.
- **Shimojima** (free rides; *Semantic Properties of Diagrams*, 2015) and **Stenning & Oberlander 1995.** A diagram can't stay vague: every spatial relation drawn is read as a claim.
- **Palmer 1992 (common region)** and **Palmer & Rock 1994 (uniform connectedness).** Shared regions and touching group more strongly than nearness, so an accidental overlap makes a group the viewer *will* see.
- **Kosslyn, *Graph Design for the Eye and Mind* (2006); Bertin, *Semiology of Graphics* (1967); Tversky 2011, "Visualizing Thought".** All say the same: what the eye groups must match what the content groups.

## Maps
- **Imhof 1975, "Positioning Names on Maps."** A name belongs clearly to its own feature, covers no other, and has preferred positions.
- **Christensen, Marks & Shieber 1995 (ACM TOG 14(3)).** Each label gets a few candidate spots and the solver picks one; when placement fails, labels are deleted by priority rather than overlapped.
- **Been, Daiches & Yap 2006, "Dynamic Map Labeling" (IEEE TVCG 12(5)).** For animated maps:
  - no popping: a label appears and disappears at most once;
  - no jumping: no sudden change of place or size;
  - decisions made ahead of time, not per frame.

## Layout and constraints
- **Dwyer, Marriott & Stuckey 2005 (VPSC).** Overlaps become separation constraints, solved with the least movement.
- **Misue, Eades, Lai & Sugiyama 1995, "Layout Adjustment and the Mental Map."** An adjustment keeps left/right and above/below order, nearness and topology.
- **Cassowary (Badros, Borning & Stuckey 2001).** Constraints have strengths (required versus preferred). Matplotlib's `constrained_layout` uses kiwisolver, an implementation of it.
- **Penrose (Ye et al. 2020).** `disjoint`, `overlapping`, `contains` and `touching` are explicit constraints: overlap is declared, never an accident.
- **Bluefish (Pollock et al. 2024).** Relations (Stack, Align, Background, Arrow) each own a dimension. There is no automatic collision handling; the authors moved text by hand.

## Typesetting and the web
- **TeX.** Boxes in a list never overlap. Overlap needs an explicit opt-in (negative glue, `\llap`, `\smash`). Content that doesn't fit is reported (an overfull box), not drawn over its neighbours.
- **CSS.** In-flow boxes don't overlap. The default `overflow: visible` paints spilled content over its neighbours, which is exactly the band-over-plot failure.

## Practitioners
- **ggrepel / adjustText.** Labels are pushed apart iteratively, with leader lines; `max.overlaps` drops labels that would hit too many things.
- **d3fc-label-layout.** Greedy, annealing and remove-overlaps strategies.
- **D3 `forceCollide`.** Repulsion per frame, which jitters when animated.
- **Manim.** No automatic handling. Practice is structural placement (`next_to`, `arrange`, `buff`) plus updaters.
- **What works in animation:** placement decided structurally and ahead of time, with deletion as the last resort.

## What we took
- **The rule:** two things share ink only when the script relates them, and a relation to a *part* licenses that part only.
- **The mechanism:**
  1. Prevent by construction: regions that tile, and a band that never overflows.
  2. Detect everything: the checker sees every view.
  3. Resolve with candidate spots and stability: keep a spot while it stays free, and keep the band's need monotone within a scene.
- **What we left for later:**
  - leader lines;
  - dropping a name to the caption as a last resort;
  - solving a whole transition at once.
