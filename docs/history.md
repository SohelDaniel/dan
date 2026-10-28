# History: how the tutor got here

The dated log of the project: what the user asked, what was found, built, measured, and dropped. The current map
(code, axioms, process, gotchas, open items) is `docs/HANDOFF.md`; read that first. Sections 1-9 below are the
first handoff (2026-10-20) and are partly out of date; the dated entries after it are the story since.

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

**Voice quality** (Inworld docs, 2026; `markup/tts.*`):
- **48 kHz:** the model's native rate. Measured: real energy above 12 kHz, −56 dB against −91 dB at 24 kHz. The audio buffer was doubled, so timing is unchanged.
- **Text normalization:** `applyTextNormalization` ON.
- **Denoising:** `enhanceGeneration`.
- **Sentence context:** `synthesisContext`, the 2 previous sentences, so each sentence continues the explanation rather than starting cold.
  - A Say prefetched by the server comes before the beat ahead of it, so the engine keeps `prefetched_` to ask with the same context its beat will.
  - A test checks they match, or the early audio would be wasted.
- **Cache:** the key now includes every setting.
- **Voice choice:** samples of 9 candidate voices plus Brian step by step are in `.tutor_cache/voice-samples/`. The user picks with `--voice <id>`. `GET /voices/v1/voices` lists 166 English system voices.

Wanted later (the user's idea): pause the lesson and ask about what is on screen. A second tab explains the current frame,
and the lesson resumes where it paused.

## 8g. 2026-10-22 (late): docs-first prompt, python blocks, pause and ask

- **Prompt:**
  - `chat/system-prompt.mjs` builds it for both the server and the eval;
  - `chat/toolbox.md` holds 27 moves, each a complete scene checked by `tests/toolbox.mjs`;
  - `chat/examples/` holds 4 annotated scenes;
  - there's a plan-in-thinking step.
  - The 9 old exemplars stay as regression scenes, and the mined idioms are no longer in the prompt.
  - Measured neutral on 60 new topics (`reports/Copying vs composing.md`); adopted for structure.
- **Python blocks** (`chat/compute.mjs`, `tests/compute.mjs`): sandboxed (`sandbox-exec`: no network, no home folder, no other programs, CPU and time limits), with data written into the scene lines before the engine sees them, so the axioms hold.
  - `each` makes lines; a cue or `with` prefix carries through; loop arithmetic is computed.
  - Generated marks share a category and carry no automatic names.
  - The eval runs blocks the same way (`expandReply`).
  - `tools/eval/prompts-compute.json` is the check set: 7–8 of 10 lessons use Python, with no syntax errors.
- **Pause and ask** (`chat/explain.mjs`; `#!pause`, `#!capture`, `--window-pos` in `main.cpp`): `?…` on the paused stage or in the browser.
  1. The stage captures the frame and the scene as of the playhead.
  2. A fork of the lesson session answers.
  3. A second window plays the answer.
  4. The lesson hears about it next turn.

  Tested end to end: about 9 s.
- **Tested and not adopted:**
  - gated per-beat repair: +0.8 points;
  - training a local model: Qwen3 8B's best of 4 is far below Claude's first try.

## 8h. 2026-10-23: overlap needs a relation

A pause-and-ask answer drew a matrix over a plane view, row names over the wrong rows and a ghost cross over the
corrected formula. Research (Mackinlay's expressiveness, Engelhardt's syntax of graphics, Marks & Reiter on false
implicatures, TeX boxes, map labelling) says the same thing: touching is read as a relation, so two things may share
ink only when the script relates them. `reports/Overlap needs a relation.md` has the sources and the numbers.

- **The checker sees views** (`picture()`, `addFrames`, `regionOf`): each view's plot area, tick numbers, title and
  dividing line are ink, and every ink belongs to a region (the band, or a view). A frame is ground only for its own
  region's things (`place::check`, `Ink::Frame`). The screenshot's scene scored 79/100 with the blind checker, 10
  with the one that sees views, and 97 after the fixes below.
- **The band never spills onto a plot** (`layoutBand`, `bandFit`, `bandNeed_`): above a plot the band is capped at 60%
  and the whole stage is drawn smaller instead (uniformly, so scales and baselines hold). The band's need only grows
  within a scene; letting it shrink made blocks re-wrap between rows and column every frame.
- **A note's name stays off the rest of a formula it names a part of** (`annotationHost`, `partLabelSide`): an enclose
  of a row is named above the formula for upper rows, below for lower ones, and the band keeps that room. Arrays
  are left as they were: their cell notes are named off the strip already.
- **A hidden note leaves no ghost** (`opacityOf`): hide fades a brace, enclose, cross, … to nothing; the readback
  keeps `opacity=0.12`, so decodability is unchanged.
- **A graph leaves room for a named brace on a column** (`layoutGraph`), on the side away from its links.
- Measured on 309 recorded lessons (`--layout-report`, worst moment): neutral overall (92.2 to 92.3), because
  two views and row notes are rare there; the screenshot's scene went from 10 to 97. Golden layouts unchanged.
  Not fixed: names crossing shapes' outlines in geometry and plots (the largest class, about 110 lessons; making a
  crossing cost more did nothing), and overlaps that last only during a transition.

