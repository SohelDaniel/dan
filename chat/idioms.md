## Visual moves, from 3Blue1Brown

Each is a move 3Blue1Brown uses, written in this language on a small example: when to use it, then the pattern.
Copy the *move* onto your topic (its things, words and numbers), not the example. Built by tools/mine from his step
comments (source file:line under each).

### Highlight a range
Use: when one stretch of a sequence matters but the rest should stay fully visible for context, such as a range, a window, or the values that qualify.
```
title "Highlight a range"
mark a array items=[1,3,5,7,9,11] label="sorted values"
Say: "A row of sorted values, all in plain view."
Say: "Only the ones |between|between three and nine count."
@between mark r enclose of=a[1:4] label="in range"
Say: "The others stay just as bright. Now the |box|box can go."
@box hide r
```
<!-- highlight_part · 3b1b _2024/puzzles/max_rand.py:273 "Highlight a given range" -->

### Highlight formula parts
Use: when a sentence names one side or term of an equation and the rest of the formula should stay fully visible, with only a box moving between the parts.
```
title "Highlight equation parts"
mark f tex tex="\textcolor{lhs}{\frac{d}{dt}e^{t}} = \textcolor{rhs}{e^{t}}"
Say: "Look at the |right|right side first: the amount itself."
@right mark r enclose of=f[rhs] label="amount"
Say: "Now the |left|left side: how fast that amount grows."
@left hide r
with mark l enclose of=f[lhs] label="rate"
Say: "The two sides are equal, and the whole formula stayed in view the entire time."
```
<!-- highlight_part · 3b1b _2021/matrix_exp.py:2886 "Highlight equation parts" -->

### Brace a group
Use: when one step or quantity applies to a whole group of items at once, so a brace over the group names that group as a single unit.
```
title "Brace a group"
mark coins array items=[1,0,1,1,0,1,1,0,1,1] label="coin flips"
Say: "We have a pile of flips, and we make |one|one update from all the data at once."
@one mark all brace of=coins[0:9] label="one update from all data"
with indicate coins[0]
```
<!-- enclose_part · 3b1b _2020/beta/beta3.py:2074 "All data at once" -->

### Box a group
Use: when several items should be treated as one group: a run of cells, a cluster, a subset the next sentence will talk about as a whole.
```
title "Box a group"
mark a array items=[3,8,5,9,2] label="values"
Say: "Five values, and three of them in a row are bigger than the rest."
Say: "Put a |box|box around those three, and they become one group."
@box mark g enclose of=a[1:3] label="the big ones"
Say: "Now we can talk about the group, not each value."
```
<!-- enclose_part · 3b1b _2026/2025_P6/p6.py:1866 "Draw a rectangle bounding the points" -->

### Cross out a claim
Use: when a tempting implication, answer or step is shown and must be marked as wrong or ruled out while staying visible, such as a common misconception.
```
title "Accurate is not predictive"
mark imp tex tex="\textcolor{green}{\text{Accurate}} \Rightarrow \textcolor{yellow}{\text{Predictive}}"
Say: "It is tempting to think that an |acc|accurate test must be a predictive one."
@acc indicate imp
Say: "But that |no|does not follow."
@no mark x cross of=imp label="does not follow"
Say: "The claim stays in view, struck out, so we can see exactly what we are ruling out."
```
<!-- cross_out · 3b1b _2020/med_test.py:562 "Accurate does not imply predictive" -->

### Isolate one item
Use: when several similar items are on stage and the next point is about just one of them: single it out so the rest recede and the voice can work on it alone.
```
title "Isolate one triangle"
schema plane x=[0,10] y=[0,6] grid=false axes=false
mark t1 polygon shape pts=[1,1, 3,1, 2,3] label="A"
mark t2 polygon shape pts=[4,1, 7,1, 4.5,4] label="B"
mark t3 polygon shape pts=[8,1, 9.5,1, 9,2.5] label="C"
Say: "Here are three triangles, all different."
Say: "Take just |this|this one and put the others aside."
@this focus t2
with hide t1
with hide t3
Say: "Now we can study it on its own."
```
<!-- focus_isolate · 3b1b _2019/moduli.py:281 "Isolate one triangle" -->

### Isolate one item
Use: when several parallel items are on stage and the next point is about only one of them. The others fade to ghosts and stay faded, so the one item can be worked on without distraction.
```
title "Isolate one"
mark a box group label="group A"
mark b box group label="group B"
mark c box group label="group C"
Say: "Three groups of checks, all running at once."
Say: "To see how it works, keep just |one|group A."
@one hide b
with hide c
with focus a
Say: "Everything we say next is about this |one2|one group alone."
@one2 indicate a
```
<!-- focus_isolate · 3b1b _2020/hamming.py:4335 "Isolate to one box" -->

