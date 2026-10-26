# Audit A5: axiom 5 (bounded focus) and voice-to-picture sync

Auditor A5. Repo read-only; all files under this folder. Written as I go: the sections below are filled in order
of the common brief (Verdicts, Rule vs code gaps, Creative freedom, Small model, Proposed fixes, Checked and fine);
the "Log" at the end holds the raw reproducer runs in the order they happened.

Note on method: I cannot hear audio. Muted runs (`--layout-report - --verbose`, `--snapshot`) use the engine's
estimated word timing (~170 wpm, 0.11 s + 0.058 s per character, +0.18 s after , ; : and +0.32 s after . ! ?).
Anything that depends on Inworld's real word timestamps is marked "suspected". `interrupt()` and `skip()` are only
reachable through the windowed `--stdin` mode (not allowed here) or a new unit test (no build allowed), so findings
about them come from reading the code and are marked "suspected (code)".

Summary in five lines. Axiom 5 holds in the normal case (a cued focus lasts its sentence, an uncued one the next,
and the dim eases back) and in the group, link, cell, `none`, two-focus and scene cases. It breaks at the edges:
a focus with no sentence after it stays forever (f3), the cells of a focused array snap instead of fading (f5),
and `interrupt()` never clears it (code). The bigger findings are on the sync side: a cue line placed one beat
early fires at the end of the previous sentence (5.9 s early in a real chat, with `--check` clean), a sentence
can fire a later sentence's line through `fireHook`'s fallbacks, the auto-pointer points at a bar when the
sentence counts ("all three"), and the voice can speak a morph's end value seconds before the bar gets there.

## 1. Verdicts

Reproducers are under scenes/ (scene text), frames/ (logs, PNGs, contact sheets), chats/ (copies of the user's
turns). Commands are relative to this folder with the repo's `./tutor`.

Axiom 5, "focus lights one thing (or a group and its parts) and dims the rest":
- Lights one thing, dims the rest to 0.2, eased both ways: HOLDS. scenes/f1-cued.dan, `--snapshot frames/f1 --at
  1.5,3.5,6.6,8.0 --mute` (frames/sheet-f1.png).
- A group and its parts (recursive), `of=` notes of the focus at 0.7: HOLDS. scenes/f4-group.dan (sheet-f4f5.png).
- A link: HOLDS (undocumented but works). scenes/f12-link.dan (sheet-cf.png, last tile).
- A cell `a[3]`: the array lit, the other cells dim: HOLDS, but to 0.3 and with no easing (they snap): small gap
  against axiom 4 and README:358. scenes/f5-cell.dan, `--snapshot --at 0.5,0.95,3.0,7.0` (sheet-f4f5.png).
- A formula part `focus f[sq]`: NOT SUPPORTED (tex lint, whole formula lit). scenes/f6b-part.dan, `--check`.
- `\textcolor{w}` glyphs lit under `focus w`: by code (render.cpp:107-111); frames could not show it (the TeX
  helper did not typeset inside my snapshots; "…" drawn). Marked unverified.

"for one sentence: the one it is cued in, or the next one if it stands between sentences":
- Cued: HOLDS (f1). Between sentences: HOLDS (scenes/f2-between.dan, sheet-f2f3.png). Two focus lines in one
  sentence: the later wins, both end with it: HOLDS (scenes/f7-two.dan). `focus none` mid-sentence: HOLDS
  (scenes/f9-none.dan). A `scene` while focused: cleared by clearStage: HOLDS (scenes/f8-scene.dan).
- A cued focus whose cue is never spoken: set and cleared in the same `closeSentence` call, so it never shows:
  HOLDS (bounded) but silent. scenes/f10-typo.dan, frames/f10-typo.log.
- The shadow engine: the same `lit` lambda with `opt_.instant` (engine.cpp:1440-1466), nothing focus-specific
  differs: HOLDS by code (not separately observable).

"Then everything is at full strength again":
- When another sentence follows: HOLDS (f1, f2, f7 frames at the end).
- When the focus is the last line: VIOLATED. scenes/f3-last.dan, `--snapshot frames/f3 --at 1.0,end --mute`:
  f3_end.png shows a and c still dim after the script finished; `--readback` prints `focus b`. In the chat this is
  "set-up for a sentence that never comes in this turn": the dim would last through Claude's thinking and the
  next turn's whole first sentence. Latent so far: of the 24 focus lines in the user's chats none ends a turn lit
  (two end on `focus none`).
- After `interrupt()`: SUSPECTED (code): engine.cpp:380-403 sets `say_.active = false` without
  `closeSentence()`, so `focus_` stays.

Voice-to-picture sync:
- A cue fires on its word, bare, glued before or after, with punctuation, two on one word, inside `{…}`, next
  to `[id]`: HOLDS. scenes/c1-forms.dan, c2-inexpr.dan, `--layout-report - --verbose --mute`.
- "A marker belongs to its sentence" (README:109): VIOLATED twice, no lint either time. (a) scenes/c5-steal.dan:
  sentence 1 speaks |go| without a line, and fires sentence 2's `@go morph` a sentence early (engine.cpp:1184-1185).
  (b) scenes/c7-early-line.dan and the real turn chats/s1020-t2.dan (chat/sessions/20261020-171259/turns/002.txt):
  a cue line placed before its Say runs at the END of the preceding sentence (`closeSentence`, engine.cpp:1190-1199)
  instead of waiting for the sentence that speaks it; 4.0 s early in the reproducer, 5.9 s early in the chat.
