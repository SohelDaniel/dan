# tutor chat

The stage is the conversation. You ask; the tutor answers by speaking and drawing on the stage as it speaks. You can type straight onto the stage window, or into the browser console, which shows a live transcript lit word by word. This module is separate from the engine: it runs `./tutor --stdin`, pipes markup into it as Claude writes, and listens to its events.

```
make && make tex             # once: builds ../tutor, installs MathJax for formulas
node chat/server.mjs         # opens the console and the stage, resuming your last session
node chat/server.mjs --new --mute --model opus --effort medium --port 4800
```

It runs `claude -p` with your logged-in Claude subscription. If `ANTHROPIC_API_KEY` is set, Claude Code bills the API instead, and the server warns you.

## How a turn works

1. You type on the stage (any key opens the prompt bar; Enter asks, Esc cancels) or in the console. The stage shows your question and a "thinking" pulse.
2. Claude has one persistent `claude -p` process per session: stream-json, no tools, no MCP servers, `--effort low`, started when the session opens. It gets a fixed system prompt (`prompt.md`, the worked examples in `exemplars/`, and the language reference from `../README.md`) and, per message, `<stage>` (what's on screen), `<memory>` (when changed), the last scene's issues, and what you heard if you interrupted.
3. It answers with a hidden ```` ```plan ```` (insight, picture, arc, formula, check question), a ```` ```scene ````, and optionally a ```` ```memory ```` block. Nothing else is shown.
4. Scene lines stream to the stage while Claude writes: set-up at once, then one beat (a `Say` and its lines) at a time. The engine speaks and reports each word (`@ev`); the console lights the transcript in sync.
5. A turn ends with a check question; the stage then shows "your turn". Typing while it talks interrupts: the stage fast-forwards and Claude is told exactly what you heard.
6. The scene is checked (issues go to Claude next turn) and folded into `state.txt` with `--readback`.

Measured (Sonnet, muted): visuals in about 2.5–5 s, voice right after.

## Teaching

`prompt.md` asks for the 3Blue1Brown way (a concrete case, the mechanism itself moving, the pattern before the formula, the formula built up by morphing and colored like the things it names), Mayer's multimedia principles (cue on the word, point, no on-screen sentences, short segments), a table for choosing the right picture per kind of idea, and explain-then-check. `exemplars/` holds the gold-standard scenes Claude learns from; each passes `--check`.

`../tools/eval/` measures it: `run.mjs` generates first answers for 12 topics with a prompt variant, checks and renders frames; `judge.mjs` has Claude score each on representation, concreteness, the key idea on stage, sync, the check question, layout (from the frames), accuracy and teaching.

## Memory

| layer | where | what |
| --- | --- | --- |
| transcript | `sessions/<id>/messages.jsonl` | what you asked and what it said; the console reloads it |
| conversation | `sessions/<id>/session.json` | Claude's session id, so reopening a session resumes with its context |
| learner | `memory/learner.md` | Claude's notes on you (level, goals, what you've covered), across all sessions. **Memory** opens it to edit |

## Buttons

- **Replay**: plays the last answer again on the stage.
- **Lesson**: every turn from the start, as one script (`lesson.txt`; `../tutor lesson.txt --record out.mp4` works on it).
- **Save scene**: copies the current stage to `library/<name>.txt`; say "load the <name> scene" to reuse it.
- **voice**: toggles Inworld narration (restarts the stage window).

## Files

| file | what |
| --- | --- |
| `server.mjs` | Node, no dependencies: input from stage and console, persistent claude, block parser and beats, stage events → SSE, interrupts, state folding, memory |
| `index.html` | the console: live transcript, sessions, memory |
| `prompt.md` | the protocol and the teaching method (`{{EXAMPLES}}` is filled from `examples/` and `toolbox.md`) |
| `system-prompt.mjs` | builds the system prompt the same way for the server and the eval |
| `examples/` | four whole scenes, each beat annotated with why its tool was chosen |
| `toolbox.md` | the moves, by what they show; each a complete, checked scene (`tests/toolbox.mjs`) |
| `exemplars/` | the earlier nine examples, kept as regression scenes (golden, append), no longer in the prompt |
| `idioms.md`, `idioms/` | moves mined from 3Blue1Brown (`tools/mine`), kept as data and checked scenes |
| `compute.mjs`, `explain.mjs` | python blocks; pause and ask |
| `sessions/<id>/` | `state.txt`, `turn.txt`, `live.txt`, `turns/NNN.txt`, `lesson.txt`, transcript |
| `library/`, `memory/` | saved scenes, learner memory |

## Pause and ask

Pause the lesson (space), then type a line starting with `?` on the stage (or in the browser): `?why is that 6`.
The stage keeps what you see and the scene as of that moment; a fork of the lesson's Claude session answers with a short
scene that continues from it, in a second window, while the lesson stays paused. Close the window and press space to go
on; the lesson hears what you asked on its next turn. Code: `chat/explain.mjs`, `#!capture` / `#!pause` in `main.cpp`.

## Computing: python blocks

A reply may carry ` ```python id=walk seed=7 ` blocks that compute data (standard library only) and hand it back with
`out(key=value)`. The server runs each in a sandbox (macOS `sandbox-exec`: no network, no reading the home folder, no
other programs; CPU and time limits; results cached by code and seed) and writes the values into the scene lines that
use them (`items={walk.steps}`, `Say: "… {walk.end}"`, `each v in walk.steps: mark s{i} dot …`) before the engine sees
them, so the engine only ever gets literal data and every axiom holds. Marks made by one `each` share one category and
carry no names from their ids. Code: `chat/compute.mjs`; tests: `tests/compute.mjs`.
