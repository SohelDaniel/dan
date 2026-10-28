# Dan

**A visual AI tutor that explains by drawing and speaking, in the spirit of 3Blue1Brown.**

You ask a question. Claude answers in a small declarative markup called Dan. A C++20 engine built on raylib
draws it, animates it, narrates it with streaming text-to-speech, and reads it back to check that the picture
means what the words say. Claude never gives a pixel: the engine owns layout, timing, colour, scales and the
camera, so the picture stays honest.

<p align="center"><img src="docs/images/stage.png" alt="Six frames from the stage: a derivative as a limit, backpropagation through a graph, a saddle surface in 3D, gradient descent over a contour field, kinetic energy as a formula and two bars, an integral as accumulated area" width="100%"></p>

```
title "How a bank transfer works"
mark alice bar account balance=100 label="Alice" unit=$
mark bob   bar account balance=20  label="Bob"   unit=$

Say: "Alice has a hundred dollars. Bob has twenty."
Say: "Alice sends Bob thirty |go|."
@go link pay arrow alice -> bob amount=30
@go morph alice.balance 100 -> 70 1.5s
with morph bob.balance 20 -> 50 1.5s
Say: "Nothing was created or destroyed. Thirty dollars just moved."
```

Three things happen when this plays. The two bars share one zero and one scale, fixed before the first frame.
The arrow draws itself on the word *thirty* and both bars ease to their new heights together. At the end,
`--readback` prints the scene in the same vocabulary, with what changed, so a checker can confirm that the
picture is the one the markup describes.

## What it does

| | |
|---|---|
| **Ask anything** | Type on the stage window or in the browser console. Claude plans the explanation, writes Dan one beat at a time, and the stage plays each line while the next is still being written. Formulas are typeset with LaTeX, with one step per sentence and a rest after it. |
| **Pause and ask** | Press space, type a question about what is on screen, and get a spoken aside about the paused frame. |
| **Lectures from files** | Give it a paper, notes, code or a JSON file. It plans a lecture in parts of a few minutes, builds each part in a write, check, review, rewrite loop, fetches the voice ahead, and plays it on the stage with checks for understanding at the end of each part. It also writes notes to print: Markdown with LaTeX and mermaid, exported to PDF. |
| **From any Claude chat** | The tutor is also an MCP server. In Claude Code, the Claude desktop app, or claude.ai in the browser, say "show me this on the stage" and the chat's Claude hands a brief to the tutor, which explains it the way it always does. |
| **Save a video** | One click renders the lesson to mp4 with the narration. |
| **Write Dan yourself** | `tutor write` opens an editor beside the stage. What you type plays as you type it, with the lints and the readback alongside. |

## The axioms

The engine enforces six rules, so that a picture can never lie about the words.

1. **Identity.** There is no delete. A re-mark updates in place, and a morph from a wrong value animates from the real one. Nothing jump-cuts.
2. **Structure.** Baselines and scales are shared and fixed for the whole script by a prescan. They never jump mid-explanation.
3. **Zero.** Nothing in the picture without a meaning. Unrelated things never share a colour, and two things touch only when the script relates them.
4. **Continuity.** Every change is an eased morph. Things grow in, links draw themselves, focus fades.
5. **Bounded focus.** `focus` lights one thing for one sentence, then everything returns.
6. **Decodability.** `--readback` prints the scene as markup, and `--check` re-parses it to the same scene.

