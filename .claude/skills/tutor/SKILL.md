---
name: tutor
description: Use when the learner asks to see something explained on the stage, says "use the tutor / the engine / the stage", wants a lecture prepared from files, or wants notes as a PDF. The tutor is a visual, spoken 3Blue1Brown-style explainer with its own tools (MCP server "tutor").
---

# The visual tutor, from this chat

The `tutor` MCP tools hand work to the tutor running on this machine. It has its own prompt, examples and
checker: you never write its markup, you write a **brief**.

- **show**: explain the current topic on the stage, aloud with drawings. Write the brief as the learner would ask
  it: the question, what this conversation already established (facts, notation, the examples used, where they
  got stuck), their level, what to show first. Plain prose, 3-10 sentences; no drawing instructions. It returns
  when the tutor starts speaking; then say "take a look at the stage" and carry on. `stage` shows what is drawn.
- **ask**: a side question about the paused stage ("why is that 6?").
- **notes**: you write the markdown (summary, definitions as LaTeX, worked examples, mermaid diagrams,
  exercises with answers); the tool prints it to a PDF in ~/Documents/Tutor notes and opens it.
- **lecture** → **lecture_build** → **lecture_play**: a long lecture from files (local paths), planned in parts
  with a question closing each. `lecture` returns the outline in ~3 minutes: show it, let the learner change it,
  and only then `lecture_build` (hours, in the background; `lecture_status` follows it). Put the conversation's
  context and the learner's wishes in `ask`.

If a tool says the tutor is not built or did not start, tell the learner to run `tutor` once in a terminal.
