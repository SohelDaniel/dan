# Audit A1: Axiom 1, Identity

"No delete. A re-mark updates in place; a morph from a wrong value animates from the real one." (HANDOFF §3)
README's longer form adds the bridge to continuity: "... animated from the real value, so there are no jump cuts."

Auditor A1 of eight, 2026-10-24. Repo read-only. Everything I made is under
`/private/tmp/claude-501/-Users-soheldanial-code-server-testing-visulaizations-tutor/37e07ea3-2a08-44c3-b78a-4c2380bf6126/scratchpad/audit-1/`
(below: `scenes/<name>.dan`, `shots/<name>_<t>.png`, contact sheets `shots/<name>_sheet.png` tiled in time order,
`checks.txt` = every `--check` and `--readback` output of batch 1). Commands are run from the repo root with `./tutor`.
Binaries used as found (tutor and tests/test_engine built 2026-10-23 11:28); `./tests/test_engine`: 31/31 pass.

One-line summary: the MODEL keeps identity everywhere I could reach (one thing per id, never removed except by
`scene`/reload, ids unique by construction, every morph starts from the real value, the chat fold never duplicates).
What the axiom's own "no jump cuts" clause does not get is the PICTURE on re-mark: a re-mark that changes a value's
type (number -> expression, the everyday "now drive it with a param" move), a link's numbers via `mark`, a re-`link`'s
ends, a re-`group`'s members and a `kind` change all cut instantly. And there is one real delete: a thing created
during a `scene` fade (a `with` line on the scene, or a line cued on the same word) is wiped, silently.

## 1. Verdicts

