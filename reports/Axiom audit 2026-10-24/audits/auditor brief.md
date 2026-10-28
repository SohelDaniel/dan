# Common brief for the axiom audit (read this first)

Repo: <repo> (a C++20 + raylib engine for the markup language Dan, files *.dan; a Node chat layer under chat/). Read docs/HANDOFF.md first: it is the map (code, commands, gotchas). The language reference is README.md between '## The language' and '## The dictionary'. The teaching protocol Claude follows is chat/prompt.md; the moves are chat/toolbox.md; examples chat/examples/*.dan; scripts scripts/*.dan.

## The axioms, verbatim (README.md 'Axioms, enforced')

## Axioms, enforced

1. **Identity.** There is no delete verb. A re-mark updates in place. A `morph … from X` that doesn't match the real value is warned about and animated from the real value, so there are no jump cuts.
2. **Structure.** Bars share a zero baseline. Scales are per property and never jump mid-script, because the whole script is pre-scanned.
3. **Zero.** Unrelated things never share a color. Two things share ink only when the script relates them (`of=`, `on=`/`in=`/`over=`, `at=`, a name and its thing, data placed on one plot), because touching reads as a relation. Everything else is kept apart by construction where it can be (the band of formulas never reaches into a plot; a note's name never sits on another part of a formula), pulled apart in graph layouts, and otherwise reported by the checker, which sees every view's plot, ticks and title too. Where position is data (axes, number line), an overlap is reported as a lint, because coinciding reads as "equal".
4. **Continuity.** Every change is a morph with easing. New marks grow in, links draw themselves, and focus fades.
5. **Bounded focus.** `focus` lights one thing (or a group and its parts) and dims the rest, for one sentence: the one it is cued in, or the next one if it stands between sentences. Then everything is at full strength again.
6. **Decodability.** `--readback` prints the final scene in the same vocabulary, plus derived facts (max, mean, order), overlaps, recent changes and lint. `--check` re-parses that readback and confirms it rebuilds the same scene.


## The same axioms as the handoff states them (docs/HANDOFF.md §3)

## 3. The axioms

The project's founding rules. Every change must keep them; say so when a proposal bends one. They are enforced in
code and listed in `README.md` ("Axioms, enforced").

1. **Identity.** No delete. A re-mark updates in place; a morph from a wrong value animates from the real one.
2. **Structure.** Shared baselines and scales, fixed for the whole script by a prescan; they never jump.
3. **Zero.** Nothing in the picture without a meaning: unrelated things never share a colour, and two things share
   ink only when the script relates them (`of=`, `on=/in=/over=`, `at=`, a name and its thing, data on one plot).
4. **Continuity.** Every change is an eased morph; things grow in, links draw, focus fades.
5. **Bounded focus.** `focus` lights one thing for one sentence, then everything returns.
6. **Decodability.** `--readback` prints the scene in the same vocabulary; `--check` re-parses it to the same scene.

And the rule that follows from them: **position comes from structure.** Placement words (`region`, `below`,
`left-of`, `pos`, ...) are retired and ignored with a lint. Columns come from links, order from declaration,
lanes from groups. Anything new is generated *before* the engine sees it (python blocks write plain lines), so the
engine stays the only author of pixels.


## The dictionary (README 'The dictionary, as implemented')

## The dictionary, as implemented

| model | channel | where |
| --- | --- | --- |
| thing | a mark with a stable id | `doMark`; there is no delete |
| magnitude | bar length / dot area, from a shared zero | `layoutFree`: one scale per property, fixed by pre-scanning every value the script ever uses |
| category | color | `colorOf`: one palette slot per category, and every uncategorized thing gets its own slot |
| order | position | the schema's axis; in a graph, the column a thing's links give it, then declaration order within the column (`place/flow`) |
| part-of | enclosure | `drawGroup`; in a graph, a lane: its parts stay together, and stages line up across lanes |
| connected / cause | line / arrow | `drawLink`: an arc over bars; in a graph, routed through the gaps between columns (never through a box), against the flow as an arc |
| equals / similar | dashed link marked `=` / `≈` | `drawLink` |
| name, value | short text | labels; the voice carries sentences |
| change | motion | `Active` morphs; categories cross-fade color |
| voice subject | focus | everything else eases to 20% |


## Rules (binding)

- READ-ONLY on the repo. No edits of any file under the repo, no `git` writes (no add/commit/stash/checkout), no `make` or any build (the binaries `./tutor` and `tests/test_*` are current; a concurrent build would collide with the other auditors). Never touch `chat/sessions`, `chat/memory`, `chat/library`, `chat/settings.json` (the user's data); you may READ `chat/sessions/*/turns/*.txt|*.dan` to see what Claude really writes.
- Allowed, read-only: `./tutor F --check`, `./tutor F --readback`, `./tutor F --snapshot OUT --at 3,8,end` (PNG frames from a hidden window; then view the PNG with the Read tool), `./tutor F --layout-report -` (JSON: layout score over time, the worst moment, and a `timeline` of say/end/tex events), `./tutor F --layout-report - --verbose` (stderr logs every line, hook and voice start with times). The built tests: `./tests/test_engine`, `./tests/test_place`, `./tests/test_viz`, `./tests/test_script`, `node tests/toolbox.mjs`, `node tests/golden.mjs` (slow, CPU-heavy: at most once, and only if it answers a question you have), `./tests/append.sh` (same; it is known to fail on scripts/space-downhill.dan and space-saddle.dan already). `node tools/audit-sessions.mjs --last N` is fine WITHOUT `--frames` into the repo.
- Everything you write (test scenes, snapshots, scratch) goes under YOUR folder only: <YOUR_DIR> (given below). Copy a repo script into your folder before playing it with anything other than --check/--readback (a windowed play writes a .readback file next to the script; never do that inside the repo). Never open a visible window (`./tutor F` without a headless flag): use --snapshot or --layout-report.
- Your notes file is <YOUR_DIR>/notes.md. Append as you go (write early, write often: if you are cut off, the notes are what survives). No other auditor writes there; you write nowhere else outside your folder.
- Every claim of a violation needs a reproducer: the scene text (saved in your folder), the exact command, what happened (quote the output or describe the frame), and what the axiom says should happen. A claim without one is marked "suspected".
- Keep an open mind. The goal is the truth about the code, not a long list. "Holds, verified by …" is a valid and valuable finding. Do not pad. Do not re-derive the whole architecture in the notes; the handoff already has it.
- The system's goal, to keep in view: a live tutor in the spirit of 3Blue1Brown that must be able to explain anything from any field (not only maths), with a real voice (Inworld TTS) speaking the Say lines while the stage animates on the cued words. Claude writes the markup; it never gives pixels. Anything that limits what can be explained matters as much as anything that breaks a rule.

## What to deliver in notes.md (in this order, with these headings)

1. **Verdicts**: for each sub-rule of your axiom: holds / violated / suspected, one line each, with the reproducer path and command.
2. **Rule vs code gaps**: the README, HANDOFF or prompt says X; the code does Y (quote both, with file:line).
3. **Where this axiom limits creative freedom**: concrete cases, at least one from outside maths (chemistry, music, grammar, anatomy, geography, history, economics, programming), with the scene you tried and what the engine did.
4. **Small model**: in your axiom's area, which jobs a small trained model (1-8B, local or hosted) would genuinely help with, and which it would not, measured against reports/Small model for tutor weak spots.md (read it; it already concluded: don't train a scene writer; small models fit narrow, label-rich, checkable jobs). Be specific: the job, the labels that exist or could be collected, how its output would be checked.
5. **Proposed fixes**, ranked by value over cost, each with: the test that would fail on today's engine and pass after, which axioms the fix touches, a size (lines, hours).
6. **Checked and fine**: what you tried that behaved correctly (so it is not re-checked later).

Also read the session notes at <scratchpad>/session-notes.md (findings from the user's hands-on session today) and the enforcement map at <scratchpad>/enforcement-map.md (where each axiom is enforced in the code, file:line, every lint, every test). Confirm or refute the session findings that fall in your area; cite them by number.
