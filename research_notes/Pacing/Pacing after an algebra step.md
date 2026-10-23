# Pacing after an algebra step

2026-10-23. The user's complaint: when the tutor does an algebraic step it says the step, the formula morphs in
about a second, and the next sentence starts at once. Good teachers (3Blue1Brown, Professor Leonard) let a step
land, say it again in other words, and move on; but not after every step. What follows: what the engine does today,
what the real chats measure, what 3Blue1Brown's own corpus and the learning research say, and the proposal.
Nothing was changed in code for this note.

## 1. What happens today

- A `Say` blocks until its audio ends (`Engine::blocking`). Lines cued inside it run on their words; uncued lines
  run after it, and a `morph` is blocking, so the next `Say` starts the instant the morph lands. **Time at rest
  with the finished formula on stage before the next sentence: 0 s.** `say_.lingering = 2.0f` only fades the
  subtitle.
- `wait Ns` exists (README "Setup lines"), is blocking, lint-free and decodable (`--check` passes a `wait 2s`
  between two sentences). The prompt never mentions it; it only says "Don't use `pause`". Claude has never
  written one: 0 `wait` lines in 59 real turns.
- The toolbox's derivation move ("A formula as the summary of what they saw") says "one formula that grows", one
  morph per step; it says nothing about the beat after a step or about one step per sentence.

Measured on the real chats (`measure-formula-sentences.py chat/sessions/*/turns/*.txt`, 59 turns, 495 sentences):

| | |
|---|---|
| sentences that change a formula | 108 (22%) |
| of those, followed directly by another formula-changing sentence | 45 (42%) |
| of those, the next sentence moves on without touching the formula | 76 (70%) |
| sentences with 2 or more formula changes | 5; the last chat (rationals, 20261023-060527) has two with three |
| words per formula sentence (mean) | 17.4 |

The rationals chat is the case the user saw: "Then a over b is |ad|a times d steps, c over d is |bc|b times c
steps, and the sum is |sum|both counts added." (three re-marks of `f` in one sentence), then straight into "The top
is integers multiplied and added…".

## 2. 3Blue1Brown's own corpus

Scene code (`tools/mine/work/videos`, 2019 on; `measure-3b1b-steps.py`):
- 2,195 formula transforms (`TransformMatchingTex/Strings/Shapes`, `TransformFromCopy`, …); **73% of the plays
  that hold one hold exactly one** (1,603 / 424 / 168 for 1 / 2 / 3+).
- 66% are followed directly by `self.wait()`; median 1.0 s, mean 1.35 s (any other play: mean 1.74 s). The
  transform itself runs 1 s by default. Code waits are the floor: he cuts to the narration in an editor afterwards.

Captions (`3b1b/captions`, 140 videos, 20,997 sentences; measured by the research agent, scripts in its report):
- Speaks at **205 wpm** (180 with silences). Clarity does not come from slow speech.
- Gap between sentences: median 0.62 s, p75 0.92, p90 1.46, p95 2.18, p99 4.8. **21% of gaps ≥ 1 s, 5.9% ≥ 2 s,
  2.8% ≥ 3 s.** So about one sentence in five gets a 1 s beat, one in 17 a 2 s beat, one in 35 a 3–4 s hold.