| sub-rule | verdict | reproducer |
|---|---|---|
| No delete verb; nothing leaves the model but `scene`/reload | holds (code) | grep: only `clearStage` (engine.cpp:659-662) and `reset` (303) clear `marks_/links_/groups_/order_`; no erase anywhere else in markup/ |
| ... except: a thing created during the scene fade is deleted | **violated** | `scenes/scene-with.dan`, `scenes/scene-cued.dan`; `./tutor F --readback`: no `mark a` in the output; `--snapshot shots/scene-with --at 0.5,1.3,1.6,2.5`: `a` is drawn at 1.3 and 1.6 s, gone at 2.5 (sheet `shots/scene-with_sheet.png`) |
| Re-mark updates in place: numbers ease | holds | `scenes/remark-num.dan`, `--snapshot --at 1.9,2.15,2.35,2.8`: bar a reads 3, 3, 6, 9 (`shots/remark-num_sheet.png`); readback `# since: mark a.value 3 -> 9 done` |
| Re-mark: category / string cross-fade | holds | `scenes/remark-str.dan`, frames 2.2/2.5/3.0: colour and "open"/"done" blend (`shots/remark-str_sheet.png`) |
| Re-mark: list of the same length travels | holds | `scenes/remark-list.dan` frame 2.4: 3 and 9 mid-flight |
| Re-mark: a property new on the re-mark | acceptable (nothing to ease from); a new `opacity=` cuts where `hide` fades | `scenes/remark-newprop.dan` frame 2.1: `a` (mark a opacity=0.12) is already a ghost, `b` (hide b) is mid-fade (`shots/remark-newprop_sheet.png`); a new `label=` is cross-faded by the label layer (frames 2.3 -> 2.8) |
| Re-mark: a property that changes TYPE (NUM -> EXPR, EXPR -> EXPR) | **violated** (the "no jump cuts" clause; axiom 4) | `scenes/remark-expr.dan`, `--snapshot --at 1.9,2.1,2.35,2.8`: P (`mark p x={k} y={k}`) is at (4,4) at 2.1 s, two frames after the line, while Q (`mark q x=4 y=4`) is still at (1.05,2.05) and reaches (4,4) only at 2.8 (`shots/remark-expr_sheet.png`). `scenes/remark-expr2.dan`: EXPR -> EXPR, P jumps (2,2) -> (5,5). The same move through `morph p.x -> {k} 0.7s` glides (`scenes/morph-to-expr.dan`, `shots/morph-to-expr_sheet.png`), so the eased path exists and is simply not taken by re-mark. History is silent about it (`# since:` lists q's two changes, nothing for p) |
| Re-mark: a `kind` change | cut, identity kept | `scenes/remark-kind.dan` frame 2.1: the bar u is a large dot at once (`shots/remark-kind_sheet.png`) |
| Re-mark: a list that changes length | cut; the removed item has no exit and a surviving item flies from the wrong cell | `scenes/remark-list.dan` frames 4.0/4.3 (5 cells -> 3 cells at once); `scenes/grammar.dan`, `shots/grammar_2.1.png`: "left" (old cell 4 -> new cell 3) is drawn ON cell 0 over "I" at the start of its travel; "that" vanishes with no fade |
| `mark` on an existing link's id | in place, **instant** | `scenes/remark-link.dan`, `--snapshot --at 1.9,2.1,2.4,3.0`: `mark w w=0.9 color=yellow` shows "w = 0.9" and yellow at 2.1; `morph v.w -> 0.9 0.7s` shows 0.51, 0.77, 0.9 (`shots/remark-link_sheet.png`). Real lessons do this: session 20261024-101323 turn 4 re-marks the labels of links lz, lw, lb, lx with `mark` |
| re-`link` (new end) and re-`group` (new members) | in place, **instant** | `scenes/relink.dan` frame 2.05: the arrowhead is already on c, the enclosure already spans d (`shots/relink_sheet.png`); positions themselves glide (layout easing), so it reads as a swap, not a flicker |
| Morph from a wrong value: numeric | holds | `scenes/morph-wrong-from.dan`, `--check`: `identity: line 5 morphs a.value from 5 but it is 3; morphing from the real value (no jump cuts)`; `# since: morph a.value 3 -> 9 done` |
| Morph from a wrong value: string | holds | same scene: `identity: line 6 morphs s.label from 'closed' but it is 's'; ...`; since `morph s.label s -> done done` |
| Morph from a wrong value: list | holds, lint text broken | same scene: `identity: line 7 morphs l.items from '' but it is '[1, 2, 3]'` (the `from` is printed empty: engine.cpp:1036 uses `ln.from.str`, which `Value::parse` never fills for a LIST) |
| Morph with no `from` | holds (session finding 2 confirmed) | same scene line 8: no lint, `morph a.value 9 -> 12 done` |
| A second morph taking over mid-flight starts from the real value | holds live; **the lint is invisible to `--check`** | `scenes/morph-takeover.dan`; `./tutor F --layout-report - --verbose 2>&1 >/dev/null \| grep lint`: `identity: line 5 morphs a.value from 9 but it is 7.93; morphing from the real value`; `./tutor F --check`: axiom warnings 0 (instant mode has already landed the first morph). The chat's `<last_scene_issues>` comes from `--check`, so it can never carry this one |
| `hide`/`show` are opacity morphs; the thing stays | holds | `scenes/hide-show.dan`: readback keeps `opacity=0.12` / `opacity=1`; frames: 0.6 s fades, a ghost outline keeps its place, nothing else moves (`shots/hide-show_sheet.png`) |
| A hidden note leaves no ghost | holds | same: the cross on b is gone at 3.5 s while `mark x cross of=b opacity=0.12` stays in the readback |
| A link whose end is hidden ghosts too | holds | same: the arrow a -> b is faint while a is hidden, back when a is shown (`render.cpp:25-30`) |
| A note whose TARGET is hidden | not covered by any rule; stays at full strength on the ghost | `scenes/hide-target.dan`, `shots/hide-target_end.png`: a bright red cross and a yellow brace "note" on an almost invisible box b |
| `hide` on a group | holds | same scene: the enclosure G fades to a ghost (drawGroup uses alphaOf) |
| `scene "..."`: ids start fresh; readback shows the new scene only; a morph in flight is dropped; focus cleared; a learner param goes with the scene | holds, sanctioned by README:322 ("ids, colors, schema reset") | `scenes/scene.dan`: readback = `title "Two"`, `mark b`, `mark a bar value=5 label="a again"` (fresh colour), no `focus`, no `k`; `# since: ... scene change` |
| `interrupt()` / `skip()`: history and readback consistent afterwards | holds (by reading; headless `--stdin` has no `#!interrupt`) | engine.cpp:380-403: `recordSet` (trails) then every Active ticked to its end (`# since:` gets "... done", the value IS the end value), then the rest of the received script runs instantly; the server folds exactly what it sent (`S.cut` stops further beats), so state.dan and the stage agree |
| `#!load` | holds (by reading) | main.cpp:356-359 `eng.load(path)` -> `reset()`: a replay from scratch, things re-enter |
| Python `each` lines re-marked next turn | holds (by reading + the fold test) | compute.mjs:10-11: the engine only ever sees literal `mark r{i} ...` lines; a second block with the same ids re-marks in place; with FEWER items the extra things stay on stage (no delete) |
| The chat path cannot duplicate a thing across turns | holds | `scenes/turn1.dan` folded to `scenes/state.dan`, then `(echo "#!chunk"; cat scenes/turn2.dan) \| ./tutor scenes/state.dan --readback --stdin` == `./tutor scenes/whole.dan --readback` (model lines identical, one `mark a` line, order a b l c); the real `.fold.dan` of session 20261024-101323 replays byte-identical to its `state.dan`; by construction `order_.push_back` happens only on creation and the three maps are kept disjoint (engine.cpp:887, 966, 987) |
| `mark a` then `link a ...` / `group a ...` | holds (linted, dropped) | `scenes/ids.dan`: `identity: 'a' is already used`; `mark h label=` on a group: `identity: 'h' is already a link/group; ids must be unique`; `--check` exits 1 |
| `a[2]`-style ids | trap, no lint | `scenes/ids.dan`: `mark c[2] box ...` is accepted as a thing whose id is the text "c[2]"; `indicate c[2]`/`focus c[2]` then look for cell 2 of `c` and fail ("never appeared", "does not exist yet"). Decodable, so `--check` passes apart from those lints |
| Two marks of one id in different views | in place, silently | `scenes/views-same-id.dan`: the second `mark a dot x=2 y=2` in view `loss` re-marks the box `a` of view `net` (kind -> dot, x/y added), it stays in `net`; no lint (`shots/views-same-id_end.png`: a dot "A2" linked to B in the graph view) |

Judgement on "is an instant jump on re-mark a violation of identity, of continuity, or acceptable?": identity, as the
model, holds in every case above (one id, one slot in `order_`, one colour, one readback line). What breaks is the
picture's continuity (axiom 4), and the project put that promise inside axiom 1 ("so there are no jump cuts"). A new
property has nothing to ease from (acceptable, though `opacity` could start from 1 as `doMorph` already assumes).
A type change between number and expression is a number before and after: no reason for a cut, and `doMorph` shows
the eased path (start = `num(*cur)`, `finalVal` keeps the expression). Link numbers cut only because `mark` on a link
skips the per-property code; "A link is a thing too" (README:113) says they should ease. A kind change and a re-link
are shape changes; a cut is defensible there, a short cross-fade would be better.

## 2. Rule vs code gaps

1. README:362 "A re-mark updates in place ... so there are no jump cuts"; HANDOFF §3.4 "Every change is an eased
   morph" vs engine.cpp:957-960 (`else { ... setProp }` for a new prop or a type change), :883 (`mark` on a link:
   `setProp` per prop), :976-979 (re-link: ends and props set directly), :994-998 (re-group). Reproducers in §1.
