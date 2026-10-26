# Verification session, 2026-10-24: findings to decide on at the end

Read-only session. Each finding: what was tried, what happened, which rule it touches, the proposed action.

## 1. A curve (or any plot-only kind) under a schema that can't draw it vanishes silently

- Tried: `schema free` + `mark a bar score v=3` + `mark f path fn={x^2/4} label="f"`, then `--check`, `--readback`, `--snapshot`.
- Happened: `--check` says decodable, 0 lints, layout 100. The readback lists the path. The frame shows only the bar. `drawWorld` in `render.cpp:184` returns early when the schema is not axes/plane with the comment "linted at load", but no such lint exists anywhere in `engine.cpp`.
- Same for: line, arrow, region, vector, circle, rect, polygon, angle, field, slope, solution, heat, contour, trails. Schemas that can't hold them: free, graph, numberline, timeline, sequence.
- With no schema line it works: the prescan infers `axes` from the path.
- Rules touched: axiom 6 (the readback claims a thing the stage does not show; the checker vouches for it), axiom 3's mirror (a meaning with no ink). The prompt warns Claude in words only ("`line`, `arrow`, `path`, world shapes, fields and trails only draw on `axes` or `plane`"); `<last_scene_issues>` would stay silent.
- Proposed: a load-time lint, "path 'f' needs axes or plane; schema free can't draw it", for every plot-only kind, making `--check` fail. Not auto-switching the schema: a named schema is the author's choice, a curve in it is a mistake to report. Test: the scene above fails `--check` on the new engine, passes on the old.

## 2. `morph` does not need a `from`; a wrong `from` is corrected (works as designed)

- Tried: `morph h.value -> 0.02 1s` (no from) and `morph h.value 5 -> 1 1s` when h was 0.02.
- Happened: no from: no lint, history says `morph h.value 2 -> 0.02 done` (the engine filled in the real start). Wrong from: identity lint "morphs h.value from 5 but it is 0.02; morphing from the real value (no jump cuts)", animated from 0.02. Axiom 1 holds in code.
- Claude writes the `from` out of habit (the README and examples show it). Harmless, self-correcting, only readability for a human.
- Possible action (small, optional): say in the README/prompt that `from` is optional and the engine ignores a wrong one, so Claude stops spending tokens on it; or leave as is. No bug.
- Side note: a dot at `x={h}` on inferred axes with no ranges ran 47,000 px off screen when h shrank; the checker reported it (layout 84). The scene's fault; this is why the prompt asks for ranges that contain the motion.

## 3. Open decision: rename `mark` to `thing`?

- The docs call every mark "a thing"; the verb is a different word, and the user asked what `mark` means.
- Best candidate: `thing` (declares a fact; a second `thing alice …` is naturally the same thing updated; no hint of pixels or position). Rejected: add/new/create (creation only), set (update only), put/place (position), draw/show (pixels; show taken), is/let/def (code).
- Cost: parser alias + readback emits the new word + every .dan file, toolbox, idioms, README, prompt, lint texts ("re-mark"), tests. About an hour, mechanical; goldens unchanged. Don't accept both spellings (decodability, Claude would mix them).
- Recommendation: decide at the end, not mid-verification.

## 4. `@cue` lines after their Say: a convention Claude is told, which the engine does not strictly need

- Rule text: prompt "Write in beats" (a beat is one Say plus the lines that belong to it) and README "a marker belongs to its sentence: it fires the @go lines between that Say and the next one".
- Tried: `@go morph …` placed BEFORE its Say vs AFTER. Both fire at 0.23 s, on the word "doubles" (verbose log); no lint either way; same end state.
- So: Claude does it because it is instructed, and because the examples all do; the engine is lenient when a whole file is played (an armed step waits for the next sentence that speaks its cue). Not a bug; the convention matters for streaming (a beat is the unit the server sends) and for reading. Possibly a lint "cue line before its sentence" if we want the convention enforced; low value.

## 5. A duration on a `mark` line becomes a colour category, silently

- Tried: `@run mark sec line x1=2 … label="secant" 5s`.
- Happened: no parse error, no lint, decodable. The readback shows `mark sec line 5s x1=2 …`: the bare token `5s` was taken as the mark's category (the parser's rule: a bare word is the kind or the category), so the line now has a category named "5s" and its own palette slot. Same family as the "unknown string becomes a colour" gotcha.
- Durations only mean something on `morph`, `send`, `wait`, `hide`, `show`. A mark enters over the fixed 1 s.
- Proposed: a parse error or lint on a bare token that reads as a duration (`\d+(\.\d+)?s`) on `mark`/`link`/`group`: "a mark takes no duration; to draw it over time morph a property". Cheap, one regex in the parser, with a test.

## 6. Answered, works as designed (for the record)

- `|cue|`: a named moment in the sentence; fires the `@cue` lines of that beat on that word (or where it sits); stripped from the voice and subtitle; one per sentence scope; both halves required (hook: lint).
- `mark`: declares a thing (id, kind, category, properties); a second mark with the same id updates in place; order of first marks is order on stage.
- `sec`: just an id; `line` is the kind; `label=` is what the learner reads; ids are never shown.
- The hook lint "lines tagged @run have no |run| spoken" means no timing anchor, not that unspoken lines can't run: plain lines run at once (before the first Say) or when the sentence ends.
- `with`: joins the line to the previous one's step: same instant, same cue, chains; any verb; at the start of the line.