### Pointer to a formula
Use: when the learner should look at a formula, or one part of it, before it is unpacked. A pointer names the whole formula, then an enclosure moves the focus to one part.
```
title "Pointing at a formula"
mark f tex tex="P(H|E) = \frac{\textcolor{num}{P(H)\,P(E|H)}}{P(E)}"
Say: "Here is the rule for updating a |belief|belief."
@belief mark ask label text="the update" at=f
Say: "Look first at the |top|top."
@top mark part enclose of=f[num] label="start here"
with hide ask
Say: "That is where the evidence and the prior |meet|meet."
@meet indicate part
```
<!-- point_at · 3b1b _2019/bayes/part1.py:2023 "Ask about a formula" -->

### Point at next guess
Use: when a sentence names one spot (the next guess, a pivot, a pointer position) and that same marker will later move to a new spot.
```
title "Next guess"
schema axes x=[0,3] y=[-2,4]
mark f path fn={x^2-2} label="f(x)"
mark g dot x=2 y=2 label="guess"
mark t line x=2 y=2 slope=4 extend=1
Say: "Follow the tangent line down to the axis. It lands on one and a half: the |next|next guess."
@next mark n dot x=1.5 y=0 color=red
with mark p label text="next guess" at=n color=red
Say: "Repeat from there, and the |move|marker slides to a closer one."
@move morph n.x 1.5 -> 1.417 1s
```
<!-- point_at · 3b1b _2021/newton_fractal.py:1696 "Mention next guess" -->

### Brace with new size
Use: when a thing already on stage needs its size or count named, then restated in a more telling form, such as a total that turns out to be a square.
```
mark g array cols=4 items=[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16] label="grid"
Say: "This grid holds |total|sixteen cells."
@total mark n brace of=g label="16"
Say: "But sixteen is really |sq|four squared, four rows of four."
@sq mark n brace of=g label="16 = 4²" color=yellow
with indicate g[0]
```
<!-- label_or_measure · 3b1b _2026/2025_P6/p6.py:745 "Show the new size" -->

### Count one by one
Use: when the order or the running count is the point: items arrive one after another and a tally keeps pace.
```
mark n param value=0
mark tally tex tex="\val{n}\ \text{coins}"
Say: "Coins land after each other, and the tally keeps up: |a|{c1.n}, |b|{c2.n}, |c|{c3.n}, |d|{c4.n}."
@a mark c1 dot coin n=1 r=10 label="1"
with morph n.value -> 1 0.3s
@b mark c2 dot coin n=2 r=10 label="2"
with morph n.value -> 2 0.3s
@c mark c3 dot coin n=3 r=10 label="3"
with morph n.value -> 3 0.3s
@d mark c4 dot coin n=4 r=10 label="4"
with morph n.value -> 4 0.3s
Say: "That makes {n} in all, counted as they arrived."
```
<!-- reveal_one_by_one · 3b1b _2025/laplace/integration.py:781 "Show numerous more" -->

### Reveal items one by one
Use: when each item in a set gets its own matching thing and the count or order is the point, such as a vector per word, a result per input, or a tally building up.
```
title "A vector per word"
schema graph
mark w1 box word label="the"
mark w2 box word label="cat"
mark w3 box word label="sat"
Say: "Each word gets its own list of numbers. First the|one|, then cat|two|, then sat|three|."
@one mark v1 box embedding label="0.3  -1.2  0.7"
with link e1 arrow w1 -> v1 label="e₁"
@two mark v2 box embedding label="0.9  0.4  -0.5"
with link e2 arrow w2 -> v2 label="e₂"
@three mark v3 box embedding label="-0.6  1.1  0.2"
with link e3 arrow w3 -> v3 label="e₃"
Say: "Three words, three vectors, one each."
```
<!-- reveal_one_by_one · 3b1b _2024/transformers/supplements.py:201 "Show word vectors" -->

### Write the equation
Use: when a law or formula is stated for the first time and its symbols stand for things already on stage. The formula appears on the word that names it, and each symbol is coloured like its thing.
```
title "Distance"
mark v bar speed=60 label="speed"
mark t bar time=3 label="time"
Say: "How far you go is speed times time, written as one |law|law."
@law mark f tex tex="d = \textcolor{v}{v}\,\textcolor{t}{t}"
Say: "Here [v]speed and [t]time keep the colours of their bars, so |read|read it off the picture."
@read focus f
```
<!-- write_formula · 3b1b _2024/manim_demo/lorenz.py:48 "Add the equations" -->

### Write the equation
Use: when a new law or formula should appear on screen as its own line, with its symbols then tied to the numbers they stand for.
```
title "Slope from masses"
mark m1 param value=16
mark m2 param value=1
Say: "The path's slope is |slope|minus the square root of the mass ratio."
@slope mark eq tex tex="\text{slope} = -\sqrt{\textcolor{m1}{m_1} / \textcolor{m2}{m_2}}"
Say: "The first mass is |a|sixteen."
@a mark v1 tex tex="\textcolor{m1}{m_1} = \val{m1}"
with send eq[m1] -> v1 1s
Say: "The second mass is |b|one."
@b mark v2 tex tex="\textcolor{m2}{m_2} = \val{m2}"
with send eq[m2] -> v2 1s
```
<!-- write_formula · 3b1b _2025/colliding_blocks_v2/blocks.py:1709 "Show slope equation" -->