- A missing marker glued to a word (`glueCue`): HOLDS on the exact word (scenes/c3-glue.dan); VIOLATED when an
  earlier word shares a 3-letter prefix (scenes/c4-glue-prefix.dan: fires on "address" instead of "add").
- `[id]word`: a 1.1 s ring on the thing (an array: the whole array; a cell: the cell), on the word: HOLDS
  (scenes/s1-subtitle.dan, c2). An id marked later: waits 2 s, then `indicate: 'p' never appeared` plus the binding
  name lint in `--check`: HOLDS (scenes/pt1-future.dan).
- `{expr}`: the value at the word, after the lines cued on it, a morph at its end value: HOLDS as written
  (scenes/e2-morph-later.dan, e3-cell-fn.dan; tests/test_engine.cpp:117, 127, 141). But the picture lags: with a 6 s
  morph cued on word 3 and `{a}` on word 12 the voice says 10 at 3.92 s while the bar reads 8 (frames/sheet-e1.png).
  VIOLATED in spirit (hears != sees), nothing warns.
- Pointing ahead: HOLDS for whole-token matching (scenes/p4-substring.dan: 25 not found in 125) and for ambiguity
  (p5-two-holders.dan: "?" -> no pointer). VIOLATED, wrong target: a count ("all three bars", scenes/p1-count.dan
  and the f1/f2 frames: the ring on bar a) and a formula's number given to a bar (scenes/p2-wrong-thing.dan: "the
  slope is two" -> bar b, since plain holders beat formulas, binding.cpp:193-194). SUSPECTED wrong: letters in
  "Plan B", "vitamin C" (p3-letters.dan) and an argument "f of two" -> the cell holding 2 (e3).
- Binding lints: at most 2 per sentence: HOLDS (scenes/b4-three-lints.dan). "nothing changes during this
  sentence" counts cued lines and set-up lines before the sentence, not uncued lines after it: HOLDS, matches the
  sentence boundary (scenes/b5-trailing-isolated.dan lints, b6-cued-isolated.dan and b3-setup.dan do not).
- The subtitle: cues stripped, `[id]` brackets gone, `{expr}` replaced by the spoken value, hooked words
  underlined, 2 s fade, a `wait` and the rest extend it: HOLDS (scenes/s1-subtitle.dan, frames/sheet-s1.png).
- The rest after a formula step (kRest 1 s, a wait counts, the opening formula exempt, a late beat counts as
  quiet): HOLDS by the unit test (tests/test_engine.cpp:457, passes today: 31 cases, 184 assertions) and by code
  (`quiet_` accumulates while nothing moves, engine.cpp:1434; the WAIT is `kRest - quiet_`, :1417). Seen live in
  the replayed chat turn: tex settles 38.90, sentence ends 42.55, next starts 43.57 (chats/s1024-t1.log).
- The chat's interrupt: the `end` event fires and `heardSoFar()` cuts at the word reached: HOLDS by code
  (engine.cpp:381, server.mjs:165-168, 291-295). The cut sentence's unfired cued lines are dropped on the live
  stage but run in the `state.dan` readback: SUSPECTED (code) divergence between the stage and Claude's picture of
  it. `skip()` is clean (it fires everything, then closes).
- Inworld word alignment with expanded numbers ("125", "0.14"): SUSPECTED early cues by one api word per
  expansion (engine.cpp:1790-1825, log entry L10). Not checked against real timestamps.
- `@go Say: "…"` is accepted by the parser and corrupts speech (a sentence lost, "Done." spoken twice):
  VIOLATED (robustness); never written in the chats (0 of 534 Says). scenes/c6-hooked-say.dan.

Session findings in my area: #4 ("`@cue` lines after their Say: the engine does not strictly need it, both fire
on the word"): REFUTED in part. That holds only before the first sentence. After any sentence, a cue line placed
before its Say belongs to the previous sentence's beat and `closeSentence` runs it at that sentence's end (L11,
real chat: 5.9 s early, no lint). The convention is load-bearing and unenforced. #6 ("both halves required (hook:
lint)"): the lint is global over the script, not per beat, so c5 and c7 pass it. #8's counts: CONFIRMED by my own
scan (420 cues in 534 Says, 1 without a line in its beat), and that one case is the misfire above.

## 2. Rule vs code gaps

1. README.md:366 and docs/HANDOFF.md §3: "Then everything is at full strength again." Code: the only place
   that clears is `closeSentence` (engine.cpp:1201); `interrupt()` (:380-403) and the end of a script do not. A
   trailing `focus x` persists and `readback` prints it (:1598); the enforcement map already states this.
2. README.md:358 (dictionary): "voice subject | focus | everything else eases to 20%". Code: the other cells of a
   focused array go to 30% with no easing (render.cpp:350-353, recomputed from `focus_` every frame, never through
   `o->lit`); `of=` notes of the focus sit at 70% (engine.cpp:1456), undocumented.
