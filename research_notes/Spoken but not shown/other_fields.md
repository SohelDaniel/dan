# Binding ("spoken but not shown"): mechanisms from other fields

Problem restated: in ~30 of 44 scenes a number / step / comparison is SAID but nothing on stage carries it.
Today `markup/binding.cpp` lints after the turn (`--check`), teaching only the next turn (docs/HANDOFF.md).
The beat plays as soon as it is written, so anything that fixes binding must act per beat, before play,
or make the bad beat unwritable.

Sources are cited inline. Where I state a translation to this engine, that is my own proposal, not a sourced claim.

---

## 1. Data-to-text NLG (Reiter & Dale, Wordsmith/AP, RotoWire)

**Mechanism.** The classic pipeline runs content determination, then document planning, microplanning
(including referring expressions), then surface realisation. Text is rendered *from* a data structure that
already holds the facts. ([Reiter & Dale 1997](https://researchers.mq.edu.au/en/publications/building-applied-natural-language-generation-systems/);
[book, CUP 2000](https://www.cambridge.org/core/journals/natural-language-engineering/article/abs/ehud-reiter-and-robert-dale-building-natural-language-generation-systems-cambridge-university-press-2000-64953750-hardback-234-pages/E36CF7EA0765492430149FF8A2A14CD5);
[survey](https://arxiv.org/pdf/1906.00500))
Wordsmith templates are "data points, synonyms, and branches". A number in the text is a slot filled from
the data row, so a template cannot state a figure the row lacks.
([InterWorks on Wordsmith](https://interworks.com/blog/jlyons/2018/09/21/tableau-extensions-natural-language-generation-with-wordsmith-by-automated-insights)).
AP's earnings stories come from Zacks data through editor-designed storylines
([Wikipedia: Automated Insights](https://en.wikipedia.org/wiki/Automated_Insights)).
**The counter-example matters.** Neural data-to-text without slots hallucinates. On RotoWire only about 60% of
summary content could be grounded in the box score, and that gap is what pushes the model to invent facts
([Wang, INLG 2019](https://aclanthology.org/W19-8639.pdf)). The fix that worked was explicit content
selection and planning *before* writing
([Puduppully et al., AAAI 2019](https://ojs.aaai.org/index.php/AAAI/article/view/4668)).
Our situation looks the same: Claude's plan block is the content plan, but the surface text is free and drifts from it.

**Translation.**
- Spoken numbers become references: `Say: "the sum is {s}"`, where the engine speaks s's current label/value.
- Make a bare digit or number word in a Say a *parse error*, not a lint. The only way to say "0.5" is then to
  have a stage object holding 0.5.
- A reference to an id that doesn't exist (or isn't visible) by that beat is also an error.
- This is the closest thing to "Claude's input can't be wrong" for *numbers*. It does nothing for verbs
  ("adds") or relations ("same weight").

## 2. Referring-expression generation and grounded robot dialogue

**Mechanism.** The Dale & Reiter Incremental Algorithm builds a description only from attributes of objects in
the *domain model* ([Dale & Reiter 1995](https://arxiv.org/abs/cmp-lg/9504020)). It cannot refer to an attribute
that the scene lacks. Its quality depends on the preference order
([van Deemter et al.](https://preview.aclanthology.org/fix_video/J02-1003.pdf), cited via search summary).
Mavridis & Roy's Grounded Situation Model puts one amodal model at the centre, with language, vision and action
modules around it. Both speech and perception read and write the *same* model
([IROS 2006](https://www.media.mit.edu/cogmac/publications/IROS_2006.pdf);
[MIT thesis](https://dspace.mit.edu/handle/1721.1/38523)).
A description it hears ("a red ball on the left") is *added* to the model as an object
([MIT Media Lab](https://www.media.mit.edu/publications/grounded-situation-models-for-robots-where-words-and-percepts-meet)).

**Translation.**
- The GSM inversion is the interesting part. When speech asserts a fact, the engine *creates* the matching
  stage state, instead of checking that one exists.
- Example: "w1 is 0.5" or `Say: "... |w1=0.5| ..."` writes the label onto w1.
- Speech and stage then share one model (the engine's readback/state.txt is already that model).
- **Weak spot.** Robots ground in perception they don't author. Here Claude authors both channels, so
  "grounding" is really about one author being consistent.

## 3. Broadcast graphics (score bugs, Vizrt/Chyron, MOS rundowns, elections, weather)

**Mechanism.**
- Score bugs are templates *bound to data fields* from the venue scoreboard feed. The commentator talks over a
  graphic that is right by construction ([Viz Flowics / Sportzcast connector](https://creationnetworks.net/products/vizrt-viz-flowics-sportzcast-connector-annual-subscription-sw-fl0000006);
  [Vizrt templates](https://www.vizrt.com/news-articles/vizrt-reveals-free-library-of-html5-graphics-templates-for-viz-flowics-users/)).
- Election night: one AP vote feed fills both the maps and the on-air totals. A race call is a separate signed-off
  event ([WHYY/AP](https://whyy.org/articles/we-dont-speculate-how-ap-counts-votes-and-calls-races)).
  Vendor pipelines generate graphic playlists from the data
  ([TV Tech](https://tvtechnology.com/news/bannister-lake-and-decision-desk-hq-team-up-for-election-coverage)).
- News: via the MOS protocol, graphics are picked from pre-approved templates *inside the script/rundown item*
  in iNEWS/ENPS. They "automatically become part of the rundown"
  ([TV Tech: DekoMOS](https://www.tvtechnology.com/miscellaneous/avid-ships-dekomos-30-244020);
  [TSL MOS](https://www.tslproducts.com/media/2864/mos-rundown-playout-system_web.pdf)).
  The graphic is a typed object living in the script line it belongs to.
- Weather: the presenter fires the next queued graphic with a clicker. Talent sets the pace and the producer
  pre-builds the sequence ([KIRO 7](https://www.kiro7.com/news/how-does-weather-green-screen-work/43344659);
  [scene launcher patent](https://patents.justia.com/patent/7800582)).

**Translation.**
- (a) Data-bound templates: a spoken number comes *from* a stage value, which is the same as section 1.
- (b) MOS: the visual is embedded in the speech line itself as a typed object. Today's `|cue|` + `@cue` is
  halfway there. Go further and let the cue word *carry* its graphic inline:
  `Say: "the weights [w1 label=0.5|add up] to one"`.
- (c) The weather clicker shows the split that works. The talker owns *timing*, and the pre-built queue owns
  *content*. A beat could declare its visual queue first, and the Say then only fires the next queued item.

## 4. Film/TV: AV scripts, storyboards, audio description, captions

**Mechanism.**
- The two-column AV script pairs video and audio row by row, "one row for each speech", so the two stay
  aligned ([Celtx](https://blog.celtx.com/essential-av-script-template-for-better-video-scripts/)).
  Final Draft AV keeps each scene's video and audio "linked"
  ([Documentary Magazine](https://documentary.org/index%2Ephp/node/33461)).
  An empty video cell is *visibly* empty, which is the format's whole power: a reviewer sees the gap.
- EPUB Media Overlays (SMIL) make the sync unit a `<par>` holding one `<text>` and one `<audio>` clip
  ([DAISY KB](https://kb.daisy.org/publishing/docs/sync-media/overlays.html);
  [W3C spec](https://w3c.github.io/epub-specs/archive/epub32/spec/epub-mediaoverlays.html)).
  The pair is the unit, so you cannot have half of one.
- manim-voiceover: animations are written *inside* `with self.voiceover(text=...) as tracker:`. Bookmarks
  trigger animations at specific words ([quickstart](https://voiceover.manim.community/en/latest/quickstart.html);
  [plugin page](https://www.manim.community/plugin/manim-voiceover/)). Its author motivates it as replacing
  "record VO separately, sync in editor" ([solmaz.io](https://solmaz.io/2023/05/)).
- Audio description is the inverse problem (picture shown, not spoken). DCMP says to describe before or during
  the action, never after ([DCMP](https://dcmp.org/learn/277-a-day-in-the-life-of-an-audio-describer)).
- FCC caption quality rules (2014) require accuracy, synchronicity, completeness and placement. Captions "must
  coincide with their corresponding spoken words"
  ([DWT summary](https://dwt.com/FCC-Adopts-Closed-Caption-Quality-Standards-02-26-2014);
  [FCC slides](https://transition.fcc.gov/statelocal/closed-captioning-presentation-4-22-2014.pdf)).
  "Completeness" here is a coverage rule over the whole programme, and binding is a coverage rule too.

**Translation.**
- Make the beat a two-column row. Split each Say into clauses, and each clause must have a non-empty visual cell:
  either a `|cue|` with an `@` line or an explicit `-` meaning "no visual, deliberately".
- An empty cell is a parse error. The explicit `-` keeps it honest without forcing junk.
- Timing rule from AD: the visual should land at or before the word, never after. The current cue model already
  does this.

## 5. Programming languages and type systems

- **Make illegal states unrepresentable** ([Minsky, Effective ML slides](https://www.cs.cornell.edu/courses/cs3110/2013fa/lectures/27/lecture27_Minsky_EffectiveML.pdf);
  [2007 quote](https://serpentine.com/2007/07/29/on-the-value-of-strong-static-typing/)).
  Translation: no bare numerals in Say (section 1), and relations as stage predicates (see Penrose below).
- **Linear vs affine types.** Linear means used exactly once; affine means at most once
  ([Wikipedia](https://en.wikipedia.org/wiki/Substructural_type_system)). Rust is affine, so it cannot force a
  value to be consumed, and `#[must_use]` is only a warning
  ([Wuyts](https://blog.yoshuawuyts.com/linearity-and-control/)). In Linear Haskell, forgetting to close a handle
  is a type error ([Acolyer on Linear Haskell](https://blog.acolyer.org/2018/01/24/linear-haskell-practical-linearity-in-a-higher-order-polymorphic-language/)).
  Translation: each claim (from the plan block or tagged in Say) is a linear resource that a visual line must
  consume by the end of its beat. The lesson from Rust is explicit: *a lint is not a guarantee*. Today's
  `binding:` lint is `must_use`.
- **ICU MessageFormat / i18n.** Messages are templates with typed arguments; a `select` or `plural` without
  `other` fails to compile ([FormatJS](https://FormatJS.io/docs/core-concepts/icu-syntax)).
  Translation: `Say: "{w1} plus {w2} is {s}"` behaves like a message with arguments bound to stage ids, and the
  engine formats them (rounding, "one half" vs "0.5") consistently with the label.
- **Typst vs LaTeX refs.** In Typst a missing label is a hard compile error; LaTeX only warns and prints "??"
  ([Typst forum](https://forum.typst.app/t/is-there-a-way-to-suppress-errors-in-case-of-a-missing-reference/3255);
  [Overleaf](https://cs.sharelatex.com/learn/Errors:There_were_undefined_references.)).
  Our current binding lint is the LaTeX model; the proposal is the Typst model.
- **Penrose.** Domain declares types and predicates; Substance states objects and relations *without saying how
  they look*; Style maps each relation to visuals ([Penrose docs](https://penrose.cs.cmu.edu/docs/ref/substance/overview);
  [Ye et al. 2020](https://penrose.cs.cmu.edu/siggraph20)).
  Translation: Claude writes relations such as `same_weight a b`, `sum s = a + b` or `greater a b`, and a fixed
  Style rule draws each one (shared colour, sum label, comparison bar). Saying "they share the same weight" is
  then *the same act* as writing the relation. That covers the relation class of binding failures, which number
  refs cannot.
- **Design by contract.** `require`/`ensure`/invariant; a failed precondition means the caller has the bug
  ([Unibe slides](https://scg.unibe.ch/download/lectures/p2/P2-03-DesignByContract.pdf);
  [UNC](https://www.cs.unc.edu/~stotts/COMP145/CRC/DesByContract.html)).
  Assertions are often turned off in production, which is the same weakness as our post-hoc lint.
  Translation: a beat's postcondition is "every claim in Say is visible on stage when its word is spoken". The
  server checks it before handing the beat to the engine.
- **Literate programming.** WEB keeps prose and code in one source; tangle extracts the program and weave
  produces the documentation ([WEB man page](https://mirrors.sdu.edu.cn/CTAN/systems/knuth/local/man1/web.1);
  [Wikipedia](https://www.wikipedia.com/wiki/WEB)).
  Translation: one source, two projections. Speech and stage could both be *projections* of one claim list
  (tangle produces stage lines, weave produces Say text). This is the boldest form and probably too big.
- **Constrained decoding.** Grammar or schema masking guarantees syntax, not semantics
  ([Chugh](https://mohakchugh.is-a.dev/blog/constrained-decoding-fsm-grammar-guided-llm-structured-output);
  [Tian Pan](https://tianpan.co/blog/2026-04-12-structured-outputs-constrained-decoding-eliminating-parsing-failures-production-llm)).
  **Candid:** the hosted Claude API offers JSON-schema structured outputs, not arbitrary custom grammars on
  streamed free text (per the overview above; verify in the API docs). So "no digits in Say" can't be enforced
  at decode time. It must be a server-side parse rule plus a retry/repair.

## 6. Education and cognitive science

- **Mayer.** Temporal contiguity: 9/9 tests, median d=1.22. Signalling: 24/28, d=0.41
  ([Cambridge Handbook chapter](https://cambridge.org/core/books/cambridge-handbook-of-multimedia-learning/principles-for-reducing-extraneous-processing-in-multimedia-learning/F29A19FCD34C542806F736E0661C05F5);
  [NUS summary](https://medicine.nus.edu.sg/medixperience/scholarly-article/mayers-12-principles-of-multimedia-instruction/)).
  The redundancy principle is about narration plus on-screen *copies of the narration text*. A label "0.5" next
  to a weight while saying "0.5" is signalling/contiguity, not harmful redundancy. Don't let "redundancy"
  argue against binding.
- **Ainsworth DeFT.** Multiple representations work only if the learner can relate them. Translation between
  representations is a cognitive *task* that design must support
  ([Nottingham repo](https://nottingham-repository.worktribe.com/output/23577631/deft-a-conceptual-framework-for-considering-learning-with-multiple-representations)).
  A spoken claim with no visual twin gives the learner nothing to translate against.
- **McNeill / Kendon.** Gesture and speech form one system. The stroke coincides with the co-expressive speech,
  which expresses the *same idea unit* without being semantically redundant
  ([McNeill Lab: Growth Point](https://mcneilllab.uchicago.edu/writing/growth_points.html);
  [newlearningonline](https://newlearningonline.com/literacies/chapter-12/mcneill-on-gesture)).
  Phonological synchrony rule: the stroke precedes or ends at the peak syllable and does not follow it
  ([Frontiers 2024](https://frontiersin.org/articles/10.3389/fpsyg.2024.1345906/full)).
  Tighter semantic links give tighter timing (same source). The growth point is the *newsworthy* point, the one
  differentiated from background ([McNeill Lab](https://mcneilllab.uchicago.edu/writing/growth_points.html)).
  **What this implies for "speech with no gesture":** speakers do not gesture every word. Gesture marks the focal,
  contrastive idea unit. So the rule should be "every *claim focus* has a visual stroke", not "every word".
  That supports a per-clause cell with an explicit "no visual" option over a blanket rule.
- **Goldin-Meadow.** Gesture-speech *mismatch* marks a transitional knowledge state in learners
  ([APS Observer](https://www.psychologicalscience.org/observer/giving-students-a-hand-william-james-lecturer-goldin-meadow-shows-the-importance-of-gesture-in-teaching);
  [UChicago news](https://news.uchicago.edu/story/deaf-childrens-gesture-mismatches-provide-clues-learning-moments)).
  **Candid:** this is about learners' gestures, not teachers'. It does not show that a teacher's unillustrated
  claim is harmful; Mayer's contiguity data is the stronger argument.

## 7. Comics and scrollytelling

- **McCloud's word/picture combinations.** Duo-specific (both say the same thing) vs interdependent (the two
  combine into what neither says alone) ([Yale TI unit](https://teachersinstitute.yale.edu/curriculum/units/2014/1/14.01.01/6);
  [Hooded Utilitarian](https://www.hoodedutilitarian.com/page/29/)).
  The binding failure is "word-specific": the words carry the claim and the picture is decoration.
  Translation: the judge rubric could label each beat by McCloud category. The target is
  "interdependent, with the number duplicated on stage".
- **Scrollytelling.** Scrollama fires `onStepEnter` per text step, and that callback updates the graphic
  ([Pudding](https://pudding.cool/process/introducing-scrollama/)).
  Idyll binds components to reactive variables, so text and charts re-render from the same variable
  ([Idyll docs](https://idyll-lang.org/docs); [Conlen & Heer](https://idl.uw.edu/papers/idyll)).
  The step is the binding unit, and inline values in Idyll prose are *derived from the same variable* as the
  chart. That is section 1's idea again, in production.
  Translation: beat = step, and `{var}` in Say equals Idyll's inline display of a variable.

## 8. Other fields

- **Aviation callouts / challenge-response.** NTSB: "for every callout, there should be a corresponding
  indication or setting". Read at a methodical pace so each indication is seen. Line audits found
  "responded without looking" 43x and "verification omitted" 113x
  ([NTSB SA-018](https://www.ntsb.gov/Advocacy/safety-alerts/Documents/SA-018.pdf);
  [Berman & Dismukes](https://hsi.arc.nasa.gov/flightcognition/Publications/Berman&Dismukes_asw_jul10.pdf)).
  Translation: a second party verifies the callout against the *instrument* (stage state), not against the
  plan. The server is the pilot monitoring. It checks each Say claim against the engine readback *after*
  applying the beat's lines and before release. Failure-mode lesson: verification degrades into habit
  ("checked") when it is cheap to fake.
- **SSML `<mark>` and bookmarks.** TTS engines fire named events when a mark is reached, with elapsed time
  ([MDN](https://developer.mozilla.org/docs/Web/API/SpeechSynthesisUtterance/mark_event);
  [Azure SSML](https://learn.microsoft.com/en-us/azure/ai-services/speech-service/speech-synthesis-markup-structure)).
  Our `|cue|` is the same thing. Nothing new, but it confirms the cue model is industry-standard.
- **Karaoke eLRC / TTML.** Per-word timestamps inline in the lyric line
  ([lrc-kit](https://cdn.jsdelivr.net/npm/lrc-kit@1.2.1/README.md);
  [BetterLyrics](https://composer.betterlyrics.org/guides/lrc-to-ttml-conversion-guide)).
  The timing lives inside the text, as with inline cues. It is only a timing analogy, not about content agreement.
- **Not researched in depth / weak analogies:** dubbing/lip-sync (it matches *timing* of a fixed picture, not
  content); sign-language avatars; teleprompters (they keep speech on script, not on picture); telestrators
  (the human draws while talking, which is grounded by the human, with no mechanism to copy).

---

## Ranked shortlist

1. **Speech numbers are references to stage values; bare numerals in Say are a hard error** (sections 1, 3a, 5:
   Wordsmith slots, score-bug data binding, ICU args, Idyll inline vars, Typst refs). For example,
   `Say: "w1 is {w1}"`, which speaks w1's label. An unresolved or invisible ref, or a literal digit / number word,
   rejects the beat before play.
   *Failure modes:*
   - Claude paraphrases ("about half", "a bit more") to dodge it.
   - Claude labels everything, which clutters the stage.
   - Number-word detection ("three", "twice", "a third") is fuzzy.
   - It covers numbers only, not verbs or relations.
   - Spoken formatting ("zero point five" vs "one half") must match the label.

2. **Claims as relations with mandatory visual style (Penrose Substance/Style, GSM "speech writes the model")**.
   A small set of relation verbs (`same`, `sum`, `greater`, `step`, `adds`) is drawn by fixed engine rules, and
   the Say references the relation, so the visual exists by construction.
   *Failure modes:*
   - Domain coverage is never complete, so Claude falls back to free text for anything outside it.
   - Engine-chosen styling can fight the scene's layout.
   - Bigger language change and prompt cost.

3. **Two-column beat grammar: every Say clause has a visual cell, or an explicit `-`** (AV script, SMIL `<par>`,
   manim-voiceover `with` block, McNeill's "stroke per idea unit"). An empty cell is a parse error, not a lint.
   *Failure modes:*
   - The rule is satisfied in letter, not spirit: a generic `focus x` on every clause, or `-` everywhere.
   - It needs the judge or binding checker to police the content.
   - It adds tokens and latency.

4. **Per-beat pilot-monitoring gate on the server** (DbC postcondition, NTSB callout/indication). After a beat's
   lines are parsed, run the binding checker against the engine's projected state *before* release. On a miss,
   auto-repair deterministically (e.g. spoken "0.5" plus an existing object with value 0.5: flash or label it on
   that word) or hold the beat for a one-shot fix.
   *Failure modes:*
   - Regex false positives stall or garble a live beat.
   - Holding a beat adds audible latency.
   - Auto-repairs look mechanical.
   - Cheap repair invites Claude to rely on it, the way "responded without looking" happens in cockpits.

5. **Linear claims from the plan block** (linear types). Each `show:` item listed in the hidden plan is a resource
   that must be consumed by a stage line within its beat (or the turn); leftovers are an error at the beat
   boundary.
   *Failure modes:*
   - Goodhart: the plan gets thinner, because Claude lists less to owe less.
   - Matching plan items to lines is itself fuzzy.
   - It only catches what Claude planned, and many misses are unplanned asides.

**Candid overall:** only #1 and #2 make the bad state *unwritable*. #3 makes it *visible and explicit*. #4 and #5
are better-timed checkers. The strongest package is #1 + #4: references for numbers, plus a per-beat gate with
auto-repair for verbs and relations until #2 exists.
