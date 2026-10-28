# D6. Holding a subject: `focus a,b` and `focus x hold`

Axiom 5: focus lights one thing for one sentence, then everything returns. Real cases need a subject held across
several sentences (two voices of a chorale, the subject of a sentence while three sentences discuss it, the
determinant turn in a real chat where the user's Claude wrote a focus line before every sentence and a group it did
not mean, which also re-lays the stage). The focus auditor proposes an explicit, bounded hold: `focus a,b` for
several ids without a group, and `focus x hold` ended by `focus none`, a `scene`, an interrupt or the end of the
turn, with a lint when a hold outlives four sentences. Also found: a trailing `focus` with no sentence after it
persists forever and reads back as `focus x`; interrupt() never clears focus; cells of a focused array snap to 0.3
instead of easing. Material: <scratchpad>/audit-5/notes.md (#2, #4, #6, #10; scenes cf1-music.dan, f3-last.dan, f5),
<scratchpad>/enforcement-map.md (axiom 5: no test exists today). Decide whether the hold is worth a bend of axiom 5 (the
author states the bound; the engine enforces it) or whether a cheaper form (focus lasting until the next focus
line, or a group without a drawn enclosure) serves; write the first focus tests either way. Axioms to defend: 5
(the bound stays explicit and enforced), 4 (dims ease), 6 (what the readback says about a held focus).
