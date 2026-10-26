# Audit A6: axiom 6, Decodability

Auditor A6, 2026-10-24. Repo read-only. All reproducers, frames and helpers are under this folder
(`audit-6/`); every path below is relative to it unless it starts with `/` or names a repo file.
Helper: `rt.sh scene.dan` runs `--check`, saves the readback, strips comments into `scene.rb.dan`, snapshots the
original and the readback at `--at end`, crops to the stage, and counts differing pixels (noise baseline: the
same scene snapshotted twice differs by 0 px). Montages are `scene.montage.png` (left original, right readback).

Axiom text (README "Axioms, enforced"): "`--readback` prints the final scene in the same vocabulary, plus derived
facts (max, mean, order), overlaps, recent changes and lint. `--check` re-parses that readback and confirms it
rebuilds the same scene." The chat (chat/server.mjs:221-224, 434-437) stores the comment-stripped readback as
`state.dan` and appends the next turn to it, so the axiom carries the whole multi-turn lesson.

Short answer to the question asked: the readback is the truth about the MODEL (what was declared, with current
values), not about the STAGE (what is drawn and how). "Re-parses to the same scene" is a fixed-point test on text,
and it passes on scenes whose re-parse looks different. In practice the text test never fails (0 of 80 eval
scenes, none of the 23 scenes in rt/, the 75 matrix cells, the real session or the six-turn lesson), which is exactly why it proves little.

## 1. Verdicts

