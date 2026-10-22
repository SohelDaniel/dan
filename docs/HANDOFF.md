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

## 8c. 2026-10-20 (evening): 2D made solid before 3D

Measured with `tools/eval/prompts-broad.json` (44 questions, every subject and question type) plus frames read by eye,
an engine audit and research (3b1b's code, Mayer, Ainsworth, Cheng, Tversky, Code2Video). Done, each committed:
- Robustness: whole Unicode blocks in the font; focus ends with its sentence; explicit shared `color=` is a relation;
  self-loops; real formula sizes in headless checks; snapshots wake the display; quotes keep commas in lists.
- Stability: boxes make a graph from the first one; the band eases and new blocks wait for room; ghosts keep their slot;
  subtitles page in two lines; bar scales ease by time.
- Fit: a scene that outgrows the stage is drawn smaller (`stageRect/toStage/fitStage`, `BeginMode2D` in `draw()`,
  never below 0.6, only shrinks within a scene, not on axes). Box names wrap.
- Vocabulary: annotations (`brace enclose underline cross check`, `of=f[part]` for `\textcolor{part}` glyphs,
  annotate.cpp); views (named `schema` lines side by side, `schemaOf`); derivations (aligned, new glyphs lit);
  grids (`array cols=`, `M[r][c]`, `M[r][:]`); area diagrams (`area parts= by=`, area.cpp); histograms
  (`numberline bin=`); trees top-down (when branching is what the graph does); `schema sequence`.
- Prompt: declare the schema first, numbers shown as spoken, physical systems drawn as their parts (heart exemplar),
  no ritual cards.
- Broad eval (judge, 1-5): picture 3.34 -> 3.64, layout 3.41 -> 3.52, fit 3.91 -> 4.05; cards 28/44 -> 0/44.
  Binding (numbers spoken but not shown) stays the weakest at ~3.1: the next prompt/lint target.

## 8d. 2026-10-20 (night): the 2D must-do list

Each committed, regressions (test, check, append, golden) green after each:
- 3b1b's measured habits (`docs/3b1b-habits.md`): 1 s entrances staggered 0.15 s; a clutter note above 8 things;
  a scene's name opens as a 62 px headline on the empty stage and goes to the corner when content arrives.
  Axis numbers were already 15-16 px; boxes already rise 10 px; formulas write glyph by glyph (no further rise-in).
- Annotations try their placements and keep the one sitting on the least; a group encloses its return arcs; link
  labels may sit on their own link, keep off group outlines and out of groups that don't hold both ends.
- Cue pre-flight: an `@score` line whose sentence never speaks `|score|` glues to the word it names ("scores").
  First parser unit tests: `tests/test_script.cpp`.
- Claude sees its frame: verified in a real session (the second message carries `frame: true` and the image).
- Server: the after-turn check and readback run in the background (no `spawnSync`). Frames cost 0.1-2 ms, so the
  engine needs no speed work yet.
- `set=learner` params get a slider under the stage (`markup/controls.cpp`); a drag reports `@ev set k v`, the
  server folds it into the stage text and sends `<learner_set>`. Simulated drags verified; a real mouse drag on a
  Retina window has not been tried yet.

Latency, measured (2026-10-21, probe and A/B evals on the 44 broad questions):
- Most of the ~9 s is Claude *thinking* before its first word (5-14 s in real chats; Haiku and Sonnet decide this
  themselves, `MAX_THINKING_TOKENS=0` doesn't stop them). A prompt line "don't deliberate" stops Sonnet thinking
  (first sentence ~4 s instead of ~7.5 s) but costs accuracy 3.70 -> 3.34, teaching 3.68 -> 3.41, binding
  3.18 -> 2.80. Rejected: the thinking is what makes answers right.
- Dropping the ```plan block saves ~1.5 s at equal judge scores but raises spoken-but-not-shown lints 41 -> 54.
  Kept.
- Done instead: the voice for each Say starts synthesising as soon as it is written (`#!voice`), the server's
  check runs in the background, Claude's process is warm when a chat opens.

Code health: engine unit tests (`tests/test_engine.cpp`); `parseLine`, `placeAllLabels` and `layout` split,
verified identical against the previous binary.

Next: 3D, then the taste scorer (3b1b priors, an AI judge, a few clicks from the user).

## 8e. 2026-10-22: spoken but not shown

The user picked "the tutor says things it never shows" (binding) as the top problem. The research, plan and every
experiment are in `reports/Fixing spoken but not shown.md` and `research_notes/Spoken but not shown/`.

**The measure.** `tools/eval/sentences.mjs` takes a frame at the end of every Say, and a judge marks each sentence's
claim as shown, partly shown or not. The scene judge sees 3 frames and can't tell. Baseline: 50% / 53% fully shown
(two runs); numbers are 53% of claims.

**Tested and rejected:**
- *A second model editing each beat before it plays.* It bound more (63%), but layout fell −0.44 and accuracy also
  dropped: additions made after the composition don't fit it.
- *A storyboard in the plan.* 55–60%, but it costs +1.4 to 2.6 s before the first word, and the gain is near chance.

**Built: `{expr}` in Say** (axiom-safe):
- What it does:
  - speaks a stage value as the word is reached, after the lines cued on that word; a morph counts at its end value;
  - pulses the thing;
  - `{a[3]}` / `{M[r][c]}` speak a cell's item.
- The live engine keeps an instant **shadow** engine (`EngineOptions::shadow`: no layout, no TeX, silent), fed the
  same text, so values are known before the voice is requested.
- `#!voice` prefetch skips Says with refs; they're requested when the beat arrives.
- The learner's slider release updates the shadow.
- The binding checker reads the spoken text, so a value read off the stage but not readable on it is still flagged.
- Readback is unchanged, which is tested; so is streamed playback against whole playback.
- Gotcha: after `git stash pop`, `make` may not rebuild (same-second mtimes). `touch` the sources first.

## 8f. 2026-10-22 (evening): visual moves mined from 3Blue1Brown

`chat/idioms.md` holds 50 short idioms in our markup, covering 32 kinds of visual move. The chat server and
`tools/eval/run.mjs` append it after the worked examples; `--no-idioms` leaves it out. Each idiom is also in
`chat/idioms/`, which `make check` covers.

How they were made: `tools/mine/` (its README is the datasheet).
1. 3Blue1Brown's step comments, filtered.
2. A taxonomy proposed by Claude, then hand-edited.
3. Labels from Sonnet and Opus; only blocks both agree on are used (κ 0.63).
4. k-center selection.
5. Translation with engine-checked repair.
6. A pass/fail vision judge: 102 of 180 passed.
7. One per kind first.

The pre-registered A/B test (`reports/Idioms A-B preregistration.md`):
- claims fully shown went from 55.3% to 59.4% (+4.2 points, 95% CI +0.2 to +8.2), just short of the +5 bar set beforehand;
- no cost to layout, accuracy, teaching or latency (+0.6 s);
- the user tried it live and found it "way better", so it was adopted.

Also new:
- **Symbol rule:** the binding checker flags a symbol said as a name (f, P, α) that nothing on stage carries. The prompt asks for `label="f(x, y) = …"`, and `label=` on a field is its legend.
- **Cache guard:** `tools/mine` never caches a failed or limit-hit model call. The eval scripts don't check for this, so scan their outputs for "session limit" before trusting a run.
- Idioms cut after review (`tools/mine/rejected.txt`; assemble fills from the next passing candidate):
  - "Park it aside": hand-positions a box on an invisible plane;
  - "Let time pass": a bar for a process;
  - "Dim the claim aside": built on a card.

  "Slide a value, watch followers" was kept: its mid frames show the live formula.

**Pointing ahead** (binding.cpp, `startSay` instant loop):
- As each word is reached, the shadow finds the one visible thing that shows a number or name said aloud: an array cell, a label, a value, a link, else a formula. That word gets a pointer (`@pt:`), which the live engine applies (`applyPointers`) to Says that haven't started.
- Several matches, or none: nothing happens. Small counts of things ("two groups"), ordinals ("step two") and code blocks are skipped.
- Measured on 176 generated scenes:
  - Claude pointed at 3% of the spoken numbers and names that were on stage;
  - with pointing ahead, 76% are pointed at;
  - about 90% of 30 audited pointers were right (77% before the count and ordinal rules).
- `--check` prints a `pointing:` line; `TUTOR_DUMP_POINT=1` lists every pointer.
- The number reader is shared with the checker and now reads "seven hundred eighty-four", "ninety-nine thousand nine hundred", "zero point one four" and "twenty one".

Wanted later (the user's idea): pause the lesson and ask about what is on screen. A second tab explains the current frame,
and the lesson resumes where it paused.

## 9. Known loose ends

- A loophole in "position comes from structure": `schema plane … axes=false` used as a free canvas to place boxes by
  coordinates (seen in a mined idiom, now cut). A lint could flag boxes/rects on an axis-less plane that aren't a
  physical arrangement.

- Voice: the user hasn't picked a voice from the samples (Brian is the default).
- The camera only works on axes/plane; there's no pan for long arrays or timelines.
- Contour labels and field legends are not in the layout checker.
- The binding checker teaches only the next turn. A scene plays while it's being written, so it can't be fixed
  before it is shown.
- `scripts/*.readback` files are generated junk; they could be removed and gitignored.
- The graph re-layout glides nodes when links arrive. Transient crossings remain for about a second.
