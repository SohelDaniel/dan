# Training data for intent -> animation markup (research 2026-10-22)

Goal: pairs of (narration sentence / visual intent) -> (what is animated at that moment), convertible to our
markup (things, links/arrows, morphs, sequence diagrams, plots, arrays).

## 1. 3Blue1Brown material

### github.com/3b1b/videos (scene code) — https://github.com/3b1b/videos
Measured from a shallow clone of the repo:
- Year folders _2015.._2026, ~375 .py files, ~516k lines in total.
- ~7.6k classes (~5.4k Scene subclasses), ~30.6k `self.play(` calls, ~900 `says/thinks` (pi-creature speech bubbles).
- **No voiceover text in the code.** Narration lives only in the captions repo (below). The code has on-screen
  Text/Tex strings and short **step comments** inside construct(): ~8.5k of them, mostly 2018+
  (2023: 1180, 2024: 1442, 2025: 1438, 2026: 1308). The most common lead words are Show (563), Test (297, noise),
  Add (266), Setup, Highlight, Zoom, Ask, Label, Emphasize, Transition.
  Examples: "# Show four points going to a square", "# Combine two solutions", "# Add sun and earth",
  "# Comment on third force". A few comments quote the narration directly
  (e.g. _2024/transformers/embedding.py: `# To illustrate "You could imagine many other directions..."`).
- Scene order: by definition order in the file, or a `SCENES_IN_ORDER` list (9 files). See stage_scenes.py.
  One video maps to a folder holding several files (e.g. _2023/clt: main.py, galton_board.py, wordy_scenes.py,
  dice_sims.py). Scenes are long and imperative, with heavy custom helpers (custom/, once_useful_constructs/),
  and the code targets manimgl, not Manim CE.
- Also mirrored on HF as pravsels/videos_3b1b_code (file_path, content).
- Repo now has a CLAUDE.md, a sign that Grant uses Claude Code on it.

### Captions: github.com/3b1b/captions — https://github.com/3b1b/captions
- Official. Per video: `<year>/<slug>/english/{transcript.txt, sentence_timings.json, word_timings.json,
  captions.srt}` plus `video_url.txt` and ~25 translations. Built with Whisper, then hand-corrected.
- sentence_timings.json = [[sentence, start_s, end_s], ...]. Word timings exist too.
- **Measured: 153 English videos, 21,126 sentences, ~511k words**, covering 2015–2024. Last push was 2024-11-30,
  so 2025–2026 videos are not in it; use YouTube auto-captions via yt-dlp (`--write-auto-subs`) for those.
- Older third-party copy: HF psetinek/3Blue1Brown_transcripts (116 videos, no timings, 2022).

### Existing alignment of 3b1b transcripts to scenes
None found. ManiBench (below) hand-derives problems from 3b1b videos but does not align them.

### How to extract (sentence, animation) pairs from 3b1b
- Pure time alignment (caption timestamps vs. rendered scene durations) is hard. The final video is edited in
  Premiere: scenes are trimmed, reordered and interleaved with footage, and wait() times differ. Rendering
  manimgl scenes from 2015–2019 often fails on API drift.
- Practical route, text matching:
  (a) Pull on-screen strings (Text/Tex/TexText, says()) and step comments per scene, in order.
  (b) Embed-match each caption sentence to the scene whose on-screen text and comment tokens overlap it, with a
      monotonic (DTW-style) order constraint, since scene order roughly follows the narration.
  (c) Have an LLM read the matched sentence plus the code span under the nearest step comment, and emit our
      markup ("teacher distillation"). The 3b1b code then serves as grounding/evidence, not as the literal target.
- Estimate: of 21k sentences, roughly 30–40% describe something visual. Realistic yield is **3–6k usable
  sentence->markup pairs**, plus ~5k **(step comment -> code block) pairs** from 2018+ code that need no
  timestamps. Step comments are terse intents ("Show the first example"), which is close to our input style but
  more imperative than narrative.
- Effort: about 1–2 weeks for the matching pipeline plus LLM conversion, then a spot-check pass. The mapping from
  captions slugs to videos folders is manual for ~150 videos (about half a day).

