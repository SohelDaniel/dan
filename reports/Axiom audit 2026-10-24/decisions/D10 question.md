# D10. `morph f.tex += "\\ x &= 4"`: appending a derivation line without re-emitting the whole formula

A derivation is one formula that grows (README). Today each step morphs the whole `aligned` block, so a 6-step
derivation emits the formula 6 times: 55% of all TeX characters and 15% of all scene characters in the corpus are
copies of the previous version, and in 58 of 351 steps the author also altered an earlier row (some are copy slips
the engine glides over silently). The language auditor proposes an append form. Decide the syntax (`+=`, or a verb,
or a property like `morph f.tex add="…"`), the exact semantics (append a row inside the aligned environment; what
if the formula has no aligned; what about replacing the last row, a common move in derivations), what the readback
prints (the full current tex: decodability), how the glyph morph treats the appended row (lit as new, old rows
stay put: audit-4 found earlier rows shift when a longer line arrives), the lints (an append to a non-formula), and
the prompt/toolbox rewrite with a before/after token count. Material: <scratchpad>/audit-8/notes.md (principle 9, fix 6),
<scratchpad>/audit-4/notes.md (formula morph findings, F4), chat/toolbox.md "A derivation that grows", chat/prompt.md
"Pacing a derivation". Axioms: 6 (readback), 4 (the append must be the same glide), 1.
