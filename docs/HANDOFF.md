# Handoff: the visual tutor (read this first)

Written 2026-10-20 for a fresh Claude instance taking over from a long session. It covers what the project is,
how it works, what has been tried, what the user thinks of it now, and what to do next.

## 1. The project in one paragraph

A **live AI visual tutor in the spirit of 3Blue1Brown**. The learner types a question. Claude answers by
**speaking** (every `Say:` line goes to Inworld text-to-speech) while **drawing on a stage**: a C++20 + raylib 6
window running the `tutor` engine. Claude writes a small declarative markup (`mark`, `link`, `morph`, `focus`,
`Say:` with `|cue|` word markers, a camera, fields, formulas). The engine plays it **while Claude is still
writing**: lines stream in beat by beat. Claude never gives pixel coordinates. The engine lays everything out
and animates it. The chat server (`chat/server.mjs`) runs one persistent `claude -p` process per chat on the
user's Claude subscription and streams beats to the stage.

## 2. Where the user is now (important)

The user's latest message, verbatim in spirit: **"the layout and stuff still sucks… I feel like it was even
better before… you need to do some serious research."**

They floated two directions and asked for research on the technicalities:
1. **A small, very fast model that decides positions** (placement/orientation), instead of hand-written rules.
2. **A simple automatic slot layout driven by the script**: the canvas is divided by how many things there are
   ("one object → canvas split into four sections, up/down/left/right; two objects → four squares", and so on).
   In other words, deterministic grid/slot templates chosen by object count and role, rather than a
   constraint solver plus nudging.

Their frustration is about **visual quality as experienced in the live chat window**, not about test scores.
Our geometric checker says scenes are nearly overlap-free (golden 99.9/100), yet the user sees bad layouts.
**Lesson: the checker's score is not the user's judgement.** Look at real frames from real chats (see §6, audit),
and ask the user what specifically looks bad (crowding? things jumping around? poor composition? too small?
empty space?) before building.

Things to check about "it was better before":
- Recent layout changes are listed in §5 (camera, fields, pointer sides, block rows, edge-region reservation,
  label stickiness, link-label staggering). Any of these could have made live scenes look worse even while
  the checker improved. The commits are small, so `git bisect` with real chat replays is practical
  (`tools/audit-sessions.mjs` plus `--snapshot` frames).
- The graph layout reshuffles when links arrive. The "flow" fix keeps columns, but nodes still glide around.
- The prompt was rewritten (decide-first, show-what-you-say, idioms), and Claude now draws more labels,
  colours and pointers, so stages are denser.

## 3. How to run things

```
make                      # builds ./tutor (and manim_demo)
./tutor scripts/zoom.txt  # play a script (voice needs .inworld_key; --mute for none)
node chat/server.mjs      # the chat: browser console + stage window (restart it after prompt/engine changes)

./tutor F --check                     # decodable? lints (layout:, binding:, zero:, tex:, hook:) + layout score
./tutor F --readback                  # the model as markup
./tutor F --snapshot OUT --at 3,8,end # PNG frames (hidden window; fails if the Mac display is asleep)
./tutor F --layout-report -           # JSON: per-0.5s scores, worst moment + issues, label flips (which/when)

make test     # doctest: tests/test_place.cpp (placement module), tests/test_viz.cpp (viz maths + expr compiler)
make check    # --check over scripts/*.txt (prints ok/FAIL, never fails make)
make golden   # tests/golden.mjs: per-script layout score vs tests/golden/*.json (--save re-baselines)
make append   # tests/append.sh: streamed (chunked --stdin) playback == whole playback (69/69)
make bench    # placement timing

node tools/audit-sessions.mjs [--last 8] [--frames DIR]   # replay the user's real chats turn by turn (LOCAL ONLY)
node tools/eval/run.mjs --variant full --prompts prompts-types.json --out DIR   # Claude generates scenes
node tools/eval/judge.mjs --out DIR --variants full --rubric v2                  # Claude judges frames (1-5)
```