2. README:362 "There is no delete verb" / dictionary "there is no delete" vs engine.cpp:867-874 + 1329-1337 +
   654-697: a `with` line joined to `scene` (execStep runs main then withs, engine.cpp:762-767) or a line cued on the
   scene's word (fireHook -> execStep, 1156-1186, ignores `blocking()`) creates a thing that `clearStage` then erases.
   Real chats never cue a scene (0 of 37 scene lines; 21 are followed by `title`, 12 by `schema`, 4 by `mark`), and
   uncued lines wait for the blocking CLEAR, so this has not bitten; it is still a silent delete.
3. README:111 "Re-marking an existing id ... never errors" vs engine.cpp:887-890: `mark` on a GROUP id is dropped
   with an identity lint (while `mark` on a LINK id is accepted, 881-886). Asymmetric and undocumented.
4. README:362 "A `morph ... from X` that doesn't match the real value is warned about" vs main.cpp:157-206: the
   warning for a takeover morph exists only in a timed run; `--check` and the shadow engine are instant, so the
   chat's `<last_scene_issues>` never shows it (`scenes/morph-takeover.dan`).
5. engine.cpp:1036: the identity lint prints `from ''` for a LIST `from` (`Value::parse`, script.cpp, fills `.list`
   not `.str`).
6. HANDOFF §6/README axiom 6: the readback prints "recent changes" vs engine.cpp:920-960: only NUM -> NUM re-marks
   carry `a.log`; category, string, list, kind, link, re-link, re-group and type-change re-marks leave no `# since:`
   line (`checks.txt`: remark-str, remark-list, remark-link, relink, remark-expr for p). Comments only, the model is
   right; but "recent changes" is what the next turn's Claude reads.