The rule that follows from them: **position comes from structure.** There are no placement words. Columns come
from links, order from declaration, lanes from groups, and a layered flow, a label placer and an exact overlap
checker decide where everything goes. See [Axioms, enforced](#axioms-enforced) for how each one is kept.

## How it works

```
 you ─── question ───▶ Claude (plan, then Dan, one beat at a time)
                            │
                            ▼  lines, streamed
                      ┌───────────┐   readback + lints   ┌─────────┐
                      │  engine   │ ───────────────────▶ │ checker │ ──▶ fed back to Claude next turn
                      │ (raylib)  │                      └─────────┘
                      └───────────┘
                        │       │
                 stage window   Inworld TTS (word timings drive the cues)
```

- **Prescan.** Before the first frame, the whole script is run instantly in a shadow engine, so every scale and
  baseline is known, and values that are computed later are already counted.
- **Layout per frame.** Ranges, then the band of formulas, then the schema layout of bodies (free, number line,
  axes, timeline, graph, 3D space), then pointers and attachments, easing, and finally labels placed map-style with
  hysteresis. Drawing only reads the results, and the checker sees the same boxes.
- **Voice.** Inworld's streaming TTS returns word timestamps. `|cue|` words in a `Say` line fire the next drawing
  line on the word itself, and a spoken number or name gets a pointer on the thing it names.
- **Lectures.** A planner writes an outline and a bible of notation and running examples. A writer builds each
  part. `--check` lints it (up to three fix rounds), a separate reviewer scores frames and the timeline, and the
  part is rewritten below 4/5 (up to twice). The best version is kept. Research behind the loop is in
  `research_notes/Lecture mode/`.
- **MCP.** `chat/mcp/server.mjs` exposes `show`, `ask`, `stage`, `notes`, `lecture`, `lecture_status`,
  `lecture_build` and `lecture_play` over stdio (Claude Code, the desktop app) and over Streamable HTTP
  (`http.mjs`, for claude.ai through Tailscale Funnel on a secret path). The chat's Claude writes only a brief; the
  tutor's own Claude writes the scene, so the 67K-character teaching prompt never loads into the chat.

## Getting started

Requirements: macOS or Linux with a C++20 compiler, [raylib 6](https://www.raylib.com), Node 22+, and
[Claude Code](https://claude.com/claude-code) signed in (the chat runs `claude -p` on your subscription). For the
voice, an [Inworld](https://inworld.ai) TTS key in `./.inworld_key`, `$INWORLD_API_KEY`, or `--inworld-key`.
Without one the tutor still plays, with timing estimated from the words.

```sh
brew install raylib pkg-config node          # macOS; on Linux, raylib from your package manager or source
make && make tex                             # ./tutor, and MathJax for formulas (once)
ln -s "$PWD/bin/tutor" ~/bin/tutor           # the launcher

tutor                 # build if needed, open the stage and the browser console on your last chat
tutor new             # a fresh chat
tutor write           # the Dan playground: an editor beside the stage
tutor claude          # the tutor in the background, Claude Code in the repo
```

The engine by itself:

```sh
./tutor scripts/derivative.dan                 # play a script, narrated (edit it while it runs: it hot-reloads)
./tutor scripts/derivative.dan --mute          # no voice
./tutor scripts/derivative.dan --record out.mp4
./tutor scripts/derivative.dan --check         # decodability and axiom lints, exit 1 on failure
./tutor scripts/derivative.dan --readback      # the final scene as markup
./tutor scripts/derivative.dan --snapshot out --at 3,8,end    # PNG frames from a hidden window
./tutor scripts/derivative.dan --layout-report -              # JSON: layout score every 0.5 s, the timeline
./tutor live.dan --stdin                       # live mode: lines piped in continue the running scene
```

Keys on the stage: `space` pause, `→` skip, `←` back a sentence, `shift ←/→` previous or next part, `R` restart,
`D` the dictionary caption, `S` screenshot. Any other key opens the prompt bar.

### Lectures

```sh
tutor lecture new paper.pdf notes.md --minutes 30 --ask "focus on the intuition"
tutor lecture show        # the outline and the bible: read it, edit the JSON if you like
tutor lecture build       # every part: write, check, review, rewrite; keeps the best version
tutor lecture notes       # notes.md and notes.pdf (LaTeX + mermaid)
tutor lecture play        # on the stage; type to ask, answer the check at the end of each part
```

The same from the browser at `/lectures`. A build takes about four minutes per lecture minute at the default
thinking level. The first lecture made this way, from the Transformer paper, was 8 parts and 35 minutes with
33 pages of notes.

### From another Claude chat

```sh
tutor claude --tools      # Claude Code with the tutor's tools, this session only
tutor mcp install         # the Claude desktop app (once; then quit and reopen it)
tutor mcp web on          # claude.ai in the browser: a Tailscale Funnel URL to add as a custom connector
tutor mcp web off
```

### Notes to PDF

```sh
node tools/notes/notes-pdf.mjs notes.md out.pdf     # Markdown with $LaTeX$ and ```mermaid blocks, A4
```

## Layout of the repository

| path | what |
|---|---|
| `main.cpp` | the CLI: play, `--check`, `--readback`, `--snapshot`, `--layout-report`, `--record`, `--stdin` |
| `markup/script.*` | the parser: verbs, kinds, schemas, `[id]` pointers and `{expr}` in `Say` |
| `markup/engine.*` | model and timeline: steps, cues, morphs, history, readback, lints, the prescan, the shadow engine |
| `markup/layout.cpp` | per-frame layout: ranges, the formula band, schema layouts, pointers, easing |
| `markup/place/` | pure C++, unit-tested: layered graph flow, VPSC-style separation, the label placer, springs, the exact checker |
| `markup/placement.cpp`, `binding.cpp`, `annotate.cpp`, `area.cpp`, `fields.cpp`, `trails.cpp`, `camera.cpp`, `controls.cpp` | the engine's side of placement; words vs picture lints and pointing; notes, areas, fields, trails, the 2D camera, learner sliders |
| `markup/space.cpp`, `markup/viz/` | 3D: one CPU projection, shapes, a camera in words, painter's order |
| `markup/render.cpp` | drawing only, reads layout results |
| `markup/tts.*`, `markup/tex.*`, `tools/tex/` | Inworld streaming TTS (48 kHz, word timings, disk cache); LaTeX through MathJax to glyph outlines |
| `rm/raymanim.*` | a small Manim-style animation library on raylib: mobjects, `Create`, `Transform`, rate functions, trackers and updaters |
| `chat/server.mjs`, `chat/index.html` | the chat: one persistent `claude -p` per chat, fences to beats, SSE transcript, interrupts, background `--check` |
| `chat/prompt.md`, `chat/toolbox.md`, `chat/examples/`, `chat/system-prompt.mjs` | the teaching protocol, 27 checked moves, annotated examples, and the prompt builder (the language section of this README is part of it) |
| `chat/lecture/` | lecture mode: store, plan, build, package, player, routes, CLI, and the prompts |
| `chat/mcp/` | the MCP server (stdio and HTTP), the desktop app registration, Funnel on and off |
| `chat/explain.mjs`, `chat/compute.mjs`, `chat/ingest.mjs` | pause and ask; sandboxed python blocks; files the learner uploads |
| `tools/write.*`, `tools/notes/`, `tools/eval/`, `tools/mine/` | the playground, notes to PDF, the evaluation harness (Claude writes, a judge scores frames), mining 3Blue1Brown's code for idioms |
| `tests/` | doctest units (place, viz, parser, engine), golden layout scores, streamed-equals-whole, and Node tests for the toolbox, lectures, notes and MCP |
| `scripts/*.dan` | 39 example explanations |
| `docs/` | `HANDOFF.md` (the map for anyone working on it), `history.md` (what was tried and measured), `guide.html` |
| `reports/`, `research_notes/` | research write-ups with sources: the axiom audit, overlap, pacing, spoken-but-not-shown, lecture mode |

## Testing

```sh
make test        # unit tests: place, viz, parser, engine (97 cases)
make check       # --check over every script in scripts/
make golden      # layout score per script against tests/golden/*.json
make append      # streamed playback == whole playback
node tests/toolbox.mjs && node tests/lecture.mjs && node tests/notes.mjs && node tests/mcp.mjs
```

Measuring with Claude in the loop (`tools/eval/`): Claude writes scenes for a question set, a judge scores
frames 1 to 5 on picture and teaching, and `sentences.mjs` asks whether each spoken claim is shown. The judge's
noise is about ±0.15, so changes are run twice before being believed. Numbers from the history: on 20
derivations, sentences with two or more formula changes went from 15-21% to 0-4%; on spatial questions the 3D
schema raised the picture score from 2.88 to 3.46.

## Status

Working and used daily by its author. Built in a few weeks of research-first sessions with Claude: every
feature started as a write-up with sources (`reports/`, `research_notes/`), got a baseline, a test that failed on
the old engine, and a look at the frames. `docs/history.md` is the full log, with the numbers and their noise, and
`docs/HANDOFF.md` lists what is open.

## License

MIT. `third_party/` holds nlohmann/json and doctest (MIT). Narration uses Inworld's TTS API under its own terms.

## The language

The language is called Dan; its files end in `.dan`. One line, one statement. `#` starts a comment.

### The six verbs

```
mark  <id> [kind] [category] key=value ...          a thing and its properties
link  <id> [kind] <a> -> <b> key=value ...          a relation: arrow (cause, maps-to), line (connected), similar, equals
group <id> part-of=[a,b,...] label="..."            a relation: part-of
morph <id>.<prop> [from] -> <to> [1.5s] [ease=..]   a change; the only way anything moves
focus <id> | none                                   what the voice is about (everything else dims, for one sentence)
Say: "text with |hook| markers"                     the voice
```

Pointing and visibility:

```
indicate <id> | <array>[k]     a ring pulses on it: point at what the voice names (not part of the model)
send <from> -> <to> [1s]       a copy travels from one thing (a cell a[2], a formula part f[x], a thing) to another,
                               which pulses as it lands: where a value comes from and where it goes
send x -> y via=<link>         the copy travels along that link (a token through a network)
indicate a[0:4] stagger=0.4s   the cells pulse one after another: counting, going through items in order
send a[0:3] -> t stagger=0.3s  each cell's copy goes in turn (parts poured into a total); onto a span as long
                               (send a[0:3] -> b[0:3] stagger=0.3s) each goes to its own cell
hide <id> [0.6s]               fade it to a faint ghost (it stays a thing: opacity=0.12); a note (brace, enclose,
                               cross, …) fades out completely: a faint cross would still say "wrong"
show <id> [0.6s]               bring it back (opacity=1)
```

Modifiers:
- `with <line>` runs alongside the previous line.
- `@hook <line>` waits until the voice reaches `|hook|`.
- `[id]` glued to a word points at that thing as the word is spoken: `Say: "each [w11]weight is learned"` pulses `w11` on "weight". It is the cheapest way to point; use it whenever a sentence names something on stage.
- `{expr}` in a Say speaks a value off the stage: `Say: "That gives |h|{h}, so cat goes in slot {h}."` says the value `h` has when the word is reached (after the lines cued on that word; a morph counts at its end value), and pulses `h` on that word. `{h.y}` reads a property, `{a + b}` an expression, as in properties and `\val{}`. The voice can't say a number the stage doesn't hold, so speak stage values this way rather than typing them. A value read off the stage still has to be readable on it: a label or a value on the thing.

### Annotations: point at a part

An annotation is a thing about other things. It sits where its target is (below something wide, beside something tall, on the side its arrows don't use) and moves with it.

```
mark s brace of=f[wx] label="weighted sum"      # a brace under the part of formula f written \textcolor{wx}{…}
mark n brace of=[x1,x2,x3] label="3 inputs"     # along several things
mark r enclose of=a[2] label="pivot"            # a box around a thing, a cell, or a part
mark u underline of=a[1:3]                      # a line under cells 1 to 3
mark c cross of=guess                           # wrong (red)
mark k check of=answer label="right"            # right (green)
```

A target is a thing, an array cell or span (`a[k]`, `a[i:j]`), or a named part of a formula (`f[id]`, the glyphs inside `\textcolor{id}{…}`; the name needs no thing of its own). Mark the annotation on the word that names the part (`@sum mark s brace of=f[wx] …`), and `hide` it when the sentence moves on.

A bare `|go|` fires where it sits in the sentence. A marker attached to a word, like `thirty|go|`, fires when that word is spoken. A marker belongs to its sentence: it fires the `@go` lines between that `Say` and the next one, so the same name can be reused sentence after sentence. Names are one word and match regardless of case.

Re-marking an existing id updates it in place, with numbers easing to their new values. It never errors. There is no delete verb (axiom 1).

### Links carry names and values

A link is a thing too: give it a name, a value, a colour, and point at it like any mark.

```
link w11 arrow x1 -> h1 label="w₁₁"                 # a name on the arrow
link w11 arrow x1 -> h1 label="w₁₁" w=0.5           # name and live value: reads "w₁₁ = 0.5"
@learn morph w11.w -> 0.8 1s                         # the number on the arrow changes as it learns
link c1 arrow x1 -> y1 color=yellow                  # same colour = the same thing (a shared weight)
mark f tex tex="y = \textcolor{w11}{w_{11}}\,x_1"     # a symbol coloured like its link
```

Colours by name: blue, orange, green, red, yellow, purple, teal, pink, brown, lime, white, grey. `\textcolor{yellow}{…}` works in formulas too.

### Mark kinds

- Data: `dot` `bar` `box` `label`.
- On axes or plane: `line` `arrow` `path` `region`, plus the world shapes below.
- Blocks: `tex` `card` `code` `array` `table`.
- `param`: an invisible number that drives others (a clock, a constant). With `set=learner` (and `range=[a,b]`) the learner sets it: a slider under the stage, and what it drives follows as they drag.

If you leave the kind out, the schema picks one: a bar if the thing has a magnitude, otherwise a dot. Graph scenes use boxes.

### World shapes (schema `plane`, or `axes`)

`schema plane x=[a,b] y=[c,d]` is a 2D world where one unit is the same length across and up. Circles are round, angles are true, and things move by morphing their `x`/`y`. Options: `grid=false`, `axes=false` (a clean world). A scene that only uses world shapes gets `plane` on its own. For motion, give the ranges, so moving things stay in view.

```
mark c  circle x=3 y=2 r=1 label="r = 1"
mark k  rect cart x=2 y=1 w=1.4 h=0.9 shape=cart label="1 kg"      # shape=block (default) | cart
mark t  polygon pts=[0,0, 4,0, 1,3]                                 # x,y pairs; each may be {expr}
mark A  angle pts=[0,0, 4,0, 1,3]                                    # vertex, then a point on each ray; shows degrees
mark F  vector force at=k side=right dx=1.5 label="F"                # from a thing (centre or side=left|right|top|bottom)
mark u  vector x=0 y=0 dx=4 dy=1 label="u"                           # or from x y; to: dx dy | to=[x,y] | to=<id>
```

A vector anchored with `at=` rides along when its thing moves.

```
mark G grid space m=[1,0,0,1]                                        # the plane's grid under a 2x2 matrix [a,b,c,d]
@go morph G.m -> [1,1,0,1] 2s                                        # the whole plane shears, smoothly
```

**Lists morph too.** `morph sq.pts -> [...]`, `morph G.m -> [...]` and `morph u.to -> [...]` glide number by number (same length). Re-marking an `array` with new `items=` sends each value travelling from its old cell to its new one (a swap visibly swaps).

**Motion with a clock.** Drive positions from a hidden `param` and morph it linearly. This is how physics plays out:

```
mark clock param value=0
mark k rect cart x={1 + 0.5*3*clock^2} y=1 w=1.4 h=0.9 shape=cart     # x = x0 + ½·a·t²
@go morph clock.value 0 -> 2 2.5s ease=linear
```

### Trails: where something has been

Add `trail=` to anything that moves on `axes` or `plane`: `trail=1.5s` leaves a fading tail (a faster thing leaves a longer one, so speed shows); `trail=all` keeps its whole path. Orbits, projectiles, pendulums, the steps of gradient descent.

```
mark p dot x={2*cos(clock)} y={2*sin(clock)} trail=1.5s label="planet"
mark gd dot x=-2.6 y=1.4 trail=all label="you"                       # re-mark x= y= step by step: a zig-zag with exact corners
```

### Fields: a function of x and y, everywhere at once (schema `plane`, or `axes`)

Write fields in terms of `x` and `y` (and any `param`, so they morph). Use `*` between letters: `x*y`, not `xy`.

```
mark E field vx={-y} vy={x} label="flow"                 # arrows: the vector at each point (zero vectors vanish)
mark E field vx={-y} vy={x} style=lines                  # streamlines: the paths things follow (best for reading a flow)
mark E field vx={-y} vy={x} style=flow                   # streamlines with motion along them
mark G field fn={x^2 + 4*y^2} style=lines                # fn= instead of vx/vy: the gradient of a function
mark S slope fn={x - y}                                  # a slope field for dy/dx = f(x, y) (schema axes, x across)
mark s1 solution of=S from=[0,1]                         # the solution curve through a point (also of=<field>: a particle's path)
mark L heat fn={x^2 + 4*y^2}                             # colour for value: dark low, bright high (blue/red if it changes sign)
mark C contour fn={x^2 + 4*y^2}                          # level curves, labelled; levels=log for valleys, levels=[1,2,4]
mark L heat fn={x^2 + y^2} label="f(x, y) = x² + y²"     # label= on a field is its legend: name the function you talk about
```

Heat and contour together make a landscape (a loss, a potential, a height map). A dot with `trail=all` walking downhill on it is gradient descent. `solution from=` morphs (`morph s1.from -> [0,2]`), so you can drag the starting point and watch the curve follow.

### The camera (axes and plane)

Say what to look at; the engine picks the path and the timing. Text and lines stay the same size on screen, and curves are redrawn for the view, so zooming into a curve really shows it becoming straight.

```
camera zoom=8 on=p                    # eight times closer, centred on p
camera zoom=50 on=f x=1               # on the curve f at x = 1 (the derivative, seen)
camera on=q                           # pan to q, same zoom
camera fit=a,b                        # frame these things
camera follow=p                       # keep p in view as it moves (camera follow=none to stop)
camera reset                          # back to the whole scene
```

One camera move per sentence, cued on the word (`@in camera zoom=8 on=p`), then let it rest. Zoom in to show a detail, then `camera reset` to give the context back.

### Space: three dimensions (schema `space`)

Only for ideas that live in three dimensions: a surface z = f(x, y), a gradient or tangent plane, the cross product,
a determinant as a volume, a 3x3 matrix moving space, rotations, solids, molecules, lattices, a loss over two
weights. Anything a flat picture shows stays flat: depth costs the learner (things hide, sizes deceive).

`schema space x=[a,b] y=[c,d] z=[e,f]` is a 3D world, z up. Give ranges that hold everything (a thing that leaves the
box is linted). Units are equal (spheres round, right angles right) unless one range is far from the others (then
each axis is scaled to the same length; `units=equal|box` decides). Options: `grid=false`, `axes=false`,
`xlabel= ylabel= zlabel=`.

```
mark p dot x=1 y=2 z=1 drop=true label="p"               # drop=true: a dashed line to the floor and a shadow (height, seen)
mark v vector dx=1 dy=2 dz=2 label="v"                    # from x y z (default the origin), or at=<id>; to=[x,y,z] | to=<id>
mark L line pts=[0,0,0, 2,1,3]                            # a segment (arrow: with a head)
mark h path xt={cos(t)} yt={sin(t)} zt={t/4} range=[0,12]  # a curve in space; it draws itself
mark S surface fn={x^2 - y^2} label="z = x² − y²"          # over the x, y range, cut where it leaves the box
mark T surface xt={cos(u)*sin(v)} yt={sin(u)*sin(v)} zt={cos(v)} u=[0,6.28] v=[0,3.14]   # parametric
mark p dot on=S x=1 y=1 drop=true label="p"               # on the surface: its z comes from S; morph p.x and it stays on it
mark B sphere x=0 y=0 z=0 r=1
mark P plane normal=[1,1,1] d=1                           # a x + b y + c z = d, the part inside the box
mark F polygon pts=[0,0,0, 1,0,0, 0,1,0]                  # a flat face (x,y,z triples): build solids face by face
mark G grid space m=[1,0,0, 0,1,0, 0,0,1]                 # the lattice and the unit cube under a 3x3 matrix; morph m
mark E field vx={-y} vy={x} vz={0.5}                      # arrows on a sparse lattice
```

The camera is said in words, never angles: `camera view=top|front|side|default`, `camera orbit=90` (turn around
the vertical), `camera tilt=20` (look more from above), `camera zoom=3 on=p`, `camera spin` (a slow turn that goes
on through the narration; `camera spin=off`), `camera reset`. **A surface needs motion**: a still picture of a
surface is ambiguous, so `camera spin` (or an `orbit`) while it is the subject. `view=front` or `view=top` to show a
right angle or a projection plainly.

Everything else works as on a plane: `morph p.z -> 3`, lists morph (`G.m`, `pts`, `to`), `trail=`, param clocks,
`indicate`/`send`/`focus`/`hide`, notes (`enclose of=p`), pointers (`at=p`), `[id]` and `{expr}` in a Say. Names and
numbers stay upright and readable; formulas stay in the band. **A height can't be read off a 3D picture**: a
number you say must be written on stage (a label with the value: `mark h label at=p label="height" value={p.z}` reads "height = 1.5").

### Blocks: text, code, data

Blocks stack in a band at the top, under formulas. With nothing else on a free stage, they are centred. On-stage text should be key terms and short statements, never the narration itself.

```
mark key card text="Each look halves what's left" label="Key idea"       # wrapped, max ~12 words
mark src code text="lo = 0\nhi = n - 1\nwhile lo <= hi:" line=1 label="search.py"
mark a   array items=[2,5,8,12,16] label="sorted"                        # cells a[0] … a[4]
mark lo  label text="lo" at=a[0]                                         # a pointer under a cell; re-mark at= to move it
mark win label text="window" at=a[1:3]                                   # a bracket under cells 1 to 3 (a span)
mark a   array items=[1,2,3,4] colors=[teal,-,teal,-]                     # cells that belong together share a colour
mark M   array cols=3 items=[1,2,3,4,5,6]                                  # a grid (a matrix, a DP table): rows of 3
mark d   area parts=[3,7] labels=[rain,dry] colors=[blue,-]               # a whole split by proportion: area is the quantity
mark d   by=[[8,2],[1,9]] bylabels=[umbrella,none,umbrella,none]          # each strip split again (d[i], d[i][j])
mark r   enclose of=M[1][:]                                               # M[r][c] a cell, M[r][:] a row, M[:][c] a column
mark t   table cols=3 cells=[step,lo,hi, 1,0,4]                          # first row is the header
```

- `\n` in `code` text starts a new line. `line=` highlights a line; `morph src.line 1 -> 3` glides the highlight.
- `focus a[3]` lights one cell and dims the others. `indicate a[3]` points at it.
- `at=<id>` on a label makes it a pointer to any thing.

### Properties

- A number is a **magnitude**. The first non-positional numeric property is drawn as size: bar length or dot area. One scale per property, fixed for the whole script (every value it will reach, computed ones too); `max=`/`min=` on a bar name a larger extent up front, for a value only a later turn will bring.
- A word is a **category**. The bare word after the kind (`mark alice bar account …`) or the first text property (`status=overdue`) is drawn as color.
- **Data on one plot shares one color.** Dots and bars placed by their data on a number line, timeline, axes, plane or space with no category are one series (twelve samples, twelve events: one ink). Give groups category words, or their own views, to tell them apart. Off a plot an uncategorized thing has its own color.
- Positional keys: `x y` (axes), `t` (timeline), `x1 y1 x2 y2` or `x y slope` (lines), `from to of` (regions; `n=6` draws the area as 6 slices, the Riemann picture), `fn` (paths), `domain=[a,b]`.
- Axis titles: `schema axes xlabel="time (s)" ylabel="speed (m/s)"`. Name what each axis means.
- Text keys: `label` (display name), `text`, `unit=$`, `color=blue` (overrides the palette).
- Look: `shape=circle|square|triangle|diamond|star|hexagon|ring` (dots; a second category channel), `r=` (fixed dot radius), `bend=40` on links (two-way links bend on their own).
- Parametric paths: `mark orbit path xt={2*cos(t)} yt={sin(t)} range=[0,6.2832]`.
- `{expressions}` make a value depend on others. Available: `{1+h}`, `{f(1+h)}` (calls path `f`), `{alice.balance - bob.balance}`, `{mean(score)}` (also `total` `maxof` `minof` `median` `count`), `{deriv(f, 1)}`, `{integral(f, 0, a.to)}`, plus `sin cos exp log sqrt abs min max pow pi`. A bare id means that thing's magnitude.

### Formulas

A `tex` mark is a typeset LaTeX formula (math mode, via MathJax). It is a thing like any other, so it can be focused, linked to, and morphed.

```
mark ke tex tex="E_k = \frac{1}{2}\textcolor{mass}{m}\textcolor{speed}{v}^2"
morph ke.tex -> "E_k = \frac{1}{2}mv^2 = \val{0.5*mass*speed^2}\,\text{J}" 1.5s
mark tag tex tex="E = v^2" x=4 y=30 size=30
```

- Write the formula in a quoted `tex=`, with single backslashes. Double quotes can't appear inside it.
- **Placement:** formulas stack in a band at the top of the stage and the schema lays out the rest below them. On axes, `x= y=` puts its centre at a data point. `size=` is the font size in px (default 38).
- **`\val{expr}`** shows a value now, with the same expressions as `{…}`: `\val{price}`, `\val{alice.balance - bob.balance}`. It updates live while the value morphs.
- **`\textcolor{id}{…}`** paints a term in the color of the thing `id` and keeps that term lit while `focus id` dims the rest, so the formula and the picture point at the same thing. Plain color names work too (`\textcolor{orange}{x}`). Don't use `\color`: it is a switch that colors everything after it.
- **Changing a formula** (`morph f.tex -> "…" 1.5s`, or re-marking with a new `tex=`) moves the symbols the two formulas share to their new places and fades the rest; what the change brings in is lit for a moment. Build an equation up step by step this way instead of making new marks.
- **A derivation is one formula that grows.** Write the steps as `\begin{aligned} 2x + 3 &= 11 \\ 2x &= 8 \end{aligned}` (the `&` lines the `=` signs up) and morph it to add each line; annotate the move on the part it changes (`enclose of=eq[three] label="take it away"`). Separate formulas side by side read as one run-on line.
- A formula is ink (foreground color) unless it has a category, `color=` or `of=`. It doesn't use up a color.
- `--check` typesets every formula and reports TeX errors and `\textcolor` names that are neither a thing nor a color.

### Layout comes from structure

You never say where a thing goes. Position is a channel of the dictionary, like color: the engine derives it from what the script already says.

- **Data** stays where its data puts it: axes, number line, timeline, plane.
- **Links** make a flow (schema `graph`): cause on the left, effect on the right, in columns. A link that skips columns is routed through the gaps; one that runs back (a cycle, a gradient) arcs. A hierarchy (every thing has at most one parent, and something branches: a call tree, a probability tree, a taxonomy) reads top-down instead.
- **Groups** (`group g part-of=[…]`) are lanes in a flow: their parts stay together in a box, and the stages of two lanes line up, so two processes can be compared side by side.
- **Order** of declaration is order on stage: left to right in a row, top to bottom in a column.
- **Formulas, cards, code, arrays and tables** stack in a band at the top.
- **Names and values** are placed beside their things by the label placer, clear of everything else.

Two words still place a thing, because they are relations:

```
mark lo  label text="lo" at=a[0]          # a pointer naming a thing (or a cell, or a span a[1:3])
mark tag label text="hot" on=sun          # touching on purpose: on= (on its top edge), in= / over= (on its centre)
mark k array items=[1,1,1] on=x[0:2]      # a row of cells seated over another's cells, aligned (a kernel on its input);
                                          # re-mark on=x[1:3] and it slides one step
mark q dot on=f x=2 label="Q"             # riding: a dot (or a label, or a vector's tail) on a curve, a line or a surface.
                                          # Give the free coordinate (x; t on a parametric path; x and y on a surface) and
                                          # the rest comes from the curve: morph q.x and Q walks along f, {q.y} and
                                          # \val{q.y} read its height, a vector at=q and a camera follow=q ride with it
```

`region= below= above= left-of= right-of= near= align= pos=` are retired: old scripts still play, the words are ignored, and `--check` says so.

### Setup lines (not part of the model)

```
title "..."
scene "Part 2"             # fade the stage out and start a fresh one (ids, colors, schema reset)
schema free | numberline [prop=score range=[0,100] bin=5] | axes [x=[a,b] y=[c,d] zero=false] | timeline | graph | sequence
wait 2s                    # a beat: nothing moves, nothing is said, the last words stay up (after a step that matters)
pause                      # wait for space / →
```

**Views: two pictures of one thing.** Name a schema and it becomes a view: the marks after it are laid out together, by that schema, in their own part of the stage. Views sit side by side in the order they are declared (stacked when one is a number line or timeline). A bare `schema net` selects a view again, for marks added later.

```
schema net graph label="the network"                       # boxes and links on the left …
mark x box input label="x"
schema loss axes x=[0,10] y=[0,4] label="its loss"          # … and the loss curve they produce on the right
mark L path fn={3*exp(-0.35*x)+0.3}
```

Each view has its own schema, so two plots can sit side by side with their own ranges (a signal in time beside its spectrum, two distributions). Formulas and cards still stack in the band above all of them.

**Conversations.** `schema sequence` puts its things (the parties) across the top, each with a lifeline, and draws every link as a message on its own row, in the order written: a handshake, a protocol, a request and its reply, who said what to whom.

**Histograms.** `bin=` on a number line drops each value into its bin (centred on multiples of `bin`), and a bin's samples stack into a column of dots: mark samples one by one or a beat at a time and the histogram grows (the central limit theorem, a die rolled sixty times). Samples have no names unless given a `label=`.

If no schema is given, one is inferred. Paths, regions or `x=` give `axes`. `t=` gives `timeline`. Boxes, or links between things with no magnitudes, give `graph`. Anything else is `free`.

## The dictionary, as implemented

| model | channel | where |
| --- | --- | --- |
| thing | a mark with a stable id | `doMark`; there is no delete |
| magnitude | bar length / dot area, from a shared zero | `layoutFree`: one scale per property, fixed by pre-scanning every literal and running the script ahead for the values it computes |
| category | color | `colorOf`: one palette slot per category; an uncategorized thing gets its own slot, except dots and bars placed by their data on one plot, which share that plot's slot (data on one plot is a relation, axiom 3) |
| order | position | the schema's axis; in a graph, the column a thing's links give it, then declaration order within the column (`place/flow`) |
| part-of | enclosure | `drawGroup`; in a graph, a lane: its parts stay together, and stages line up across lanes |
| connected / cause | line / arrow | `drawLink`: an arc over bars; in a graph, routed through the gaps between columns (never through a box), against the flow as an arc |
| equals / similar | dashed link marked `=` / `≈` | `drawLink` |
| name, value | short text | labels; the voice carries sentences |
| change | motion | `Active` morphs; categories cross-fade color |
| voice subject | focus | everything else eases to 20% |

## Axioms, enforced

1. **Identity.** There is no delete verb. A re-mark updates in place. A `morph … from X` that doesn't match the real value is warned about and animated from the real value, so there are no jump cuts.
2. **Structure.** Bars share a zero baseline. Scales are per property and never jump mid-script, because the whole script is pre-scanned and run ahead: the values it computes (`{expr}`, what a morph drives through one) count as much as the ones it writes. Live, where a later beat can bring a value outside the scale, the scale moves once, eased, at the next sentence start, and the stage says so (`structure:`).
3. **Zero.** Unrelated things never share a color. Two things share ink only when the script relates them (`of=`, `on=`/`in=`/`over=`, `at=`, a name and its thing, data placed on one plot), because touching reads as a relation. Everything else is kept apart by construction where it can be (the band of formulas never reaches into a plot; a note's name never sits on another part of a formula), pulled apart in graph layouts, and otherwise reported by the checker, which sees every view's plot, ticks and title too. Where position is data (axes, number line), an overlap is reported as a lint, because coinciding reads as "equal".
4. **Continuity.** Every change is a morph with easing. New marks grow in, links draw themselves, and focus fades.
5. **Bounded focus.** `focus` lights one thing (or a group and its parts) and dims the rest, for one sentence: the one it is cued in, or the next one if it stands between sentences. Then everything is at full strength again.
6. **Decodability.** `--readback` prints the final scene in the same vocabulary, plus derived facts (max, mean, order), overlaps, recent changes and lint. `--check` re-parses that readback and confirms it rebuilds the same scene.

Readback looks like this:

```
mark alice bar account balance=70 label=Alice unit=$
mark bob bar account balance=50 label=Bob unit=$
link pay arrow alice -> bob amount=30
# derived balance: max=alice min=bob mean=60 total=120 order: alice > bob
# since: morph alice.balance 100 -> 70 done; morph bob.balance 20 -> 50 done
```

## raymanim directly

```cpp
struct MyScene : rm::Scene {
  void construct() override {
    auto c = rm::Circle({640, 360}, 100, rm::col::blue);
    play(rm::Create(c));
    play(rm::Transform(c, rm::Square({640, 360}, 180, rm::col::orange)));
    auto k = rm::Tracker(1);
    // updaters = manim's always_redraw
    c->addUpdater([k](rm::Mobject &m, float) { /* rebuild from k->value */ });
    play(rm::ChangeValue(k, 3, 2));
  }
};
```

raylib also defines a `Transform` struct, so write `rm::Transform` (or qualify everything with `rm::`, as above).
