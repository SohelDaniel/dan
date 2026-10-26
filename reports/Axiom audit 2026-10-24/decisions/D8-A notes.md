# D8-A: rename the verb `mark` to `thing`?

Engineer D8-A, 2026-10-24. Worktree: `.claude/worktrees/agent-aabdb1e1308f9e0ed` (HEAD 6038197). Nothing committed.
This decision is a judgment with evidence; the notes follow the brief's headings, with "Research" and "Cost"
sections added where the decision text asks for them.

## Problem

The docs call every mark "a thing" (README language section: 38 uses of thing/things, 3 of "mark(s)" as a noun;
lints say "no thing called 'x'"), while the verb that creates one is `mark`. The user, reading Dan as a human
(and now typing it in `tutor write`), found `mark` opaque and asked what it means (session-notes §3, §6). The
question is whether `thing` would be better for Claude, the actual author, and what a rename costs.

Reproduced (what the engine does today with each spelling):

```
printf 'thing p dot x=1 y=2\n' > thing-probe.dan; ./tutor thing-probe.dan --check
parse: line 1: unknown verb 'thing'   | thing p dot x=1 y=2
decodable: yes ... layout score: 100/100          # the line is dropped, fail-soft, the stage stays empty
```

Baseline error rate of the verb as it is (does `mark` cause any errors?):
- Audit 8 (80 eval scenes + 63 session turns, 5,351 lines): 3 parse errors, none about the verb; 0 verb
  substitutions; the 20 live "unknown verb" lines were the lint-leak bug, not a verb slip.
- My own census of the user's real chats (41 sessions, 63 turn files, 1,955 non-blank lines, read-only):
  head word after stripping `@cue` and `with`: mark 626, morph 220, link 159, indicate 92, send 60, schema 59,
  title 54, hide 50, scene 37, focus 30, group 4, wait 2, camera 2, show 1. No other head word at all.
- Does `mark` prime pixel thinking? Retired placement words in the chats: `region=` 8, `pos=` 5, `below=` 4,
  `right-of=` 3, all in sessions dated 2026-10-17..20, none since; 0 `x=`/`y=` on non-plot kinds. Nothing ties
  them to the verb.

So there is no model-side failure to fix. The case for `thing` is vocabulary hygiene (one word for the noun and
the verb) and human readability.

## Cost inventory (b): what a rename touches

Counted in the worktree (`git grep`, `grep -c`), lines whose head is the verb (after optional `@cue`/`with`):

| where | verb lines | notes |
|---|---|---|
| `*.dan` (102 files) | 521 | scripts 268, chat/idioms 164, chat/exemplars 58, chat/examples 31 (examples are in the prompt) |
| `README.md` | 59 (57 in the language section) | the language section is part of the prompt; plus ~10 prose uses: "Re-marking", "like any mark", "### Mark kinds", "mark samples one by one" |
| `chat/toolbox.md` | 86 | plus 5 prose "re-mark" in "Tools:" lines; every move is a test (`tests/toolbox.mjs`) |
| `chat/idioms.md` | 164 | data, not in the prompt |
| `chat/prompt.md` | ~10 incl. the `each v in ...: mark` form | prose: "Re-`mark` to update", "every mark earns its place", "re-mark `items=`" |
| `docs/guide.html` | 71 code lines, 120 word forms | the human guide, hand-written HTML |
| `tools/write.html`, `tools/write.mjs` | 6 + 3 | the playground: keyword set `KW`, `TAKES_ID`, help text "mark id kind category k=v … a thing (re-mark = update in place)" |
| `tests/test_engine.cpp`, `tests/test_script.cpp`, `tests/compute.mjs` | 24 string-literal lines (55/9/25 word forms) | scenes inside C++/JS strings: `"@grow mark a box"`, `"mark t table ..."`, `each r in walk.rows: mark p{i}` |
| `markup/script.cpp` | verb table line 564, aliases 561-562, errors "mark needs an id", "one kind and one category per mark" | |
| `markup/engine.cpp` | readback `o << "mark "` (1572), captions "re-mark X -> same mark, updated in place" (884, 962), "between the marks" (980, 983), lint "which is never marked" (237, 241, and the filter at 230) | |
| `markup/binding.cpp` | lint text "a shared category on marks" (391) | |
| `tools/mine/mine.mjs` | the translator prompt ("re-mark updates in place") | |
| `docs/HANDOFF.md`, `docs/history.md`, `reports/*`, `research_notes/*`, `docs/plan-*.md` | prose and quoted scenes | history stays as written (dated); the map must change |
| `tests/golden/*.json` | keys are file paths, values layout scores | unchanged by a pure rename |
| the user's data: `chat/sessions/*` (41), `chat/library/*`, `chat/settings.json` | old turns and stages spelled `mark` | must keep parsing: a silent legacy alias in the parser forever, or a migration of the user's files (asking first) |