3. README.md:109: "A marker belongs to its sentence: it fires the `@go` lines between that `Say` and the next
   one". Code: `fireHook` falls back to any later line, then any earlier line (engine.cpp:1184-1185, "older
   scripts"), and `closeSentence` (:1190-1199) runs a never-fired line at its sentence's end even when a later
   sentence speaks its cue. Neither case is linted: the `hook:` lints (:232-235) compare the set of all cues with
   the set of all tags, not per beat.
4. chat/prompt.md:171-175 ("A beat is one Say: line plus the lines that belong to it… Every |cue| needs an @cue
   line, and every @cue line needs its |cue|") is stated as a writing rule; the engine enforces neither per beat.
   The real misfire in chat/sessions/20261020-171259/turns/002.txt went through `--check` clean.
5. README.md:92: "`{expr}` … says the value h has when the word is reached (after the lines cued on that word;
   a morph counts at its end value)". True, and it means the voice can run ahead of the picture by the morph's
   remaining time (e1: 2.7 s). No rule or lint bounds that gap; the prompt's "Sync is the point" (prompt.md:172)
   is not protected here.
6. README.md:91: "`[id]` glued to a word … pulses `w11` on 'weight'" says nothing about a hidden target or a
   target not yet on stage. Code: a ghost still gets the ring (`drawPulses` ignores opacity, render.cpp:423-433); a
   target that arrives within 2 s is pointed at late, later than that it is dropped with a lint (engine.cpp:1310).
   Fine, just undocumented.
