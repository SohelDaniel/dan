# Audit 8: Dan as a language an AI writes (nine principles, expressive reach, the voice)

Auditor A8, 2026-10-24. Read-only on the repo; everything written is under this folder (audit-8/).
Scope: not an axiom but the syntax, its ergonomics for the model, and what it can and cannot say, measured against
the nine principles in principles.md and against the goal (a live voiced tutor for any field).

Corpus measured: the 80 eval scenes (tools/eval/out-pacing/*/full-opus-xhigh/*/scene.txt + check.txt), the 63
session turn files (copied to audit-8/sessions-copy/, checked with `tutor F --check` into audit-8/sessions-check/),
and the live stage logs (chat/sessions/*/stage.log, 25 sessions). Scripts: audit-8/tools/*.py (census, texlints,
textcolor2, sessions, aligned, forms). Probes: audit-8/probes/*.dan with outputs in probes/out/*.txt; frames in
probes/snaps/, probes/subs/all_subs.png (subtitle crops), fields/out/*_end.png and sheets/sheet1-6.png.
Token counts are chars/4 (no tokenizer available offline); treat them as +-25%.

Headline: Claude writes Dan almost without syntax error (3 dropped lines in 5,351; 0 TeX errors; 0 broken cues in
23,438 spoken words). The language's real costs are elsewhere: (1) a bare word is a kind or a category depending
on a list, and kind names are ordinary nouns (area, label, param, heat, field, solution), so a category word
that happens to be a kind name drops the whole line, and once dropped the frame of a scene (product_rule); (2) a
`\textcolor{name}` that names nothing silently paints nothing, in 5 of 80 scenes, and 21 of 80 fail `--check`
partly for it; (3) a derivation re-emits its whole formula per step: 55% of all TeX characters and 15% of all
scene characters are copies of the previous version; (4) four prose collisions in `Say` (`[word]`, `{...}`,
`|x|`, `||v||`) silently vanish from the voice; (5) three silent spoils: a typo'd schema name becomes an empty
view, a typo'd or capitalised key becomes a colour, a `with` chain behind a failed line runs at the wrong moment.

## 1. Verdicts (the nine principles)

Format per principle: how Dan measures up (evidence) / verdict / cheapest change and the axiom it touches.

### P1. Forward-only: PARTLY
Real forward dependencies (the author must know the future when writing the line):
- Schema ranges must contain all later motion (prompt.md:177). Claude obeys: 55 of 55 `schema axes|plane|space`
  lines in the 80 scenes carry ranges; still 4 of 80 checks report "runs Npx off the screen". Whole-file play needs
  no ranges (the prescan reads morph targets: probes/p1-no-ranges-morph.dan, `--check` clean with a dot morphed to
  x=40 on bare `schema axes`); only the stream does (enforcement-map axiom 2, item 3).
- `\textcolor{name}` resolves only if `name` is a thing, a part some LATER or earlier annotation/indicate/send names,
  or a colour (engine.cpp:274-288). Forward is fine (p1-textcolor-later-part: `brace of=f[ke]` two lines later,
  clean). A name never annotated silently paints nothing (render.cpp:105-117 falls through to `colorByName`, which
  fails, so the glyphs keep the base ink) and lints. Eval: 49 such uses in 5 of 80 scenes (integration_by_parts:
  `\textcolor{u}` x17, `\textcolor{v}` x14, no thing u or v: the intent "u and v each get a colour" has no cheap
  spelling), producing 174 lint lines (one per formula version), the most frequent lint in the corpus.
- `morph`/`focus`/`indicate` of a thing marked later: the change is lost with a lint (p1-morph-before-mark:
  "morph: no thing called 'x' (line 1)"; p1-focus-before-mark). Rare in the corpus (0 in eval).
Tolerated (the prescan or the arming absorbs them, so they are NOT forward dependencies in practice): a `@cue` line
before its Say or two sentences after (p1-cue-line-before-say, p1-cue-two-sentences-later: both fire, no lint;
confirms session finding 4); a `link` before one end exists (p1-link-before-mark: lint "one is not on stage yet"
but the link draws once B arrives, sheets/sheet5.png bottom right; 33 such lints live, 10 in the session checks:
false alarms); `at=a[0]` before the array (silent, fine); a hook line that arrives after its sentence (engine.cpp:
98-112 runs it late). No length prefixes, no counts before content, no closers anywhere.
Cheapest change: (a) an unknown `\textcolor{name}` declares a named part with its own palette slot
(`colorForKey("part:"+name)`), consistent across versions; the lint becomes a note. Touches axiom 3 (a slot is
claimed for a name the author related to a term: within "a name and its thing"). (b) The `link:` lint fires only
if an end never appears, as `indicate:` already does (engine.cpp:1310). Touches nothing.

### P2. Structure at line starts, never in whitespace: MET
Verbs, `@cue`, `with`, `Say:`, `#` all at line start; `trim` strips tabs/CR/indent; blank lines and ``` fences are
skipped (script.cpp:600-603). Verified: probes/p2-whitespace.dan (tabs, CRLF, 4-space indent, blank runs: all
clean). A Markdown bullet or numbering kills that line only ("unknown verb '-'", "'1.'"). Spaces inside `{expr}`
in a Say are protected (parseSay, script.cpp:192-196); spaces inside `[...]` lists are fine (tokenizer depth).
The one positional dependence is semantic, not whitespace: a line's beat is decided by the nearest Say above it
(server.mjs:382-390, engine.cpp:1190-1202). Cheapest change: none needed; optionally strip a leading `- `/`1. `.

### P3. No matching at a distance: MET for the parser; the derivation idiom reintroduces it
The unit of balance is one line: an unterminated quote or unbalanced bracket fails that line alone (p6-unbalanced:
line 1 dropped, line 2 drawn). Nested lists `by=[[8,2],[1,9]]` and a comma inside quotes are handled
(script.cpp:65-80). The only long-range balancing is inside TeX (`\begin{aligned}...\end{aligned}`, braces), the
model's trained skill: 0 TeX errors in 143 files (689 formula versions). The fence pair is tolerated by the server
(a missing closing fence still ends the scene, server.mjs:420-424).
BUT the idiom "a derivation is one formula that grows" (README:292, prompt.md:122) asks the model to re-emit the
whole previous formula verbatim per step: 351 aligned-to-aligned steps in the corpus; 293 (83%) are exact
appends; 58 (17%) alter an earlier row while adding one (some deliberate: moving `\textcolor` emphasis,
substituting a number; some indistinguishable from copy slips, e.g. projectile_time row 2 `\textcolor{tf}{t}` to
`t`). The engine glides whatever differs (render.cpp:152-179), so an unintended edit of a row the learner has
already read passes silently. Worst single re-emission: 509 characters (quadratic_formula).
Cheapest change: an append form for a step, e.g. `morph f.tex += "x^2 + 6x &= -2" 1.2s` (parse-time sugar: the
engine still morphs to the full string; readback prints the whole formula). Touches axiom 6 only in that the
readback is not the source form (already true for `hide`, `re-mark`, link kinds). Saves ~15% of all scene
characters (see P9).

### P4. The parser counts, the model doesn't: PARTLY
What the model must keep consistent today, and what happens when it slips (probes/p4-*):
| the model keeps | on a slip | corpus |
|---|---|---|
| unique ids across mark/link/group | lint "already used", the second dropped | 0 clashes in 143 files |
| `cols=` dividing `items` | silent, ragged last row | 0 of 8 |
| `pts=` pairs | silent, odd value dropped (sheet5 top-left) | 0 |
| `m=` 4 or 9 numbers | silent, identity drawn (sheet5 top-right) | 0 |
| indices `a[k]`, `at=a[7]`, `indicate a[9]` | silent for pointers/indicate; lint for `{a[7]}` | n/a |
| span lengths in `send a[0:4] -> b[0:1]` | silent | n/a |
| `morph ... from` | the engine corrects and lints (the right model) | 6 wrong of 134 with a from |
| `|cue|` and `@cue` pairing | lint; `glueCue` rescues a tag matching a spoken word | 7 unique hook lints / 1,568 cues (0.4%) |
| items length on re-mark | accepted (a real change) | fine |
So the parser already refuses to make the model count where it matters most (ids are auto for links, `from` is
corrected, arrays re-size, spans are inferred); the remaining counts are small and Claude gets them right at
this scale; the risk is that the slips are silent. Cheapest change: arity/index lints in `analyze` (pts odd,
m not 4/9, items % cols, index past the end at the time of use): ~30 lines, touches no axiom.

### P5. Near-zero escaping: PARTLY (no escapes exist, which is right; four prose collisions bite silently)
There is no escape character at all, and Claude never invents one: `\"` occurs 0 times in 143 files. Good.
Resolved in favour of literal text (probes/p3-*): `label="x = 2, y = 3"`, `label=x=2`, `label="item #3"`,
`label=item#4`, unicode in labels and bare categories (`etat`, `w11 -> 90 deg e -3 x 2` in their real glyphs),
`tex="50\% \#3 \{a\}"`, `items=["1,000", "2,000"]`, a Say with inner `"..."` (subtitle: She said "hello there" and
left.), apostrophes.
Bites:
- In `Say`: `[a]` is a pointer (removed from speech), `{1,2,3}` is spoken as "1" with NO lint, `f{x}` loses the
  braces (lint "no value on stage"), `|x|` (absolute value) is a cue and vanishes, `||v||` becomes "v", `[sic]`
  vanishes with a "never marked" lint. Proof: probes/subs/all_subs.png (subtitles = spoken words). Corpus: 4 "[id]
  never marked" in 343 pointers, 0 `{expr}` lints in 142, 0 broken cues: Claude avoids maths notation in prose,
  but a tutor of maths will want "|x|" and "{1,2,3}" eventually.
- No `"` inside `tex=` (README:287 says so) and, undocumented, none inside `code text=`: `print(\"hi\")` draws the
  backslashes (sheets/sheet4.png bottom right); `\"` inside tex is a TeX error ("Missing argument for \"",
  p3-backslash-quote-in-tex). Python/JS/C strings in a `code` block must use single quotes.
- A bare `#5` after a category is a comment and vanishes silently (p3-hash-in-text); `//` only comments at line
  start: a mid-line `// note` fails the whole mark line ("unexpected 'a'", g-midline-comment; 2 such lines in the
  session turns).
Cheapest change: in `analyze`, demote a `[word]` whose base is never marked, a `|word|` with no `@word` anywhere
(and not glue-able), and a `{...}` that names nothing on stage to literal text (keep the lints). For `"` in code:
let the tokenizer honour single quotes as a value quote (`text='print("hi")'`); today a single-quoted value works
only if it has no space. Touches no axiom.

### P6. Fail soft: PARTLY (one line is the blast radius, with three leaks)
Classification (reproducers probes/p6-*, outputs probes/out/):
- Fail-soft, visible to Claude next turn (server check() keeps `parse:` lines, server.mjs:237), never to the
  learner: unknown verb, unbalanced brackets, an unknown morph option (`ease linear` without `=` drops the whole
  motion), three bare words, kind-given-twice, duplicate id, broken TeX (formula missing, lint), `group` with no
  members. Each drops one line; the rest plays. Measured rate: 3 real dropped lines in 5,351 (0.06%), all the same
  pattern (`rect area`, `box param` x2), plus 20 live lines from the lint-leak bug (last seen 2026-10-21).
- Fail-soft, SILENT: `schema chart x=[0,5]` (a typo) becomes a view named "chart" of schema free (script.cpp:
  471-476) and the stage ends up empty (sheet5 bottom-left, "dot 'p' runs 31px off the screen"); `Label=`,
  `lable=`, `labl=` become a colour category and no name is shown (engine.cpp:470-475; README:269 teaches the
  rule); `mark a box red` is a category called red, drawn blue (sheet4 bottom-left); `from=/to=` on a timeline
  thing are ignored (history-timeline); arity slips (P4); `wait two seconds` = 1 s; `mark ... 5s` = a category
  (finding 5); `label=none` prints "none".
- Fail-hard (one bad line spoils more than itself): (1) a `with` chain behind a failed main line joins the PREVIOUS
  step: p6-with-after-failed2 shows `with mark B` running at 0.02 s, at the start of the sentence instead of on
  |go| (script.cpp:611-617 pushes onto `steps.back()` regardless); (2) when the dropped line is the frame of the
  scene (product_rule's base rectangle: judge layout 2, "the strips float detached"); (3) the typo'd schema (empty
  stage). The parser never rejects a document.
Cheapest changes (all parser, ~25 lines total): a second kind word becomes the category with a lint; `with` lines
behind a failed line are dropped with a parse error, never re-attached; a view declared with props but no schema
kind, whose name is not an existing view, is a parse error naming the schemas. Touch no axiom.

### P7. One way per thing, one meaning per marker: PARTLY (synonyms harmless, context-dependent tokens harmful)
Synonyms the parser accepts (script.cpp): `re-mark`/`remark`/`mark`; `point`/`indicate`; `hide`/`show` (= a
morph of opacity); `->`/`→`/`--`/`<->`/`—`; link kinds `cause`,`maps-to` to arrow, `connected` to line;
`number-line`; group members as `part-of=`/`members=`/`of=`/`parts=`/a bare list/bare words (6 forms,
p7-group-forms); `focus none|all|nothing`; `camera reset|home`; `dur=`/`duration=`/`1.5s`/`1500ms`; `true|yes`,
`false|no`; `Say:`/`Say `; `mark x arrow a -> b` promoted to a link; `scene` quoted or bare. Readback normalises
all of them, Claude copies the README's spelling (0 variants observed), so they cost nothing: keep, don't document.
Context-dependent tokens, and whether they bite:
| token | meanings | harm |
|---|---|---|
| a bare word on `mark` | a kind if in the 34-name list, else a category | HARMFUL: kind names are nouns (area, label, param, heat, field, solution, plane, region, surface, vector, path, grid, table, check, cross, card, code, line, box, bar, dot, angle, contour, slope, sphere, circle, rect, polygon, brace, enclose, underline, arrow, tex, array): 3 dropped lines in 143 files, all this; will rise outside maths |
| `at=` | pointer target (label), anchor (vector), a TIME if numeric (engine.cpp:165; p7-at-number gives schema timeline) | moderate |
| `on=` | seat on cells, touch a thing, a curve (misbehaves: finding 7) | moderate |
| `of=` | annotation target, region's curve, solution's field, group members | low |
| `to=` | region end, vector tip, morph direction (`->`) | low |
| `x` | data coordinate; the variable in `fn=`; a tex's position on axes (README:284 `x=4 y=30 size=30`) | low; the README example looks pixel-era |
| `line` | mark kind, link kind, `line=` (code highlight), `style=lines` | low |
| `label` | a kind and a key | low (but `mark L line label` drops, p6-kind-twice) |
| `plane`, `grid`, `axes` | schema/kind/option | low |
| `range=` | number-line range, slider range, parametric t range | low |
| `|` | cue delimiters vs maths | HARMFUL in prose (P5) |
| `[]` `{}` | pointer/list/cell/span vs TeX vs prose | HARMFUL in prose (P5) |
| bare colour word | a category, not the colour | moderate (sheet4) |
| `schema <word>` | a schema, a view name, or a typo | HARMFUL (P6) |
| `->` | link, morph, send | none |
Cheapest change: none for synonyms. For the harmful ones: the P5/P6 fixes, plus "a bare colour word is also the
colour" (claim the slot with that colour; touches axiom 3 benignly: colour still means category).

### P8. Stay near the training distribution: MET (adherence), with one semantic exception
Borrowed shapes: `key=value`, `->`, LaTeX (MathJax, including `\textcolor`), Markdown fences, Python blocks,
screenplay `Say: "..."`, Python slices `a[i:j]`, `M[r][c]`, `#` comments, `\n`. Invented: `|cue|`, `@cue`, `with`,
`[id]word`, `{expr}` in prose, `\val{}`, `\textcolor{id}` meaning "the colour of thing id", `f[part]`, relation
keys `of= on= at=`, `send`, `schema`. Error signatures per use over 143 files (tools/forms.py; unique lints from
the checks): cues 7/1,568 (0.4%); `[id]` 4/343 (1.2%); `{expr}` 0/142; `\val{}` 0/109; `with` 0/1,222; `send`
0/153; parse 3/5,351 lines; `\textcolor{id}` 49 unresolved uses of 2,371 (2.1%) but each is a silent loss of the
intended colour and a failed `--check`. Adherence to the novel rules is high and flat over the scene: lint
position by quintile [65, 24, 46, 66, 70] (n=271), binding lints mean position 0.59, tex 0.51: no late-scene
collapse. Longest real turn 86 lines / 5,052 chars (~1,260 tokens); longest eval scene 79 lines / 7,353 chars
(~1,840 tokens): below the 2K-8K region where LongProc sees decay. The two live mistakes are both the
kind-as-category clash, i.e. the model treating the language as more English than it is.
Cheapest change: none for fluency. The one invented form with a real failure mode is `\textcolor{id}` semantics
(fix under P1). Keep scenes under ~2K tokens (already true).

### P9. Cheap tokens, narrowed: PARTLY
Budget (410,743 chars, 143 files; tools/forms.py): Say lines 33.6%, formula lines 31.0%, mark/link/group lines
25.3%, pointing/visibility/camera/wait 3.5%, setup 2.7%, non-tex morphs 2.2%. Within those: `\textcolor{..}{}`
overhead 8.7% of everything (2,371 uses x ~15 chars), `@cue` prefixes 2.1%, `|cue|` text 2.0%, re-emitted formula
prefixes 15% of everything (55% of TeX chars). A typical eval scene is ~900 tokens, a live turn ~480; max ~1,840.
ASCII: 99.8% (407 + 270 non-ASCII chars, mostly superscript 2, minus, middle dot, times in labels, which the
README itself teaches; negligible cost). `with morph u.dx` + `with morph u.dy` pairs: 30 in the corpus (a
`to=[x,y]` exists for vectors; a position pair morph for dots would save ~20 chars each: not worth a form).
Cheapest change: the derivation append form (P3): -15% characters overall, and it removes the copy. Do NOT
shorten `\textcolor` (P8: it is the trained macro). `|cue|` reuse is already cheap (one word).

## 2. Rule vs code gaps

1. README:109 "A marker belongs to its sentence: it fires the `@go` lines between that Say and the next one" and
   prompt.md:171 "Write in beats" vs script.cpp:624-628 + engine.cpp:1190-1202: a `@go` line before its Say or two
   sentences after also fires on |go| (probes/p1-cue-line-before-say.dan, p1-cue-two-sentences-later.dan: no
   lint, same end state). The rule is a convention for streaming/readability, not enforced (finding 4 confirmed).
2. README:60 "`#` starts a comment" and script.hpp:13 "Comments start with # or //" vs script.cpp:603 (`//` only
   at line start) and tokenize script.cpp:147 (`#` only when a token starts with it): `mark a box label="A" // a
   note` fails the whole line (probes/g-midline-comment.dan: "unexpected 'a'"); `mark c box tag #5` drops `#5`.
3. README:269 "The bare word after the kind ... or the first text property (`status=overdue`) is drawn as color" vs
   engine.cpp:470-475: ANY string-valued key not in `kTextKeys` is the category, so `Label=`, `lable=`, `labl=`
   become colours and the name disappears (probes/p6-caps-key.dan, p7-typo-key.dan). The README documents the trap
   as a feature; nothing lints a near-miss of a known key.
4. README:111 "Re-marking an existing id updates it in place ... It never errors" vs script.cpp:292-298: a re-mark
   (or mark) whose category word is a kind name errors and is dropped (`mark A rect area ...`, product_rule and
   probes/p6-kind-twice.dan: 3 of 3 lines dropped: `rect area`, `line label`, `circle dot`). Also a re-mark may
   change the kind silently (`mark a box` then `mark a dot x=1 y=2`: probes/g-remark-kind.dan reads back as a dot).
5. prompt.md:173 "A cue name is one word of letters, never with spaces" vs script.cpp:183-186, 218-222: any
   non-space characters, case-folded; `|2x|`, `|go2|` work (probes/g-cue-digits.dan). A cue with a space stays as
   literal pipes in the subtitle (probes/snaps/sheet6.png bottom-left) and `glueCue` still fires `@go` on "|go".
   Harmless but the prompt is stricter than the code.
6. prompt.md:175 "Every `|cue|` needs an `@cue` line, and every `@cue` line needs its `|cue|`" vs engine.cpp:
   232-235 (lint only) and script.cpp:576-586 (`glueCue` glues a tag to a word that starts with it, 3+ letters):
   the code is more lenient than the rule; the lint text ("have no |x| spoken, so they only run when their sentence
   ends") is right.
7. README:62 "The six verbs" vs script.cpp:563-567: 16 verb words (mark, re-mark, remark, link, group, morph,
   focus, indicate, point, send, hide, show, wait, pause, schema, camera, scene, title, Say). Fine as teaching;
   `point` and `remark` are undocumented synonyms.
8. README:262 documents `\n` in `code` text but not that `"` cannot appear in it; README:287 says it for `tex=`.
   `print(\"hi\")` draws the backslashes (probes/snaps/p3-code-with-quotes_end.png).
9. Enforcement map lint table, engine.cpp:967 `link: <id> connects 'a' and 'b' but one is not on stage yet`: the
   link is created and draws when both ends exist (probes/p1-link-before-mark.dan, sheet5 bottom-right), so the
   lint reports a tolerated forward reference as a problem (33 live occurrences, 10 in the session checks).
10. main.cpp:191 counts `tex:` lints as axiom warnings and fails `--check` (exit 1), while README:360-367 lists no
    axiom for `\textcolor` names; 21 of 80 eval scenes have meta ok=false, 5 of them for unresolvable colour keys
    that were never a thing (tools/textcolor2.py). The check fails for a colour the stage silently never showed.
11. README:323 setup line for `schema` omits `plane` and `space` (documented in their own sections) and `auto`
    (script.cpp:461 accepts it). README:284 `mark tag tex tex="E = v^2" x=4 y=30 size=30`: `y=30` on axes is a
    data point; the example reads like a pixel position and is inside the part of the README that is prompt.
12. README:146 `to=[x,y] | to=<id>` and `link a <-> b`: `<->` parses as undirected (script.cpp:312) and draws a
    plain line with no heads (probes/g-reversible.dan, sheet6 bottom-right); a two-headed arrow is not expressible.
13. Undocumented but useful: `label=""` suppresses the auto id-name on a structural line (probes/g-nolabel.dan,
    sheet6 top-left: `label=none` prints "none"; `label=""` and `name=""` print nothing). Every unnamed world shape
    otherwise shows its id capitalised (L1...L5 staff lines, W1...W4 wires, Rod, D1...D3 in the field scenes), which
    is noise the author cannot see coming from the README.
14. The lint-leak bug (reports/Small model...: "the previous turn's lint text was parsed as scene markup") is in the
    live record: 20 `parse: unknown verb 'lint:'/'parse:'` lines in sessions 20261020-223156 and 20261021-003930.
    Not seen since; status unknown from the code I read (server.mjs:327 feeds `<last_scene_issues>` to Claude; the
    leak was Claude echoing it inside the scene fence, which the parser then dropped line by line: fail-soft held).

## 3. Where the language limits creative freedom (the field matrix)

Scenes in audit-8/fields/*.dan, checks in fields/out/*.txt, frames fields/out/*_end.png, contact sheets
sheets/sheet1-4.png (+ sheet6 for the retries). All 14 decode; layout 100 except anatomy 97, geo 97, music 79.

| field | tried | result | verdict |
|---|---|---|---|
| chemistry | H2 + Cl2 -> 2 HCl: atoms as `circle`s driven by a param, bonds as `line`s with `{h1.x}`-style endpoints, break = `hide`, form = new lines (chem.dan); a bond as `link b1 line h1 -- h2` on a plane (chem-link.dan) | both work; the link even carries a label; the reaction reads (sheet1 top) | possible with a workaround: no bond kind (a line must copy both atoms' coordinates as expressions), no two-headed reversible arrow (link `<->` draws headless: sheet6), electrons/orbitals/charges have no form but labels |
| music | a staff (5 `line`s), notes as `rect`s whose width is duration, two voices as two categories, a param playhead (music.dan) | works (sheet1 bottom-left); layout 79 because note names cross staff lines (a staff IS that); staff lines show L1...L5 until `label=""` | possible with a workaround: no note heads/stems/clef; rhythm only as width; the checker penalises the staff |
| grammar | a parse tree (graph, boxes, parent-to-child links) plus an agreement link `equals n -- v` and a shared category `plural` (grammar.dan) | works; the agreement link turns the hierarchy into a left-to-right flow (the tree is no longer single-parent), still readable (sheet1 bottom-right) | possible; note: any cross link flattens a tree |
| anatomy | a closed `path xt= yt=` loop, three blood dots riding it via a param (each repeats the loop's formula), a valve `line` rotating (anatomy.dan); plus the repo's heart.dan | works (sheet2 top-left); dots carry ids D1...D3 unless labelled "" | possible; finding 7's `on=<path>` would remove the repeated formula |
| geography | a watershed `polygon`, a river and tributaries as parametric `path`s, a drop riding the river, the sea as a rect (geo.dan) | works schematically (sheet2 top-right) | possible as a schematic; impossible as a real map (no data import; a python block could emit polygon points, which is the intended route) |
| history | eras with `from= to=` on `schema timeline` (history-timeline.dan); eras as rects on a `plane` (history-plane.dan); eras as rects on `axes` (g-history-axes.dan) | timeline: `from=/to=` silently ignored, the eras sit as free boxes and the axis zooms to 475.5-476.4 around the one event (sheet2 bottom-left); plane: equal units squash 900 years x 4 lanes into hairlines (sheet2 bottom-right); axes: works (sheet6 top-right) | spans on a timeline: IMPOSSIBLE today (the schema has only points); workaround: `schema axes` + `rect`; `plane` is unusable for it |
| economics | supply/demand shift with a param and the equilibrium as an expression (econ.dan); money flow as two boxes and two links with `send ... via=` (econ-flow.dan) | both work (sheet3 top) | possible; the author still solves for the intersection by hand (the small-model report's "geometric references" point) |
| programming | a `code` block with a moving highlight, the call stack as an `array` that grows and shrinks by re-mark, a frame as a 2-column grid with a `cross` on a cell (prog.dan) | works (sheet3 bottom-left) | possible; gap: no `"` inside code text (string literals), so Python/JS samples must use single quotes |
| probability | a tree with `p=` on links beside a Venn diagram of two circles, as two views (prob.dan) | works: the tree is top-down, links read "p 0.3", the circles overlap with no zero lint (sheet3 bottom-right) | possible; the intersection lens is IMPOSSIBLE to fill or point at (no boolean region) |
| physics | a pendulum: rod + bob from an expression param (phys-pendulum.dan); a circuit: wires as lines, three charges moving round a rectangle via `clamp`-built piecewise expressions (phys-circuit.dan) | both work (sheet4 top); the circuit needs 4 clamps per coordinate and cannot loop (no `mod`, no path-following) and lints "9 things on stage" | pendulum possible; circuit possible with a heavy workaround: a dot riding a path (`on=<path> s={c}`) is the missing primitive, the same as finding 7 |

Cross-cutting limits found: (1) a point on a path/curve (finding 7) would simplify anatomy, geography, physics;
(2) no spans on `timeline`; (3) no two-headed arrow; (4) no `"` in `code`; (5) structural lines need `label=""`
to stay quiet, undocumented; (6) the checker scores a staff or a circuit loop as overlaps; (7) a category word
that is a kind name (heat, field, solution, area, region, plane, vector, surface) drops the line: the collision
is most likely exactly in these fields.

## 4. Small model

Measured against reports/Small model for tutor weak spots.md (don't train a scene writer; small models fit
narrow, label-rich, checkable jobs). At the language level:
- A syntax repair pass: NO. The measured parse-error rate is 3 lines in 5,351 (0.06%), all one pattern, plus a
  server bug. Ten lines of parser leniency (fixes 1-3 below) cover every observed case; a model has nothing to
  learn from 3 examples and would add latency to every beat.
- A Dan-fluency fine-tune as the principle-8 remedy: NO. There is no fluency gap to close: cues pair 99.6%,
  pointers resolve 98.8%, TeX 100%, `with` 100%, no decay within a scene. What principle 8 costs here is tokens
  (~900 per scene) and seconds, and a fine-tuned small writer is the scene writer the report rejects (and Claude
  cannot be fine-tuned). The thing to watch is scene length: if turns grow past ~2K tokens, re-measure the
  late-scene lint rate before concluding anything.
- An escaping fixer: NO. Zero `\"`, zero broken cues, zero arity slips in the corpus. The prose collisions
  (`|x|`, `{1,2,3}`, `[sic]`) are decidable from the id set at parse time (fix 5) without a model.
- Reading a formula aloud for `{f.tex}` ("say it in words"): a narrow, checkable job with free labels, but a
  deterministic engine already exists in the MathJax ecosystem (Speech Rule Engine, the same toolchain tools/tex
  uses). Prefer code; a small model only if SRE's output is judged unnatural, with SRE output as the label source.
- Fits elsewhere, not the language: the report's binding-repair tagger (which mark a spoken number belongs to) and
  a personal taste critic. Nothing in this audit changes that ranking.
Labels that exist for any of the above: the `parse:`/`lint:` lines of every check (tools/eval/*/check.txt,
stage.log), i.e. a few hundred lines, dominated by one lint (`\textcolor`), which fix 2 removes.

