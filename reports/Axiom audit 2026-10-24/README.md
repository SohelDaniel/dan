# Axiom audit, 2026-10-24: what is here

The user asked to "get to the bottom of everything": verify that the markup (Dan) and the engine obey the six axioms
and the position rule, and that the language is a good one for an AI to write. Read in this order:

1. `session notes.md`: the user's own hands-on questions in the playground (`tutor write`) and what each answer
   turned out to be (findings 1-9, with their final status at the end of `merged findings.md` §6).
2. `merged findings.md`: the result. §1 what holds, §2 the 22 verified violations (W1-W23) with reproducers,
   §3 the ranked fixes (tiers 1-4), §4 the 11 design decisions (D1-D11), §5 the small-model answer, §7 the language
   against the nine principles. Each "WRONG" row was re-run before it was believed (`verified reproducers.md`).
3. `audits/`: the eight auditors' full notes, one per axiom plus the position rule and the language. They cite
   reproducer scenes in a scratchpad that no longer exists; the scene text is usually quoted in the notes.
4. `AI-markup principles.md`: the user's research on what makes a markup good for an AI to write (nine principles).
   `enforcement map.md`: where each axiom is enforced in the code (file:line as of commit 6038197; lines have moved).
5. `decisions/`: for each decision, the question (`Dk question.md`); for the ones not yet merged, the engineer's
   notes and a patch (see the status table below). `engineer brief.md` is the brief they all followed.

## Status of the fixes and decisions (as of commit 16ecfcb)

Merged (each with a test that fails on the old engine; see `docs/history.md` 8m):
W5/W6 scales (D5) · W13 riders on curves (D1) · W1 cue lines one beat early · W4 function morphs ·
W3 re-marks ease · D4 one data ink · D9 path between two things (engineer B's version) · W11/W23 slips · W2 draw: lint.

Not merged, patch ready (all tested green on base 6038197 by their engineer; apply with
`git apply --3way "decisions/<name>.patch"`, then resolve, rebuild, run `make test check golden append`):

| decision | verdict | what it does | conflicts expected |
|---|---|---|---|
| D2 name the cut | ship | `morph … 0s` is a cut: drawn with an afterimage, named in the history, `continuity:` lint if uncued; README/prompt/toolbox teach it | README, engine, tests, main.cpp, HANDOFF/history (drop those hunks) |
| D3 colour stability | ship with caveats | a category keeps its colour across scenes; `palette input=blue …` line in the readback so folds/restarts colour like the window; named colours skipped by new kinds, `zero:` lint on a clash (fires in ~10 of 41 real lessons) | README, engine, tests |
| D6 hold a focus | ship with caveats | `focus a,b` and `focus x hold` (ended by `focus none`, a scene, an interrupt, the turn's end, or the fifth sentence with a lint); a trailing focus no longer persists; array cells ease; first focus tests; needs `#!turn-end` from the server | README, main, engine, script, tests |
| D7 trails across turns | ship | the readback carries a trail's path so a fold keeps it | engine, trails.cpp, tests |
| D8 rename mark->thing | don't rename | a 3-line gloss of `mark` in README, the guide and the playground cheat sheet | applies cleanly |
| D10 append a derivation line | ship with caveats | `morph f.tex += "…"` appends a row; earlier rows stay put; -37% tokens on the toolbox derivation, -8% on real lessons; wider later rows end off centre | engine, script, tests |
| D11 camera on timelines | ship | `camera zoom/fit/follow/reset` on number lines and timelines; 1D ranges as double; new `scripts/geology.dan` | engine.hpp, layout.cpp, tests, HANDOFF/history |

The audit's tier 2-4 list (colour clashes, the readback carrying scales and views, `\val{}` jiggle, focus clearing
on idle, label hysteresis, checker false alarms, furniture in palette colours) is in `merged findings.md` §3.

## How the work was run (for whoever repeats it)

Auditors and decision engineers were subagents with a shared brief (`audits/auditor brief.md`,
`decisions/engineer brief.md`): read-only for auditors; each engineer in its own git worktree, test first, every
regression, a verdict. The plan was two engineers per decision and a pick per pair; only D9 got its pair (both chose
the same design, B's implementation was more careful). The usage limit ran out mid-wave (Fable credits), the rest
continued on Opus, and the second engineers were stopped to save the weekly limit. Lesson: launch fewer agents at
once, and write notes to disk early (that is what made the restart cheap).
