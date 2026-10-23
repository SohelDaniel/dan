## The toolbox: what you want the learner to see, and the tools that show it

Compose these: a scene is several of them working together, in your own topic's things and numbers. Each entry is the
complete move (every part of it, not half), on a throwaway example. Don't copy the example; use the move.

### Point at what you name
Use whenever a sentence names something on stage: the eye goes where the voice is.
Tools: `[id]` glued to the word; `{id}` speaks a value off the stage and points at its thing; `indicate` for a whole
thing between words.
```
mark a array items=[4,9,2] label="scores"
Say: "The [a]scores are in, and the best one is {a[1]}."
```

### Mark out a part
Use when a sentence is about a piece of something: some cells, one term of a formula, a group of things.
Tools: `enclose`, `brace`, `underline` with `of=` (a thing, a cell span `a[1:3]`, a formula part `f[id]`); `hide` it when
the sentence moves on. Pairs with: `\textcolor{id}{…}` to name formula parts.
```
mark f tex tex="E = \textcolor{ke}{\tfrac{1}{2}mv^2} + \textcolor{pe}{mgh}"
Say: "This |k|part is the energy of motion."
@k mark b brace of=f[ke] label="motion"
Say: "And this part is the energy of |h|height."
@h hide b
with mark e brace of=f[pe] label="height"
```

### Rule something out, or confirm it
Use for a wrong guess, a misconception, a case that fails; and for the one that works.
Tools: `cross of=`, `check of=`. Keep the crossed thing visible: the learner should see what was wrong.
```
mark g box guess label="heavier falls faster"
mark r box result label="same time"
Say: "The tempting answer is |no|wrong: they land |yes|together."
@no mark x cross of=g
@yes mark c check of=r
```

### Count, or go through items in order
Use when the order or the count is the point.
Tools: `indicate a[0:4] stagger=0.4s` (cells one after another); things declared together already enter one after another.
```
mark a array items=[7,7,7,7,7] label="five sevens"
Say: "Count them: |count|five."
@count indicate a[0:4] stagger=0.35s
```

### Show where a value comes from and where it goes
Use when a number moves: into a formula, onto the other side of an equation, through a network.
Tools: `send x -> y` (a copy travels and lands); `via=<link>` to travel along a link. Then the target shows the new value.
```
mark a array items=[3,4] label="sides"
mark f tex tex="c^2 = \;?"
Say: "The two sides go |in|into the formula: twenty-five."
@in send a[0:1] -> f stagger=0.3s
with mark f tex="c^2 = 3^2 + 4^2 = 25"
```

### Combine parts into a total
Use for a sum, a count, a vote, a product: many become one. Show the parts going in AND the total arriving.
Tools: `send a[i:j] -> total stagger=` then the total's value, both on the words that say them.
```
mark a array items=[2,5,1] label="parts"
mark t box label="total"
Say: "Add them |add|up: the total is |sum|eight."
@add send a[0:2] -> t stagger=0.3s
@sum mark t label="total 8"
```

### Apply the same step along something (slide and compute)
Use for convolution, a moving average, matching a pattern along a string: a small thing visits each position and
computes there. At every stop show the work, not just the slide.
Tools: seat it with `on=x[i:j]`, re-mark `on=` to slide; `send` the covered cells into the output; the result lands as said.
```
mark x array items=[1,2,3,4] label="input"
mark y array items=[" "," "] label="output"
mark k array items=[1,1,1] on=x[0:2] label="kernel"
Say: "Multiply and |add|add: |sum|six."
@add send x[0:2] -> y[0] stagger=0.3s
@sum mark y items=[6," "]
Say: "Slide |s|one step: |sum|nine."
@s mark k on=x[1:3]
with send x[1:3] -> y[1] stagger=0.3s
@sum mark y items=[6,9]
```

### Turn each item into a new one
Use for "double each", "square each", a function applied element by element: input and output side by side.
Tools: `send a[i:j] -> b[i:j] stagger=` (each lands on its own cell), then the new values.
```
mark a array items=[1,2,3] label="x"
mark b array items=[" "," "," "] label="x squared"
Say: "Square |each|each one."
@each send a[0:2] -> b[0:2] stagger=0.3s
with mark b items=[1,4,9]
```