### Morph into simpler form
Use: when one step of a derivation rewrites an expression into an equivalent, simpler one and a part carries through unchanged.
```
title "One step of a derivation"
mark f tex tex="\textcolor{yellow}{4!} = 4 \cdot 3 \cdot 2 \cdot 1"
Say: "Four factorial is four times three times two times one."
Say: "Multiply it |out|out, and the right side collapses into one number."
@out morph f.tex -> "\textcolor{yellow}{4!} = 24" 1.5s
Say: "Equal value, simpler form."
```
<!-- transform_formula · 3b1b _2026/spheres_talk/volumes.py:611 "Half factorial fact" -->

### Exponentiate each part
Use: when one step of a derivation applies the same function to every term of an expression already on screen, such as turning scores into e to the scores before normalizing.
```
title "Exponentiate each part"
mark f tex tex="\textcolor{orange}{2},\ \textcolor{orange}{1},\ \textcolor{orange}{3}"
Say: "Here are three scores. Let's |exp|exponentiate each one."
@exp morph f.tex -> "e^{\textcolor{orange}{2}},\ e^{\textcolor{orange}{1}},\ e^{\textcolor{orange}{3}}" 1.2s
Say: "Every score now sits in an |pow|exponent, so every part is positive."
@pow mark pos tex tex="e^{x} > 0"
with indicate f
```
<!-- transform_formula · 3b1b _2024/transformers/ml_basics.py:2103 "Exponentiate each part" -->

### Plug in a number
Use: When a general expression or formula should be evaluated at one specific value, so the learner sees each symbol replaced by the number and then the arithmetic collapse to a result.
```
title "Plug in a number"
mark f tex tex="f(\textcolor{blue}{x}) = 2 + 3\textcolor{blue}{x} + \textcolor{blue}{x}^2"
Say: "Here is a function of |x|x, with the same x showing up three times."
@x indicate f
Say: "Now plug in |one|one: every x becomes a one."
@one morph f.tex -> "f(\textcolor{blue}{1}) = 2 + 3\cdot\textcolor{blue}{1} + \textcolor{blue}{1}^2" 1.2s
Say: "What's left is plain |sum|arithmetic: two plus three plus one is six."
@sum morph f.tex -> "f(\textcolor{blue}{1}) = 2 + 3 + 1 = 6" 1.2s
```
<!-- substitute_values · 3b1b _2022/puzzles/subsets.py:2208 "Plug in 1" -->

### Worked numeric example
Use: when a formula should be shown on one concrete number, substituted and simplified a step at a time until the result appears.
```
title "Entropy, worked example"
mark eq tex tex="E[I] = \log_2\left(\frac{1}{p}\right)"
Say: "Sixteen equally likely outcomes, so each has |p|probability one sixteenth."
@p morph eq.tex -> "E[I] = \log_2\left(\frac{1}{1/16}\right)" 1.2s
Say: "One over that is |inv|sixteen."
@inv morph eq.tex -> "E[I] = \log_2(16)" 1.2s
Say: "Two to the fourth is sixteen, so the answer is |ans|four bits."
@ans morph eq.tex -> "E[I] = \log_2(16) = 4\,\text{bits}" 1.2s
with mark ok check of=eq label="result"
```
<!-- compute_example · 3b1b _2022/wordle/scenes.py:1602 "Examples of good entropy" -->

### Compare side by side
Use: when two cases computing the same kind of quantity should be contrasted, with both formulas and results staying visible, so the learner sees which is bigger.
```
title "Compare to the naive way"
mark avg param value=1.75
mark navg param value=2
mark h array items=[1,2,3,3] label="Huffman code lengths"
mark hs tex tex="\frac{1}{2}\cdot 1+\frac{1}{4}\cdot 2+\frac{1}{8}\cdot 3+\frac{1}{8}\cdot 3 = \val{avg}"
Say: "The smart code averages |h|{avg} bits per symbol."
@h indicate hs
Say: "Now |n|compare it to the naive code, which gives every symbol {navg} bits."
@n mark n array items=[2,2,2,2] label="naive code lengths"
with mark ns tex tex="\frac{1}{2}\cdot 2+\frac{1}{4}\cdot 2+\frac{1}{8}\cdot 2+\frac{1}{8}\cdot 2 = \val{navg}"
Say: "Side by side, the naive one |more|costs more."
@more mark worse enclose of=ns label="more bits"
with indicate hs
```
<!-- compare_side_by_side · 3b1b _2026/cross_entropy/robot.py:1053 "Compare to the naive encoding" -->

### Compare side by side
Use: when two cases of the same process or quantity must be contrasted, with both staying visible so the learner can see where they diverge.
```
title "Two cases, side by side"
schema graph
mark a1 box case label="guess: speed"
mark a2 box result label="green, gray, gray"
link la arrow a1 -> a2
group g1 part-of=[a1,a2] label="Case A"
Say: "Take the first |one|case and its result."
@one indicate a2
mark b1 box case label="guess: abide"
mark b2 box result label="gray, yellow, gray"
link lb arrow b1 -> b2
group g2 part-of=[b1,b2] label="Case B"
Say: "Now |other|another case, with the first still there: the same steps, a different outcome."
@other indicate b2
```
<!-- compare_side_by_side · 3b1b _2022/wordle/footnote.py:286 "Compare to bug" -->