7. script.hpp:23-24 says a ghost is "still there (identity), taking no room" vs the frames (`hide-show`): a hidden
   thing keeps its slot and its room; nothing reflows. The prompt (chat/prompt.md:119 "it keeps its place, so nothing
   jumps") describes the code; the header comment does not.
8. README:83-84 says a hidden NOTE fades out completely; nothing says what a note does when its TARGET is hidden, and
   the code keeps it at full strength (render.cpp:14-24 rescales only the note's own opacity) while a link follows
   its ends (render.cpp:27-29). `scenes/hide-target.dan`.
9. The layout checker skips ghost marks and links (placement.cpp:131, 244) but not ghost groups (270-277): a hidden
   enclosure still counts as ink. `scenes/chem-atoms.dan`: `--check` layout score 0/100, six "overlaps group 'm3'"
   messages about a box drawn at opacity 0.12.
10. Parser: `mark c[2]` is a legal id (script.cpp readMark takes `toks[1]` as is) although `[` means "cell of"
    everywhere else (`baseId`, engine.hpp:63). No lint.
11. README:329-335 (views): nothing says ids are global across views; the engine re-marks the first thing and never
    moves it (`scenes/views-same-id.dan`), with no lint.

## 3. Where this axiom limits creative freedom

- Chemistry, a reactant that ceases to exist (`2H₂ + O₂ -> 2H₂O`). With molecules as things
  (`scenes/chem-molecules.dan`): the two H₂ boxes re-mark into H₂O (colour cross-fade, fine); O₂ can only be hidden,
  so a ghost "O₂" box stays beside the products for the rest of the lesson (`shots/chem-molecules_end.png`). It
  reads as "O₂ is still here, faintly", which is the wrong chemistry. With atoms as things and molecules as GROUPS,
  the identity-true model (atoms persist, bonds change), `scenes/chem-atoms.dan`: the two water groups form by
  re-`group` (good), but the O₂ group cannot be dissolved: `group m3 part-of=[]` is a parse error ("group needs
  part-of=[a,b,...]", `scenes/chem-atoms-empty.dan`) and `hide m3` leaves a ghost enclosure that straddles both
  waters (`shots/chem-atoms_end.png`) and zeroes the layout score. "No delete" plus "no empty group" blocks the
  honest picture of a reaction. The same shape: a cell dividing (one group of organelles becoming two), a company
  being split, a chord changing (the old chord's grouping must vanish).
- Grammar, deleting a word (`scenes/grammar.dan`: "I know that you left" -> "I know you left"). Cells are not things,
  so the model allows it, but the picture cuts: the array shrinks at once, "that" vanishes with no exit, and "left"
  starts its travel ON cell 0 over "I" (`shots/grammar_2.1.png`): render.cpp:387 takes the travel origin from
  `cellRect(m, from[j])` in the NEW geometry, so an old index beyond the new length wraps to the wrong cell. The
  same happens for a stack pop, a removed list node, a histogram re-binned with fewer bins.
- Data re-plotted with fewer points (python `each` on a second turn, or a Claude re-mark of `r0..r9` as `r0..r4`):
  the five stale things stay, and Claude would have to `hide` each by id (ids it may not remember). By construction;
  not run (it is the plain re-mark path verified in §1).
- History and economics cope well: an empire that ends, a firm that exits, a note that stops sounding are exactly
  what `hide` gives (a trace at 0.12 that keeps its place), and the toolbox's "Tidy what's done" is the move. No
  scene needed beyond `hide-show.dan`.
- The cost of "ids are global and immortal": within one scene an author can never reuse an id for a new thing (the
  `views-same-id` case), and after a `scene` a learner's slider is gone with the rest (chat/server.mjs keeps
  `learnerSet` per turn, so this is at least consistent).

## 4. Small model

Measured against `reports/Small model for tutor weak spots.md` (don't train a scene writer; small models fit narrow,
label-rich, checkable jobs; most failures are mechanical and belong to code). In this axiom's area the conclusion is
stronger than the report's: there is no job for a model at all.
- Enforcing identity is pure bookkeeping and the engine already does it completely (one map per id, no erase, the
  real value as every morph's start). Every gap in §2 is a code fix of 5-25 lines with a deterministic test.
- The one decision near identity that is judgement, "is this a re-mark of the thing on stage or a new thing?", is
  made by Claude with the `<stage>` block in front of it, and it gets it right in the chats I scanned (57 cross-turn
  re-marks in 41 sessions, all intentional updates of the same thing; no case of a new id for an old thing that the
  fold then duplicated, because the fold cannot duplicate). Labels for a tagger would have to be invented; nothing in
  the sessions marks "should have been a re-mark". Not worth building.
- "Hide or new scene?" (tidying) is taste, not identity; the report's step 6 (log the user's redo/reject choices)
  is the only path to labels for it, and a frontier model with the last frame does it today.
- A model could in principle learn to write `from` values correctly, but the engine ignores wrong ones; session
  finding 2 already concluded the field is decorative. The cheaper move is a sentence in the prompt.

## 5. Proposed fixes, ranked by value over cost

1. Ease every numeric re-mark and every re-marked link/group property. Factor the per-property loop of
   `doMark` (engine.cpp:931-960) into `updateProps(Obj &, const Props &)` and use it for marks, `mark` on a link
   (883), re-link (978) and re-group (997); in the numeric case test `cur->isNumeric() && p.val.isNumeric()`, start
   from `num(*cur)`, `a.to = num(p.val)`, `a.finalVal = p.val` (the expression survives, exactly as `doMorph` does
   at 1059-1066); a new `opacity` starts from 1. Test (tests/test_engine.cpp, a non-instant engine as the "shadow"
   test at 127 uses): play `schema axes x=[0,6] y=[0,6]`, `mark k param value=4`, `mark p dot x=1 y=1`,
   `link w arrow p -> k w=0.5` ... then append `mark p x={k}` and `mark w w=0.9`, `update(1/60)` once: today
   `num(p,"x") == 4` and `w == 0.9` (fail); after, both strictly between the old and new values, and the readback
   at the end still prints `x={k}`. Axioms: 4, and the no-jump clause of 1; 6 unchanged. ~25 lines, 1-2 h.
2. No delete during a scene fade. In `tickActives` CLEAR (1329-1337), erase only the ids that existed when the
   `scene` line ran (store `order_` in the Active's `dest`/a vector at 867-874), and let things created during the
   fade keep their entry (re-claim their colour after `clearStage`); or, simpler, queue `with`/cued lines that
   arrive while a CLEAR is in flight and run them when it lands. Test: `scene-with.dan` and `scene-cued.dan` read
   back with `mark a bar value=3 label=a`. Axiom 1. ~15 lines, 1 h. Also a parse-time lint "`with` after `scene`
   runs before the stage clears" if the queue is not built.
3. Allow an empty group: `group g part-of=[]` parses (script.cpp readGroup), draws nothing (groupBounds already
   returns 0 width), reads back as written. Test: `chem-atoms-empty.dan` passes `--check` with layout 100 and the
   readback keeps `group m3 part-of=[]`. Axioms 1 and 3 (a dissolved molecule leaves no ink). ~6 lines, 30 min.
4. Ghosts: (a) skip ghost groups in `picture()` (placement.cpp:273, `if (ghost(gt->second)) continue;`): test
   `chem-atoms.dan` scores 100 with no "overlaps group 'm3'"; (b) a note of a hidden target ghosts with it
   (render.cpp:14-24: for `isAnnotationKind`, multiply by the target's `opacityOf`, as `ghost()` does for links):
   test `hide-target.dan`, `anyInk(picture(), "x") == nullptr` after `hide b`, as the test at test_engine.cpp:347
   checks for a hidden note. Axioms 3/5 (hide semantics). ~10 lines, 1 h.
5. Three small lints/logs: (a) `mark c[2]`: parse error "an id can't contain [ ]: c[2] means cell 2 of c"
   (readMark); test: `ids.dan` fails to parse that line. (b) engine.cpp:1036: print `ln.from.markup()` so the list
   lint reads `from '[7, 7, 7]'`. (c) give every re-mark branch an `a.log` ("mark s.status open -> done") and push
   a "set" line for the direct branch, so `# since:` matches "recent changes"; test: `remark-str.dan`'s readback
   contains `# since: mark s.@category open -> done done`. Axiom 6. ~15 lines, 1 h.
6. Array travel on a length change (render.cpp:355-385): compute origins with the OLD cell count (a `cellRect`
   overload taking the count), and draw removed items fading out at their old cells. Test: factor the origin
   mapping into a function and check that for old `[I,know,that,you,left]` -> new `[I,know,you,left]` the origin of
   "left" is old cell 4, not cell 0; or a snapshot-based check that "left" is never drawn left of cell 2. Axiom 4.
   ~20 lines, 1-2 h.
7. Documentation only: README:111 and the prompt: `from` is optional and a wrong one is corrected (session finding
   2); ids are global across views and scenes end a learner's slider; a hidden thing keeps its room (fix the
   script.hpp:24 comment).

## 6. Checked and fine

- `./tests/test_engine`: 31 cases, 184 assertions pass (identity tests at lines 41, 103, 347 included).
- NUM -> NUM re-mark: 0.7 s eased, non-blocking, logged (`remark-num`). A re-mark during a morph in flight takes
  over from the mid value (`settleActive`, engine.cpp:1004-1021, read; the takeover morph in `morph-takeover.dan`
  shows the same rule for morphs: starts from 7.93).
- Category and label re-marks cross-fade over 0.8 s (`remark-str`); same-length lists travel (`remark-list`
  frame 2.4, `listNums` layout.cpp:213-226 for numeric lists).
- Morph from a wrong value (numeric, string, list) and from no value: corrected, linted, history shows the real
  start (`morph-wrong-from`). Session finding 2 confirmed.
- `hide`/`show`: 0.6 s opacity morphs; ghost at 0.12 keeps its place; a hidden note leaves no ghost; a link ghosts
  with a hidden end and returns with `show`; a group honours opacity; the readback keeps `opacity=` (`hide-show`).
- `scene`: readback shows the new scene only; fresh colour slots; `focus_` cleared; the learner's param wiped with
  the scene; an in-flight morph dropped (no "done" line live, a "done" line in instant mode: comments differ, the
  model does not) (`scene.dan`).
- Cross-turn: streamed (`#!chunk`) == whole for a re-mark of a mark and of a link; one line per id; the real
  session's `.fold.dan` reproduces its `state.dan` exactly; the learner slider's `mark id value=v` appended to
  state.dan is an ordinary re-mark (server.mjs:188).
- Ids: `link`/`group` on a used id are linted and dropped; `mark` on a group id linted and dropped; `mark` on a
  link id updates it (sanctioned); no sequence of lines can put one id twice in `order_` or in two maps.
- `interrupt()`, `skip()`, `#!load`: by reading, the readback after them is the end state the history claims.
- `--check` fails (exit 1) on every identity lint it can see (main.cpp:189-205).

---
## Appendix: working log (chronological, as appended during the audit)
### Code reading, 2026-10-24 (before any reproducer)

Containers: `marks_`, `links_`, `groups_` (maps by id) and `order_` (engine.hpp:450-453). The only code that
removes from them is `clearStage()` (engine.cpp:659-662, run from the CLEAR Active of `scene`) and `reset()`
(engine.cpp:303, on load/`#!load`). Grep over markup/*.cpp for erase/remove/pop_back on those four names finds
nothing else. So "no delete" holds by construction at the model level.

Ids are unique across the three maps by construction: `doMark` on a group id warns and returns (887-890),
`doLink`/`doGroup` on a mark/other id warn and return (966, 987); `order_.push_back` happens only on creation.
Two things with one id, or one id twice in `order_`, cannot arise from any line sequence. `doMark` on a LINK id is
the one asymmetric case: it is accepted and updates the link's props in place (881-886), with no lint (README
sanctions it: "A link is a thing too: give it a name, a value, a colour, and point at it like any mark").

Re-mark branch (920-960), per property type:
- category word changed: STR Active 0.8 s on "@category" (colour cross-fade)        -> eased
- NUM -> NUM (different): NUM Active 0.7 s, non-blocking, logs "mark id.k a -> b"   -> eased, in `# since:`
- LIST -> LIST (different text): STR Active 0.9 s (strBlend; listNums eases same-length lists; arrays travel)
- STR -> STR (different): STR Active 0.8 s cross-fade
- everything else -> direct `setProp` (957-960): a property NEW on the re-mark (no `cur`), any TYPE CHANGE
  (NUM -> EXPR, EXPR -> EXPR, EXPR -> NUM, STR -> NUM, ...). `recordSet` only feeds `hist_` (trails.cpp:32-39),
  NOT `since_`, so the readback history is silent about these jumps.
- `kind` changes in place instantly (`m.kind = ln.kind`, 921); `view` is never changed on a re-mark.
Links (883) and re-`link`/re-`group` (976-979, 994-998): direct setProp for every prop, ends and members: instant.
`morph` on a link works through `find()` (links are Objs), so a link's number CAN ease, via morph, not via mark.

Morph: numeric start is always `num(*cur)` (1050), string/list start is the real text (1032); a wrong `from`
warns and is otherwise ignored (1033-1037, 1053-1057). Lint texts at 1036 and 1056. Confirms session finding 2.
Note the string lint prints `ln.from.str`, which for a LIST `from` may be empty (to verify).

`scene`: CLEAR Active, blocking (Active::blocking defaults true, engine.hpp), 0.7 s; lines after it wait
(update loop stops while blocking(), engine.cpp update). `clearStage` wipes the model, erases NUM/STR actives
without logging them, keeps `since_` and pushes "scene change", clears focus_, colour slots, scales.
Two ways a thing can be created DURING the fade and then wiped (a real delete of a declared thing): a `with`
line joined to the scene step (execStep runs main then withs), and a line cued on the same |cue| as the scene
(fireHook -> execStep ignores blocking). To reproduce. Real chats never cue a scene (0 of 37 scene lines).

`interrupt()` (380-403): NUM actives in flight get a `recordSet` (hist_) then every Active is ticked to its end
(since_ gets "... done" from tickActives), then the WHOLE remaining script runs instantly (`while (!finished())
update(0)` with instant=true). The server stops sending beats after the cut (`S.cut`), and the fold is
stateBefore + turn.dan (what was sent), so stage and state.dan agree. Headless `--stdin` only knows `#!chunk`,
so this is verified by reading, not by a run.

Chat path: state.dan = folded readback (model lines only, comments stripped, server.mjs:221-224); turn N+1 is
checked/folded as stateBefore + turn (server.mjs:432-436). A re-mark of a turn-1 id is an in-place update in the
fold; duplicates are impossible (above). A learner slider appends `mark id value=v` to state.dan (188): a re-mark.

### Runs, batch 1 (checks.txt has every --check and --readback output; scenes in scenes/, frames in shots/)

- morph-wrong-from.dan: all three lints fire exactly as the code says; the list one prints `from ''` (the LIST
  `from` has no `.str`): cosmetic bug, engine.cpp:1036. History: `morph a.value 3 -> 9 done; morph s.label s ->
  done done; morph l.items "[1, 2, 3]" -> [3, 2, 1] done; morph a.value 9 -> 12 done` (the real starts).
  `--check` exits 1 with "axiom warnings: 3". Session finding 2 confirmed.
- morph-takeover.dan (second morph cued while the first is in flight): live run (`--layout-report - --verbose`)
  lints `identity: line 5 morphs a.value from 9 but it is 7.93; morphing from the real value (no jump cuts)` and
  the second morph starts from 7.93 (no jump). `--check` (instant) shows NO identity lint: in instant mode the first
  morph has already landed. So the chat's `<last_scene_issues>` can never carry this lint; only the stage's stderr.
- scene-with.dan: `scene "Two"` + `with mark a bar value=3`: the readback after the scene has NO `a` (only `b`). The
  with-line runs in the same execStep as the scene, then clearStage wipes it. A declared thing deleted, no lint.
- scene-cued.dan: `@go scene "Two"` + `@go mark a …` on one cue: same, `a` is gone from the readback. Fired by
  fireHook -> execStep, which does not wait for the blocking CLEAR.
- scene.dan: readback after the scene shows the new scene only (title "Two", b, "a again" with a fresh colour);
  `# since:` keeps the pre-scene history ("morph a.value 3 -> 9 done; scene change" in instant mode; live, the
  in-flight morph is erased without a "done" line, so the two histories differ: comments only, not the model).
- remark-*.dan readbacks: NUM re-marks log `mark a.value 3 -> 9 done`; category, string, list, link, re-link,
  re-group and type-change re-marks leave NO `# since:` entry at all (only NUM re-marks and morphs have `a.log`).
- remark-expr.dan readback: `# since:` has q's two NUM entries and nothing for p (NUM -> EXPR, the direct setProp).
- ids.dan: `link a …` on a mark id -> `identity: 'a' is already used`, link dropped; `group a` same; `mark h
  label=` on a group -> `identity: 'h' is already a link/group; ids must be unique`; `mark c[2] box …` is ACCEPTED
  as a mark whose id is the string "c[2]", and then `indicate c[2]` / `focus c[2]` look for "c" (baseId) and fail:
  "indicate: 'c[2]' never appeared", "focus: 'c[2]' does not exist yet", plus the static "refers to 'c[2]', which
  is never marked". Decodable (readback prints `mark c[2] …`). A trap with a cheap parse-time fix.
- views-same-id.dan: `mark a dot x=2 y=2` in a second view, when `a` is a box in the first view: silently
  re-marks the first `a` in place (kind box -> dot, x/y added), it stays in view `net`; no lint. Ids are global
  across views; the author's intent (a second thing called a, in the loss view) is impossible and unreported.
- turn1/turn2 fold (headless `--stdin` with `#!chunk`, the chat's own path): streamed == whole, one `mark a` line,
  `mark l label="L2"` folded into the `link l` line, order a b l c. The real session 20261024-101323's `.fold.dan`
  replayed with `--readback` is byte-identical to its `state.dan`. No duplicate is possible (see code reading).
- hide-show.dan readback: `mark a … opacity=1` after show (the prop stays), `mark x cross of=b opacity=0.12`,
  `group g … opacity=0.12`; history logs all four opacity morphs.
- chem-atoms.dan: `hide m3` (the O₂ group whose atoms moved into the two waters) keeps the ghost enclosure in the
  MODEL and the layout checker counts it: "dot 'hc' overlaps group 'm3'" x6, layout score 0/100 for a hidden
  box. chem-atoms-empty.dan: `group m3 part-of=[]` is a parse error ("group needs part-of=[a,b,...]"): a group
  can never be dissolved.
