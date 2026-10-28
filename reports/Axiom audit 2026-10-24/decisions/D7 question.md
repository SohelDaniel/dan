# D7. Trails across a turn boundary: carry or confess

A dot with `trail=all` (gradient descent, a hurricane track, an orbit) draws where it has been. The chat stores the
stage between turns as the readback, and the readback has no form for a trail's history: after the fold the path is
gone (audit-6 rt/hurricane.dan, rt/trails-descent.dan), and `<last_frame>` shows a picture the learner did not see.
Options: carry (a history key the engine writes and reads back, e.g. the sampled points, bounded in length), or
confess (the readback marks the trail as lost and the prompt tells Claude a trail does not survive a turn), or
re-derive (when the motion came from a param morph the history is recomputable from the script, but the fold has
no script). Material: <scratchpad>/audit-6/notes.md (#8), markup/trails.cpp, the readback code (engine.cpp readback()).
Axioms to defend: 6 (the readback is the scene), 1 (a thing's history is part of it), 4 (a re-drawn trail must not
pop). Mind decodability's round trip (--check) and append.sh.
