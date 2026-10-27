# Handoff: a map of the visual tutor

Read this first. It describes the project as it is now: where things are, the rules, how we work, what bites, and
what is open. The dated story of how it got here (what was tried, measured, kept or dropped) is `docs/history.md`.

**Keep this file current.** Each session ends by updating it (see `CLAUDE.md`): change the map where the code
changed, add gotchas, move finished items out of "Open", and add a dated entry to `docs/history.md`.

Last updated: 2026-10-27 (lecture mode: a long lecture prepared from files, part by part, with notes to print; part 1 of a test lecture built, the rest waits for the user's go).

## 1. What it is

A live AI tutor in the spirit of 3Blue1Brown. The learner asks a question in the browser, and Claude answers by
speaking (Inworld text-to-speech) while drawing on a stage window (`./tutor`, C++20 + raylib 6).
- Claude writes a small declarative markup called **Dan** (files end in `.dan`): `mark`, `link`, `morph`, `focus`,
  `Say:` with `|cue|` words, schemas, formulas, a camera, fields, 3D.
- **Claude never gives pixels.** The engine owns layout, timing, colour, scales and the camera numbers.
- The stage plays the lines while Claude is still writing them, one beat at a time.
- The chat server (`chat/server.mjs`) runs one persistent `claude -p` per chat on the user's subscription.

## 2. Running it

```
tutor                 # build if needed, open the tutor on the last chat (bin/tutor, linked from ~/bin)
tutor new             # a fresh chat
tutor claude          # the tutor in the background + Claude Code in the repo (--dangerously-skip-permissions)
tutor restart | stop | log
tutor write [f.dan]   # a playground: an editor in the browser, the stage beside it; what you type plays as you type
tutor lecture new F… [--minutes N] [--ask "…"]   # plan a lecture from files (PDF, md, JSON, code, images)
tutor lecture build|notes|voice|play|show|list [ID] [--parts 3,5]   # also: Lectures in the browser (/lectures)
```

- In the browser: model, thinking level, voice on/off, which voice (Inworld's 166 English voices) and speed.
- **Save video** (browser header): the whole lesson as an mp4 with the narration, in `~/Movies/Tutor`, named after the
  chat. `tutor F --record out.mp4` renders in a hidden window as fast as it can (no key hints, no dictionary caption);
  the voice comes from the disk cache when the lesson was spoken before.
- **Lectures** (`chat/lecture/`, page `/lectures`): plan (one call: outline + bible) -> the learner reads/edits it ->
  build each part (write, `--check` fixes <=3, a separate reviewer call on frames, rewrite below 4/5 <=2, keep the
  best) -> `lecture.dan` + `index.json` -> `--prepare-voice` -> notes (`notes.md` -> `notes.pdf`). Everything in
  `chat/lectures/<id>/` (the user's data, gitignored). Play: `tutor lecture play` runs `chat/lecture/player.mjs`:
  typing asks, an answer after a part's closing question is checked; both become an aside on the same stage.
  Keys: `←` back a sentence, `shift ←/→` parts. Unfinished lectures play as a preview of the parts built.
- **Notes to print**: `node tools/notes/notes-pdf.mjs notes.md [out.pdf]`: LaTeX (MathJax) + mermaid, printed by
  Chrome via puppeteer-core (`cd tools/notes && npm install` once).
- **Images, PDFs and text files**: paste, drop, or 📎 in the browser. They are kept in the chat's `attachments/` folder and sent
  to Claude with the message as image / document blocks (photos scaled to 1600 px in the browser), with an
  `<attachments>` note in the context.
  The choices are remembered in `chat/settings.json` (gitignored); flags given to `tutor` win.
- **After changing the engine, the prompt or the server, run `tutor restart`.** `bin/tutor` rebuilds the engine
  first, and the server relaunches the stage.

The engine by hand:

```
make                                  # ./tutor
./tutor scripts/zoom.dan [--mute]      # play a script
./tutor F --check                     # decodable? lints (layout:, binding:, zero:, tex:, hook:, pointing:) + score
./tutor F --readback                  # the scene as markup
./tutor F --snapshot OUT --at 3,8,end # PNG frames from a hidden window
./tutor F --layout-report -           # JSON: layout score every 0.5 s, worst moment, label flips, the timeline (say/end/tex)
```

Regressions, run after every engine change:

```
make test       # unit tests: place, viz (incl. 3D maths), script parser, engine
make check      # --check over scripts/*.dan (prints ok/FAIL)
make golden     # layout score per script vs tests/golden/*.json (--save re-baselines, only on purpose)
make append     # streamed playback == whole playback
node tests/toolbox.mjs   # every toolbox move in chat/toolbox.md still plays clean
node tests/compute.mjs   # python blocks
node tests/lecture.mjs   # lecture mode without Claude: outline checks, packaging, the joined lecture plays
node tests/notes.mjs     # notes to PDF
```

Measuring with Claude in the loop (`tools/eval/`):

```
node tools/eval/run.mjs --variant full --prompts prompts-broad.json --out DIR   # Claude writes scenes
node tools/eval/judge.mjs --out DIR --variants full --rubric v2                  # a judge scores frames 1-5
node tools/eval/sentences.mjs --out DIR                                          # is each spoken claim shown?
node tools/eval/pacing.mjs --out DIR --variants full-opus-xhigh                  # the beat after a formula step (rests, gaps)
node tools/audit-sessions.mjs [--last 8] [--frames DIR]                          # replay the user's real chats
```

Question sets: `prompts-broad` (44, every subject), `prompts-types` (question types), `prompts-3d` (24 spatial,
12 that only tempt 3D), `prompts-compute` (python), `prompts-novel`, `prompts-algebra` (20 derivations, for pacing). The judge's noise is about ±0.15, so run twice
before believing a small difference.

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

## 4. The code

| where | what |
|---|---|
| `main.cpp` | CLI: play, `--check`, `--readback`, `--snapshot`, `--layout-report`, `--stdin` (the live mode the chat uses), voice flags |
| `markup/script.*` | the parser: verbs, kinds, schemas, `[id]` pointers and `{expr}` in Say |
| `markup/engine.*` | model and timeline: steps, cues, morphs (`Active`), history, readback, lints, the prescan, the instant **shadow** engine (values known before they are spoken), key sets `kPositional`/`kTextKeys`, the **rest** after a formula step (`kRest`, `quiet_`, `restDue_`) and the `timeline` log |
| `markup/layout.cpp` | per-frame layout: ranges, the formula band (`layoutBand`, `bandFit`), schema layouts, pointers and attachments, easing, `fitStage` |
| `markup/placement.cpp` | engine side of placement: `picture()` (all ink for the checker, with regions and view frames), labels, link geometry |
| `markup/place/*` | pure C++, unit-tested: `flow` (layered graph layout), `qp`, the label placer, the exact checker (`check.*`), springs |
| `markup/space.cpp`, `markup/viz/space.*` | 3D: one CPU projection (`viz::Space::project`), shapes, the camera in words, painter's-order drawing |
| `markup/render.cpp` | drawing only, reads layout results |
| `markup/annotate.cpp`, `area.cpp`, `fields.cpp`, `trails.cpp`, `camera.cpp`, `controls.cpp` | notes (brace, enclose, ...), area diagrams, fields, trails, the 2D camera, learner sliders |
| `markup/binding.cpp` | "words vs picture": `binding:` lints and **pointing ahead** (a spoken number or name gets a pointer) |
| `markup/expr.*` | expressions, compiled for fields |
| `markup/tts.*`, `markup/tex.*` | Inworld streaming TTS (48 kHz, sentence context, disk cache); LaTeX through `tools/tex` (MathJax) |
| `chat/server.mjs` | the chat: settings, fence parser (```plan / ```scene / ```python / ```memory), beats to the stage, SSE transcript, interrupts, background `--check` fed back next turn |
| `chat/system-prompt.mjs` | builds the system prompt (shared with the eval): `chat/prompt.md` + README's language section + `chat/examples/` + `chat/toolbox.md` |
| `chat/compute.mjs` | sandboxed python blocks (`sandbox-exec`), output written into scene lines |
| `chat/explain.mjs` | pause and ask: `?...` answers about the paused frame in a second window |
| `chat/ingest.mjs` | the learner's files: kind, Claude content blocks (text files as `<file>` text, PDFs as documents), PDF text by page, a lecture length from the size |
| `chat/lecture/` | lecture mode: `store` (folders), `plan` (+`plan-prompt.md`), `build` (+`part-prompt.md`, `critic-prompt.md`), `package` (join, notes via `notes-prompt.md`), `player` (live play, asides), `routes` (browser API; jobs run as detached `cli.mjs` processes with `job.log`/`job.pid`), `cli` |
| `chat/lectures.html` | the Lectures page: upload, plan, edit, build, play, notes |
| `tools/notes/notes-pdf.mjs` | markdown notes -> PDF (math lifted past markdown, MathJax SVG, mermaid, A4) |
| `chat/index.html` | the browser console |
| `tools/write.mjs`, `tools/write.html` | `tutor write`: the Dan playground. Whole lines go to `tutor --stdin` as typed (appended at the end; an earlier edit reloads the unchanged prefix, `#!interrupt`s it and plays the rest); a panel shows `--check` lints, the score and the readback; examples and toolbox moves load into the editor. Files in `playground/` (gitignored) |
| `README.md` | the language reference. **Between "## The language" and "## The dictionary" it is part of the prompt**: editing it changes what Claude writes |
| `tools/eval/`, `tools/audit-sessions.mjs`, `tools/compare-commit.sh`, `tools/mine/` | measuring, replaying real chats, A/B against an old commit, mining 3Blue1Brown's code |
| `reports/`, `research_notes/`, `docs/plan-*.md` | research write-ups with sources, per topic |

Layout per frame (`Engine::layout`): ranges, then the band of formulas, then the schema layout of bodies, then
pointers and attachments, then easing, then labels. Drawing only reads the results, and the checker sees the same
boxes. Stage `{40,66,1200,494}`, subtitles `{120,586,1040,100}`.

## 5. How we work

The user's process, and what has worked:
1. **Research before building, widely.** Papers, docs, real source code (3Blue1Brown's repo, manim, Graphviz/ELK),
   forums, and other fields: linguistics, cartography, typesetting, film, perception, chemistry depiction. Write it
   up in `reports/` or `research_notes/<topic>/` with sources. Mark claims not checked against a source.
2. **Plan, and say which axioms the plan touches.** For big changes, present the plan and wait for a yes.
3. **Measure first.** Get a baseline on the right question set before changing anything. Pre-register the bar for
   adopting a prompt change (`reports/Idioms A-B preregistration.md` is the model).
4. **Build small, test, iterate.** Each fix gets a test that fails on the old engine. Run the regressions (§2).
5. **Look at frames.** `--snapshot`, then read the PNGs (contact sheets: `montage a.png b.png -tile 2x -geometry
   800x450+3+3 out.png`). **The checker's score is not the user's judgement**: once it said 99.9 while the user saw
   a bad layout.
6. **Commit each milestone** with what was measured in the message, then update this file.
7. **Report plainly.** What improved, what didn't, numbers with their noise. Don't oversell.

Clean, readable code: match the surrounding style (short comments that say why, small named functions), no dead
code, no stubs left behind.

## 6. Gotchas

- **Make skips rebuilds after `git stash`/`pop`** (same-second mtimes). Use `make -B` or `touch` the sources.
- **Never rebuild `./tutor` while an eval is running**: the eval runs the binary, and a half-written one fails.
- **Snapshots fail when the display sleeps.** `caffeinate -u -d -t 900 &` first.
- **Never take full-screen screenshots** (one once caught a private window). Use `--snapshot`.
- **`--snapshot` renders offscreen without high-DPI.** Anything about the drawing transform must also be checked in
  the real window: `TUTOR_SHOT=out.png:3 ./tutor f.dan --mute` (a `BeginMode2D` bug once showed only on Retina).
- **An unknown string property becomes a colour category** (`kPositional`/`kTextKeys` in engine.cpp). A new
  positional key must be added there, or it silently turns into a category.
- **The prompt includes part of README.md.** A wrong example there teaches Claude the mistake. Example: `height=`
  and `z=` are positional, so a height is shown with `value={p.z}`, not by itself.
- **Eval caches:** a run that hit a session limit can leave junk. Scan outputs for "session limit" before trusting.
- **Layout feedback loops:** anything that sizes the band from the zoom and the zoom from the band will oscillate.
  The band's need only grows within a scene (`bandNeed_`).
- **`bandFit` must be infinite when no plot is on stage**, or every scene shrinks and the corpus "improves" falsely.
- **raylib's `CheckCollisionLines` writes to its output pointer even when given null.**
- **Name clashes with raymanim globals** (`H`, `W`, ...) in engine code: pick other names for locals.
- **3D:** labels, the placer and the checker all go through `Space::project`; never compute a 3D screen position
  any other way. The fit has a 0.96 margin; an exact fit fails tests on rounding.
- `ANTHROPIC_API_KEY` in the environment makes `claude -p` bill the API; the server warns.
- **Subagents cost the user's weekly limit.** Eleven max-effort agents at once used up the Fable credits mid-run
  (2026-10-24) and the user had to stop the second wave. Launch few at a time, make them write notes to disk early,
  and say what a fan-out will cost before starting one.
- **Patches made in worktrees are against the commit they started from.** After other merges, `git apply --3way`;
  appended tests conflict at the end of `tests/test_engine.cpp`: keep both blocks and check the closing brace.
- **A cue line belongs after its sentence.** Written before the sentence that says it, it is held for its word (and
  linted `hook:`); fireHook never reaches forward to a later sentence's line.
- **`draw:` lints are axiom warnings** (`--check` exit 1): a curve, field or world shape under free/graph/numberline/
  timeline/sequence. They are static (analyze); `reset()` clears runtime `zero:`/`identity:` lints, so a load-time
  lint must not use those prefixes.
- **Scales come from literals and from running the script** (`sampleValues`, the shadow when live). A live beat that
  widens a scale waits for a sentence start and lints `structure:`.
- **Changing the engine's timing shifts the golden samples.** A 1 s rest moved every later sample; one 3D script
  then caught a transient overlap with the old placer. Re-baseline (`--save`) only when the shift is the intent.
- **A lecture build runs `./tutor`** (check, snapshots, layout report): don't rebuild the engine while one runs (as
  with evals). Build engine changes to a scratch binary meanwhile (`make -n tutor -B`, change `-o`).
- **The lecture reviewer sees the window's chrome** (key hints, dictionary line) in `--snapshot` frames; the prompt
  tells it to ignore them. In the first test it scored part 1 3/5 three times: rewrites fixed some problems and
  made others, so the build keeps the best version, not the last.
- **Live control lines for lectures**: `#!load F LINE` (what comes before LINE is already on stage), `#!seek STEP`;
  `#!capture` also writes `step.txt` (where to come back); events `part N`, `seek N`, `waiting N` (a `pause` held).
- Chats from before 2026-10-23 keep `.txt` files (`state.txt`, `turns/*.txt`); newer ones write `.dan`. The server
  and the audit tool read both.

## 7. Rules from the user

- API keys are shared on purpose (`.inworld_key`). Use them; no warnings about it.
- **Chat sessions (`chat/sessions/`), learner memory (`chat/memory/`), the library (`chat/library/`) and
  `chat/settings.json` are the user's data.** Never commit, delete or overwrite them without asking.
- Commit progress in git. Commit messages end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Plain, direct answers. Admit what's wrong. They judge by what they see on the stage.
- Ask before big or hard-to-undo changes. Small fixes the user asked for: just do them, then report.

## 8. Where things stand

Done and measured (details in `docs/history.md`):
- 2D: graphs as layered flow; annotations, views side by side, derivations, grids, area diagrams, histograms, trees.
- Teaching: a decide-first prompt, the toolbox of 27 checked moves, 4 annotated examples, `{expr}` in Say,
  pointing ahead, python blocks, pause and ask.
- Voice: 48 kHz, sentence context, a picker in the browser.
- Pacing: one formula step per sentence and a 1 s rest after it (engine); a digest sentence and `wait 2s`/`3s`
  after subgoal and crux steps (prompt). On 20 derivations: 2+ changes per sentence 15-21% -> 0-4%, rest >= 0.8 s
  1-2% -> 98-100%, gaps >= 2 s 0-2% -> 14-16%; judge's teaching 4.05 -> 3.88 (noise ±0.15).
- Overlap needs a relation: the checker sees views; the band never spills onto a plot.
- 3D (`schema space`): chosen right for 34/36 questions; on spatial questions picture 2.88 to 3.46, teaching
  3.12 to 3.58.

- The axiom audit (2026-10-24): scales see computed values, riders on curves, cues on their word, function morphs
  blend, re-marks ease, one data ink, path-as-link, slips spoil one word, the `draw:` lint. `reports/Axiom audit
  2026-10-24/README.md` is the map of the audit and of what is still unmerged.
- The chat takes images and PDFs (paste, drop, 📎) and saves a lesson as a video (`Save video`, `~/Movies/Tutor`).
- Playground: `tutor write` (an editor beside the stage). The markup is called Dan; files are `.dan`.
- Lecture mode (2026-10-27): files -> planned, built, reviewed parts -> a lecture on the stage with asides, and
  notes to print. One test lecture planned (the Transformer paper, 8 parts, 20 min), part 1 built (~16 min, ~0.8M
  input / 90K output tokens, reviewer 3/5).

The newest working session's notes are the last entry of `docs/history.md`.

## 9. Open

Next candidates, roughly by value:
- **Lecture mode, next** (the user wanted to see part 1 before the rest is built: `tutor lecture build attention`
  builds parts 2-8, ~2 h). Then: try an aside and a check answer in the real window (built, not yet tried with a
  person); measure what a part costs in `/usage`; the reviewer never reached 4/5 (look whether rewrites converge, or
  stop at the first 3 with no hard problem); `send` copies stay drawn over their originals (seen in part 1 at 66 s:
  an engine issue); a browser Play starts a second stage window beside the chat's.
- **Finish the axiom audit.** Seven tested patches are ready but not merged: D2 the cut, D3 colour stability,
  D6 `focus a,b hold`, D7 trails across turns, D8 a gloss of `mark`, D10 `morph f.tex += "…"`, D11 the camera on
  timelines. Each is in `reports/Axiom audit 2026-10-24/decisions/` with its engineer's notes; the README there has
  the table (verdict, what it does, which files conflict). They were made against commit 6038197: apply with
  `git apply --3way`, resolve (keep both sides of appended tests; drop HANDOFF/history hunks), rebuild, run every
  regression, look at frames, commit one at a time. The user agreed to the plan but has not seen each design: show
  them what each changes in the markup before merging the ones that change the language (D2, D6, D10, D11).
  Then the audit's tier 2-4 fixes (`merged findings.md` §3).
- **3D view too small when formulas stack above it.** Give a space view more room, or put formulas beside it.
- **Tangent planes are drawn as big clipped sheets.** A local patch at the point would read better.
- `flux` and `gradsurf` still stay flat though they need 3D.
- Names crossing shapes' outlines in geometry and plots (the largest class of overlaps in real lessons, ~110).
- Overlaps that last only during a transition (the graph re-layout glides nodes when links arrive).
- The "flicker": keep the mental map between turns.
- A lint for the invisible-plane loophole (`schema plane axes=false` used as a free canvas).
- Ideas offered, not started: "prepare like Grant" (plan the whole lesson first), a taste judge trained on the
  user's clicks.
- **Pacing, next:** the share of gaps >= 2 s (14-16%) is above the 4-10% bar set from 3Blue1Brown; watch real
  lessons before tuning "one step in six". The judge can't see time: a pacing criterion needs the timeline.
- **`make append` reports 4 DIFFERS on `space-saddle`/`space-downhill`**: an artefact, not a decodability break. The
  streamed readback carries one stale `# lint: space: … camera never moves` comment; the model lines are identical.
  Fix: re-derive that lint on append like the hook lints, or compare model lines only in `tests/append.sh`.
- **Housekeeping:** `.claude/worktrees/` holds ~27 agent worktrees from the audit (untracked; every useful patch is in
  `reports/`). Remove them with `git worktree list` + `git worktree remove --force <path>` and `git branch -D` the
  `worktree-agent-*` branches, after checking nothing else is wanted. One throwaway chat, `chat/sessions/
  20261024-194917`, came from testing attachments (the user's data: ask before deleting).
- Small: no camera pan for long arrays or timelines; contour labels and field legends aren't in the checker;
  `scripts/*.readback` files are generated junk.