- Long holds (2.5–6 s) follow **statements of a result or a just-performed step**, not questions ("…add together
  all of the entries on one of these diagonals." → 3.5 s; "…is called the span of those two vectors" → 3.7 s).
- His restatement idiom is "In other words, …" (58 uses) with a *different framing*, never the same words again.
  "Take a moment" (80) is itself the beat: the gap after it is only 0.70 s.
- In his words (Numberphile podcast, The Hope Diamond): "to understand it, it has to be paced better"; viewers
  told him to "slow the hell down" and he added the three-to-four-second holds Brady Haran describes.
- "Pause and ponder" (35 uses) is an instruction to the viewer at a checkpoint, not a description of his silences.

## 3. The learning research (sources in the agent's report, copied below in §6)

- **Segmenting** (Mayer & Pilegard 2014): learner-paced segments beat a continuous stream, median d = 0.79 over
  10 tests. **System-paced works too**: Spanjers, van Gog & van Merriënboer 2010: animations that paused
  automatically **2 s** after each segment let novices learn the same with less effort; experts unaffected
  (expertise reversal). Hasler et al. 2007: the gain is not extra time; adding the same time at the end does
  nothing, the pause has to sit at the boundary.
- **Transient information** (Leahy & Sweller 2011; Wong et al. 2012): spoken and animated steps vanish; long
  stretches lose, short segments win.
- **Event boundaries** (Zacks, via Spanjers): what sits at a marked boundary is remembered better than what is
  between. A pause plus a cue of what changed marks the boundary (Spanjers et al. 2012: pauses and cues each do
  part of the work).
- **Within-presentation pause time** (Stahl 1994, ERIC ED370885): the teacher stops and gives "3 or more seconds
  of uninterrupted silence to process the just-presented information", in bite-sized chunks. Tobin 1987: a 3 s
  threshold for higher-level outcomes; Rowe: teachers typically wait under 1 s.
- **Subgoal labels** (Catrambone 1998; Atkinson et al. 2000): naming what a group of steps did ("that's the common
  denominator") beats reading the steps out. Finer than one step adds nothing (VanLehn 2011).
- **Restating** (Chi et al. 2001; Wittwer & Renkl 2008): a tutor repeating the step in the same words does not
  help; prompting ("why did the sign flip?") or re-framing does. Self-explanation after each line does (Chi 1994).
- **Speech**: sentence-final pauses 0.8–1.2 s, within-sentence 0.4–0.7, paragraph longer (Liu et al. 2022).
  Lectures run 125–160 wpm; comprehension drops above 200 for second-language listeners.
- **Pause procedure** (Ruhl, Hughes & Schloss 1987): 2-minute pauses every 12–18 min raise recall; the macro beat,
  not the per-step beat, and it needs an activity.

## 4. The proposal (summary; the full plan with the measurement bar is in the chat of 2026-10-23 and history.md)

Three tiers, budgeted by structure, not by step count:
1. **Every step: one formula change per sentence, and it rests.** Engine: after a sentence whose beat changed a
   `tex`, rest 1 s with the formula still before the next `Say` starts (3b1b's floor; Grant's p75 gap).
2. **A subgoal step** (its result is what the next step works on, or it produces the named thing): one *digest*
   sentence that reads the result off the stage in other words, pointing at each part (`[f]`, `indicate f[part]`,
   `{expr}`), then `wait 2s`. About one step in six.
3. **The crux** (at most one per answer, named in the plan): the digest as a prompt or a re-framing ("in other
   words…", "why did…?"), then `wait 3s`.
Level `expert`: tiers 2 and 3 off. Routine manipulation (distribute, collect) gets a label, never a beat.
Target: 4–10% of sentence gaps ≥ 2 s (Grant: 6%); today 0%.

Axioms: none bent. No new ink, no placement, no delete; a rest is a still frame between eased morphs; pointers in
the digest are `indicate`/`[id]`, not model; `wait` already reads back. The one engine change (the automatic rest)
moves timing further *into* the engine.

## 5. Open questions

- Should Claude write seconds (`wait 2s`, the existing word) or a structural word (`beat`, `beat long`) with the
  engine choosing seconds? Start with `wait`: no syntax, measurable now; add the word only if the numbers say so.
- The subtitle fades after 2 s; during a 3 s rest it should stay so the words can be re-read.
- A gap metric from the engine's `say`/`end` events, so the eval can measure rests without a judge.

## 6. Sources (as gathered by the research agent; [unverified] = not checked against a primary source)

- Numberphile podcast, "The Hope Diamond" with Grant Sanderson: https://static1.squarespace.com/static/5b2cf993266c07b1ca7a812f/t/619baa50bd070630df6c0010/1637591632942/The+Hope+Diamond+-+with+3blue1brown.pdf
- 3b1b captions: https://github.com/3b1b/captions ; code: https://github.com/3b1b/videos ; manimlib defaults `default_wait_time = 1.0`, `DEFAULT_ANIMATION_RUN_TIME = 1.0`
- Lex Fridman #118 transcript: https://podcasts.happyscribe.com/lex-fridman-podcast-artificial-intelligence-ai/118-grant-sanderson-math-manim-neural-networks-teaching-with-3blue1brown ; Dwarkesh: https://www.dwarkesh.com/p/grant-sanderson ; FAQ: https://www.3blue1brown.com/faq
- Rowe 1986, J. Teacher Education 37(1), https://doi.org/10.1177/002248718603700110 ; Tobin 1987, https://doi.org/10.2307/1170357 ; Stahl 1994, https://files.eric.ed.gov/fulltext/ED370885.pdf
- Ruhl, Hughes & Schloss 1987, https://doi.org/10.1177/088840648701000103 ; Bachhel & Thaman 2014, https://doi.org/10.7860/jcdr/2014/8260.4691
- Mayer & Pilegard 2014 (Cambridge Handbook of Multimedia Learning, segmenting/pretraining/modality) ; Mayer & Chandler 2001, https://doi.org/10.1037/0022-0663.93.2.390 ; Rey et al. 2019, https://doi.org/10.1007/s10648-018-9456-4 [abstract only]
- Spanjers, van Gog & van Merriënboer 2010, https://repub.eur.nl/pub/19682/201020040109.pdf ; Spanjers et al. 2012, https://doi.org/10.1016/j.compedu.2011.12.024 [abstract only] ; Spanjers et al. 2011, https://doi.org/10.1002/acp.1832
- Leahy & Sweller 2011, https://doi.org/10.1002/acp.1787 ; Wong, Leahy, Marcus & Sweller 2012 [secondary]
- Atkinson, Derry, Renkl & Wortham 2000, https://doi.org/10.3102/00346543070002181 ; Catrambone 1998, https://doi.org/10.1037/0096-3445.127.4.355 ; Renkl & Atkinson 2003, https://doi.org/10.1207/S15326985EP3801_3 ; VanLehn 2011, https://doi.org/10.1080/00461520.2011.611369
- Chi et al. 1994, https://pcl.sitehost.iu.edu/rgoldsto/courses/cogscilearning/chiselfexplanation.pdf ; Chi et al. 2001, https://doi.org/10.1207/s15516709cog2504_1 ; Wittwer & Renkl 2008, https://doi.org/10.1080/00461520701756420 ; Dunlosky et al. 2013, https://doi.org/10.1177/1529100612453266
- Liu et al. 2022 (pause lengths), https://www.frontiersin.org/journals/psychology/articles/10.3389/fpsyg.2022.778018/full ; Tauroza & Allison 1990 via https://files.eric.ed.gov/fulltext/ED566202.pdf [secondary] ; Griffiths 1992, https://doi.org/10.2307/3587015
- Khan Academy on Mayer: https://blog.khanacademy.org/how-khan-academy-videos-are-made-to-help-you-learn/
- Professor Leonard: only viewer perception found (Medium, Learnamic, Reddit snippets); "never skips a step" is the user's observation [unverified].