### Match terms across equations
Use: when two expressions have the same structure and the learner should see which term corresponds to which, such as an equation and its transformed version, or a model and its formula.
```
title "Same shape"
mark p1 param color=orange
mark p2 param color=teal
mark p3 param color=purple
mark f tex tex="\textcolor{p1}{x''(t)} + \textcolor{p2}{\mu x'(t)} + \textcolor{p3}{k x(t)} = 0"
mark g tex tex="\textcolor{p1}{s^2} + \textcolor{p2}{\mu s} + \textcolor{p3}{k} = 0"
Say: "The spring equation and its |acc|s equation are mirror images: acceleration matches s squared, |fric|friction matches mu s, and the |spring|spring term matches k."
@acc send f[p1] -> g[p1] 1s
@fric send f[p2] -> g[p2] 1s
@spring send f[p3] -> g[p3] 1s
```
<!-- overlay · 3b1b _2025/laplace/shm.py:536 "Show mirror image" -->

### Swap in place
Use: when one thing has several discrete variants or cases (functions, rules, versions) and you want to step through them one at a time in the same spot, so the eye compares them instead of hunting for a new place.
```
title "Swap in place"
mark f tex tex="y = x" size=60
Say: "Start with a |lin|line, swap in a |par|parabola, then a |cub|cubic."
@lin morph f.tex -> "y = x" 0.5s
@par morph f.tex -> "y = x^2" 0.5s
@cub morph f.tex -> "y = x^3" 0.5s
Say: "Same place each time, so what changes is only the |pow|power."
@pow mark e enclose of=f label="the only change"
```
<!-- swap_case · 3b1b _2019/diffyq/part3/staging.py:55 "Maybe do it for left variant, maybe not..." -->

### Cycle through cases
Use: when one slot or sentence has several alternative versions and you want to show them one after another, each replacing the last in place.
```
title "Cycle through cases"
mark w array items=[it,was,the,best] label="formal"
Say: "Here is one line of text, written the |formal|formal way."
@formal indicate w[0]
Say: "Now say it in a |casual|casual voice."
@casual mark w array items=[yo,that,was,fire] label="casual"
Say: "Now as a |coder|programmer would say it."
@coder mark w array items=[git,push,force,done] label="code"
Say: "Four cells, three voices, and each one changes what comes next."
```
<!-- swap_case · 3b1b _2026/cross_entropy/entropy.py:2276 "Cycle through alternate phrases" -->

### Slide a thing away
Use: when something should travel to a new place unchanged, while whatever is tied to it follows: a distance growing, a line stretching, a pushed object.
```
title "Move a thing"
schema plane x=[0,12] y=[0,6] grid=false axes=false
mark earth dot x=2 y=3 r=10 color=blue label="Earth"
mark sun dot x=5 y=3 r=18 color=yellow label="Sun"
mark ray line x1=2 y1=3 x2={sun.x} y2=3
Say: "Here is the Sun, with a line running to it from the |earth|Earth."
@earth indicate earth
Say: "Now slide the Sun |away|away, and the line stretches to follow it."
@away morph sun.x 5 -> 10 2s
Say: "Same Sun, same size, just |far|farther off."
@far indicate sun
```
<!-- move_object · 3b1b _2025/cosmic_distance/planets.py:1535 "Move sun away" -->

### Sweep line over waveform
Use: when a tutor wants to examine a function, signal or sequence one moment at a time: a marker travels along the axis and reads off the value where it lands.
```
title "Sweeping a waveform"
schema axes x=[0,10] y=[-2,2] xlabel="time" ylabel="intensity"
mark f path fn={sin(x)+0.5*sin(3*x)} label="waveform"
mark clock param value=0
mark scan line x1={clock} y1=-2 x2={clock} y2=2 label="now"
mark p dot x={clock} y={f(clock)}
Say: "A line |sweep|sweeps across time, and the dot reads the intensity wherever the line meets the curve."
@sweep morph clock.value 0 -> 10 6s ease=linear
Say: "One moment at a time, the whole |shape|waveform gets read."
@shape indicate f
```
<!-- sweep_scan · 3b1b _2022/piano/fourier_animations.py:543 "Line passing over waveform" -->

### Slide a value, watch followers
Use: when one quantity should be dragged through several values (a point, a parameter) while a readout and the picture that depend on it update live.
```
schema plane x=[-2,2] y=[-2,2]
mark re param value=-1
mark im param value=0
mark s dot x={re} y={im} trail=all label="s"
mark e tex tex="s = \val{re} + \val{im}\,i"
Say: "Here is [s]s, sitting at negative one, and the formula reads it off."
Say: "Slide it |right|right toward zero, then |zero|stop on zero."
@right morph re.value -> 0.2 2s ease=linear
@zero morph re.value -> 0 1s
Say: "Now lift it |up|straight up, and end at i."
@up morph im.value -> 1 1.5s
```
<!-- vary_parameter · 3b1b _2025/laplace/exponentials.py:681 "Move s around, end at i" -->

