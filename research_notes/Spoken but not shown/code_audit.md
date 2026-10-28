# Audit: "spoken but not shown", per-beat check feasibility (read-only, 2026-10-22)

Repo: <repo> (nothing modified). Inputs copied here.
Supporting files in this dir: timing.mjs (session timings), timeit.py (--check timing), lints.mjs ->
binding_lints.tsv (every binding lint with its Say), gaps.mjs -> gaps.json (uncovered claim kinds), probe*.txt.

## 1. Beat flow
- Claude stream-json text_delta -> onText (chat/server.mjs:361-367) -> fullLine fence parser (349-360): plan/scene/memory.
- sceneLine (339-347): on a Say: `#!voice <Say>` sent at once (341), then the PREVIOUS beat goes toStage (342).
  Pre-first-Say lines go immediately (345). Last beat flushed at fence close / finish (348, 356, 373-374).
- toStage (331-338): appends to turn.txt, appendStage -> stage stdin (191-196).
- Stage: reader thread (main.cpp:286-297) -> frame loop batches lines, `#!` lines split chunks (main.cpp:331-358)
  -> Engine::appendText (engine.cpp:63-99): Script::append (script.cpp:572-599), analyze, queueVoice (1530),
  late hooked lines run now (84-96).
- Timeline: Engine::update (1295-1303) executes steps as soon as nothing blocks; Say blocks until audio done
  (blocking() ~1180). startSay (1008-1052), tickSay (1103-1176): words fire hooks (fireHook 1054-1083 -> `@cue` lines).
- Earliest complete beat: in the server at the moment the NEXT Say line is parsed (server.mjs:340-342) or fence close.
- Look-ahead: the stage never waits for the next Say itself; it plays what it has. The SERVER holds each beat
  until the next Say arrives, so the stage's look-ahead is whatever backlog builds.
- Timing (36 logged turns, meta in messages.jsonl): median first token 7.9 s, first stage line 9.6 s
  (plan ~1.7 s), voice 1.6 s after stage. Writing: median 1.2 s per beat (done-stage / #Say).
  Speaking: median 15.8 words per Say ~ 6.1 s at 2.6 words/s. Claude writes ~5x faster than it speaks; a 9-beat
  scene is fully written ~9 s after the first scene line, while it takes ~50 s to play. Only beat 1 is latency-critical.

## 2. checkBinding per beat
- Runs only in instant mode: startSay snapshots sayFacts_ (engine.cpp:1009); closeSentence calls checkBinding
  (engine.cpp:1098). Live window is non-instant -> never runs. Readback drops binding lints (engine.cpp:1509-1510).
- Depends on: stageFacts() (binding.cpp:52-94: opacity>0.3, props, actives' targets, tex with \val filled),
  touched ids (script ranges), and `last` = no later Say in script (binding.cpp:102-103).
- BUG for per-beat use: in `--check --stdin` with `#!chunk` per beat (main.cpp:164-172), every Say is "last"
  when it runs, so the NUMBER check is skipped for every sentence. Verified with probe_chunked.txt: whole file 5
  binding lints, chunked 1. Also lints are printed only at end (main.cpp:175-190), not per chunk.
- Live state at arrival != state at play (cursor lags), so per-beat checks need a shadow instant engine
  (in-process second Engine, or a persistent headless `--check --stdin` process).
- Cost: process startup 12 ms (title-only script); 13-15 ms for scripts without tex; ~103-110 ms with any tex
  (node helper start, tex.cpp available()/start(), even when cached). All 39 session turns concatenated
  (305 Says, 989 lines): 122 ms warm, 245 ms cold. => engine work per beat << 1 ms; one-time ~100 ms tex start.

## 3. Coverage (39 real turns, 305 Says, re-checked with current binary as the server does: readback(prev)+turn)
- 34 binding lints: number 22, sameness 6, action 5, each-own 1, symbol 0. (Server logs show 26 with older binaries.)
- Max 2 lints per sentence (binding.cpp:106); questions and the final sentence skip numbers (140).
- False positives from the number parser: "seven hundred eighty-four" -> 700 (hyphen not split);
  "zero point one four" -> "four", "zero point two eight" -> "eight" (decimals only take one digit, 163/179).
- Not detected at all (gaps.json heuristics over the 305 Says): comparisons 16 ("ten times bigger", "climbs
  faster", "steeper"), multiplicative/change words 32 ("doubled", "a quarter", "explodes"; "doubles" not in the
  action regex 241), deictic "this curve/that point" 35 (16 with no cue and no pointer), process/steps 40,
  extremes 6. Fractions/words ("half", "quarter", "twice") never parsed. Named things never drawn: only [id]
  pointers to unmarked ids are linted (engine.cpp:202); a noun like "the kernel" with no thing is not.
  "action" check only fires when the sentence touches NOTHING (242).

## 4. Language
- No interpolation in Say. parseSay (script.cpp:185-232) handles `[id]` (-> hook "@pt:id", fireHook
  engine.cpp:1059-1065 = indicate) and `|cue|`. `{s.value}` in a Say is spoken literally, no lint (probe2.txt).
- Values exist elsewhere: `{expr}` props (Value::parse, expr.cpp), `\val{}` in tex resolved by texSource
  (engine.cpp:1376-1396) at render time.
- TTS text = Say words joined: queueVoice at append (engine.cpp:1530-1556), prefetchVoice (1520-1528), keyed by
  text (tts.cpp:103-116), single FIFO worker (118-130). Interpolation must resolve before these, using the model
  state at the beat's START (not at append time): resolve in a shadow instant engine / server, then send the
  resolved Say text (also in `#!voice`). TTS speaks digits ("0.14"); alignWords tolerates splits (engine.cpp:1558+).

## 5. Plan
- Parsed into msg.plan (server.mjs:359) before any scene line; used only for the log/broadcast at the end (396).
- Free text; `show:` = "claim -> how; claim -> how" on one line (prompt.md:6-12). 34 logged plans, older ones use
  other keys (insight/picture/arc/formula/check). Partly checkable: numbers/quoted labels in show: vs stageFacts.

## 6. Obstacles
- Append-only: Script::append, no retract; `#!load` = full reload from scratch. Repair must happen before send.
- `#!voice` prefetch is sent before the beat is complete (server.mjs:341): a rewritten Say wastes that request and
  re-queues behind others on the single TTS worker.
- Holding a beat for checking adds latency only to beat 1 (critical path ~1.2 s already).
- Late hooked lines still fire (engine.cpp:84-96): repairs can be appended as cue lines even after the Say began.
- Interrupt: server stops sending (S.cut, 332), stage fast-forwards everything received (engine.cpp:331-353).
- Claude's single persistent process is busy streaming; a model-based repair needs a second process.
- `last`/question heuristics and the 2-per-sentence cap would need changing for per-beat use.