## 5. Proposed fixes, ranked by value over cost

Each: the test that fails on today's engine and passes after; axioms touched; size.
1. A second kind word on `mark` becomes the category (lint "'area' is also a kind name; taken as the category").
   Test: probes/p6-kind-twice.dan `--check` has no `parse:` lines and reads back `mark A rect area ...`,
   `mark L line label ...`, `mark c circle dot ...`; today 3 parse errors. Axioms: none (readback re-parses the same
   way). ~6 lines in script.cpp:292-298, 15 minutes.
2. An unknown `\textcolor{name}` declares a named part with one palette slot, consistent across versions; the lint
   becomes informational. Test: a formula `\textcolor{u}{u}` with no thing `u`: a glyph colour test asserting the
   `u` glyphs are not the base ink, and `--check` exit 0; today plain ink and exit 1 (probes/p1-textcolor-never-
   named.dan). Axiom 3: a slot is claimed for a related name (allowed by "a name and its thing"); zero's colour
   budget shrinks by one slot per such name (the lint when slots run out already exists). ~30 lines in engine.cpp
   (colorForKey) and render.cpp:105-117, 1-2 hours including `focus f[u]`.
3. `with` lines behind a failed main line are dropped with their own parse error, never attached to the previous
   step. Test: probes/p6-with-after-failed2.dan with `--layout-report - --verbose`: no "line 4: with mark B" at
   0.02 s and a `parse: line 4` message; today B runs at 0.02 s. Axioms: none. ~5 lines in script.cpp:595-623.
