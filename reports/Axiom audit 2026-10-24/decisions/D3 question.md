# D3. Colour stability: a category keeps its colour across scenes and across turns

Today palette slots are claimed in order of first appearance and reset at every `scene`, so `input` is blue in scene
one and orange in scene two (verified: <scratchpad>/verify/sc_3.5.png vs clash_end.png), and a category changed by re-mark
claims its slot late while the re-parse of the readback claims it early (the chat's fold drifts: purple vs blue).
The HANDOFF lists "the flicker: keep the mental map between turns" as open. Options the auditors raised: a
lesson-wide slot map (category word -> slot) surviving clearStage, reset only by a new chat; or colour as a function
of the category name (a stable hash into the hues, collisions resolved by first appearance), which also makes the
readback's colours reproducible; or emitting `color=` in the readback. Material: <scratchpad>/audit-3/notes.md (#7 and
verdict 7, b4_scenes), <scratchpad>/audit-6/notes.md (#6, rt/colour-slots.dan), the dictionary row "every uncategorized thing
gets its own slot" (README). Also consider the named-colour clash (`color=blue` next to a palette blue; audit-3 #5)
since both touch the slot table. Axioms to defend: 3 (unrelated things never share a colour, also across scenes),
6 (a re-parse gives the same colours), 1 (a category is the same thing across the lesson).