## 8i. 2026-10-23: 3D (schema space)

Plan: `docs/plan-3d.md`. Research: `research_notes/3D/explainers.md`. The code is in `markup/space.cpp` and `markup/viz/space.*`.
- **How it is drawn.** One CPU projection (orbit camera, z up, narrow perspective, fitted to the cylinder the box sweeps
  as it turns), drawn as 2D strokes and triangles sorted by depth. So labels, the placer, the checker, readback and
  `--check` all work on projected positions; GPU 3D was rejected (it breaks snapshots and headless runs).
- **Kinds:** dot (`drop=true`), vector (dz), line/arrow, path (xt yt zt), surface (fn=, or u,v), sphere, plane, polygon
  faces, grid (3x3), field (vx vy vz).
- **Camera:** in words (view= orbit= tilt= spin zoom= on= reset).
- **Parity:** everything 2D has (trails, notes, pointers, send, focus, `{expr}`); a ghost pass so things behind
  surfaces still show.
- **Lints:** a thing leaving the box; a surface with a camera that never moves; binding (heights must be labelled).
- **Eval** (`tools/eval/prompts-3d.json`: 24 spatial questions, 12 that only tempt 3D; Opus high; judge v2 and sentences):
  - choice right 34/36 (before 12/36; flat questions stayed flat 12/12);
  - on the 3D questions: picture 2.88 to 3.46, accuracy 3.12 to 3.54, teaching 3.12 to 3.58, layout 2.62 to 2.83;
  - flat questions unchanged or a little better;
  - claims shown 62% vs 60% (noise).
- **Next:** the judge's main complaint is a small, crowded 3D view when formulas stack above it (a band that
  gives a space view more room, or formulas beside it), and tangent planes drawn as large clipped polygons
  (a local patch would read better).

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

## 2026-10-23: the `tutor` command, a voice picker, the handoff as a map

The user will use the tutor daily and come back to Claude with what they don't like.
- `bin/tutor`, linked from `~/bin/tutor`: `tutor` opens it (rebuilding the engine if needed), `tutor new`,
  `tutor claude` (the tutor in the background plus Claude Code in the repo with `--dangerously-skip-permissions`),
  `tutor restart | stop | log`.