## Research (a): does a keyword's spelling change an LLM's write-error rate?

Done by a research agent (web), with the load-bearing sources re-fetched and checked by me (marked "checked").
The principles file already holds the general studies; the new material is what bears on a pure rename.

**There is no published experiment that isolates a keyword rename (grammar identical, spelling changed) and
measures LLM generation accuracy.** What exists is adjacent:

| study | what it varied | finding | bearing |
|---|---|---|---|
| Miceli-Barone et al. 2023, ACL Findings (arXiv 2305.15507) | redefines builtins (`len, print = print, len`) and asks which body is right | models prefer the un-swapped body; GPT-3.5/4 under 4% right; inverse scaling for OPT/GPT-3; swapping *non-builtin* names shows the same pattern with smaller losses | mechanism, Direct: the cost of a token is proportional to how strongly its pretrained meaning conflicts with the in-context one |
| Hooda et al. 2024, ICML (arXiv 2402.05980) | renames user identifiers (random / shuffled); keywords explicitly excluded | Llama-2 7B flips 25-29% of pass/fail; GPT-4 moves ~1.6% | Indirect: weak models lean on names, strong ones barely |
| ReCode, Wang et al. 2022 (arXiv 2212.10264) | perturbs the function name, docstring, syntax, format | relative pass@1 drop (worst of 5): docstring 61-75%, syntax 73-93%, **function name 42-47%**, format 43-46% (9 models of 2022) | Indirect: a name's spelling matters less than wording or syntax |
| Marinov et al., LMPL 2026 (arXiv 2609.00579), **checked** | a toy language with its EBNF and semantic rules IN THE PROMPT; `KeywordSwap` swaps the meanings of familiar operators (+ means -), `KeywordObf` replaces operators and keywords by novel single-token symbols (⋆ for +) | on human-written programs "median accuracy falls by 19pp under KeywordSwap versus 32pp under KeywordObf" (Ministral 3B-14B, Qwen2.5-Coder 3B-32B, DeepSeek-Qwen 14B/32B); conclusion: "LLMs lean on pre-training priors rather than systematically applying the given rules". The per-model worst cases quoted in its text do not match its tables, so I cite only the medians | Indirect (a prediction task, exotic glyphs): the only study with the grammar fixed and only spellings changed. Novel spellings cost more than misleading familiar ones, even with the spec in the prompt; but the novel spellings were tokenization-hostile glyphs, not English words |
| "When Names Disappear" 2025 (arXiv 2510.03178), CodeCrash 2025 (arXiv 2504.14119) | obfuscated or misleading identifiers, reasoning tasks | meaningless names cost about as much as misleading ones for identifiers; misleading cues cost 23% on average | Indirect |
| Anka, Dec 2025 (arXiv 2512.23214), **checked** | a DSL designed for LLM authoring, taught by "a concise syntax guide (approximately 100 lines)" | Claude 3.5 Haiku: 99.9% parse success, 95.8% task accuracy vs 91.2% in Python; principles stated: "One canonical form per operation", "Anka uses English keywords: FILTER, MAP, AGGREGATE, WHERE, INTO", "Verbose syntax trades brevity for clarity"; no experiment varies keyword spelling | Precedent: an LLM-first DSL chose plain English imperative verbs, one form per operation; a never-seen vocabulary is learned from a 100-line guide with 0.1% parse failure |
| Grammar prompting, Wang et al. NeurIPS 2023; MTOB, Tanzer et al. ICLR 2024 | a grammar or a grammar book in the prompt | +4 to +6 points on unfamiliar DSLs, nothing on familiar ones; a wholly new language learned to within 6-11 chrF of a human | Indirect: unfamiliarity itself is cheap when the spec is in context |
| Stefik & Siebert 2013, ACM TOCE 13(4), **checked** (PDF text) | HUMAN novices rate words for a concept (0 = 0% intuitive, 10 = 100%), then program in six languages incl. Randomo (random keywords) | loops, non-programmers: repeat 6.88, again 6.43, loop 6.30 vs foreach 2.99, while 2.37, for 2.13; programmers: loop 7.88, repeat 7.49 vs foreach 4.35. Accuracy: Quorum, Python, Ruby significantly above Randomo (M = .341, SD = .173); Perl not (p = .458), Java not | Direct for human readers: with identical semantics, a word that says what the line does triples novice intuitiveness; experts prefer what they already know |