4. A view declared with properties but no schema kind, whose name is not an existing view, is a parse error that
   names the schemas. Test: probes/p6-bad-schema.dan `--check` prints `parse: line 1: unknown schema 'chart' ...`;
   today decodable with an empty stage. Axioms: none. ~6 lines in script.cpp:471-476.
5. Prose collisions in Say: `[w]` never marked, `|w|` with no `@w` and no glue target, `{...}` naming nothing on
   stage: keep the text literal in speech and subtitle (keep the lints). Test: `Say: "the absolute value |x| grows"`
   speaks "the absolute value x grows" (spokenText test in tests/test_engine.cpp); today "the absolute value
   grows" (probes/subs/all_subs.png). Axioms: none. ~40 lines in engine.cpp analyze + parseSay, half a day.
6. A derivation step append form `morph f.tex += "..." 1.2s` (parse-time: new = old with `\\ ...` inserted before
   `\end{aligned}`, or appended if no aligned). Test: the toolbox derivation written with `+=` reads back identical
   to the full form and the glyph morph is the same (append.sh-style equality). Axiom 6: readback prints the full
   formula (as it does for `hide`). ~40 lines in script.cpp readMorph + engine doMorph (needs the current value:
   resolve at exec time, like `from`), half a day. Then update the toolbox entry; expect -15% scene characters.