## 7. Proposal (the user's): a point ON a curve, so "Q walks to P" is one morph. Today `on=f` on a dot misbehaves.

- Today a point on a curve is written by repeating the function: `mark q dot x={2+h} y={f(2+h)}`, and it walks by morphing the hidden `h`. The relation "q is on f" exists only in the author's head; the engine sees two expressions that happen to agree.
- The user's idea: declare the relation, `mark q dot on=f x=4`, and let y come from the curve; then `morph q.x -> 2 2s` walks it along f. Fits the axioms: position from structure (y derived from the relation), zero (q shares the curve's ink because it is related), identity (one morph of one property). Also what the language already says for arrays: `on=` seats one thing on another.
- Tried: `mark p dot on=f x=2`, `mark q dot on=f x=4`, `morph q.x -> 2.1`. Happened: no error; both dots are placed at the top edge of the curve's bounding box, top-right corner, overlapping (zero lint, layout 70): the generic "seated on its top edge" attachment (`layout.cpp` ~379, written for arrays and labels) applied to a curve. So `on=<path>` is accepted and wrong today.
- Proposed: give `on=<path|line>` a meaning for dots (and vectors' tails, labels): x is the author's, y from the curve (`fn`, or the nearest point for parametric and lines); the readback keeps `on=f x=2` (no y). Then "walks the path" is `morph q.x -> 2`. A lint when `on=` targets a curve from a kind that can't ride it. Test: a dot on=f at x=2 sits at (2, f(2)); morphing x keeps it on f; the derivative example rewritten with on=f reads back the same picture.
- Note for the prompt afterwards: the example and toolbox should use `on=f` for points on curves, and the slope's `\val{(f(2+h)-f(2))/h}` becomes `{(q.y - p.y)/(q.x - p.x)}`.

## 8. Brainstorm (the user's): inline actions in the sentence instead of `|cue|` + `@cue line` pairs

- The question: why `|one|` in the Say plus `@one indicate slope` on its own line, rather than a Markdown-like mark inside the sentence that is the action itself?
- What exists inline already: `[id]word` (point at a thing on the word: the same thing as `@x indicate id`) and `{expr}` (speak and point at a value). So `|one|one` + `@one indicate slope` could be `[slope]one` today.
- Measured in the real chats: 369 cues vs 75 inline pointers in Say lines; 43 of the cued lines are bare `indicate` (could be inline now). In the 80 eval scenes: 126 cued `indicate`, 1 hook lint in 80 scenes (the pairing rarely fails for Claude; it failed once for me when writing the toolbox by hand).
- What is cued most: mark (156 / 527), morph (132 / 353). Those carry arguments (ids, numbers, durations, `with` chains); inline they would bloat the sentence and the subtitle/voice would need to strip them (as |cue| and [id] already are).
- Assessment: for an AI author the pair form costs little (1 lint in 80 scenes) and keeps sentences readable and the voice text clean; the inline form is right for *pointing* and is under-used (Claude writes the two-line form for indicate 43 times). Two cheap moves: (a) the prompt/toolbox should prefer `[id]word` over `|cue|` + `@cue indicate id`; (b) possibly let `[id]` also accept a part, `[f.sq]` or `[a[2]]`, so pointing at a formula part or a cell is inline too. A full inline-action syntax is not recommended: arguments, durations and `with` groups don't belong inside spoken text.
- Decision after discussion (user): one uniform rule ("to make something happen on a word: a cue in the sentence, a line under it") is simpler for the model than a rule plus a pointing exception; the two-line indicate costs only tokens and the pairing almost never fails. Keep the teaching as is; `[id]word` stays available, not pushed. No action.

## 9. From the enforcement mapping (before the eight audits; to be confirmed by them)

- Streamed beats can move prescanned scales: dot radius jumps when a later beat raises the max (layout.cpp:72, no easing); bar scale eases to a new value instead of staying fixed (layout.cpp:1066-1069); axes/number line/timeline ranges only widen when a new mark arrives, so a morph-only beat can push a value off the plot (layout.cpp:443-445). Axiom 2 in the live mode. (A2)
- Instant changes with no morph: `mark` on a link (engine.cpp:883), a re-mark adding a new property or changing its type (957-960), re-`link`/re-`group` (978, 997), `morph … 0s` (1033), data-placed marks snapping (layout.cpp:807). Axiom 4. (A4, A1)
- No test exercises `focus` (axiom 5); none for the retired-placement-word lint, the identity morph lint, or scale stability across an appended beat.
- Two separate "related" checks (zeroCheck's related() vs the checker's intended()/allow) that never consult each other. (A3)

## 4 (corrected by audit A5). The cue-after-Say convention IS load-bearing after the first sentence

- My test placed `@go` before the FIRST Say, where it is merely armed and fires on the word. With a sentence before it, a cue line placed one beat early is run by that sentence's `closeSentence` ("never cued: runs at the end of its sentence"): verified, `verify/early.dan`: the morph ran at 4.80 s, the word "doubles" came at 7.02 s. No lint: the hook lint is global (a |go| and an @go exist somewhere), not per beat. A5 found it in a real chat (20261020-171259 turn 2: a "dead end" cross appeared 5.9 s before the voice said it).
- Action: per-beat hook lints ("@go in beat N has no |go| in sentence N"), ~40 lines, with a test. Priority: high (silent, real-chat, breaks sync, the thing the user watches).