To make contact sheets: `montage a.png b.png -tile 2x -geometry 800x450+3+3 out.png` (ImageMagick is installed),
then Read the PNG. Never take full-screen screenshots: one once captured a private window. Use `--snapshot`.

## 4. Architecture (files that matter)

| file | what |
|---|---|
| `main.cpp` | CLI: play, `--check`, `--readback`, `--snapshot`, `--layout-report`, `--stdin` (live mode used by the chat), voice, `TUTOR_VOICE` |
| `markup/script.{hpp,cpp}` | the parser: verbs mark/link/group/morph/focus/indicate/hide/show/Say/wait/schema/title/scene/camera; `[id]` pointers in Say; a forgiving `mark a -> b` read as a link |
| `markup/engine.{hpp,cpp}` | the model and timeline: steps, cues (`@cue` lines fire on words), actives (morphs), history for trails, readback, lints, `analyze()` prescan, key sets `kPositional`/`kTextKeys` (an unknown STRING prop becomes a colour **category**, a classic pitfall) |
| `markup/layout.cpp` | **per-frame layout**: `computeRanges`; block band (tex/card/code/array/table stack at top, or flow in rows when crowded); schema layouts `layoutFree/Graph/Numberline/Axes/Timeline` (`layoutGraph` is the adapter to `place/flow`); `at=` pointers (free side) and `on=/in=/over=` attachments; easing of marks and link waypoints; `placeAllLabels` |
| `markup/placement.cpp` | the engine↔placer adapter: `picture()` (everything as ink for the checker), `layoutCheck`, `markLabels`, `placeAllLabels` (labels with springs, stickiness), link geometry (`linkGeometryAt`: routed links from `linkRoute_`, arcs, straight) |
| `markup/place/*` | **pure C++ placement module** (no raylib; unit-tested): `flow` (the layered graph layout: model-order cycle breaking, longest-path columns, barycentre insertion with a memo for stability, lanes from groups, label-sized gaps, `solve1d` heights, routed links with spread ports), `qp` (1-D least squares under separation, Dykstra), map-style label placer, exact checker (overlaps, links through boxes, links along links, composition fill/offset), critically damped spring |
| `markup/render.cpp` | drawing only (reads layout results); axes ticks; links; pointers; formulas (`drawTex` morph by glyph matching) |
| `markup/camera.cpp` | camera on axes/plane (zoom/on/fit/follow/reset), anchored and van Wijk zoom paths |
| `markup/fields.cpp` | field/slope/heat/contour/solution kinds |
| `markup/trails.cpp` | trails recomputed from recorded history (deterministic) |
| `markup/binding.cpp` | "words vs picture" checker (`binding:` lints, --check only) |
| `markup/viz/*` | pure maths: zoom path, dividing tick chain, marching squares, streamlines, colormaps, trail times |
| `markup/expr.{hpp,cpp}` | expression language + `compile()` to a stack program for fields |
| `markup/tts.*`, `markup/tex.*` | Inworld streaming TTS (default voice Brian); LaTeX via `tools/tex` (MathJax → paths) |
| `rm/raymanim.*` | drawing helpers (text, strokes, `strip()` tapered polylines, arrows, axes) |
| `chat/server.mjs` | chat server: fence parser (```plan / ```scene / ```memory), beats → stage, SSE transcript, interrupts, `<last_scene_issues>` from `--check` |
| `chat/prompt.md` | **the system prompt** (+ README "The language" section + `chat/exemplars/*.txt` appended) |
| `README.md` | the language reference. The part between "## The language" and "## The dictionary" is fed to Claude |
| `docs/plan-camera-fields-trails.md` | the research and plan for camera/fields/trails (with sources) |

**Layout pipeline per frame** (`Engine::layout`):
1. ranges;
2. blocks into a top band (or rows);
3. schema layout of bodies in the remaining stage;
4. `pos=`/`at=` pointers;
5. attachments (`on=`/`in=`/`over=`), placed like pointers;
6. easing of `pos` toward `tpos`;
7. `placeAllLabels`.

Draw only reads the results. The checker `picture()` sees the same boxes. The stage is `kStage {40,66,1200,494}`;
the subtitle zone is `{120,586,1040,100}`.

## 5. What was done (chronological, short)

**Before git** (summarised from the earlier session):
- engine, chat server, LaTeX, memory;
- streaming and a persistent Claude process;
- the "Jarvis" stage-as-medium mode;
- an eval harness;
- research on why AI layouts fail (LLM-chosen coordinates fail, solvers and relations work, exact checks beat
  vision judges, VLM layout score uncorrelated with geometric overlaps r≈0.05);
- the **placement engine plan** (kiwi + VPSC + map label placement + springs): milestones 1–7 implemented,
  `pos=` deprecated in favour of `region=`/`below=`/`near=`/`on=`.

**In git, 2026-10-19:**
- **Graph flow fix**: link-less graphs lay out in columns by category, not on a circle.
- **Voice**: default Brian; `TUTOR_VOICE`, `--voice`. Samples were rendered for the user to choose; no pick yet.
- **Camera / fields / trails** (research: van Wijk zoom, Laidlaw streamlines study, Jobard-Lefer, d3-contour,
  Crameri colormaps, manim internals): see `docs/plan-camera-fields-trails.md`.
- **Teaching overhaul** (research: Graesser question types, LearnLM, Study mode, Khanmigo, refutation texts,
  expertise reversal, Code2Video/TheoremExplainAgent/Paper2Video). It changed:
  - `chat/prompt.md`: decide first (question type, level, shape, frame, claims to show, ending), conditional
    moves, no ritual "Your turn", a "show what you say" claim→encoding table, visual idioms;
  - exemplars: varied endings, `# ask:` headers, plus a quick-fact example;
  - engine: link `label=` + value, link `color=`, `[id]` pointers, array `colors=[…]`, span pointers `at=a[1:3]`;
  - the binding checker.
- **Eval results** (12 question types, 2 runs each, v2 judge, 1–5):

  | | Before | After |
  |---|---|---|
  | fit | 3.67 | 4.08 |
  | natural | 2.75 | 3.96 |
  | picture | 3.17 | 3.55 |
  | layout | 2.67 | 3.42 |
  | teaching | 3.42 | 3.75 |
  | binding | 3.0 | 3.17 |

  The judge's noise is about ±0.15.
- **Glitch fixes from real chats**:
  - pointers take a free side;
  - hidden blocks take no room; crowded bands flow in rows (chosen up front);
  - `hide` fades formulas;
  - label stickiness limited to passing lines;
  - tangent labels stay near their point;
  - no duplicate slope labels;
  - edge regions reserved;
  - link labels staggered.

  The audit improved from 298/632 bad sampled moments to 5/632. **The user still says layout sucks.**

**Engine bugs found and fixed along the way**:
- `m=` was treated as a matrix everywhere (energy formula showed "E = ?");
- the "opacity 0.12" link label;
- re-marks overlapping in instant mode;
- fixed items separated by the solver (axis bands crossing at a centred origin);
- raylib `CheckCollisionLines` writes to its output pointer even when given null (it crashed).

## 6. Tools you will want first

- `tools/audit-sessions.mjs`: replays `chat/sessions/*/turns/*.txt` cumulatively and reports per turn: worst,
  mean, bad samples, label flips, top issue. `--frames DIR` snapshots the worst moments. **Sessions are the
  user's private data: never commit them, never delete them** (an earlier mistake deleted `chat/memory/learner.md`;
  it was restored).
- `--layout-report -` JSON now includes `worstIssues` and `flips` (which label, when).
- Eval harness under `tools/eval/` (question-type set `prompts-types.json`; v2 judge rubric with fit/binding/
  natural/picture/layout/accuracy/teaching). Judges are noisy, so run twice.

## 7. Suggested next step (for the layout complaint)

1. **Ask the user** to show 2–3 frames that look bad and say what's wrong in each. Run the audit with `--frames`
   on their latest sessions and look at the frames yourself, not just at scores. Compare against an older commit
   (e.g. `8407f0c` or `a1615e3`) by building it in a `git worktree` and rendering the same chat. That tells you
   whether "it was better before" is real, and which change caused it.
2. **Research the user's two ideas** properly (they asked for it), and present options with evidence before
   coding:
   - **Slot/grid layout by object count and role**: presentation-template literature (PowerPoint/Keynote
     layouts, the "rule of thirds", Swiss grid systems), automatic slide layout (e.g. DesignScape, layout GANs,
     LayoutGPT), Code2Video's 6×6 anchor grid (which beat free placement in their ablation), and manim's
     `arrange`/`next_to` conventions. A deterministic "composition template chooser" (1 thing: centre;
     2: left/right; 3: triangle or row; 4: 2×2; plus a dedicated band for formulas and subtitles) is simple and
     predictable. It may beat solver nudging for the user's taste.
   - **Small fast placement model**: e.g. a distilled layout model, or an LLM pass (Haiku) that picks slots
     (not pixels) from a fixed vocabulary of anchors. Cost: latency on a live stream. Research says LLMs are bad
     at coordinates but fine at choosing named slots.
3. Whatever is chosen: keep Claude out of pixels, keep `--check`/golden/append green, and judge by **frames of
   real chats** with the user.

## 8. User preferences and rules

- They share API keys deliberately (Inworld key in `.inworld_key`). Use them, no warnings.
- They want: deep research (papers, docs, real code, other fields), a plan, then build, **test and iterate**, and
  **commit progress in git** (repo at this folder; `.gitignore` excludes the key, sessions, memory, caches).
- Plain, direct answers. Admit what's wrong. Don't oversell scores. They judge by what they see.
- Don't delete or overwrite user data (chat sessions, `chat/memory/learner.md`) without checking.
- Never full-screen capture; use `--snapshot`.
- Commits end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Restart `node chat/server.mjs` after engine or prompt changes. It relaunches the stage.

## 8b. 2026-10-20 (later): position comes from structure

The user's frame of a "dense vs transformer" chat (boxes on boxes, an arrow through a box, three arrows on one
line) traced to two layout systems owning halves of one scene: `region=`/`right-of=` took nodes out of the graph
layout and a kiwi solver squeezed them back in. Research (dot, ELK model order, Bluefish's one-owner rule,
Penrose, Mermaid/D2 issues; Code2Video, ALGOGEN; engraving, chemistry depiction, metro maps, film continuity)
and the project's founding rule agreed: position is a channel of the dictionary, derived from structure.
- Placement words (`region below above left-of right-of near align pos`) are **retired** (user's decision):
  ignored with a lint. `at=` and `on=/in=/over=` remain. kiwi, the 2-D separator, regions and skyline are gone.
- The graph schema is one layered flow (`markup/place/flow.cpp`, unit-tested): links make columns, declaration
  order is order, groups are lanes with stages aligned, links routed through gaps with fanned ports.
- `tools/compare-commit.sh` and `audit --sheet`: "it was better before" did not hold up against `8407f0c`.
- Snapshots segfault when the display sleeps: run `caffeinate -u -d -t 900 &` first.
- Known: boxes can overlap for ~0.5 s while gliding when a new column forms (newcomers snap, old ones ease).

## 9. Known loose ends

- Voice: the user hasn't picked a voice from the samples (Brian is the default).
- The camera only works on axes/plane; there's no pan for long arrays or timelines.
- Contour labels and field legends are not in the layout checker.
- The binding checker teaches only the next turn. A scene plays while it's being written, so it can't be fixed
  before it is shown.
- `scripts/*.readback` files are generated junk; they could be removed and gitignored.
- The graph re-layout glides nodes when links arrive. Transient crossings remain for about a second.