7. README.md:107 lists `f[id]` as a target "a thing, an array cell or span, or a named part of a formula" (for
   annotations and `indicate`). `focus f[id]` is accepted without an error of its own, lights the whole formula
   and raises a misleading tex lint ("neither a thing on stage, a part an annotation points at, nor a color
   name", engine.cpp:285) because `focus` does not count as pointing at a part while `indicate` does
   (tests/test_engine.cpp:467).
8. README.md:88 "`@hook <line>` waits until the voice reaches `|hook|`": `<line>` includes a Say in the parser
   (script.cpp:538-553 reads `@hook` before the `say:` check); the engine cannot play a cued sentence (c6).
9. docs/HANDOFF.md §4 says binding.cpp does "pointing ahead (a spoken number or name gets a pointer)". It gets
   one even when the number is a count of things on stage: the binding lint knows counts (`counted`,
   binding.cpp:313) but startSay's `consider` (engine.cpp:1132-1146) does not consult it.

## 3. Where this axiom limits creative freedom

1. Music, a chorale (scenes/cf1-music.dan, cf2-music-group.dan; frames/sheet-cf.png). "Listen to the outer two,
   soprano and bass" then two sentences about their contrary motion. `focus` takes one id, so sentence 2 lights only
   the soprano (tile 1), and the sentences that discuss the pair have everything back at full strength (tile 2): the
   subject is lost exactly while it is discussed. To keep both lit for three sentences one must (a) declare a
   `group outer part-of=[sop,bas]`, which draws an enclosure and re-lays the stage into lanes (tile 3: the boxes
   moved), ink and motion the lesson did not ask for (axiom 3's "a relation you did not mean"), and (b) write
   `focus outer` before every sentence. The user's own determinant turn does exactly that
   (chat/sessions/20261017-045709/turns/002.txt: `focus i` / Say / `focus det` / Say / `focus j` / Say / `focus det`).
   "One sentence" is the right default against stale dimming, but a paragraph-long subject needs an explicit,
   bounded way to hold it, and two things need to be focusable without a group.
2. Grammar, a parse tree: "the subject" stays lit while three sentences explain the predicate. Same mechanics as
   (1): one focus line per sentence, or a group around the subject's words.
3. Chemistry, a functional group in a formula: `focus f[oh]` for the hydroxyl written `\textcolor{oh}{OH}` is not
   a focus target (f6b). It needs a real thing named `oh` on stage so that `focus oh` lights the glyphs: a dummy
   mark for the sake of a highlight.
4. Counting a named subset: "the outer two" earns the lint "says two, but no 2 is on stage and nothing visible
   counts 2" (cf1, `--check`): `counted` compares with kinds and categories, not subsets. In history, anatomy or
   music, numbers are mostly counts of a subset ("the two hemispheres", "both chambers").
5. Going backwards through a sequence: "walks the tape backwards" with `indicate tape[3:0] stagger=0.3s`
   (chat/sessions/20261024-101323/turns/001.dan line 47) pulses all four cells at once with a lint; a reversed span
   is not a span. The voice says op by op, the picture says all at once.
6. Pointing at a count: "all three bars" pulses the bar whose value is 3 (p1). In lessons where numbers are
   counts (a fugue in three voices, three branches of government) the auto-pointer is wrong as often as right.

## 4. Small model

Measured against reports/Small model for tutor weak spots.md (its conclusion: no scene writer; small models only
for narrow, label-rich, checkable jobs, and only after the deterministic fixes).

Would genuinely help:
- Cue placement when the marker is missing (the job `glueCue` does by string prefix, script.cpp:576-586). Input:
  the sentence and the cued line (verb, id, label); output: the word index. Labels exist for free: every `|cue|`
  Claude did write is a (sentence, line) -> word pair; the user's chats hold 420, the eval runs hundreds more.
  Checked by construction (the word must be in the sentence) and by `--check` afterwards. Only runs on the slip
  path (1 in 420 here), so a wrong guess costs little; a 1-2B tagger at ~0.2 s per beat fits the report's table.
- Auto-pointer disambiguation, the report's "binding-repair tagger" (step 7): given the sentence and the stage's
  candidate holders (the output of `holderOfValue`/`holderOfName`, with "?" cases), decide value / count / ordinal /
  argument / none, and which holder. Labels: Claude's explicit `[id]` and `{id}` pointers (75 inline in the chats,
  more in the evals) plus the engine's own auto pointers accepted or rejected by a judge; checked because the
  answer must be a holder the engine already found. But first the rules in §5 item 3 (counts, ordinals, the
  formula preference) take the bulk; the tagger is for what is left.

Would not help:
- Focus lifetime, the interrupt divergence, the cell snap: rules and state plumbing, not judgement.
- Word-timing alignment (L10): a deterministic number-to-words expansion of the script's words before matching
  Inworld's normalised words removes the drift; no model needed.
- "Voice ahead of the morph" (e1): arithmetic on durations the engine already knows.
- A sync critic that reads a sentence and the changes during it (the regex at binding.cpp:401 is crude): the only
  labels are the judge's binding scores, which the report measured as weakly correlated with the lint (-0.20) and
  noisy (±0.15); the report says code first, and I agree.

## 5. Proposed fixes, ranked by value over cost

1. A cue belongs to its sentence, enforced (axioms 5 and the sync rule; touches nothing else). (a) In
   `closeSentence`, leave a never-fired cue line armed when a later Say speaks its cue, and lint
   `hook: line N (@dead) sits in the beat of line M, which never says |dead|; it fires on line K`. (b) Drop
   `fireHook`'s cross-sentence fallbacks (engine.cpp:1184-1185), keeping only lines before the first Say, and lint
   `hook: |go| in line N has no @go line in its beat`. Tests that fail today: c5-steal (after sentence 1 ends,
   `b.value` is still 5; the morph starts at sentence 2's |go|: read `timeline`/`since_`) and c7-early-line (the
   cross's `bornAt_` is after the |dead| hook time; `--check` prints the new lint). ~40 lines in engine.cpp, two
   tests; 2-3 h. Value: the real-chat misfire (5.9 s early, silent) is this.
2. Focus ends with the voice, not only with a sentence (axiom 5). Clear `focus_` in `interrupt()`, and when the
   stage has finished (`finished()` in `update`) once the subtitle's lingering is over, so a trailing `focus x`
   can't outlive the turn. Test: scenes/f3-last.dan in a live engine (no voice), updated 3 s past the end:
   `readback(false)` has no `focus` line and every mark's `lit` is 1; today it prints `focus b` and lit is 0.2.
   ~8 lines; 1 h. Side effect: the server's `state.dan` no longer carries `focus` across turns (fine).
3. Pointing ahead: no pointer for counts and ordinals (sync). Hoist `counted` out of `checkBinding` and use it in
   startSay's `consider`; add cell/slot/index/row/column/node/layer/voice to `pointable`'s ordinal list. Tests:
   scenes/p1-count.dan's `pointingReport()` says "0 pointed ahead" (today 1); the existing test at
   test_engine.cpp:221 still says 2. ~20 lines; 1-2 h. (p2's plain-beats-formula and p3's letters: document as
   known limits; a sentence-aware choice is the tagger's job.)
4. The cells of a focused array fade like everything else (axioms 4 and 5). Keep a per-mark eased factor for the
   cell dim (target 1 when `focus_` is a cell of this array), cells draw at `1 - 0.7 * factor` (or 0.8 to match the
   20% everywhere; then fix README:358 to whichever is chosen). Test: a live engine, `focus a[3]` then one 16 ms
   frame: the other cells' factor is below 0.2 (today it is 1 at once); after 1 s it is above 0.95. Needs a small
   accessor (`cellDim(const Mark&, int)`). ~15 lines; 1 h.
5. A lint when the voice would overtake a morph (sync). At shadow time, for a `{id}`/`{id.prop}` word with a NUM
   morph of that prop cued on an earlier word of the same sentence, estimate the gap with startSay's word model;
   if the morph's remaining time at the word exceeds ~1.5 s, warn `say: {a} is spoken about 3 s after a 6 s morph
   of a.value starts: the voice says 10 while the bar still reads ~8; say it in the next sentence or shorten the
   morph`. Test: scenes/e1-morph-earlier.dan lints, tests/test_engine.cpp's `kRefs` (a 1 s morph) does not. ~30
   lines; 2 h.
6. `interrupt()` keeps the stage and its readback in step (axioms 1, 5, 6). Fire the cut sentence's unfired cued
   lines instantly (as `skip()` does through `tickSay(0)`) before fast-forwarding, and clear `focus_`. Test: a
   live engine, a script whose first sentence cues a morph on its last word and a focus on its first; update 0.5 s,
   `interrupt()`; `readback(false)` shows the morphed value and no `focus` line (today: the old value, `focus a`).
   Suspected until the test runs. ~10 lines; 1 h.
7. Reversed spans walk backwards: `a[3:0]` in `staggerTargets` yields 3,2,1,0 (sync; axiom 6 unchanged since
   indicate is not in the readback). Test: `indicate a[3:0] stagger=0.3s` pulses `a[3]` first (like the test at
   :230). ~10 lines; 30 min.
8. `glueCue` prefers an exact word over an earlier prefix match. Test: scenes/c4-glue-prefix.dan fires on "add"
   (~1.6 s), not "address," (0.58 s). 3 lines; 20 min.
9. The parser refuses `@cue Say:` ("a sentence can't be cued; sentences play in order"). Test: c6-hooked-say.dan
   is a parse error. 3 lines; 15 min. Low value, never seen, but it corrupts speech when it happens.
10. An explicit, bounded way to hold a subject (bends axiom 5 openly; the author states the bound):
    `focus a,b` for several ids without a group, and `focus x hold`, ended by `focus none`, a `scene`, an
    interrupt or the end of the turn, with a lint when a hold outlives four sentences. Tests: cf1-music.dan with
    `focus sop,bas hold`: both lit through sentences 3-4, back after `focus none`; without `hold` sentence 3 is
    lit as today. ~40 lines + README/prompt text; half a day. Value: medium; today Claude pays a line per
    sentence and a group it did not mean.

## 6. Checked and fine

- Cue forms: bare, glued before/after, with punctuation, two on one word, inside `{…}`, beside `[id]` (c1, c2).
- A cued focus lasts its sentence; an uncued one the next; two in one sentence; `focus none`; `scene`; a typo'd
  cue's focus never shows (f1, f2, f7, f9, f8, f10). Group, link and cell targets (f4, f12, f5 apart from the snap).
- The eased dim (7/s) for things: nearly back 0.3 s after the sentence, fully back at 1.7 s (f1 frames).
- `{a[4]}`, `{f(2)}`, `{g.tex}` lint, `{a}` before its morph says the old value (e2, e3; tests :117-:160).
- Pointing: whole-token matching (25 not in 125), two holders -> none, a future `[p]` waits 2 s then lints,
  `{a + b}` is not pointed at and the lint asks for the number on stage (p4, p5, pt1, c2).
- Binding: at most two lints a sentence; "nothing changes" respects the sentence boundary (b3, b4, b5, b6).
- Subtitle text, underlines, fade and the `wait` extension (s1).
- The rest after a formula step: test :457 passes; live replay shows it (chats/s1024-t1.log).
- `skip()` fires everything then closes the sentence; `interrupt()` emits `end`; `heardSoFar()` cuts at the word
  reached (code).
- The `#!voice` prefetch asks with the same context the beat will ask with (test :302). Note: a sentence with
  `{expr}` is not prefetched (engine.cpp:1662, "its values are read once its beat arrives"), so its audio starts
  only when the beat arrives: latency, not sync.
- Hooks fire from the audio actually heard (`heard()` minus output latency), not from samples sent (engine.cpp:
  1252-1254, tts.hpp:71).
- The user's newest turn (chat/sessions/20261024-101323/turns/001.dan): every cue on its word, both `[f]`
  pointers, the 1 s rest; no focus misuse anywhere in the 24 focus lines of the chats.
- `./tests/test_engine`: 31 cases, 184 assertions pass on the current binary.

## Log (raw runs, in order)

### L1. Focus lifetime, 11 scenes (scenes/f*.dan, frames/f*.png, frames/*.log, frames/sheet-*.png)

Commands: `./tutor scenes/<s>.dan --layout-report frames/<s>.report.json --verbose --mute` (log in frames/<s>.log) and
`./tutor scenes/<s>.dan --snapshot frames/<s> --at <times> --mute`. Contact sheets: frames/sheet-f1.png (f1 at 1.5,
3.5, 6.6, 8.0), sheet-f2f3.png (f2 at 1.0, 4.0, 8.5; f3 at 1.0, end), sheet-f4f5.png (f4 at 3.0, 7.0; f5 at 0.5,
0.95, 3.0, 7.0), sheet-f6f7.png (f6 at 3.0, 7.0; f6b at 3.0; f7 at 2.0, 4.0, 7.0), sheet-f8f10.png (f8 at 3.0, 6.2,
8.0; f9 at 2.0, 4.0; f10 at 3.0, 7.0).

- f1-cued: `|f|` fires at 2.23 s, `focus b`; a and c dim during the sentence (3.5 s); sentence ends 6.30; at 6.6 s a
  and c are nearly back (the 7/s ease), at 8.0 fully back. HOLDS.
- f2-between: `focus b` runs at 2.02 s right after sentence 1 ends; dims through sentence 2 (ends 7.03); all back at
  8.5. HOLDS ("the next one if it stands between sentences").
- f3-last: `focus b` is the last line. Frame f3_end.png: a and c still dimmed after the script finished; the
  readback would print `focus b`. VIOLATED in the letter of "Then everything is at full strength again": nothing
  bounds a focus that has no sentence after it. (Real chats: see L2 for how often a turn ends this way.)
- f4-group: focus g (a, b): the members and the enclosure lit, c and d at 0.2; back at 7.0. HOLDS.
- f5-cell: focus a[3]: frame f5_0.95 (0.08 s after the cue) shows the other cells ALREADY at their dim level while
  the box "other" is still fully lit (it eases). The cells dim to 0.3 (render.cpp:353 `dimmed`), not 0.2, and
  without easing: `dimmed` is recomputed from `focus_` each frame, it never goes through `o->lit`. Back at 7.0.
  Small gap against axiom 4 ("focus fades") and the README ("everything else eases to 20%").
- f6-textcolor: focus w with `\textcolor{w}` in f: bar x dims, bar w lit; the formula could not be checked in the
  snapshot (see the tex note below). The code path is render.cpp:107-111 (glyph lit forced to 1 when focus_ ==
  g.color), m.lit 0.2 for the other glyphs.
- f6b-part: `focus f[sq]` (sq is only a \textcolor name): lint `tex: \textcolor{sq} is neither a thing on stage, a
  part an annotation points at, nor a color name`; by the `lit` lambda the whole formula stays lit (o.id ==
  baseId("f[sq]")), and render.cpp:111 compares focus_ ("f[sq]") with g.color ("sq"), never equal. So focus on a
  formula part is not a thing; `indicate f[sq]` is. Creative-freedom item.
- f7-two: two cued focus lines in one sentence: the second replaces the first (2.0: a lit; 4.0: c lit); back at 7.0.
  HOLDS.
- f8-scene: focus a then `scene`: the fade carries the dim, clearStage clears focus_, the new scene is lit. HOLDS.
- f9-none: `focus none` cued mid-sentence restores everything before the sentence ends. HOLDS.
- f10-typo: `@zzz focus a` with no |zzz| spoken: closeSentence runs it at 6.22 s and clears focus_ in the same call:
  no effect at all, only the hook lint. Not a violation; a silent no-op (a cued focus with a typo'd cue vanishes
  rather than running late like other verbs).
- Auto-pointer side finding, seen in f1 and f2: the sentence "Now all three are back at full strength again" gets
  `hook |@pt:a| at 0.58s` (verbose log) and frame f2_8.5.png shows the pointer ring around bar a (value 3): "three"
  is the count of bars, the engine pointed at the bar whose value is 3. See L4.

### L2. Focus in the user's real chats (chat/sessions/*/turns/*, read only)

24 `focus` lines against 534 `Say:` lines (4.5%): 13 cued (`@cue focus x`, or `with focus x`), 11 uncued between
sentences, 10 of the 24 are `focus none` (Claude clears by hand what the engine clears anyway). No turn ends on a
lit focus (two end on `focus none`), so the f3 persistence has not bitten yet. The first session
(20261017-045709/turns/001-003.txt) re-issues `focus det` before several sentences in a row: Claude wants a subject
lit across sentences and pays one line per sentence for it. 20261022-185048/turns/001.txt:24 uses `with focus a[6]`
(a cell). Nothing in the chats dims by accident.

### L3. Cue timing (scenes/c1-forms.dan, c2-inexpr.dan; frames/c1-forms.log, c2-inexpr.log)

`./tutor scenes/c1-forms.dan --layout-report - --verbose --mute`:
- bare `|one|` fires at 0.35 s (attached to the next word "here,"); `|two|plane:` at 1.78 s (start of "plane:");
  `plane|three|,` at 3.57 s (start of "plane,"); `|four||five|cues`: both at 4.83 s. Punctuation glued to the word
  changes nothing. A marker glued AFTER a word fires at the START of that word, same as glued before (README: "fires
  when that word is spoken"). HOLDS.
- c2: a cue inside braces `{a |go| + b}` is extracted and fires (0.80 s); the expression still reads 7. `[a]|pt|that`
  fires `@pt:a` and `pt` on the same word (2.23 s). HOLDS. The value 7 spoken through `{a + b}` is not pointed at
  (only `{id}`, `{id.prop}` and cells get a pointer) and the binding lint says "7 is not on stage": correct, the
  README says a value read off the stage must be readable on it.

### L4. `{expr}` and pointing ahead (scenes/e*.dan, p*.dan; frames/check-all.txt, sheet-e1.png)

- e1-morph-earlier: `@go morph a.value 3 -> 10 6s` cued on word 3, `{a}` on word 12. Verbose: the morph starts at
  0.63 s, `{a}` is reached at 3.92 s; the voice says "10" (end value, as the README promises) while frame
  e1_3.95.png shows the bar's label at 8 (e1_5.0: 9; e1_6.7: 10). The next sentence waits for the morph (morph
  Actives block), so the voice is 2.7 s ahead of the picture inside this sentence. Documented behaviour, but it is
  a hears-vs-sees mismatch; nothing warns about it. See the proposals.
- e2-morph-later: `{a}` before the cue says 3 (the value at the word). HOLDS.
- e3-cell-fn: `{a[4]}` -> 16, `{f(2)}` -> 4, `{g.tex}` -> the `say:` formula lint. HOLDS. Side findings: "Cell
  four" gets the binding lint "says four but no 4 is on stage" (the index row shows 4 but `stageFacts` does not
  read index rows; `pointable()`'s ordinal list has step/part/stage/... but not cell/slot/index/row/column); and
  "f of two" auto-points at cell a[0] (holds 2): the argument of f is not the array cell.
- p1-count: "All three bars are here" -> `point: "three" -> a` (bar a has value 3). The binding lint has
  `counted()` (a number that counts visible things is satisfied) but startSay's `consider()` does not use it, so
  a count is pointed at the thing whose value happens to equal it. Also seen in f1/f2 ("Now all three are back").
  WRONG TARGET, reproducible: `TUTOR_DUMP_POINT=1 ./tutor scenes/p1-count.dan --check`.
- p2-wrong-thing: bar b value=2 and formula "y = 2x"; "The slope of this line is two" -> `point: "two," -> b`.
  Plain holders beat formulas (binding.cpp:181-183), so the pointer goes to the bar, not the formula the sentence is
  about. WRONG TARGET (adversarial but plausible).
- p3-letters: "Plan B", "vitamin C" -> pointers at boxes labelled B and C. The binding lint excludes these with
  `notSymbol` (binding.cpp:452) but the auto-pointer in startSay does not: fine when the boxes are B and C, wrong
  when a carbon atom labelled C is on stage in a nutrition sentence.
- p4-substring: "twenty-five" is not matched inside 125 (whole-token regex): no false pointer; the lint says 25 is
  not on stage. HOLDS. "one hundred twenty-five" -> pointer at a (125) on its first word "one". HOLDS.
- p5-two-holders: two bars at 3, "three" -> holder "?" -> no pointer, no lint. HOLDS.

### L5. Binding lints vs the sentence boundary (scenes/b*.dan)

- b5-trailing-isolated: `Say: "Now it grows."` followed by an UNCUED `morph a.value -> 6` (which plays after the
  sentence) -> lint "says something grows, but nothing on stage changes during this sentence". Correct: the change
  is after the sentence. b6 (cued `|g|grows`) -> no lint. b3 (the morph as set-up before the sentence) -> no lint,
  correct (set-up plays as the sentence starts). HOLDS.
- b1-trailing: no lint, because the first sentence's `touched` includes the set-up marks before it (they grow in
  during it): the check is lenient for an opening sentence.
- b4-three-lints: five claims, two lints issued (binding.cpp:307 `issued++ < 2`). HOLDS ("at most 2 per sentence").

### L6. Subtitle (scenes/s1-subtitle.dan, frames/sheet-s1.png)

Frame s1_3.0.png: "A is 3 and b is 4, so together 7: seven hundred eighty-four is not here." Cues stripped, `[b]`
brackets gone, `{a}` `{b}` `{a + b}` replaced by 3, 4, 7; words that carry a hook underlined once spoken; the current
word yellow. s1_8.5.png (inside `wait 3s`, 1.9 s after the sentence ended): still readable (render.cpp:747, the
wait extends `lingering`). Fade otherwise: 0.75 alpha for 1.4 s then 0.6 s to nothing (2 s total). HOLDS.
"Numbers read whole": test_engine.cpp:201 passes today (L8).

### L7. A real chat replayed (chats/s1024-t1.dan = chat/sessions/20261024-101323/turns/001.dan; chats/s1024-t1.log)

`./tutor chats/s1024-t1.dan --layout-report chats/s1024-t1.report.json --verbose --mute`: 9 sentences, every cue
fires on its word (|bk| 3.72 s in, |ops| 2.82 s, |rec| 2.70 s, |local| 2.17 s, |back| 1.60 s, |mm| 0.87 s), both
`[f]` pointers fire, the tex morph cued at 37.40 settles at 38.90, the sentence ends 42.55 and the next starts
43.57: the 1 s rest. No focus in this turn. One sync miss, Claude's: `indicate tape[3:0] stagger=0.3s` for "walks
the tape backwards" -> lint `stagger: 'tape[3:0]' is not a span of cells (a[0:3]), so it happens at once`: the
voice says "backwards, op to op" and all four cells pulse at once. A reversed span is not expressible.

### L8. Cue glue, future pointers, unit tests (scenes/c3-glue.dan, c4-glue-prefix.dan, pt1-future.dan)

- c3: `@score` with no |score| spoken is glued to the word "score" at parse time (script.cpp:573-586) and fires at
  0.30 s; no hook lint since the hook is now spoken. HOLDS.
- c4: `@add` with no |add| spoken, sentence "At this address, we add one more": glued to "address," (0.58 s), not
  to "add" (~1.6 s): `glueCue` takes the first word that matches exactly OR by a 3+ letter prefix, so an early
  prefix match beats a later exact match. The morph plays on the wrong word. Small bug, only when Claude forgot the
  marker (the hook lint would otherwise have fired; 1 case in 80 eval scenes per the session notes).
- pt1: `[p]` spoken two sentences before `mark p`: no static lint (p is marked somewhere), the PULSE waits 2 s then
  `indicate: 'p' never appeared`; `--check` adds the binding lint "says p as a name, but no p is on stage". HOLDS
  (reported twice, in different words).
- `./tests/test_engine`: 31 cases, 184 assertions, all pass today (includes the rest-after-a-formula test at :457,
  numbers read whole at :201, {expr} at :117/:127/:141, pointing ahead at :221).

### L9. interrupt(), skip() and the chat's transcript cut: from the code (not runnable here; suspected (code))

`engine.cpp:380-403 interrupt()`: emits `end <uid>` (so the server's `heardSoFar()` stops at the word reached:
`word` events are emitted with the index of the word just reached, server.mjs:165-168, `slice(0, word+1)`), stops
the voice, sets `say_.active = false` WITHOUT calling `closeSentence()`, lands every Active, then runs
`update(0)` to the end in instant mode. In `update()` the cursor skips every armed line (`if (!hook.empty())
continue;` at :1428). Consequences, by code:
1. The cued lines of the interrupted sentence that had not fired yet are never executed on the live stage
   (`hookFired_` stays false). Uncued lines after it DO run. The server builds `state.dan` from `--readback` of
   `stateBefore + turn.dan` in a fresh instant engine (server.mjs:434-437), where every cued line of every Say runs
   (instant startSay fires all hooks). So after an interrupt, Claude's picture of the stage (state.dan) contains
   changes the live stage never made. The next turn's morphs start from the real values (axiom 1), so nothing
   jumps, but Claude may narrate a value the stage does not show.
2. `focus_` is not cleared: a focus set in the cut sentence dims the stage through Claude's thinking time and the
   whole first sentence of the next turn, until that sentence's `closeSentence`.
3. Those never-fired hooked lines stay fire-able: `fireHook`'s fallbacks (`fireIn(next, end)`, `fireIn(0,
   sayStep_)`, engine.cpp:1186-1188) let a later sentence that speaks the same cue name without a line of its own
   fire them, turns later.
`skip()` (:1368-1393) is clean: it clamps the word starts to `say_.t`, runs `tickSay(0)` which fires every hook,
the trailing hooks and `closeSentence()`. The transcript: `heardSoFar()` splits `spokenText` on spaces, and a
spoken `{expr}` value with a space inside (a cell holding "New York", a `{s.state}` of two words) would shift the
cut by one word. Minor.

### L10. Inworld timestamps and `alignWords` (engine.cpp:1790-1825): suspected, depends on Inworld's output

Matching is by normalised word with a look-ahead of 4 and a 3+ letter prefix rule. A script word that Inworld's
text normalisation expands into several words ("125" -> "one hundred twenty five", "0.14" -> "zero point one
four", "784") matches none of them exactly, so `k = j` (the first expanded word, the right start) and `j` advances
by one: the following script words are matched one api word late each, until a later exact match within the
look-ahead re-syncs. During the drift a cue fires at the time of an EARLIER api word than its own (early by one
word per unmatched expansion). The README tells Claude to speak stage values as `{id}`, which are formatted
numbers, so this is the common case for every spoken value above twenty. Not verified against real timestamps
(no audio here). The cache in .tutor_cache keeps `.words` files, so it could be checked offline with the words of
a cached utterance against a script that says a multi-word number.

The hooks are fired from `say_.t = heard` (samples handed to the device minus `kOutputLatency`, tts.hpp:71), so
the cue is tied to what was heard, not to what was sent. Good design; no finding.

### L11. A cue line one beat early fires at the end of the previous sentence (scenes/c7-early-line.dan; chats/s1020-t2.dan)

`./tutor scenes/c7-early-line.dan --layout-report - --verbose --mute`: `@dead mark c cross …` sits after sentence 1
(which never says |dead|) and before sentence 2 (which says |dead| as its last words). Verbose: `[3.83s] @dead
never cued: runs at the end of its sentence`, the cross appears at 3.83 s; `hook |dead| at 4.02s into the line`
= 7.85 s. The cross is 4 s early. `--check`: no hook lint (|dead| is spoken somewhere and @dead exists
somewhere; the static check is global, engine.cpp:232-235).

The same in the user's chat, chat/sessions/20261020-171259/turns/002.txt lines 20-22 (copied to
chats/s1020-t2.dan): `@dead mark c1 cross of=n11 label="dead end"` runs at 15.52 s at the end of "…pick one.";
the voice says "Dead end" at 21.43 s. The "dead end" cross stood on the tree for 5.9 s before the words, with
`--check` clean (0 `hook:` lines). This is the one cue in 420 (L2's scan: 420 cues spoken in 534 Says, 1 with no
line in its own beat) and it misfired silently.

### L12. c5-steal, c6-hooked-say, f12-link (scenes/, frames/*.log)

- c5-steal: sentence 1 says |go| with no @go line in its beat; sentence 2 has `|go|drops` + `@go morph b.value ->
  1 1s`. Verbose: `[2.87s] hook |go|` -> `line 5: @go morph b.value -> 1 1s` fires during sentence 1 ("nothing
  should change yet"); at 6.25 s sentence 2's |go| finds nothing left. `--check` clean. fireHook's fallbacks
  (engine.cpp:1184-1185) hand a sentence's line to an earlier sentence.
- c6-hooked-say: `@go Say: "Second…"` parses; at the cue the second Say replaces the running one mid-sentence,
  "Done." starts twice (timeline: `2.07 say 3`, `2.07 say 4`, `3.07 say 4`), sentence 1 never ends. Never
  written in the chats (0 of 534 Says), but the parser allows it.
- f12-link: `focus lk` on a link: the arrow "drives" stays lit, the other arrow and the boxes dim (frame
  f12_2.5.png in sheet-cf.png). Links can be focused.
