# D5. Scales that stay fixed: expression-driven values, and the live mode where beats arrive one at a time

Axiom 2 says scales are fixed for the whole script by a prescan and never jump. Verified breaks (<scratchpad>/verify/log.md):
the prescan reads literals only (engine.cpp see(), Value::NUM), so a bar driven by `{expr}` slides the shared scale
every frame (r1: bar a at value 3 fills the stage at 1 s and is a sliver at 9 s; a real gravity turn's bar never
changed height while the voice said it dropped to a quarter); a dot at `x={k}` on inferred axes leaves the plot
while `--check` says 100 (instant mode lays out the end state only); in live mode each appended chunk re-prescans
and re-scales what is on stage (bars ease to a quarter, axes 0-2 -> 0-14, dot radii JUMP in one frame, the auto
schema flips free->axes). The structure auditor proposes, in order: (1) prescan by evaluation using the shadow
engine (it already runs the whole script instantly ahead of the voice: let its extents feed the prescan); (2) make
`--check` lay out the start state and re-run layout at each sentence close so it sees motion; (3) a live-mode policy:
widen once, eased, at a sentence boundary, with a `structure:` lint fed back to Claude; no new syntax; author ranges
stay primary. Material: <scratchpad>/audit-2/notes.md (all of it; scenes r1..r16; the design recommendation),
<scratchpad>/enforcement-map.md (axiom 2), <scratchpad>/audit-4/notes.md (F8, dot radius). This is the biggest decision: scope it
honestly (what you ship now vs later), keep the feedback-loop gotcha in mind (HANDOFF §6: anything that sizes the
band from the zoom and the zoom from the band oscillates), and keep every golden the same unless the change is the
intent. Axioms to defend: 2 above all; 4 (no jumps when a scale must move); 6 (the readback could carry the scale as
`max=`, audit-6 #3).