Vocabulary precedents (official docs, fetched by the agent): Vega-Lite "The mark property ... a string describing a
mark type" (`area, bar, circle, line, point, rect, rule, square, text, tick`); Protovis "All graphical elements in
Protovis are called marks"; HTML `<mark>` = highlighted text (a different, weaker prior); PlantUML `participant`
/ `actor`, Mermaid `participant`, Structurizr `person`, Terraform `resource` head a declaration with the KIND noun;
Graphviz declares a node by its bare id. No DSL known to either of us uses `thing` as a declaration keyword
(Inform 7 uses "thing" as its base kind, in sentences: "The table is a thing in the kitchen").

What this says for `mark` vs `thing`:
- Both are single, common English tokens: the spelling-cost term (KeywordObf's glyphs, `llllIII`) is zero for both.
- The documented first-order hazard is a word whose pretrained meaning CONFLICTS with the in-context one. `mark`
  has a compatible prior (grammar of graphics: a mark is a graphic primitive whose kind is a parameter, exactly
  Dan's role) and a mild sideways one (highlight, "mark as"). `thing` has an empty prior: safe, but it gives the
  model no free semantics, and as a line head it is a noun where every other Dan verb is an imperative (link, morph,
  focus, indicate, send, hide, show, wait) or the kind itself (schema, camera, scene, title).
- Expected effect of the rename on Claude's error rate: nil to small, in either direction, inside the judge's
  ±0.15. The literature's real rule is "never give a strong-prior word a meaning its prior contradicts", which
  Dan already obeys for `mark`.
- For HUMAN readers the evidence is direct and large: a word that says what the line does helps novices
  (Stefik), and the returning author prefers what they know. `thing` is transparent but empty; `mark` is domain
  jargon with the right meaning for a chart-literate reader and the wrong one ("highlight") for a web developer.

## Designs considered

1. **Keep `mark`; say what it means where a human first meets it.** No language change. The README verb table's
   gloss ("a thing and its properties") becomes "put a thing on the stage: its id, kind, category and properties;
   marking the same id again updates it in place". The human guide and the playground help already say
   something close ("a thing (re-mark = update in place)"). The noun stays "thing" everywhere: `mark` (verb) puts
   a thing (noun) on stage, as "draw" puts "a drawing". Cost: 1-3 prose lines. Axioms: none. Principle 8: keeps
   the grammar-of-graphics prior. Principle 7: one verb, one noun, defined once.
2. **Rename the verb to `thing`** (the decision's candidate). Parser table, readback's canonical word, lint and
   caption texts, every `.dan`, README, prompt, toolbox, guide, playground, tests, HANDOFF; silent legacy aliases
   `mark`/`re-mark`/`remark` in the parser so the user's 41 sessions, the library and old scripts keep playing
   (principle 7 says don't teach two spellings; an untaught alias has the same status as `point`/`remark` today).
   Axiom 6 holds: readback prints one word. Measured below (pilot + migration dry run). Costs that the count of
   lines does not show: (i) `thing` has no participle or "re-" form, so "re-mark", "marked", "marking", "never
   marked", "unmarked" all need another verb; in the B edition 10 of the 32 prose edits had to introduce
   "declare ... again" or "`thing` again", i.e. the vocabulary does not become one word, it becomes `thing` plus
   an unofficial verb; (ii) "every thing" reads as "everything" in prose; (iii) a noun heads the line where every
   other verb is an imperative; (iv) the user's old sessions read back as `thing` while their files say `mark`.
3. **Kind first, no verb: `dot p x=2`, `box alice label=Alice`** (PlantUML `actor Bob`, Structurizr `person`,
   Terraform `resource`). The most natural line for a human, one token shorter, and it would end the one real
   verb-adjacent failure the audit found (a category word that is a kind name: `mark A rect area` drops the line),
   because the head word is always the kind. Rejected for this decision: the kind is optional today, so a
   kind-less line still needs a head word (which would be... `thing`), giving two forms (principle 7); 34 head
   words instead of one for Claude to keep; a grammar change in every file; and the kind/category clash has a
   6-line fix in the audit (fix 1). Worth remembering if the language is ever redesigned for human authors.
4. **Another verb** (`put`, `add`, `new`, `set`, `show`, `draw`, `place`, `is`, `let`, `def`, `declare`): each
   fails on one axis, as the session notes found: creation-only or update-only words misdescribe the re-mark,
   `put`/`place` suggest position (against "position comes from structure"), `draw`/`show` suggest pixels (`show`
   is taken), `is`/`let`/`def` read as code, `declare` is long and code-flavoured. `mark` is the only short
   English verb whose domain meaning (a graphic primitive, Vega/Protovis/Bertin) and ordinary meaning ("mark a
   point", "mark it again") both fit, and it alone gives the update its name, "re-mark".

## Pilot (c): 10 + 10 scenes, Sonnet, low effort

Design: the same 10 questions from `prompts-broad.json` (fractions, bayes, balancing, dna, heart, quicksort, tcp,
supply, ww1, passive: non-maths fields with many marks, where the audit found the kind/category clashes), one
scene each, `tools/eval/run.mjs --variant full --model sonnet --effort low --jobs 2`, in my worktree.
- Arm A: the tree as committed (prompt files unchanged). System prompt on record: `pilot/system-A.md`
  (65,497 chars, 174 `mark` lines).
- Arm B: the "thing edition": `rename.py` over README.md, chat/prompt.md, chat/toolbox.md, chat/examples/*.dan
  (185 verb lines), then the 32 prose edits in `prose-B.py` (each asserted to match once), and a one-line parser
  alias `thing -> mark` so `--check` can score it. The B edition passes `node tests/toolbox.mjs` (31 ok, 0 FAIL)
  and the four examples check clean. System prompt: `pilot/system-B.md` (65,786 chars, 175 `thing`, 0 `mark`).
  The arms differ only in the verb's spelling and the prose that names it.
- Metric script: `pilot-metrics.mjs` (deterministic counts from scene.txt, check.txt, meta.json). Outputs:
  `pilot/A/full/<id>/`, `pilot/B/full/<id>/` (reply, scene, check, frames, meta).

| | A `mark` | B `thing` |
|---|---|---|
| check ok | 10/10 | 9/10 (supply: three `zero:` lints, points coinciding on axes; nothing to do with the verb) |
| decodable | 10 | 10 |
| parse errors / unknown verbs | 0 / 0 | 0 / 0 |
| verb lines (head word) | 97 `mark`, 0 `thing` | 92 `thing`, 0 `mark`; other heads: morph 22, link 13, indicate 11, title 10, schema 9, hide 8, wait 1, focus 1 |
| unique lints (not layout) | 11 (binding 9, hook 2) | 19 (binding 10, hook 3, zero 3, stagger 1) |
| lines / chars | 241 / 14,508 | 242 / 14,886 |
| seconds per scene | 10-39 | 11-39 |

Reading: Sonnet at low effort writes `thing` lines as fluently as `mark` lines, copies the renamed prompt
exactly, never mixes the spellings and never puts a verb-shaped word where the noun now stands (`thing c box
party label="client"`, `@open thing k1 check of=c`, `with thing pv at=a[3]`). The lint difference is content
(numbers said but not shown, a cue without its line, coinciding dots), the kind that varies 0-4 per scene from
sample to sample; at n=10 per arm it is noise, and I claim nothing from it. What the pilot establishes is only
the precondition for a rename: no gross harm. It cannot show a benefit, and the audit's pre-registered A/B
(44 questions x 2 spellings x 2 runs, judge + parse rate) would be the test if anyone wanted one; the
expected effect from the research above is nil, so I do not recommend spending it.

Frames: `pilot/A/full/*/frame_*.png`, `pilot/B/full/*/frame_*.png` (both arms rendered).

## Chosen design

**Design 1: keep `mark`.** Nothing in the language changes. What changes is one gloss where a human first meets
the verb, so the complaint that started this ("what does `mark` mean?") is answered on the page rather than by
renaming the language:
- README, the verb table (inside the prompt): `mark ... a thing and its properties` becomes `mark ... put a thing
  on the stage: id, kind, category, properties; the same id again updates it`. One line; the toolbox test and
  `make check` stay green (the table is a code block the parser never sees).
- `docs/guide.html` and `tools/write.html` (the playground help) carry the same sentence.

Axioms: none touched (no behaviour changes). Principle 7: one verb, one noun, each defined once; `mark` makes
a thing, as "draw" makes a drawing. Principle 8: `mark` keeps its matching prior (grammar of graphics).

Why not design 2, in one paragraph: the verb causes zero errors in ~7,300 real lines; the research gives no
mechanism by which an empty-prior noun would write better than a compatible-prior verb, and the pilot shows the
two spell the same; the rename costs ~1,100 verb lines, ~60 prose and code-string edits, a permanent legacy
alias for the user's data, and it does not even achieve the one-word vocabulary it promises, because a noun has
no "re-" form (10 of 32 prose edits needed "declare ... again"). The human-reader gain is real but small, and a
one-line gloss buys most of it. If the user wants `thing` for their own reading anyway, the migration below is
measured and ready; it is a taste decision the user owns, not an engineering one.

What Claude's markup looks like before/after: unchanged (`mark alice bar account balance=70 label=Alice`).

## Migration dry run (cost, measured): the whole rename applied to the worktree, then reverted

Tools (all in this folder, re-runnable): `rename.py` (the mechanical pass over verb lines: line heads with
optional `@cue`/`with`, fenced and inline code, scenes inside C++/JS string literals, `each ...:` templates),
`prose-B.py` (32 hand edits in the prompt-facing prose), `migrate-code.py` (27 exact edits in code: parser table
with `thing` canonical and `mark`/`re-mark`/`remark` as silent legacy spellings, the readback word, 2 captions,
3 lint texts, 3 comments, the each-expander's head word, the server's slider line, the playground's keyword sets
and help, the mining prompt, 2 HANDOFF lines).

Result: `migration.diff`, **120 files changed, 1,062 insertions, 1,062 deletions** (line for line): 909 verb
lines in .dan/idioms/guide/tests/playground + 185 in the prompt files, plus the prose and code edits. Not done
(would be needed): `docs/guide.html` inline `<code>mark …</code>` mentions outside `<pre>` blocks (HTML has no
backticks for the script to key on), `reports/`, `research_notes/`, `docs/history.md`, `docs/plan-camera-fields-trails.md`
(historical: leave), and the user's data (41 sessions, library: covered by the legacy alias, never rewritten).
Engine rebuilt and the full suite run on the migrated tree (outputs `migration-test.txt`, `migration-check.txt`,
`migration-golden.txt`, `migration-append.txt`):

```
make test    doctest: 21/21, 14/14, 8/8, 31/31 cases passed (35,525 + 18,066 + 42 + 184 assertions); compute tests ok
make check   124 ok, 0 FAIL (93 scripts/examples/idioms + 31 toolbox moves), exit 0
make golden  mean layout score 97.4 over 52 scripts, 0 WORSE, exit 0   (a rename cannot move a pixel, and did not)
make append  152 identical, 4 different: scripts/space-downhill.dan (chunks of 2, 3, 5), scripts/space-saddle.dan
             (chunks of 2): the known pre-existing DIFFERS (stale lint comment, model identical), same as HEAD
./tutor scripts/bank.dan --readback   -> "thing alice bar account balance=70 label=Alice unit=$"
legacy: "mark p dot x=1 y=2 / re-mark p x=3" --check decodable, reads back "thing p dot x=3 y=2"
```

So the mechanical cost is about 1,100 lines and 60 hand edits, an hour of work plus review, and every regression
stays green. The permanent costs are the legacy alias (two spellings in the parser forever, one taught) and the
prose that has to say "declared again" where it used to say "re-mark".

## Outside the field

Two borrowings, both because the decision is about words, which software research rarely measures:
- Programming-language human factors (Stefik & Siebert 2013): the only controlled data on keyword spelling with
  semantics held fixed. It transfers to the human half of the question (the user reads and now types Dan) and
  says: a word that says what the line does helps a newcomer a lot; a returning author prefers the word they
  know. It does not transfer to Claude, which is not a novice reader of a 65k-character spec.
- Cartography/graphics vocabulary (Bertin's marks, Vega-Lite's `mark`, Protovis "all graphical elements are
  called marks"): the word `mark` is the established name for exactly Dan's primitive, a graphic element whose
  kind is a parameter. That is the prior principle 8 wants the model to bring.

## Implementation (what is in the worktree now)

Design 1 only. `gloss.py` applied; the pilot's parser alias and the whole migration were reverted
(`git checkout -- .`), and the engine rebuilt from the committed sources (`make -B tutor`; readback prints
`mark` again, a `thing` line is "unknown verb" again, as at HEAD).
- `README.md` line 65, the verb table (inside the prompt): gloss "put a thing on the stage; the same id again
  updates it in place". 1 line.
- `docs/guide.html` line 410, the same sentence in the human guide. 1 line.
- `tools/write.html` line 81, the playground's cheat sheet. 1 line.
No engine, parser, test or script changes. `patch.diff`: 3 files, 3 insertions, 3 deletions. The untracked
`tools/tex/node_modules` is the symlink the brief asked for.

## Tests

No behaviour changes, so no new engine test (the brief's "a test that fails on the old engine" applies to
behaviour changes; there is none). What was tested:
- The rename path, as if it were shipped: `tests/toolbox.mjs` on the B edition, 31 ok 0 FAIL (`pilot/toolbox-B.txt`);
  the four prompt examples `--check` clean; and the full suite on the migrated tree (above).
- The pilot's deterministic counts (`pilot-metrics.mjs`), both arms, with the exact system prompts on record.
- The gloss: the system prompt still builds and carries it (`dump-system.mjs` on the final tree, below), and the
  regressions below.

## Regressions (final tree: HEAD 6038197 + the gloss)

```
make test     (final-test.txt)  doctest 21/21, 14/14, 8/8, 31/31 cases passed; compute tests 20 ok; exit 0
make check    (final-check.txt) 124 ok, 0 FAIL, exit 0  (93 scripts/examples/idioms + 31 toolbox moves)
make golden   (final-golden.txt) mean layout score 99.6 (new placer) / 97.4 (old) over 52 scripts, 0 WORSE, exit 0
make append   (final-append.txt) 152 identical, 4 different: space-downhill (chunks 2, 3, 5), space-saddle
              (chunk 2) = the known pre-existing DIFFERS named in the brief (stale lint comment, model identical);
              the same four on the migrated tree; make exits 2 on them, as at HEAD
node tests/toolbox.mjs   runs inside make check: 31 ok
```
Every difference explained: none beyond the known four.

## Frames

Looked at `pilot/A/full/tcp/frame_end.png` and `pilot/B/full/tcp/frame_end.png`: both proper sequence diagrams
(client and server boxes, three labelled arrows); B ends with a green "ready" check on each party, A with one
check on the server. Nothing in either frame is about the verb; the scenes differ as two samples differ. Frames
for all 20 pilot scenes are under `pilot/*/full/*/frame_{6,14,end}.png`. The gloss changes no pixel, so no frame
of the final tree was needed beyond the suite's own renders (golden, append).

## Limits and risks

- The pilot is 10 scenes per arm with Sonnet at low effort: it detects only a gross effect (roughly a 0/10 vs
  4/10 failure rate), and the production model differs. It shows the rename is not harmful at that level, nothing
  finer. The decisive A/B the audit pre-registered (44 x 2 x 2, judge + parse rate) was not run, on purpose: the
  expected effect is nil and it would cost ~180 Opus generations to confirm a null.
- The research found no direct study of a pure keyword rename on generation; the conclusion is inferred from
  adjacent evidence (prior conflicts cost, novel spellings of common words cost ~nothing with a spec in context)
  and from this project's own 7,300 real lines with zero verb errors.
- The human-readability gain of `thing` is real for a newcomer (Stefik) and unmeasured here: the user is the only
  human author today, and said what they found opaque. The gloss addresses that complaint in the three places a
  human reads the verb; whether it is enough is theirs to say.
- The gloss is 10 words in a 65k-character prompt: it could in principle nudge Claude's writing, like any prompt
  edit; it says nothing the prompt did not already say elsewhere ("Re-marking an existing id updates it in place").
- Noticed, not fixed (not this decision): `docs/guide.html` says "the eight mark kinds"; there are 34.

**Migration plan, if the user wants `thing` for their own reading anyway** (measured above; about an hour):
1. In one commit, from the tools in this folder: `rename.py --apply` over `*.dan`, README.md, chat/prompt.md,
   chat/toolbox.md, chat/idioms.md, chat/examples/*.dan, docs/guide.html, docs/plan-3d.md, tests/*.cpp,
   tests/compute.mjs, tools/write.mjs, tools/write.html, chat/compute.mjs, tools/eval/variants/base.md
   (1,094 verb lines); `prose-B.py` (32 prose edits) and `migrate-code.py` (27 code edits: parser table with
   `thing` canonical and `mark`/`re-mark`/`remark` as silent legacy spellings, readback word, captions, lints,
   the each-expander head, the server's slider line, the playground keyword sets, the mining prompt, HANDOFF);
   then by hand: the inline `<code>mark …</code>` mentions in guide.html, the README dictionary row, the
   remaining "marked" English where it now misleads.
2. Never teach both spellings (principle 7): no `mark` left in README, prompt, toolbox, examples; the alias exists
   only so the user's sessions, library and old scripts play. Do not rewrite the user's files.
3. `make -B`, then `make test`, `make check`, `make golden`, `make append` (expect exactly the known four DIFFERS),
   `node tests/compute.mjs`; `tutor restart`; a `tutor write` smoke test (the highlighter's keyword set changed).
4. HANDOFF: the §1 verb list, axiom 1's wording ("the same id declared again updates in place"), a gotcha that
   old stages read back as `thing`; history.md entry with the pilot numbers.
5. Optional evidence, only if someone wants it: the audit's pre-registered A/B before adopting, with the bar
   "parse errors fall or the judge rises by more than 0.15"; the pilot here predicts neither.

## Verdict

**Don't rename; ship the gloss (3 lines).** Four reasons, in order of weight:
1. There is no error to fix: 0 verb errors and 0 substitutions in 5,351 eval lines and 1,955 real-chat lines;
   the pilot's `thing` arm was equally clean (0 parse errors, 92/92 lines right), so the rename would be a change
   with no measurable upside for the author who writes 99% of Dan.
2. The evidence that exists points the other way or nowhere: keyword spelling costs an LLM only when the word's
   prior conflicts with its use (not the case for `mark`, which carries the grammar-of-graphics meaning Dan
   needs) or when the spelling is tokenization-hostile (neither word); an LLM-first DSL (Anka) chose plain
   English verbs and one canonical form, which `mark` already is.
3. `thing` does not deliver the one-word vocabulary it promises: a noun has no "re-" form or participle, so the
   docs and lints would say "declared again" where they say "re-mark", i.e. the verb comes back under another
   name; and `thing` would be the only noun heading an action line.
4. The cost is real and partly permanent: 120 files / 1,062 lines, 60 hand edits, a legacy alias forever
   (the user's data cannot be rewritten), old stages reading back in a spelling their files never used.
The human complaint is legitimate and is answered where it arose: the verb table now says what `mark` does and
that marking the same id again updates it. If the user still prefers `thing` after reading that, it is a taste
call they own; the migration above is measured, scripted and green, and takes an hour.

Axioms touched by the shipped change: none. By the rename, had it shipped: 6 (one canonical word: kept, `thing`
in readback; the untaught legacy alias is the same kind of thing as `point`/`remark` today).