### Slide four points apart
Use: when a shape or configuration depends on one setting and you want to show it forming as that setting changes, such as points reaching a special arrangement or a family of cases settling into one.
```
schema plane x=[-3,3] y=[-3,3] grid=false axes=false
mark loop circle x=0 y=0 r=2
mark s param value=0
mark d0 dot corner x={2*cos(0.4)} y={2*sin(0.4)}
mark d1 dot corner x={2*cos(0.4+s*1.5708)} y={2*sin(0.4+s*1.5708)}
mark d2 dot corner x={2*cos(0.4+s*3.1416)} y={2*sin(0.4+s*3.1416)}
mark d3 dot corner x={2*cos(0.4+s*4.7124)} y={2*sin(0.4+s*4.7124)}
Say: "Four points sit bunched together on a loop."
Say: "Now |spread|spread them out along the loop, and watch where they settle."
@spread morph s.value 0 -> 1 3s
Say: "They land on the corners of an |square|inscribed square."
@square mark sq polygon pts=[{d0.x},{d0.y},{d1.x},{d1.y},{d2.x},{d2.y},{d3.x},{d3.y}] color=yellow
```
<!-- vary_parameter · 3b1b _2024/inscribed_rect/loops.py:108 "Show four points going to a square" -->

### Shrink one bar
Use: when comparing two quantities and one should visibly drop to a fraction of the other, e.g. a cost cut to 60% or a second option that turns out smaller.
```
title "Shrink one bar"
mark a bar quantity length=10 label="first"
mark b bar quantity length=10 label="second"
Say: "Two bars, the same length, both at ten."
Say: "Now the second one |shrinks| to six tenths of the first."
@shrinks morph b.length 10 -> 6 1.5s
Say: "Same start, but the second is now clearly |shorter|shorter."
@shorter indicate b
```
<!-- grow_shrink · 3b1b _2026/2025_P6/p6.py:1747 "Make the second bar shorter" -->

### Shear back into square
Use: when a shape or the whole plane is deformed by a matrix, and the point is that undoing the shear returns the original. Examples are a shear, a change of basis, or showing two shapes are the same up to a linear map.
```
schema plane x=[-1,6] y=[-1,4]
mark G grid space m=[1,1,0,1]
mark sq polygon pts=[0,0, 2,0, 4,2, 2,2] label="shape"
Say: "The plane has been |slanted|sheared, and the square leans over with it."
@slanted indicate sq
Say: "Shear it |back|back, and the lean comes out."
@back morph G.m -> [1,0,0,1] 2s
with morph sq.pts -> [0,0, 2,0, 2,2, 0,2] 2s
Say: "Same shape, now a |square|square again."
@square indicate sq
```
<!-- transform_space · 3b1b _2024/inscribed_rect/loops.py:2342 "Shear back into square" -->

### Transform a vector
Use: when a tutor wants to show what a matrix or mapping does: the input arrow stays, a copy carries to its image while the whole plane deforms.
```
title "Transform a vector"
schema plane x=[-1,5] y=[-1,4]
mark G grid space m=[1,0,0,1]
mark u vector x=0 y=0 dx=2 dy=1 label="in"
Say: "Here is a [u]vector: two across, one up."
Say: "Now apply the |map|map. The plane shears, and a copy of the arrow rides along to its new |spot|spot."
@map morph G.m -> [1,1,0,1] 2s
@map mark w vector x=0 y=0 dx=2 dy=1 label="out"
@map morph w.to -> [3,1] 2s
@spot indicate w
Say: "It lands three across, one up: same arrow, new place."
```
<!-- transform_space · 3b1b _2024/transformers/attention.py:1806 "Show vector transformation" -->

### Split into parts
Use: when a tutor wants to show what a whole is made of: break one thing into its pieces and keep the total visible.
```
title "Break it into parts"
mark w area parts=[12] labels=[class] colors=[grey] label="a class of 12"
Say: "Here is a |whole|whole class of twelve students."
@whole indicate w
Say: "Split it into its |parts|parts: six juniors, four seniors, and two staff."
@parts mark w area parts=[6,4,2] labels=[juniors,seniors,staff] colors=[blue,orange,teal] label="the same 12"
Say: "Same twelve, but now you can see what it is |made|made of."
@made indicate w
```
<!-- decompose_split · 3b1b _2024/puzzles/added_dimension.py:1002 "Break up inner cells" -->

### Split into tokens
Use: when a whole (a sentence, a word, a number) must be seen as a row of separate pieces before anything is done to each piece.
```
title "Splitting into pieces"
mark s array items=["the cat sat on the mat"] label="one sentence"
Say: "Here is one |whole|sentence, a single block."
@whole indicate s[0]
Say: "Now break it up into |pieces|tokens, one cell for each."
@pieces mark s array items=[the,cat,sat,on,the,mat] label="tokens"
Say: "Six pieces, and now each can be |counted|handled on its own."
@counted mark n brace of=s label="6 tokens"
```
<!-- decompose_split · 3b1b _2026/cross_entropy/language_models.py:824 "Break up into tokens" -->