### A weighted sum
Use for "inputs times weights, added" (a neuron, a grade, a score): the weights live on the links, each product
appears on its link as it is said, and the node shows the sum.
Tools: `link … label= w=`; re-mark a link's label to show its product; the node's label for the sum.
```
schema graph
mark x1 box input label="x₁ = 2"
mark x2 box input label="x₂ = 3"
mark s box node label="Σ"
link w1 arrow x1 -> s label="w₁" w=0.5
link w2 arrow x2 -> s label="w₂" w=-1
Say: "Each input is |mul|multiplied by its weight, and the node |add|adds them: minus two."
@mul mark w1 label="2 × 0.5"
with mark w2 label="3 × −1"
@add mark s label="Σ = 1 − 3 = −2"
```

### A share of the whole
Use for "normalise", "percent of", "probabilities add to one": show the total, then each part divided by it.
Tools: the parts as an array; the total; re-mark the parts as shares on the word.
```
mark a array items=[2,3,5] label="votes"
mark t box label="total 10"
Say: "Divide each by the |total|total, and the shares |add|add up to one."
@total send a[0:2] -> t stagger=0.25s
@add mark a items=[0.2,0.3,0.5] label="shares"
```

### Repeat until it settles
Use for iterative methods (Newton's method, gradient steps, a simulation converging): each step is its own beat and
the path stays visible.
Tools: re-mark the state step by step with `trail=all`; each re-mark on the word "again" / "next".
```
schema axes x=[0,4] y=[0,4]
mark p dot x=3.5 y=3.5 trail=all label="guess"
Say: "Step |one|toward the answer, and |again|again."
@one mark p x=2.4 y=2.4
@again mark p x=2.05 y=2.05
```

### Parts that add up (superposition)
Use for waves, forces, vectors: the parts in their colours, then their sum in white on the same picture.
Tools: `path` per part with its `color=`; the sum as one more path, appearing on the word.
```
schema axes x=[0,6.3] y=[-2.2,2.2]
mark a path fn={sin(x)} color=blue label="one"
mark b path fn={sin(2*x)} color=orange label="two"
Say: "Add them, point by point, and you get the |sum|sum."
@sum mark s path fn={sin(x)+sin(2*x)} color=white label="sum"
```

### Compare quantities
Use when "bigger", "twice", "more than" is the claim. Same baseline, side by side, difference marked.
Tools: `bar` marks (bars are only for comparing quantities); a `brace` on the difference; values readable on the bars.
```
mark old bar price=40 label="before"
mark new bar price=60 label="after"
Say: "The new price is |more|half again as much."
@more indicate new
```

### Same, or different
Use for "these share", "the same weight", "each has its own", groups and classes.
Tools: colour means identity: one shared `color=` (links) or category (marks) on exactly the things that are the same;
`colors=[teal,-,teal]` on array cells; distinct labels for "each has its own".
```
mark a array items=[1,2,3,4,5,6] colors=[-,teal,-,teal,-,teal] label="numbers"
Say: "The |even|even ones share a colour."
@even indicate a[1]
```

### Cause and effect, the steps of a process
Use for mechanisms, pipelines, chains of events: things as boxes, causes as arrows; the flow reads left to right.
Tools: `mark … box`, `link … arrow a -> b label=`; `send x -> y via=<link>` to move something through the chain;
`group … part-of=[…]` for stages that belong together.
```
schema graph
mark heat box cause label="heat"
mark ice box state label="ice"
mark water box state label="water"
link m arrow ice -> water label="melts"
link h arrow heat -> ice
Say: "Add |heat|heat and the ice |melt|melts."
@heat send heat -> ice
@melt indicate m
```

### Messages between parties
Use for protocols, conversations, requests and replies: who sends what to whom, in order.
Tools: `schema sequence`; parties as boxes; each message a `link` in the order written, labelled.
```
schema sequence
mark c box party label="client"
mark s box party label="server"
Say: "The client |ask|asks, and the server |reply|replies."
@ask link m1 arrow c -> s label="GET /"
@reply link m2 arrow s -> c label="200 OK"
```

### A hierarchy, a branching process, recursion
Use when one thing splits into several: a call tree, a probability tree, a family, a taxonomy. It draws top-down.
Tools: boxes and a `link` from each parent to each child; re-mark a node's label to fill in its result.
```
schema graph
mark f3 box call label="f(3)"
mark f2 box call label="f(2)"
mark f1 box call label="f(1)"
link a arrow f3 -> f2
link b arrow f3 -> f1
Say: "f of three asks two smaller |q|questions."
@q indicate f2
with indicate f1
```

### A quantity that changes, and what depends on it
Use for "as x grows…", a rate, a parameter worth playing with. Morph one value; everything written in terms of it follows.
Tools: `mark k param value=`; `{expr}` in other things' properties; `morph k.value -> …`; `set=learner` gives a slider.
```
schema axes x=[0,4] y=[0,16]
mark k param value=1 range=[0,3] label="steepness k" set=learner
mark f path fn={k*x^2} label="k·x²"
Say: "Turn up the |k|steepness and the curve climbs faster."
@k morph k.value 1 -> 3 2s
```

### Motion and physical systems
Use for anything that moves or has parts in real places: draw the parts where they are, and move what moves.
Tools: `schema plane x=[…] y=[…]`; `circle` `rect` `polygon` `vector`; a hidden `param` clock with `x={…}`; `trail=`.
```
schema plane x=[0,10] y=[0,6]
mark t param value=0
mark ball circle x={1 + 2*t} y={5 - 0.5*t^2} r=0.3 trail=all label="ball"
Say: "It moves across steadily and |falls|falls faster and faster."
@falls morph t.value 0 -> 3 2.5s ease=linear
```

### Functions, slopes and areas
Use for rates, derivatives, integrals, growth: the curve itself, with the thing the idea is about drawn on it.
Tools: `schema axes` with `xlabel`/`ylabel`; `path fn=`; a `line` through two points or with `slope=`; `region of= from= to= n=`;
`camera zoom= on=` then `camera reset`.
```
schema axes x=[0,4] y=[0,9] xlabel="x" ylabel="x²"
mark f path fn={x^2} label="x²"
mark a region of=f from=0 to=3 n=6 label="area"
Say: "The area under it, in |slices|slices."
@slices indicate a
```

### Many samples, a distribution
Use for averages, spread, randomness: show the samples themselves piling up.
Tools: `schema numberline … bin=`; `dot` marks with values, added a beat at a time; `{mean(score)}`.
```
schema numberline prop=score range=[0,10] bin=1
mark s1 dot score=4
mark s2 dot score=5
mark s3 dot score=5
Say: "Each roll lands in its |bin|bin, and the pile grows."
@bin mark s4 dot score=6
```

### Proportions and conditional probability
Use for "a fraction of a fraction", base rates, mixtures: area is the quantity.
Tools: `area parts=[…] labels=[…]`, `by=[[…],[…]]` to split each strip again; point at a piece with `d[i][j]`.
```
mark d area parts=[1,9] labels=[sick,healthy] colors=[red,-]
Say: "One in ten is |sick|sick."
@sick indicate d[0]
```

### A formula as the summary of what they saw
Use after the picture: each symbol in the colour of its thing; a derivation is one formula that grows.
Tools: `tex` with `\textcolor{id}{…}`; `morph f.tex -> "…"` (shared symbols glide, new ones light up);
`\begin{aligned}` lines for steps; `\val{}` for live values.
```
mark v param value=3
mark f tex tex="d = \textcolor{v}{v}\,t = \val{v*2}"
Say: "Distance is speed times |time|time."
@time indicate f
```

### Two views of one thing
Use when a second picture adds something: a network and its loss, a signal and its spectrum. Tie them by colour.
Tools: named schemas (`schema net graph label="…"`, `schema loss axes … label="…"`), each followed by its marks.
```
schema data numberline prop=v range=[0,10] label="the data"
mark p1 dot v=3
mark p2 dot v=7
schema stats graph label="its summary"
mark m box stat label="mean 5"
Say: "Two points, and their |mean|mean."
@mean indicate m
```

### Zoom into a detail
Use when the claim lives at a small scale: a curve looking straight up close, one cell of a big grid.
Tools: `camera zoom=8 on=<id>` on the word, then `camera reset` (axes and plane only).
```
schema axes x=[0,4] y=[0,16]
mark f path fn={x^2} label="x²"
mark p dot x=2 y=4 label="P"
Say: "Up |close|close, the curve is nearly straight. |back|Back out."
@close camera zoom=12 on=p
@back camera reset
```

### Grids and tables
Use for matrices, boards, dynamic-programming tables, truth tables: cells in rows and columns.
Tools: `array cols=… items=[…]`; `M[r][c]`, a row `M[r][:]`, a column `M[:][c]` for `enclose`, `focus`, `indicate`.
```
mark M array cols=3 items=[1,1,1,1,2,3,1,3,6] label="paths"
Say: "Each cell is the |sum|sum of the one above and the one to the left."
@sum mark r enclose of=M[1][1]
```

### Tidy what's done
Use when the stage fills up or a part of the explanation is over.
Tools: `hide <id>` dims it to a ghost in place (nothing else moves; a note such as a cross or an enclose disappears); `scene "…"` starts a fresh stage for a new topic.
```
mark a box step label="step 1"
mark b box step label="step 2"
Say: "Step one is |done|done."
@done hide a
```