7. Lints for silent slips: a duration token on mark/link/group (finding 5), a bare number or colour word as a
   category (and make a colour word also the colour), a key within edit distance 1 of a known key or capitalised
   ("unknown key 'lable=' became a colour category; did you mean label=?"), pts odd, m not 4/9, items % cols,
   index past the end. Tests: probes/f5-duration-on-mark.dan, p7-colour-bare.dan (A drawn red), p7-typo-key.dan,
   p4-*.dan each gain a lint; today silent. Axioms: none (3 benignly for the colour word). ~60 lines in
   engine.cpp analyze, half a day.
8. `link:` "not on stage yet" fires only if an end never appears (like `indicate:`). Test: probes/p1-link-before-
   mark.dan `--check` has no link lint; today one. Axioms: none. ~10 lines.
9. `mark p path a -> b` (and any unknown head word in a link) becomes a link of the default kind with a lint,
   instead of "unexpected 'path' in link". Test: probes/p7-path-between-boxes.dan reads back `link p arrow a -> b
   label=route`; today dropped. Principle 7 says don't document it as a synonym; principle 6 says don't reject it.
   ~4 lines in script.cpp:314-318.
10. Single quotes as a value quote in the tokenizer, so `code text='x = "a b"'` works (today it splits at the
    space). Test: a code mark with a double-quoted string literal draws the quotes (snapshot or readback equality).
    Axioms: none (readback keeps double quotes outside, and must switch to single quotes when the text holds a
    `"`: one more line in Value::markup). ~15 lines.