### Draw the sum arrow
Use: when two quantities that add (vectors, phasors, forces, waves) should be shown combining into one result: draw the sum as its own arrow from the origin, in its own colour.
```
title "Sum of two vectors"
schema plane x=[-1,6] y=[-1,5]
mark u vector x=0 y=0 dx=3 dy=1 label="u"
mark v vector x=0 y=0 dx=1 dy=2 label="v"
Say: "Two arrows, [u]u and [v]v, both starting at the origin."
Say: "Add them, and one new arrow appears: the |sum|sum."
@sum mark s vector x=0 y=0 dx=4 dy=3 label="u + v" color=green
with indicate s
Say: "It reaches four across and three up, because three plus one is four, and one plus two is |three|three."
@three mark eq tex tex="\textcolor{u}{\begin{pmatrix}3\\1\end{pmatrix}} + \textcolor{v}{\begin{pmatrix}1\\2\end{pmatrix}} = \textcolor{green}{\begin{pmatrix}4\\3\end{pmatrix}}"
with focus s
```
<!-- combine_gather · 3b1b _2023/optics_puzzles/adding_waves.py:409 "Show the sum of the two vectors" -->

### Append snippet to copy
Use: when a new thing is built from a whole piece plus a small part of another, such as a mixed document, a spliced sequence, or a concatenation.
```
mark a array items=[the,cat,sat] label="A"
mark b array items=[on,a,warm,mat] label="B"
Say: "Make a new one: first a |copyA|copy of all of A."
@copyA mark c array items=[the,cat,sat] label="new"
with send a -> c 1s
Say: "Then take just a small |snip|snippet of B, and append it on the end."
@snip mark c items=[the,cat,sat,warm,mat]
with send b[2] -> c 1s
with send b[3] -> c 1s
Say: "One new thing, made of a whole and a |part|part."
@part indicate c[3]
```
<!-- combine_gather · 3b1b _2026/cross_entropy/language_tree_visualization/language_tree.py:501 "Take a small snippet of B and append it to a copy of A" -->

### Arrow from thing to code
Use: when something is translated into another form, such as a phrase into bits, a word into a number, or an input into its representation. Show the source, then draw the arrow to its image.
```
title "Encoding a phrase"
schema graph
mark phrase box input label="INFORMATION THEORY"
Say: "Here is a |phrase|phrase we want to send."
@phrase indicate phrase
Say: "Draw an |arrow|arrow from it to the bits that stand for it."
@arrow mark code box output label="11110"
with link enc arrow phrase -> code label="encode"
Say: "Those five bits are |bits|thirty in binary, one short code for the whole phrase."
@bits indicate code
```
<!-- connect_map · 3b1b _2026/cross_entropy/entropy.py:1900 "Encoding a phrase" -->

### Reveal link families in turn
Use: when two kinds of relation share some of the same things and you want each family drawn as its own batch, in order, so the viewer sees where they overlap.
```
title "Two chains, one set"
schema graph
mark n1 box item label="1"
mark n2 box item label="3"
mark n3 box item label="4"
mark n4 box item label="2"
Say: "Four numbers in a row: one, three, four, two."
Say: "First the |up|climbing chain, each step to something bigger."
@up link u1 arrow n1 -> n2 color=green
with link u2 arrow n2 -> n3 color=green
Say: "Then the |down|falling ones, drawn after, so they hang off the same three and four."
@down link d1 arrow n2 -> n4 color=orange
with link d2 arrow n3 -> n4 color=orange
Say: "Green only goes up, orange only goes down, and some numbers sit on both."
```
<!-- connect_map · 3b1b _2026/2025_P6/p6.py:2315 "Built in the order they'll actually be revealed (increasing first, then decreasi" -->

### Show a ladder of levels
Use: when a scale of reference levels, such as bits, tiers or thresholds, should appear all at once as one organised structure that later items are measured against.
```
title "Bit levels"
mark note card text="Each extra bit doubles the outcomes" label="Key idea"
Say: "Information is measured against a ladder of |rungs|bit levels."
@rungs mark lad table cols=2 cells=[bits,outcomes,0,1,1,2,2,4,3,8,4,16]
Say: "Zero bits is one outcome, one bit is two, and every |step|step up doubles it."
@step indicate lad
Say: "Anything we measure later lands on one of these rungs."
```
<!-- show_structure · 3b1b _2026/cross_entropy/entropy.py:1723 "Show bit lines" -->