| sub-rule | verdict | reproducer, command |
|---|---|---|
| The readback prints the scene in the same vocabulary (one canonical spelling; marks, links, groups, views, camera, focus, hidden things round-trip) | **holds** for everything the model holds | `rt/synonyms.dan`, `rt/numberline-syn.dan`, `rt/hide-link.dan`, `rt/space-cam.dan`, `rt/view-order.dan`, `rt/apostrophe.dan`, `rt/param-learner.dan`: `./rt.sh rt/<f>.dan` (0 to 30 stage px, text identical). Synonym table in the work log. |
| ... and only things that are on the stage | **violated** | `matrix.sh` -> `matrix.out`: 75 of 75 cells (15 plot-only kinds x 5 schemas free/graph/numberline/timeline/sequence) list `mark K ...` in the readback, pass `--check` (rc 0, layout 100) and change 0 stage pixels. Confirms session finding 1 and widens it to every kind. |
| ... and nothing the engine ignores (canonical form) | **violated** (small) | `rt/placement-words.dan`: `region=left`, `below=a` are linted "is ignored" and kept in the readback; `./tutor rt/placement-words.rb.dan --check` lints them again. In the chat the state carries them for the rest of the lesson. |
| Derived facts, overlaps, recent changes, lint | **holds** in the CLI, misleading twice, and **never reaches Claude** | `rt/derived-hidden.dan` (`max=a` names a ghost), `rt/derived-two-views.dan` (two views pooled into one `order:`); `rt/overlapping.dan` fine (equal values stack on the number line, no overlap, correct). chat/server.mjs:223 drops every `#` line, so `# derived/# since/# overlapping/# lint` are for humans and `tutor write` only. `binding:` excluded on purpose (engine.cpp:1646-1647) and still printed by `--check` (`rt/binding-lint.dan`). |
| `--check` confirms the readback rebuilds the same scene | **violated as a guarantee** | `rt/scale-history.dan` (10895 px: bar scale from the prescan), `rt/morph-new-prop.dan` (24432 px: auto schema free vs axes), `rt/colour-slots.dan` (197 px: A purple vs blue), `rt/trails-descent.dan` (19344 px: trail gone), `rt/hurricane.dan` (369 px: the track gone), `rt/band-history.dan` (4408 px: band height). All say `decodable: yes`. |
| The chat path: readback(turn1)+turn2 == readback(turn1+turn2) | **holds** for the real session and a 6-turn timeline, **violated** with views | `chat/`: turns 001-004 of chat/sessions/20261024-101323: chained fold == whole fold == the server's state.dan. `history/`: 6 turns, same. `turns/ev-*` (an empty view declared at the end of a turn) and `turns/rs-*` (`schema left` re-selected with no mark after): the fold drops the line, the next turn's marks land in the other view; `diff ev-A.dan ev-B.dan` shows `schema right free` missing. |
| ... and the picture the learner saw is the picture the state replays | **violated** | `chat/live-vs-state.montage.png`: live.dan vs state.dan of the real session, 21079 stage px: a taller formula band and a different graph layout on the left (the learner's). `<last_frame>`, the background `--check` and a restart all show the right one. |
| `tests/append.sh` failures on space-downhill / space-saddle | artefact, not a break | `append/`: the only diff is a `# lint: space: a surface is shown but the camera never moves` comment raised by the prescan on a streamed prefix (engine.cpp:138-139) and never retracted; model lines identical. |

Session findings in my area: **1 confirmed** (as a decodability failure too, all 75 cells). **7 reproduced**
(`rt/on-curve.dan`): the readback keeps `on=f x=2`, the round trip is exact (0 px, "decodable: yes"), so
decodability holds while the meaning is wrong; `--check` exits 1 only because the two misplaced dots happen to
overlap (`zero: p and q overlap by accident`), not because a dot cannot ride a curve. A lint for `on=<path>` from a
kind that cannot ride it (A3's side) is what would make `--check` say the true reason.

## 2. Rule vs code gaps

1. README.md:367 (and HANDOFF §3): "`--check` re-parses that readback and confirms it rebuilds the same scene."
   main.cpp:178-183: `model = eng.readback(false); again.loadText(model); again.runToEnd(); same = model ==
   again.readback(false)`. The text of two readbacks is compared; nothing about the scene (schema decided, ranges,
   scales, colours, ink, band, camera state) is. Six scenes above pass with different pictures.
2. README.md:367: the readback has "derived facts, overlaps, recent changes and lint". chat/server.mjs:223:
   `.filter((l) => l.trim() && !l.startsWith('#'))`. Claude never receives them. chat/prompt.md never mentions them.
   The promise is true only for `./tutor F --readback` by hand and the `tutor write` panel (tools/write.mjs:96-98).
3. render.cpp:184 `if (!isAxes(schemaNow())) return; // linted at load`. engine.cpp has no such lint (grep
   `"needs axes"`, `"can't draw"`: none; the lint table in the enforcement map has none). The engine even captions
   the path as drawn ("thing -> a path · a function -> a curve", matrix/timeline.path_end.png).
4. chat/prompt.md:180: "`<last_frame>` comes with an image: the stage exactly as the learner saw it at the end of
   your last turn." chat/server.mjs:351, 433, 437: `live = stateBefore + turn` where `stateBefore` is the folded
   state; `snapshotFrame(live)`. The frame is a fresh instant replay of the model text, and it differs from the
   window (chat/live-vs-state.montage.png).
5. engine.cpp:244-249: `layout: '<id>' region= is ignored: position comes from structure`. engine.cpp:1569-1571:
   the readback writes every prop, so the "ignored" word becomes part of the canonical form and is re-linted on every
   re-parse (and every chat turn through `<last_scene_issues>`).
6. tests/append.sh header: "must read back identical to the script played at once". It compares the commented
   readback, so a lint that depends on how much of the script has arrived (the `space:` surface lint) fails it
   without any model difference. The model-only comparison passes for both scripts.
7. HANDOFF §2 / README:14 "`--check`: decodability + axiom lint, exit 1 on failure". main.cpp:205: exit 0 needs
   `same && ok && zero == 0` where `ok` is only the first parse (`eng.load`) and `zero` counts `zero:`/`identity:`/
   `tex:` lints. `binding:`, `layout:`, `hook:`, `say:`, `camera:`, `space:` lints never fail it; the layout score
   never fails it. The server (chat/server.mjs:234-239) forwards only indented lines and `parse:` lines, so a
   `decodable: NO` would reach the browser as `ok:false` and never reach Claude.
8. chat/README.md:20 says the scene is folded into `state.txt`; it is `state.dan` now (chat/server.mjs:77 keeps
   `.txt` only for old sessions). Trivial.
9. HANDOFF "Open": "`scripts/*.readback` files are generated junk". engine.cpp:1477-1484 still writes
   `<script>.readback` on every windowed play (not on `--snapshot`/`--check`: those are instant or quiet). Nothing
   reads the file (grep `\.readback` in chat/, tools/, bin/, Makefile: only .gitignore:26). Six sit in scripts/
   today, untracked; the chat writes `live.dan.readback` into every session folder.

## 3. Where this axiom limits creative freedom

- **A lesson whose picture is a history.** `rt/hurricane.dan` (geography): a storm dot with `trail=all` walked
  across the Gulf in two morphs. The readback is `mark storm dot x=-89 y=29.5 trail=all`: the track is gone after
  the fold (montage `rt/hurricane.montage.png`). Same for gradient-descent steps (`rt/trails-descent.dan`), a
  projectile, a reaction path on an energy landscape, a pendulum's tail. Across a turn boundary the live window
  still has the trail, but `<last_frame>`, the `--check` score and any restart do not, and Claude's `<stage>` says
  nothing about it. The language has no way to write a trail as data, so Claude cannot repair it either.
- **A scale set by something that is no longer there.** `rt/scale-history.dan` (economics: a bar that fell from 10
  to 2 beside one at 4). The lesson's point is the collapse; the readback says `v=2`, `v=4`; after a restart b
  fills the room and the collapse is invisible. Claude's next turn cannot know the scale it is drawing on.
- **Two views across turns.** `turns/ev-*`: turn 1 draws a network and declares `schema right free` to count
  things in next turn (a grammar tree beside its sentence, a molecule beside its energy bars would be the same
  move). The fold drops the empty view; turn 2's bars land inside the graph view. `turns/rs-*`: ending a turn on
  `schema left` (back to the network) is dropped too. The author must re-declare the view at the start of every
  turn, which the prompt never says.
- **No delete means ghosts in the stage text forever.** The real session's state carries `mark c cross
  of="f[guess]" opacity=0.12` for a formula part that no longer exists; `history/s6.dan` carries two hidden notes.
  Linear in hides, small, but Claude must read them every turn and nothing tells it they are dead.
- **The long lesson itself is fine.** `history/`: six turns of a Roman-history timeline (events, an enclose, a
  brace, re-marked labels, a group, two hides) fold to 16 lines / 892 bytes for 31 lines of lesson, chained fold ==
  whole fold, same picture. The stage-as-text stays small because it is bounded by things on stage, not by turns.
- **Strings.** A label with single quotes around a double-quoted phrase (`'the "big" one'`) is a parse error that
  drops the whole mark (`rt/quote-in-string.dan`); apostrophes and escaped `\"` are fine (`rt/apostrophe.dan`).

## 4. Small model

Measured against reports/Small model for tutor weak spots.md (narrow, label-rich, checkable jobs only; never a
scene writer):
- **Summarising the readback for Claude: no.** The readback is already the smallest exact description (15 lines for
  a 184-line lesson; 16 lines for 6 turns). Claude must reuse ids, values and views verbatim; a paraphrase loses
  them, and the only check of a summary would be the readback itself. A model that "describes the stage" is a model
  that can assert things not on stage, which is the failure this axiom exists to prevent. The bloat case does not
  arise (§3).
- **"Does the frame show what the text lists?" as a VLM judge: not needed.** Finding 1 is detectable exactly: the
  checker's `picture()` has no Ink for plot-only kinds outside axes/plane (layout.cpp:151 bodies only when
  `isAxes(s)`, placement.cpp:90), so "every visible mark owns at least one Ink" is a deterministic census. The
  scale/colour/band/trail differences are detectable exactly by a scene digest (§5, fix 1). Labels for a judge
  (80 eval `check.txt` + frames) would teach it to approximate a computation the engine already does.
- **The one place a small model could help is adjacent, not here:** ranking which `<last_scene_issues>` matter
  (the prompt says "fix them if they matter"); labels could come from Claude's own next-turn edits. That is a
  binding/layout job, A3/A4 territory, and the report's step 7 covers it.

## 5. Proposed fixes, ranked by value over cost

1. **Readback keeps empty views and the current view.** engine.cpp:1556-1575: after the marks, emit `schema <v>
   <kind> props` for every view in `views_` not yet declared, and a bare `schema <curView_>` when `curView_` is not
   the last mark's view (and, with no views, nothing changes). Test (new, in test_engine or a `tests/turns.sh`):
   `readback(fold(t1)+t2) == readback(t1+t2)` for `turns/ev-*` and `turns/rs-*`; fails today (the diff shows
   `schema right free` / `schema left` missing), passes after. Axioms: 6 only. ~8 lines, half an hour.
2. **A lint for a kind the schema cannot draw** (shared with A3; session finding 1's proposal): at load, for
   `isWorldKind || isFieldKind || path/region/line/arrow/solution/grid` under a non-axes, non-space schema (or view):
   `"<kind> '<id>' needs axes or plane; schema <s> can't draw it"`, counted as an axiom warning in main.cpp:189 so
   `--check` fails. Plus a census test: every mark with opacity > 0.3 owns an Ink in `picture()` for every scene in
   scripts/ and chat/examples/. Test: every `matrix/*.dan` fails `--check` after (rc 0 today); scripts/*.dan still
   pass. Axioms: 6 and 3. ~25 lines, 1-2 hours including the matrix as a test.
3. **The readback carries the bar scale it was drawn on.** layout.cpp:1026-1027 already honours `max=`/`min=` on a
   bar as "the author's promise". In `readback`, when `maxVal_[mp]` (the prescan extent) exceeds every current value
   of that property, write `max=<maxVal_>` on the first bar of that property (and `min=` for negatives). Test:
   `rt/scale-history.dan` round trip 0 px (10895 today); `tests/golden` unchanged. Axioms: 2 (keeps the fixed scale
   across the fold) and 6. ~15 lines, 1 hour. The same idea for dots' `maxAbs_` (area scale).
4. **The readback pins the auto schema.** engine.cpp:1553: emit `schema <autoSchema_>` as a model line instead of
   the `# schema (auto)` comment (the decision was made once per scene anyway, engine.cpp:143-150). Test:
   `rt/morph-new-prop.dan` round trip 0 px (24432 today; the re-parse currently switches free -> axes). Trade-off to
   state: the live window re-decides the auto schema on each append (`analyze(false)`), the pinned state would not;
   fix 1 of the digest (below) reports when they diverge. Axioms: 6, touches 2 (structure fixed for the scene). ~3
   lines; also re-baseline nothing (readback text changes only for auto scenes; `make append` compares both sides
   with the same rule).
5. **`--check` compares a scene digest, not only text.** After both runs, compare per view: schema and ranges;
   band height; the decided camera; for every mark: kind, `colorOf` slot, rounded body box, "has ink"; print the first
   differing field ("decodable: NO: a's colour slot 3 vs 0", "band 180 px vs 96 px"). Keep exit 1 on text
   difference; make digest differences a count printed every run and failing only when `TUTOR_STRICT` is set until
   fixes 3-4 land (many scripts morph a bar, so a strict digest fails today for the right reason). `tests/append.sh`
   compares model lines plus digest, not comments (also clears the space-downhill/saddle failures). Tests: the six
   §1 reproducers print a digest difference (none today); every `scripts/*.dan` prints none after fixes 3-4 except
   trails and band cases, which are listed. Axioms: 6. ~120 lines, a day.
6. **Colour slots survive the fold.** Today a category changed by re-mark claims its slot late (engine.cpp:925,
   952, 1040) and the re-parse claims it early (`rt/colour-slots.dan`: purple vs blue). Options, in order of
   preference: (a) claim slots by category name deterministically (a stable hash into the ten hues, collisions
   resolved by first appearance) so colour is a function of the text, which also serves the open "flicker between
   turns" item; (b) emit `color=<name>` in the readback for every categorised thing whose slot differs from what a
   fresh parse would claim (cheap, but turns categories into fixed colours in the state). Test: `rt/colour-slots.dan`
   round trip 0 px. Axioms: 3 and 6; (a) is a design change the user should approve. (b) ~20 lines; (a) ~40 lines
   plus a golden re-look.
7. **Drop ignored placement words from the readback.** engine.cpp:1569: skip `region below above left-of right-of
   near align pos` (NOT `on in over`, which place on purpose, engine.hpp:41-44). Test: `./tutor
   rt/placement-words.rb.dan --check` has no `layout:` lint after (two today). Axioms: 6, the "position from
   structure" rule. 3 lines.
8. **Trails: either carry or confess.** The language has no data form for a trail. Cheapest honest step: the
   readback writes `trail=all` only together with a `# trail: lost across a fold` comment and the prompt says a
   trail does not survive a turn boundary; the better step is a `pts=` history key the engine writes and reads (the
   dot then draws its stored path). Test: `rt/hurricane.dan` round trip 0 px. Axioms: 6, 1 (the history is part of
   the thing's identity). ~60 lines for the key.
9. **`# derived` honesty.** Mark ghosts (`max=a (hidden)` or exclude opacity < 0.3), and one `# derived` line per
   view. Test: `rt/derived-hidden.dan`, `rt/derived-two-views.dan` readback text. 10 lines. Only humans and `tutor
   write` see these today, so low value unless the server starts passing comments to Claude.
10. **Server.** Forward `decodable: NO` into `<last_scene_issues>` (chat/server.mjs:236-238 filters it out), and
    either make the window write the end-of-turn frame itself (it has the true picture; a `TUTOR_SHOT`-style hook on
    idle) or reword prompt.md:180 to "a replay of the stage from its text". 5 lines either way.
11. **Remove the `.readback` writer** (engine.cpp:1481-1484 and .gitignore:26; delete the six files in scripts/ and
    stop littering session folders). Nothing reads it. 6 lines.

## 6. Checked and fine

- Synonyms canonicalise to one spelling: `re-mark`/`remark`; `mark w arrow a -> b` -> `link`; `cause`/`maps-to` ->
  `arrow`; `connected` -> `line`; `→` -> `->`; `members=|of=|parts=|bare` -> `part-of=[...]`; `yes/true` -> 1;
  `number-line` -> `numberline`; `camera home`, `focus all`, `point` are not model and vanish. (`rt/synonyms.dan`,
  `rt/numberline-syn.dan`.)
- Hidden marks, links and groups read back as `opacity=0.12` and re-parse to the same frame (`rt/hide-link.dan`,
  `rt/derived-hidden.dan`: 0 px).
- Views: the unnamed schema becomes a `main` view and is written back (`rt/views-main-schema.dan`, 0 px);
  interleaved view switches are written in creation order with bare re-selections (`rt/view-order.dan`).
- Space cameras: relative moves accumulate, an absolute `view=`/`reset` prunes earlier lines, `spin` stays last
  (`rt/space-cam.dan`, 30 px; test_engine.cpp:406 covers the order). The spin phase is time-dependent by design.
- 2D camera: a released camera (`camera fit`/`reset`) leaves no camera line and the frame matches (`rt/camera-fit.dan`
  0 px); `follow=`/`zoom= on=` specs are kept (engine.hpp:568).
- Params and `{expr}`: values stay expressions, `# value:` comments follow; the server's learner-set line folds into
  `value=` (`rt/param-learner.dan`).
- Strings: apostrophes, `=`/`#`/spaces (quoted), escaped `\"`, numeric-looking labels (quoted) all round-trip
  (`rt/apostrophe.dan`). Numbers print at 6 significant digits (a 1e-6 relative loss at most).
- Auto axes ranges after a morph: the same nice range (`rt/auto-range.dan`, 0 px).
- Equal values on a number line stack, so no `# overlapping` and no `zero:` lint is right (`rt/overlapping.dan`,
  frame `rt/overlapping_end.png`).
- Transients (`indicate`, `send`, `focus` mid-sentence, `wait`, `hide`'s fade, morph easing, camera flights) leave no
  trace in the end state: the synonyms scene differs at `--at end` by easing only and is pixel-identical at t=6 s
  (`rt/synonyms.t6.montage.png`, 0 px). Note for the server: `--snapshot --at end` settles 1.5 s (main.cpp:252), which
  was not enough for that group enclosure; the frame sent to Claude can catch an unsettled layout.
- `binding:` lints: raised in instant runs, printed by `--check`, excluded from the readback, forwarded to Claude by
  the server (`rt/binding-lint.dan`).
- The real session's chained fold equals the whole-lesson fold and the stored state (chat/); the six-turn timeline
  does too (history/).
- No eval scene ever printed `decodable: NO` (80 check.txt files).

---
# Appendix: work log (chronological, raw)

### append.sh on the two space scripts (reproduced on copies in audit-6/append/)
Command (per script, per n in 2 3 5): whole `./tutor F --readback | grep -v '^# readback'` vs `tail -n +5 F | awk
chunks | ./tutor head4 --readback --stdin`. Result: space-downhill differs at n=2,3,5; space-saddle only at n=2.
The whole diff in every case is ONE added comment line in the streamed readback:
`# lint: space: a surface is shown but the camera never moves; ... add camera spin ...`
Every `mark`/`schema`/`camera` line is identical (files audit-6/append/*.one.txt vs *.liveN.txt). So this is NOT a
decodability break of the model: the prescan lint (engine.cpp:138-139, raised in `analyze`) fires on a streamed prefix
that holds the surface but not yet the camera line; `warn` de-duplicates into `lint_` and nothing retracts it when the
camera arrives. Whole play sees the camera and never warns. A readback-comment artefact (a stale lint), and since the
server folds all `#` lines away (chat/server.mjs:223) it never reaches state.dan. It does reach Claude only via the
live stage's stdout, not via <last_scene_issues> (that comes from --check on the whole text). Verdict: artefact; fix is
to re-derive this lint on append like `hook:` lints (engine.cpp:228-231) or to compare model lines only in append.sh.

### The round trip: "decodable: yes" while the picture differs (audit-6/rt/, helper audit-6/rt.sh)
rt.sh runs `--check`, saves `--readback`, strips comments into `<name>.rb.dan`, snapshots original and readback at
`--at end`, and counts differing pixels (`compare -metric AE`). Noise baseline: the same scene snapshotted twice
differs by 0 pixels, so any count is real. Montages `<name>.montage.png` (left original, right readback) were viewed.

| scene | --check | pixels | what differs (seen in the montage) |
|---|---|---|---|
| scale-history.dan: bars a=10,b=4, `morph a.v -> 2` | yes | 11331 | original: bars on the prescan scale (max 10), b at 2/5 height; readback `v=2`,`v=4` re-parsed: b fills the room. Axiom 2's scale is history the readback drops. |
| morph-new-prop.dan: free bars, `morph a.x -> 5` | yes | 24850 | original is `# schema (auto) free`; the readback line `mark a bar v=3 label=a x=5` re-parses to auto schema AXES (grid, a at x=5). The auto schema is not in the model text; `--check` compares text, so it cannot see it. |
| colour-slots.dan: `mark a box x`, b y, c w, then `mark a z` | yes | 486 | A is purple (slot claimed 4th) in the original, blue (slot claimed 1st) after re-parse. Colour slot order is claim history; the text `mark a box z` is the same both ways. |
| trails-descent.dan (copy of scripts/descent.dan) | yes | 20480 | the zig-zag trail (`trail=all`) is gone after re-parse: the readback has the dot's final x,y only. |
| camera-fit.dan: `camera zoom=4 on=p` then `camera fit` | yes | 277 | frames look the same; the readback rightly has no camera line (released). Noise-level, not a finding. |
| derived-hidden.dan | yes | 436 | same picture (ghost a). Comment `# derived v: max=a ...` names the hidden bar as the max. |
| views-main-schema.dan | yes | 0 | the engine turns the unnamed schema into a `main` view; readback emits `schema main axes x=[0, 10] y=[0, 10]`. Fine. |
| quote-in-string.dan: `label='the "big" one'` | yes | 0 | single-quoted strings are a parse error (line dropped, `parse:` on stderr); the readback is honest about it (no mark a). Parser limit, not decodability. |
| apostrophe.dan: `"Newton's law"`, `Grant's`, `"say \"hi\""` | yes | 0 | all round-trip (escaped quote emitted as `\"`). Fine. |
| derived-two-views.dan | yes | 0 | `# derived v:` pools bars of two views into one order (`c > a > d > b`). Misleading comment. |

Conclusion so far: `model == model2` (main.cpp:178-183) is a fixed-point test on the TEXT, not a test that the text
rebuilds the scene. Everything the readback drops that still shapes the picture (prescan extents, colour-slot claim
order, trails history, the auto-schema decision, which was made from MARK lines only while the readback merges a
morphed prop into the mark line) passes it.

### Finding 1 as a decodability failure: kinds × schemas matrix (audit-6/matrix.sh, results audit-6/matrix.out)
For each of 15 plot-only kinds (path, line, arrow, region, vector, circle, rect, polygon, angle, field, slope,
solution, heat, contour, grid) under each of 5 schemas that cannot draw them (free, graph, numberline, timeline,
sequence): base scene vs base + the kind. Measured: `--check` exit code, parse errors, whether `--readback` lists
`mark K`, and the stage pixels base vs base+K (cropped to the stage).
Result: ALL 75 cells: check rc=0, 0 parse errors, the readback lists K, 0 pixels changed. The readback claims a thing
the stage never drew, `--check` vouches for it ("decodable: yes", layout 100), and no lint is raised. render.cpp:184
`if (!isAxes(schemaNow())) return; // linted at load` names a lint that does not exist (grep of engine.cpp: none).
Confirms session finding 1, and widens it to every kind and schema listed. (A3 covers the "meaning without ink"
side; here the point is that both the readback and --check should say so and do not.)

### The chat path on a real session (audit-6/chat/; copies of chat/sessions/20261024-101323)
Server (chat/server.mjs:221-224, 434-437): state_n = fold(state_{n-1} + turn_n), fold = `--readback` with every
`#` line dropped. Reproduced with turns 001-004: chained fold == fold of the whole lesson == the server's state.dan
(byte-identical, 15 lines for a 184-line lesson). So the text is consistent at turn boundaries for this session.
BUT the picture is not: snapshot of live.dan (what the window played) vs snapshot of state.dan (what a restart,
`/api/voice`, `/api/replay-turn`, the background `--check` and the frame sent to Claude all use): 21079 stage pixels
differ; montage audit-6/chat/live-vs-state.montage.png shows (left, live) a taller formula band (it had held the
3-line formula of turn 3: `bandNeed_` only grows, layout.cpp:639) and a different graph layout (nodes added over
turns keep the memo's mental map; the re-parse lays the final graph out at once: "previous layer" top-left, x/w/b in
one column, link labels elsewhere). Both are the same scene by the text; the learner saw the left one.
Consequence for the chat: `check(live)` scores and lints the RIGHT picture, `snapshotFrame(live)` sends Claude the
RIGHT picture as "the stage as the learner saw it", and the learner, after a voice change or restart, gets the right
one too. The readback is the truth about the model, not about the stage.

### A long lesson through the fold (audit-6/history/: six turns of a Roman-history timeline)
Turns t1..t6 (events with t=years, an enclose, a brace, two hides, a re-marked label, a group). Fold after each
turn as the server does. State size: 5, 8, 10, 13, 14, 16 lines (215 to 892 bytes) for a 31-line lesson: the stage
as text stays small and bounded by things on stage, not by turns. Chained fold == whole fold (same text). Whole vs
state snapshot: 574 px (the subtitle's top edge in the crop; montage audit-6/history/montage.png is the same
picture). Ghosts stay forever as `opacity=0.12` lines (two here; in the real session `mark c cross of="f[guess]"
opacity=0.12` points at a formula part that no longer exists). Faithful; the only bloat is ghosts, linear in hides.

### Synonyms and the canonical form (audit-6/rt/synonyms.dan, numberline-syn.dan, hide-link.dan, space-cam.dan)
Accepted by the parser and what the readback emits:
- `re-mark`/`remark` (script.cpp:565) -> folded into the one `mark` line with current values.
- `mark w arrow a -> b` (script.cpp:564) -> `link w arrow a -> b`. `cause`/`maps-to` -> `arrow`; `connected` -> `line`;
  `→` -> `->` (script.cpp:313-320). An unnamed link gets the id `a->b`.
- `group g members=|of=|parts=|part-of=` and bare members (`group g2 c`) -> `group g part-of=[c]`.
- `yes/true` -> `1`, `no/false` -> `0` (Value::parse); numbers printed at 6 significant digits (Value::markup:
  `1234567.89` would come back as `1.23457e+06`, a loss of 0.0002%: harmless).
- `number-line` -> `numberline` (script.cpp:457). `camera home` -> `reset`; `focus all|nothing` -> `none`;
  `point` -> indicate. None of reset/none/indicate/wait/pause/send/@cue/with are model, so they do not appear.
- `hide X` -> `opacity=0.12` on a mark, a link or a group (all three work and round-trip).
- Space cameras: an absolute move (`view=`, `reset`) prunes the earlier relative lines; the readback keeps
  `camera view=top`, `camera orbit=30`, `camera zoom=1.5 on=p`, spin last. Round trip 30 px (noise).
- Retired placement words (`region=`, `below=`, ...): accepted, linted "is ignored", and KEPT in the readback
  (`mark a box label=a region=left`). The re-parse lints them again; in the chat state.dan carries them for the rest
  of the lesson and `<last_scene_issues>` repeats the lint every turn (placement-words.dan: `--check` of the readback
  file shows the two `layout:` lints again). The canonical form should not contain words the engine ignores.
- Not expressible: a single-quoted string with spaces (`label='the "big" one'`) is a parse error (the tokenizer
  only groups double quotes); the line is dropped and the readback is honest about it. Double quotes inside a string
  are escaped (`\"`) and round-trip; apostrophes round-trip.
Synonyms round trip gave 1216 px: the montage shows link arrowheads not yet at the enclosure of `g2` in the
readback frame: the group enclosure and link ends were still easing at `--at end` (the original had extra
transient lines giving it time). A snapshot artefact, not a model difference (see the note on `--at end` below).

### Other round trips (audit-6/rt/)
- band-history.dan: a 4-line formula morphed to `a = b` over a plot. Same text; 4408 stage px differ: the original
  keeps the tall band (`bandNeed_` only grows within a scene), the re-parse gives the plot the room back.
- param-learner.dan: `param` + `{expr}` dot; the server's learner-set line (`mark h value=2.5`) appended to the
  folded state folds into `value=2.5` and the `# value:` comments follow. Fine.
- binding-lint.dan: `binding:` lints are raised in instant runs (engine.cpp:1200) so `--check` prints them indented
  and the server passes them to Claude; `--readback` leaves them out (engine.cpp:1646-1647). As designed.
- 0 of the 80 eval `check.txt` files under tools/eval say `decodable: NO`: the text round trip never fails in
  practice; it is weak, not failing.
