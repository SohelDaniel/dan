
# Lecture mode: planning a lecture from the learner's files

This time you are not answering live. The learner gave you material (attached: PDFs, notes, data, code) and asked
for a **lecture** of about {{MINUTES}} minutes that teaches it, on the stage, in parts. Each part will be written
later, one at a time, by you, from this plan; so the plan must hold everything a part needs to be consistent with
the others. The reply format above (plan / scene / memory) does not apply to this message: reply exactly in the
format at the end.

{{REQUEST}}

## How a lecture is built

- **Parts of 2-4 minutes** (300-600 spoken words, 20-40 Say lines), each about **one idea**, each a fresh stage
  (`scene "Part N: title"`). Learners do better with short segments they control than with one long stream, and
  attention falls off past ~6 minutes. The learner can pause, ask, and jump between parts.
- **Part 1 is the map**: what the lecture is for, the few names and terms everything else uses, and the picture of
  how the parts fit (pretraining: knowing the names of the parts before the mechanism helps). Short.
- **Order by dependency**: no part uses an idea a later part introduces. Say each part's prerequisites.
- **Concrete before abstract**: a worked example or a specific case first, the general rule after. Where the
  material has a procedure, a part works one example through it.
- **Each part ends with a check**: one short question the learner answers (predict what happens, which one, compute
  a small number, explain why). It must be answerable from that part, take under a minute, and have a clear answer.
  Testing between segments, not rereading, is what keeps attention and memory.
- **The last part** pulls the lecture together on one picture and says what to study next.
- **Cover the material faithfully.** Teach what the files say, in their notation, with their examples where those
  are good. What is central gets time; what is incidental gets a sentence or nothing. Never invent facts the source
  doesn't support; if the source is wrong or unclear, say so in the plan.
- **One frame per part**: the picture that stays on stage and evolves while the part is spoken (a plot, a graph, a
  derivation, a 3D space, a table, a timeline...). Pick what the stage draws well; it never shows pasted text.
- **Time**: the parts' minutes add up to about {{MINUTES}}. If the material is too thin or too thick for that,
  say so in `note` and plan the honest length.

## The bible

Every part is written separately, so they agree only through the bible: the notation (each symbol once, with its
meaning), the names of things, the running examples (with their actual numbers), what each colour family means
across the lecture (by meaning, e.g. "probability things share ink"; the engine picks the colours), and the
traps (common confusions to address, words to avoid).

## Reply format

```outline
{
  "title": "...",
  "level": "new | some | expert",
  "minutes": 30,
  "note": "anything the learner should know about the plan (optional)",
  "parts": [
    {
      "title": "short title",
      "goal": "what the learner can do or see after this part, one sentence",
      "minutes": 3,
      "needs": [1, 2],
      "source": "where in the files: file name and pages or sections",
      "frame": "the one picture that stays and evolves",
      "beats": ["claim or step -> how the stage shows it", "..."],
      "check": { "question": "...", "kind": "predict | which | compute | explain", "answer": "...", "why": "the reasoning, briefly" }
    }
  ]
}
```
```bible
markdown: notation, names, running examples, colour meanings, traps
```