## 2. Existing NL -> Manim / narration -> animation datasets
(HF search "manim" returns 85 datasets: https://huggingface.co/datasets?search=manim)
- **bespokelabs/bespoke-manim** — https://huggingface.co/datasets/bespokelabs/bespoke-manim
  1,000 rows, ~180 MB. Fully synthetic: question, title, **narration** (a paragraph script),
  **visual_elements [{description, timestamp "0:00-0:15"}]**, equations, ManimCE python_code, rendered video.
  This is the closest existing match to narration -> visual beat. Visual element descriptions are time-bucketed,
  not per sentence. LLM-generated, so the code is often simplistic or diverges from the descriptions.
  Gives ~4–8 beats per item, ~5k (beat description -> visual) pairs; align narration sentences to the beats by
  timestamp (rough).
- **ManimBench v1 (SuienR/ManimBench-v1)** — https://huggingface.co/datasets/SuienR/ManimBench-v1
  Under 1k rows: LLM-generated description, human-reviewed description, ManimCE code, Basic/Intermediate/Advanced.
  Clean but small, and the descriptions are of whole snippets ("a circle transforms into a square"), not narration.
  Paper: https://doi.org/10.1007/978-3-032-07938-1_26
- **ManiBench (nabin2004/ManiBench)** — https://huggingface.co/datasets/nabin2004/ManiBench, arXiv 2603.13251.
  Pilot has 12 problems derived from 3b1b videos (150–200 planned), with required visual events. Eval only.
- **nabin2004/AOS-Narrated-Manim-400** — https://huggingface.co/datasets/nabin2004/AOS-Narrated-Manim-400
  361 manim-voiceover VoiceoverScene scripts. `with self.voiceover(text=...)` blocks give **exact
  sentence->animation pairing**, but agent-generated. Also nabin2004/manim-narrated-dpo-400.
- **manim-voiceover code on GitHub**: GitHub code search shows ~744 Python files with `self.voiceover(text=`
  and ~1,950 mentioning VoiceoverScene (counts approximate). Each block is a literal (narration sentence,
  animations played during it) pair, human-written. After deduplicating tutorial copies, expect **~3–8k pairs**.
  Scrape with `gh search code` plus a clone, then parse with Python AST. This is the best "naturally aligned"
  source found. Library: https://github.com/ManimCommunity/manim-voiceover
- **Instruction -> code pools (prompt is a request, not narration)**: ArunKr/cleaned-manim-dataset (~26k
  instruction/output pairs merged from ~19 repos, including issues; noisy, many duplicates, some non-code),
  nabin2004/manim-sft-10k, BechusRantus/manim-code-dataset (3,145), thanhkt/manim_code,
  generaleoley/manim-codegen, Edoh/manim_python, generative-manim/generative-manim-human-feedback (<1k),
  nmsofficial/Manim-8600-Prompts (prompts only). Useful only for teaching "what visual primitives exist";
  low value for narration -> intent.
- **TheoremExplainBench** — arXiv 2502.19400, https://arxiv.org/abs/2502.19400. 240 theorems (eval prompts),
  agent produces >5-min Manim videos with narration. Prompts only, plus whatever agent outputs they released.
  Use as eval topics, not as training pairs.
- **Code2Video / MMMC** — arXiv 2510.01174, HF YanzheChen/MMMC. 117 full 3b1b-style videos plus 339
  timestamped clips across 13 subjects. Video plus metadata, **no code**. Alignment would need a VLM to describe
  frames. Low yield.
- **Paper2Video** — arXiv 2510.05096, https://github.com/showlab/Paper2Video. 101 papers with talk videos and
  slides (~16 slides per video). Slides, not animation. Not useful.
- **ManimLayout-1K / EduRequire-500** — arXiv 2605.15585 ("See Before You Code"). 1k Manim animations giving
  ~22.6k scene-layout samples. Layout supervision, which could matter for our layout engine. Release status
  unclear.
- Khan Academy: no public aligned dataset. YouTube transcripts are available, but the visuals are freehand
  drawing with no structure. Not usable without heavy VLM work.
- MathVista and similar: static image QA, not relevant.

## 3. Other narration + visual sources
- **LPM (Lecture Presentations Multimodal)** — arXiv 2208.08080, ICCV 2023. 180+ h of video, 9k+ slides,
  10 lecturers, slide figures weakly aligned to spoken segments. Visuals are static slide figures with no structure.
  Low value.
- **AVLectures** — WACV 2023, https://openaccess.thecvf.com/content/WACV2023/papers/S._Unsupervised_Audio-Visual_Lecture_Segmentation_WACV_2023_paper.pdf
  86 courses, 2,350 lectures, transcripts plus OCR. Chalkboard/slide video. Not usable for structure.
- VideoXum, HowTo100M: wrong domain (summarisation, instructional video). Skip.
- AI2D (~5k science diagrams with constituent/relationship annotations, arrows labelled) and SciCap
  (~400k figure-caption pairs): static. AI2D's arrow and relationship graphs are mildly useful for
  "X affects Y" -> link idioms, but small.
- **Text -> diagram**: ibm-research/MermaidSeqBench (132 NL->sequence-diagram samples, arXiv 2511.14967;
  directly maps to our sequence-diagram primitive, eval-size); mermaidchart/mermaid-syntax (~2.7k Mermaid with
  prompts and topological summaries); MananSuri27/Flowchart2Mermaid (1.3k image->Mermaid);
  moogin/json-mermaid (10–100k). Good for the "A talks to B" / flow sub-grammar after conversion (easy:
  Mermaid -> our markup is mechanical). Effort: low.

## 4. "Visual idiom" datasets (verb/relation -> visual encoding)
- No dataset found mapping verbs/relations ("doubles", "cancel", "shrinks", "talks to") to visual encodings
  for diagrams or animation.
- Nearest neighbours: HAIVMet (1,193 DALL-E images of 958 linguistic metaphors) and V-FLUTE (6k figurative
  image-caption entailment, arXiv 2405.01474). These are photo-like illustrations, not diagram motion.
  KineTy (ECCV 2024): 600k kinetic-typography videos from 584 templates with "dynamic captions" of motion. Text
  motion only. Not usable.
- Conclusion: the idiom table must be authored by us. Seed it by mining 3b1b step comments for verb ->
  animation-call co-occurrence (e.g. "Show ... grow" -> GrowArrow, "Highlight" -> Indicate/FlashAround,
  "Transition" -> TransformMatchingTex). That gives a measured verb->primitive prior from ~8.5k comments.

## Ranked
1. manim-voiceover code on GitHub plus AOS-Narrated-Manim-400: true per-sentence alignment, ~3–8k pairs,
   about 1 week (scrape, AST parse, LLM-convert Manim to our markup).
2. 3b1b captions plus 3b1b scene code (text-matched, LLM-distilled): highest quality and style, ~3–6k sentence
   pairs plus ~5k comment->block pairs, about 2 weeks.
3. bespoke-manim: ~5k beat-description pairs (synthetic), 2–3 days.
Supplementary: Mermaid sets for the sequence-diagram sub-grammar (~3k, 1 day).
Not usable: LPM, AVLectures, Paper2Video, MMMC (no code), Khan, visual metaphor sets, KineTy, MathVista.
Realistically, the largest high-quality source is **synthetic**: have Claude write intent->markup pairs
directly against our grammar, using the above as topic and style seeds.