### Show the ring
Use: when items belong in a cycle or loop (seasons, notes, clock hours) and the whole arrangement should appear at once, so the viewer sees the shape before any single item.
```
title "Seasons as a ring"
schema plane x=[-4,4] y=[-4,4] grid=false axes=false
Say: "Four seasons, and they all settle onto one |ring|ring at once."
@ring mark c circle x=0 y=0 r=3
with mark sp dot season x=3 y=0 r=9 label="spring"
with mark su dot season x=0 y=3 r=9 label="summer"
with mark au dot season x=-3 y=0 r=9 label="autumn"
with mark wi dot season x=0 y=-3 r=9 label="winter"
Say: "After [wi]winter comes [sp]spring again: a cycle, not a line."
```
<!-- show_structure · 3b1b _2024/inscribed_rect/loops.py:3080 "Show the circle" -->

### Extend the graph
Use: when a function is known only on a stretch and the point is that the same rule keeps going beyond it: analytic continuation, extrapolation, or a formula outside its first domain.
```
title "Extend the graph"
schema axes x=[-1,10] y=[0,4] xlabel="x" ylabel="f(x)"
mark f path fn={2 + sin(x)} domain=[2,6] label="known part"
Say: "Here is a function, drawn only between two and six."
Say: "But the rule doesn't stop there, so |extend|extend it past both ends."
@extend mark left path ext fn={2 + sin(x)} domain=[-1,2] color=orange label="extension"
with mark right path ext fn={2 + sin(x)} domain=[6,10] color=orange
Say: "Same rule, same curve, just carried |on|on."
@on indicate right
```
<!-- plot_graph · 3b1b _2025/laplace/main_equations.py:1423 "Extend the graph" -->

### Bars for probabilities
Use: when a tutor wants to show a distribution as bars, one per outcome, each as tall as its probability, building up one at a time.
```
title "A distribution as bars"
schema free
Say: "Each outcome gets a bar as tall as its probability. First, |half|one half."
@half mark b1 bar outcome p=0.5 label="1/2"
Say: "Then |quarter|one quarter, then |eighth|one eighth, then |sixteenth|one sixteenth."
@quarter mark b2 bar outcome p=0.25 label="1/4"
@eighth mark b3 bar outcome p=0.125 label="1/8"
@sixteenth mark b4 bar outcome p=0.0625 label="1/16"
Say: "The rarer the outcome, the |short|shorter its bar."
@short indicate b4
```
<!-- show_distribution · 3b1b _2026/cross_entropy/entropy.py:1227 "Add bars to graph" -->

### Predict the next word
Use: when introducing a model that reads an input and outputs a single guess, such as next-word prediction, a classifier's label, or a function's output.
```
title "Predict the next word"
schema graph
mark text box input label="Paris is the capital of"
mark model box process label="language model"
Say: "The words go |in|in to the model."
@in link feed arrow text -> model
Say: "Out comes a single |guess|guess for the next word."
@guess mark next box output label="France"
with link out arrow model -> next
Say: "Just one word, [next]France, and that is the whole job."
```
<!-- run_process · 3b1b _2024/transformers/chm.py:112 "Single word prediction" -->

### Single ball, step by step
Use: when a random or dynamic process should be watched as one run before many runs are combined, such as a Galton board, random walk or any step-by-step simulation.
```
schema plane x=[-3,3] y=[-0.5,4.5] grid=false axes=false
mark p1 dot peg x=0 y=3 r=6
mark p2 dot peg x=-1 y=2 r=6
mark p3 dot peg x=1 y=2 r=6
mark ball dot x=0 y=4.2 r=9 trail=all color=orange label="ball"
Say: "One ball, let go from the |top|top. It hits the first peg and |flip|flips a coin, right this time."
@top morph ball.y -> 3.2 0.6s ease=linear
@flip morph ball.x -> 1 0.6s
with morph ball.y -> 2.2 0.6s
Say: "Another peg, another coin flip, left this time, and it |lands|lands at the bottom."
@lands morph ball.x -> 0 0.7s
with morph ball.y -> 0 0.7s
Say: "The path it left is just one run of the process."
```
<!-- run_process · 3b1b _2023/clt/galton_board.py:552 "Single ball bouncing, step-by-step" -->

### Local axes at a point
Use: when you want to measure a small change at one spot on a curve, such as a derivative, a linearisation or a tangent, and need a fresh pair of axes (dx across, df up) centred on that spot.
```
schema axes x=[0,6] y=[0,9]
mark f path fn={x^2/4} label="f(x)"
mark p dot point x=3 y={f(3)} label="P"
Say: "Take a point P on the curve and draw tiny axes centred on it: |across|dx runs across, and |up|df runs up."
@across mark ax line x1=2.4 y1=2.25 x2=3.6 y2=2.25 color=grey label="dx"
@up mark ay line x1=3 y1=1.65 x2=3 y2=2.85 color=grey label="df"
Say: "Now |in|zoom in, and these are the only axes that matter."
@in camera zoom=8 on=p
```
<!-- geometric_construction · 3b1b _2026/print_gallery/conformal_maps.py:836 "Draw a set of local axes labeled dx and dy" -->