- The browser picks the voice (the server lists Inworld's English voices, `GET /tts/v1/voices`, once per run) and
  the speed; the stage restarts on the current scene. Model, thinking, voice and speed persist in
  `chat/settings.json` (gitignored); flags win.
- `docs/HANDOFF.md` is now a map of the current state; the old chronological handoff became this file.
  `CLAUDE.md` loads the map and asks each session to update it with every commit.

## 8j. 2026-10-23: pacing after an algebra step (research and plan only, no code)

The user: after an algebraic step the tutor should pause a moment and say it again so it lands, the way 3Blue1Brown
and Professor Leonard do, but not after every step. Asked for the regressions, the audit, research and a plan.

Regressions: test, check, golden green; **`make append` fails on two 3D scripts** (space-saddle in chunks of 2,
space-downhill in chunks of 2, 3, 5; 152 identical, 4 different), reproducible on a clean tree, not touched here.
Audit of the last 8 chats: worst 75.1 (code over a formula, 20261022-211954), the rest 82–100.

Found: a `Say` blocks until its audio ends, an uncued morph blocks until it lands, and the next `Say` starts at
once: 0 s at rest with the finished formula. `wait Ns` exists, is blocking and decodable, and Claude has never
written one (0 in 59 turns); the prompt only says "Don't use `pause`". In the real chats 108 of 495 sentences
change a formula; 42% are followed by another formula sentence, 70% by a sentence that moves on; the rationals
chat has two sentences with three formula changes each.

3Blue1Brown measured: one transform per play 73% of the time, a 1 s wait after 66%; captions: 205 wpm, median
gap 0.62 s, 21% of gaps ≥ 1 s, 6% ≥ 2 s, 3% ≥ 3 s, the long holds after statements of a result; restates with
"in other words" and a new framing, never the same words. Research: system-paced 2 s pauses at segment
boundaries help novices and cost experts nothing (Spanjers 2010); time added at the end does not (Hasler 2007);
label the subgoal (Catrambone); a tutor repeating itself verbatim does not help, prompting or re-framing does
(Chi 2001). Write-up with sources: `research_notes/Pacing/`.

Plan proposed (awaiting the user's yes): one formula change per sentence and an automatic 1 s rest after a formula
sentence (engine); a digest sentence that reads the result off the stage in other words plus `wait 2s` after a
subgoal step, a prompt or re-framing plus `wait 3s` after the crux, none at `expert` (prompt + toolbox); a gap
metric from the engine's say/end events, a `prompts-algebra` set, pre-registered bar. No axiom bent.

## 8k. 2026-10-23 (later): the beat after an algebra step, built; the language named Dan

The user said yes to the plan (8j) and asked for the markup to have a name and its own extension.

**Built.** Engine: after a sentence whose beat created or changed a formula, a 1 s rest with nothing moving before
the next sentence (`kRest`, `quiet_`, `restDue_`); an explicit `wait` counts toward it; the subtitle stays up
through a rest or a wait; a `timeline` log (say start, say end, formula settled) in `--layout-report`; `indicate
f[part]` names a part for the `\textcolor` lint (the digest's pointers tripped it: 16/40 check failures, now
the baseline's 3/20). Prompt: "Pacing a derivation" (one step per sentence, digest + `wait 2s` after a subgoal
step, question or re-framing + `wait 3s` after the crux, none at expert, label routine steps), a `beats:` plan
line, `wait` allowed only as that beat. Toolbox: "A derivation that grows, with a beat after the step that
matters". Tests: the rest (three cases plus the wait), the lint; toolbox 31/31.

**Measured** (`tools/eval/pacing.mjs`, `prompts-algebra.json`, two runs each, opus xhigh; table in
`research_notes/Pacing/`): changes per formula sentence 1.18-1.23 -> 1.00-1.05 (2+ in a sentence 15-21% -> 0-4%);
rest after a formula sentence median 0.02 -> 1.02 s (>= 0.8 s: 1-2% -> 98-100%); gaps >= 2 s 0-1.8% -> 14-16%
(above the 4-10% bar: 1.7 beats per 12-sentence answer); judge's teaching 4.05 -> 3.88 (noise ±0.15), binding
lints 18 -> 28-30 per 20 scenes; check-ok equal after the lint fix. Golden: the old placer caught a transient
overlap on space-determinant once the timing shifted; re-baselined. `make append` still fails on the two 3D
scripts from before.

**Dropped / not done:** a structural word (`beat`) instead of seconds, until the seconds version is watched; a
judge criterion for pacing (the judge can't see time); an editor grammar for `.dan`.

**Dan.** The markup is called Dan; files end in `.dan` (scripts, examples, exemplars, idioms renamed; new chats
and library saves write `.dan`, older `.txt` chats still open; golden baselines re-keyed).

## 8l. 2026-10-24: `tutor write`, a playground for the language

The user wanted to write `.dan` by hand and see it appear live, to learn the language by doing. Built
`tools/write.mjs` + `tools/write.html` (`tutor write [file]`, port 4748): an editor in the browser with Dan
highlighting, a line gutter, an examples menu (scripts, worked examples, every toolbox move), replay / skip / send
buttons, and a side panel with the engine's check (lints, layout score, decodable) and the readback, refreshed
600 ms after typing stops. The stage is `tutor --stdin` (muted unless `--voice`): whole lines typed at the end
are appended and play live; an edit earlier in the file writes the unchanged prefix to a hidden file, `#!load`s
it, `#!interrupt`s (instant to that state) and appends the rest, so the changed part animates from the right
state. The line being typed waits for its newline (Cmd+Enter sends it). Exercised through the API: append,
partial line, forced partial, earlier edit, load, skip, replay, SSE events. No engine change. Also written, not
committed: a personal HTML map of the architecture and the language for the user (on their Desktop).

## 8m. 2026-10-24: the axiom audit, and the fixes that make a difference

The user asked for a session to "get to the bottom of everything": verify the markup against the axioms. Findings
from their hands-on questions went into session notes; then eight auditors (one per axiom, the position rule, and
the language against nine research-backed principles for AI-written markup) each read the code read-only and
wrote reproducers; every violation was re-run before it was believed. 22 verified violations; 11 design decisions.
For each decision an engineer implemented and tested a solution in its own worktree; for D9 an independent twin
did the same and the better one (B) was picked. The twins for the rest were stopped to save the weekly limit.

Merged today, each with a test that fails on the old engine and every regression green:
- Scales see computed values (prescan by evaluation; live widening at a sentence start, eased, linted). D5.
- A thing rides a curve: `dot on=f x=2`, `on=S x= y=`, `on=orbit t=`; the derivative example uses it. D1.
- A cue line written one beat early waits for its word; it ran at the end of the previous sentence (a real
  "dead end" cross 5.9 s early). Two toolbox moves taught the bug; fixed with six scripts.
- A curve morphed to another function blends instead of vanishing (a real Fourier lesson lost its curve).
- Every re-mark eases (expressions, links, groups) through one updateProps().
- Uncategorised data on one plot shares one ink (a dice histogram was confetti). D4.
- `path from=a to=b` is read as the link it means. D9 (engineer B).
- A second kind word is a category, a stray duration is dropped (both with notes); a shape the schema can't draw
  is a `draw:` axiom warning.
Measured: golden unchanged except one legacy-placer transient (re-baselined, cue timing); append the 4 known
space-* comment differences; 1 of 35 real lessons gains a note (a line that used to be lost now draws).
Left: the patches listed in HANDOFF §9; the audit's tier-2..4 list (colour clashes, readback carrying scales and
views, \val{} jiggle, focus clearing on idle, label hysteresis, checker false alarms).

## 8n. 2026-10-24 to 10-26: the rest of the audit session, attachments, video

After the fixes in 8m the user asked for two features, then closed the session.
- **Images and PDFs in the chat** (065d0de): paste, drag-and-drop or 📎 in the browser; photos scaled to 1600 px;
  kept in the chat's `attachments/` and sent to Claude as image/document blocks (claude -p stream-json accepts
  `document` blocks with a base64 PDF; checked first). Tested end to end on a headless server: Claude named a word
  inside an attached PDF and described an attached plot.
- **Save video** (16ecfcb): a header button records the whole lesson (all turns) as an mp4 with the narration into
  `~/Movies/Tutor`. `--record` now renders in a hidden window, unpaced, without the key hints, progress bar or
  dictionary caption. A 35 s lesson renders in 6 s (voice from the disk cache), h264 + aac.
- **The audit's material** was copied from the session scratchpad into `reports/Axiom audit 2026-10-24/` (merged
  findings, session notes, the eight audits, the principles, the enforcement map, and the seven unmerged decision
  patches with their engineers' notes and a status table). Process notes and the usage-limit lesson are in its README.
Earlier in the same session (8j-8l): pacing after algebra steps, the language named Dan (`.dan`), `tutor write`,
and a personal HTML guide to the architecture on the user's Desktop (`~/Desktop/dan-guide.html`, not in the repo).
Left: see HANDOFF §9 "Finish the axiom audit".

## 8o. 2026-10-27: lecture mode

The user asked for a second mode: give it files (PDF, JSON, markdown, anything) and it prepares a lecture of 30
minutes or more, scaled to the material, split into parts, built step by step with as much time as it needs, played
in the stage window (not an mp4) so they can pause and ask. Mid-session they added: notes to follow on paper
(markdown with mermaid and proper LaTeX, questions), exported to PDF by a program.
Decided with the user: a question is answered **on the stage** (an aside, then back to the moment), each part ends
with a **short check**, building is **quality first**.
- Research (`research_notes/Lecture mode/`): long generated lessons work as planner -> coder -> a critic that looks
  at frames (Code2Video; TheoremExplainAgent's videos mostly had layout problems without one; PresentAgent);
  segments of a few minutes (Mayer's segmenting; engagement falls past ~6 min, Guo et al. 2014), names first
  (pretraining, median d = 0.75), a test between segments halves mind wandering (Szpunar et al. 2013).
- Built: `chat/ingest.mjs` (the chat now takes text files too: they were dropped silently); `tools/notes/
  notes-pdf.mjs` (Chrome prints; tested with a broken diagram reported, not dropped); `chat/lecture/` (plan, build
  loop, package, notes, player, CLI, browser page); engine: `seekStep`/`seekPart`/`backSentence` (fast-forward by
  the instant mode interrupt() uses, so the picture is the script's own), keys `←` and `shift ←/→`, a part caption,
  `#!load F LINE`, `#!seek`, `step.txt` in captures, events `part`/`seek`/`waiting`, `--prepare-voice`.
- Changed from the plan: no new `check` verb. A part ends with its question and the existing `pause`; the player
  knows each check (question, answer, why) from the outline. The language did not change.
- Measured on the Transformer paper (15 p, 20 min asked): the plan took 180 s (70K in, 18K out) and was good
  (8 parts, a bible with exact toy numbers; it found the paper's own 41.8 vs 41.0 BLEU contradiction). Part 1:
  lints 8 -> 2 -> 1 -> 0 in three fix rounds (~1 min each, resumed sessions); reviewer 3, 3, 3 over two rewrites
  (each fixed some problems and made others); kept v4; 16 min, ~785K input (540K cached), ~90K output. Its
  reviews were specific and mostly right (smeared send copies, faint roadmap, 0-based slots for "the third word").
  Two were about our setup: the window's chrome in snapshots (the prompt now says to ignore it) and made-up
  illustrative numbers (the part prompt now says to label them).
- Regressions: make test (51 engine cases, 2 new), check 124 ok, golden all same, append the 4 known space-*
  differences, toolbox/compute ok, tests/lecture.mjs 13 ok, tests/notes.mjs 6 ok.
Left: the user decides on parts 2-8 after seeing part 1; asides and checks are untested with a person (HANDOFF §9).

### 8o, continued: 2026-10-28, the first whole lecture
The user watched part 1's frames and said go on, with thinking "longer" (xhigh). Parts 2-8 built from 02:23 to
06:05 (3 h 42 min for 7 parts: 27-37 min each), then voice and a 33-page notes PDF (8.6K words, 9 mermaid
diagrams, no print problems). Lecture: 35 min of narration against 20 asked (every part ran longer than its
plan). Reviewer on the kept versions: 3, 4, 3, 4, 4, 3, 3, 3. Pattern: the first clean draft scores 3, the first
rewrite 3, the second rewrite 4 in three parts of seven; the best version is kept, so a failed rewrite costs only
time. Found and fixed: `--layout-report` printed `nan` for an empty stage, which broke the builder's reading of
part 6's reports (four of its six versions evaluated blind, one scored 1/5 for "no narration"); the engine now
prints 0 and the builder tolerates nan. The summary showed the last attempt's score instead of the kept version's
(fixed). Cost of the whole lecture from the session logs: see the commit message.

## 8p. 2026-10-28: the tutor from any Claude chat (MCP)

The user asked what the easiest interface is: they chat with Claude in Claude Code, the desktop app and the
browser, and want to say "use the engine" without pasting the conversation. Research: MCP servers give tools to
Claude Code and the desktop app (local stdio); skills teach Claude when to use them (Claude Code only); plugins
only bundle; claude.ai in the browser can't reach a local program. Built `chat/mcp/server.mjs` (SDK 1.32): eight
tools over the running tutor's HTTP API; the chat's Claude writes a brief (the conversation in the learner's words)
and the tutor's own Claude writes the scene, so the prompt, examples and checker are inherited without loading
them into the chat. `/api/send` takes `handoff: true` (a note tells the tutor where the brief came from). Found
in testing: MCP clients time a call out at 60 s, so `show` returns at the first spoken sentence (4 s in the test)
and the stage goes on; a transient "SSL certificate has expired" from `claude -p` once failed a turn. Tried from
this Claude Code session: a brief about the lecture build loop became a 7-sentence explanation with the loop drawn.
`tutor mcp install` registers it (Claude Code user scope; the desktop config after a y/N). `tests/mcp.mjs`: 7
checks over a stub. Left: the desktop app untried by the user; the browser needs a tunnel.
Then the user asked for two things: the browser too, and not loading the tools in every Claude Code session.
Research: claude.ai custom connectors take a remote MCP URL reachable from Anthropic's servers, authless allowed
(support.claude.com); Claude Code has `--mcp-config FILE` for one session. Built `chat/mcp/http.mjs` (the same
tools over Streamable HTTP, stateless, a secret path) and `web.sh` (Tailscale Funnel on/off; the Mac already has
Tailscale, stopped), `tutor claude --tools`, and `install` now writes `mcp.json` and removes any user-scope entry.
tests/mcp.mjs: 9 checks (two over HTTP). The browser path is untried end to end: Tailscale was stopped.