11. Document `label=""` (README blocks/world shapes) and warn in the prompt's pitfalls that kind names cannot be
    category words until fix 1 lands. 10 minutes.
12. Bigger, under creative freedom: spans on `timeline` (`from= to=` as an era bar), a dot `on=<path>` with a
    parameter (finding 7), a two-headed link (`<->` with heads). Days each; axioms: position from structure (the
    era's position is its data), identity/continuity unchanged.

## 6. Checked and fine

- All 80 eval scenes and all 63 session turns decode (`decodable: yes`); the readback round-trip held on every probe
  including unicode labels, a category with spaces (quoted on readback), lists with quoted commas, `label=x=2`.
- Whitespace tolerance: tabs, CRLF, indentation, blank lines, fences inside the scene (p2-whitespace).
- One line is the unit of failure for every parse error (unknown verb, unbalanced, bad option); the rest plays.
- Forward references the engine absorbs: `@cue` before its Say; a hook line arriving after its sentence; a link
  before one end (draws later); `at=` before its target; `\textcolor` of a part annotated later; morph targets
  outside the given range in whole-file play (prescan).
- `morph ... from` wrong: corrected and linted (6 of 134 in the corpus); `morph` without from: fine.
- Cue names: case-insensitive, digits allowed, reusable per sentence; `glueCue` rescues a tag that names a spoken
  word; 0 broken cues in 23,438 spoken words.
- `{expr}` in Say: spaces inside braces don't split words; cell refs `{a[4]}` speak the cell; numbers via
  rm::fmtNum (integers plain, 3 significant digits below 0.1, else 2 decimals trimmed).
- Quotes: a Say with inner double quotes keeps them; apostrophes fine; `#` and `%` inside quoted tex fine; `#`
  inside a label fine.
- Synonyms normalise on readback (group forms, link kinds, arrows, focus words, re-mark).
- `mark w arrow a -> b` is a link (documented behaviour); `link` without an id gets `a->b`.
- Scene length stays well under the decay region; lint density is flat across the scene.
- Session finding 8's numbers reproduce: 126 cued `indicate` (+105 `with indicate`) in the 80 scenes, 1 hook
  lint; corpus-wide 7 unique hook lints per 1,568 cues. The "no inline actions" decision stands on the numbers.
- Session finding 5 reproduces exactly (`5s` becomes a category) and generalises: every bare non-kind token on a
  mark is a category: a duration, a number (`mark a bar 3`), a colour word, a placement word, a quoted phrase, a
  `true`; and every unknown string key is one too (typos, capitals). Seven ways to misfire, all silent.
- Session finding 3 (`mark` to `thing`): nothing measurable decides it. The verb causes 0 errors in 5,351 lines;
  Claude never substitutes another verb (the 20 "unknown verb" lines live were the lint leak). Principle 8 leans
  to `mark` (an English verb with precedent: HTML `<mark>`, "mark a point"; `thing` as a verb has none), principle
  7 is indifferent (one word either way). The A/B that would decide: the 44 broad prompts x {mark, thing} with
  README/toolbox/examples rewritten, two runs each; metrics: parse errors per 1k lines, `--check` pass rate, judge
  mean (noise +-0.15), tokens per scene, and the rate of a substituted verb; pre-register adopting `thing` only if
  parse errors fall or the judge rises by more than 0.15. Expected: no difference; keep `mark`, never accept both.
- The user's "path between two boxes": Claude wrote it 0 times in 143 files; `link` is the form and `send x -> y
  via=` is the moving version; accept `path` leniently (fix 9) but don't teach it.

## 7. The voice as a channel (asked for beyond the principles)

What the author controls: the words; `|cue|` (sync, stripped from speech and subtitle); `[id]word` (pointing,
stripped); `{expr}` (a stage value spoken, formatted by rm::fmtNum: 2 decimals at most, no units, no "about");
sentence boundaries (one Say = one TTS request with a few lines of context, tts.cpp:106-205); `wait Ns` (silence
between sentences, by design); `pause`; punctuation (Inworld's prosody; a trailing `?` also puts the stage into
'listening', server.mjs:177). Global and the learner's, not the author's: voice, speed (`speakingRate`), text
normalisation (`applyTextNormalization ON`).
What a tutor needs that is missing: (1) a beat inside a sentence ("...and that, is the derivative"): only
punctuation today, and whether Inworld honours "..." is unverified; (2) emphasis on the one word the sentence turns
on; (3) slower delivery for the crux (per-Say, not global); (4) predictable pronunciation of symbols (w11 with
subscripts, sigma, f'(x), x squared go through normalisation; unverified how they are read; the author cannot give
a reading); (5) a way to say a formula: `{f.tex}` is refused ("say it in words", engine.cpp:1741), which is the
right default for a voice, but no help is offered (an optional `spoken="..."` on a tex mark, or a verbaliser,
would let a digest say the line it points at); (6) `{expr}` with units or rounding ("about 4.1 seconds" needs the
author to write the words around a 2-decimal number). Is "nothing but words, cues and values" right? For sync and
binding, yes: the voice never says a number the stage lacks, and the model cannot drift into stage directions.
For delivery, it is thin: 3Blue1Brown's narration is paced by silence and stress as much as by words. The cheapest
additions that keep the grammar (one line, one statement): a `|pause|` built-in cue (a 400 ms hold; no `@pause`
line needed) and a `Say slow: "..."` option; both need Inworld's SSML or chunking, so measure first.

## Appendix: interim evidence log (kept verbatim; the sections above are distilled from it)

### Census of the 80 eval scenes (tools/census.py, texlints.py; eval-stats.json)
- 80/80 decodable. Parse errors: 1 line of 3,421 (0.03%): `@area mark A rect area ...` -> "kind given twice ('rect' and 'area')" (product_rule; the base rectangle never drew; judge: layout 2, "never rendered because of the parse error").
- Hook lints: 1 in 80 scenes (the same line: its |area| cue lost its line).
- Lint prefixes: tex 174, binding 94, zero 14, layout 6, identity 3, hook 1. ALL 174 tex lints are `\textcolor{name} is neither a thing ... nor a color name`; 0 real TeX errors in 80 scenes.
- Size: mean 42.8 lines / 3,614 chars (~900 tokens at chars/4), max 79 lines / 7,353 chars (~1,840 tokens). Says mean 10.8; cues 14.3/scene (1,148 total); [id] pointers 267; {expr} in Say 114.
- `\begin{aligned}` lines: 417 lines, 97,948 chars = 34% of all scene characters. `\textcolor{`: 2,128 uses (~26/scene; max 100 in one scene).
- Non-ASCII: 407 chars total: small.
- `\"` inside strings: 0 (Claude never JSON-escapes here).
- Consecutive `morph u.dx` / `with morph u.dy` pairs: 18.
- Lint position in scene (quintiles): [65, 24, 46, 66, 70] of 271; binding lints mean position 0.59, tex 0.51. Mild late-scene tilt, no collapse.
- 21 of 80 meta.json have ok=false (any axiom lint fails the check; 5 scenes for free `\textcolor` keys).
- Off-screen: 4 "runs Npx off the screen" in 4 of 80 checks; 0 "outside the box"; 55 of 55 schema lines carry ranges; `max=` used 0 times.

### Live sessions (chat/sessions/*/stage.log, 25 sessions, 63 turn files)
- parse errors live: 20x "unknown verb 'lint:'/'parse:'" = the lint-leak bug (sessions 20261020-223156, 20261021-003930), and 2x "kind given twice ('box' and 'param')" (`mark w box param label="w = 3"`): real losses in a live lesson.
- hook lints live: 1. Other live lints: link 33 (link before its ends), zero 9, tex 6, say 3.
- Session turns checked alone (sessions-check/): 63/63 decodable; parse 2 (the same two lines); hook 6; lint prefixes (excluding never-marked artefacts of turn-only files): binding 75, layout 40, hook 6, zero 3, list 2, stagger 1; link 10; tex 0.
- Session turns: mean 30.6 lines / 1,930 chars (~480 tokens), max 86 lines / 5,052 chars (~1,260 tokens); says 8.5, cues 6.7, [id] 1.2, {expr} 0.4 per turn; aligned share 4%; textcolor 243; dx/dy pairs 12; `\"` 0; mid-line `//` 2 lines.

### Probe results (probes/*.dan, outputs in probes/out/*.txt; commands: `tutor F --check`, `tutor F --readback`)
P1: link before its end: lint, link kept and drawn later. Pointer before array: fine. `\textcolor{ke}` with a LATER `brace of=f[ke]`: fine; with none: lint + exit 1. morph/focus before mark: lost with a lint. `@go` before its Say or two sentences after: fires silently. No ranges + morph to x=40 whole-file: fine.
P3/P5: inner quotes kept; `[a]` eaten (pointer); `{1,2,3}` spoken "1" with no lint; `f{x}` loses braces (lint); `|x|` eaten (cue), `||v||` -> "v", `[sic]` eaten (lint). `\"` in tex: TeX error; in code: backslashes drawn. `#` inside quotes fine; bare `#5` dropped as a comment. Unicode fine. `label=x=2`, lists with quoted commas fine. A cue with a space stays literal.
P4: cols/pts/m/send-span mismatches silent; index out of range silent for pointers, lint for `{a[7]}`; items length change accepted; duplicate id across kinds: lint + dropped.
P6: unknown verb/unbalanced/bad morph option/three bare words/kind-twice: line dropped with `parse:`; `schema chart x=[0,5]` -> a view, empty stage; `Label=`/`lable=`/`labl=` -> colour category, no name; `wait two seconds` -> 1 s; unquoted Say fine; `with` after a failed main line joins the previous step (runs at 0.02 s) or errors "'with' has nothing before it" when there is none; mid-line `//` drops the line; `- mark`/`1. mark` dropped.
P7: `at=3` -> timeline; `mark a box red` -> category red drawn blue; `mark a bar 3` -> category "3"; `mark p path a -> b` -> dropped; six group forms, five arrow forms and three link-kind aliases all normalise; `<->` -> headless line; `focus all|nothing|none`; `indicate a b` ignores b; `indicate a, b` -> id "a,".
Finding 5: `mark sec line ... 5s` reads back as `mark sec line 5s ...`.
Formula re-emission (tools/aligned.py, 143 files): 689 versions, 535 re-emissions; TeX 27% of chars; common-prefix re-emission 61,052 chars = 15% of all / 55% of TeX; 351 aligned-to-aligned steps: 293 pure appends, 58 change an earlier row.
Forms (tools/forms.py): uses: textcolor 2,371; @cue lines 1,602; |cue| 1,568; with 1,222; morph 711; tex versions 689; part/span refs 527; [id] 343; send 153; {expr} 142; morph-with-from 134; \val 109. Errors (lint lines in checks, with duplicates): textcolor unknown 294; hook 14; identity wrong-from 6; [id] never marked 4; parse 3; stagger 2; {expr} 0; value 0; with 0; TeX 0. Char budget: Say 33.6%, formula lines 31.0%, mark/link/group 25.3%, textcolor overhead 8.7%, pointing/camera/wait 3.5%, setup 2.7%, morphs 2.2%, @cue prefixes 2.1%, |cue| text 2.0%.
Static corpus checks: 0 Say words with an odd number of `|` (23,438 words); 0 of 8 `cols=` arrays ragged; 0 odd `pts=`.
Field scenes: 14/14 decodable; layout 100 except anatomy 97, geo 97, music 79; circuit lints "9 things on stage at once".