### Drop lines to axes
Use: when a point's coordinates, or a pair of numbers, need to be read off a plane. Drop one auxiliary line to each axis so each number has a visible source.
```
title "Reading a point"
schema plane x=[0,6] y=[0,5]
mark p dot x=3 y=2 label="P"
Say: "A point floats in the plane. Where is it, in numbers?"
Say: "Drop a line straight |down|down to the x-axis."
@down mark v line x1=3 y1=0 x2=3 y2=2 color=teal label="x = 3"
Say: "Then run one |across|across to the y-axis."
@across mark h line x1=0 y1=2 x2=3 y2=2 color=orange label="y = 2"
Say: "Where they land are its coordinates: three, and |two|two."
@two indicate p
```
<!-- geometric_construction · 3b1b _2021/matrix_exp.py:2107 "h_line and v_line" -->

### Flash in direction
Use: when a single direction (a difference vector, a force, a drift) should be felt as a whole field. Show the arrow once, then fill the space with flow lines that all stream along it.
```
title "Flow in a direction"
schema plane x=[-4,4] y=[-3,3] grid=false axes=false
mark d vector x=0 y=0 dx=2 dy=1 color=yellow label="direction"
Say: "Here is one |dir|direction: two across and one up."
@dir indicate d
Say: "Now |fill|fill all of space with it, so every point streams the same way."
@fill mark E field vx={2} vy={1} style=flow color=yellow
```
<!-- show_field · 3b1b _2024/transformers/embedding.py:1315 "Flash in direction" -->

### Picture and numbers
Use: when one thing has two representations that should stay tied together, such as an image and its pixel values, a curve and its formula, or arrows and a matrix.
```
title "Picture and numbers"
mark img array cols=3 items=[0,1,0,1,1,1,0,1,0] colors=[-,teal,-,teal,teal,teal,-,teal,-] label="the picture"
Say: "Here is a tiny |pic|picture, a plus sign made of pixels."
@pic indicate img
Say: "The machine never sees it that way. Each pixel's |copy|brightness is copied out as a number."
@copy mark nums array cols=3 items=[0,1,0,1,1,1,0,1,0] colors=[-,teal,-,teal,teal,teal,-,teal,-] label="what the machine reads"
with send img[1] -> nums[1] 1s
Say: "Same |same|thing, two views: the picture for us, the numbers for the machine."
@same focus nums
```
<!-- two_views · 3b1b _2024/transformers/ml_basics.py:495 "Show input array" -->

### Zoom in on detail
Use: when one small part of a bigger picture needs a closer look, such as a point on a curve, a cell, or a region. Zoom in on the word that names it, then reset to restore the context.
```
title "Zoom in"
schema axes x=[0,10] y=[0,10]
mark f path fn={x^2/10} label="f(x) = x²/10"
mark p dot point x=6 y={f(6)} label="P"
Say: "Here is a whole curve, and one |pt|point on it."
@pt indicate p
Say: "Now |in|zoom in on that point, and look at what the curve does there."
@in camera zoom=12 on=p
Say: "Up close, it is almost a straight line. Now |out|back out."
@out camera reset
```
<!-- zoom_in · 3b1b _2026/cross_entropy/arithmetic_coding.py:571 "Zoom in on the "m"" -->

### Zoom out to restore
Use: after zooming into a detail, to hand the whole picture back and show how the detail fits in the context.
```
title "Back to the whole"
schema axes x=[0,6] y=[0,9]
mark f path fn={x^2/4} label="f(x) = x²/4"
mark p dot x=3 y={f(3)} label="P"
Say: "Zoom |in|in on P: up close, the curve is nearly a straight line."
@in camera zoom=12 on=p
Say: "Now |back|back out, and the whole curve returns, with P just one point on it."
@back camera reset
with indicate p
```
<!-- zoom_out_reset · 3b1b _2019/moduli.py:216 "Back to classes" -->

### Zoom back out
Use: after zooming into a detail, to hand the whole picture back and tie the detail to the bigger structure.
```
title "Zoom back out"
schema axes x=[0,6] y=[0,9]
mark f path fn={x^2/4} label="f(x) = x²/4"
mark p dot point x=2 y={f(2)} label="P"
Say: "Zoom |in|in on P, and the curve looks like a straight line."
@in camera zoom=12 on=p
Say: "But |out|back out, you see it was a curve all along, bending the whole way."
@out camera reset
with indicate f
```
<!-- zoom_out_reset · 3b1b _2024/transformers/mlp.py:296 "Reference emergent structure" -->

### Remove one thing
Use: when a tutor needs to take one item out of a set, such as a removed square, a dropped case or an eliminated option, while leaving it visible as a faint ghost.
```
title "Remove one square"
mark s1 dot square shape=square r=14
mark s2 dot square shape=square r=14
mark s3 dot square shape=square r=14
mark s4 dot square shape=square r=14
mark s5 dot square shape=square r=14
mark s6 dot square shape=square r=14
Say: "Here are six squares, all in play."
Say: "Now |remove|remove the last one."
@remove hide s6 0.8s
with mark rule card text="Remove one square" label="Rule"
Say: "Five are left, and the removed one stays as a faint ghost."
```
<!-- fade_remove · 3b1b _2020/chess.py:748 "Remove a square" -->
