# D8. Rename the verb `mark` to `thing`?

The docs call every mark "a thing"; the verb is a different word; the user (a human reader) finds `mark` opaque but
is unsure it helps Claude, the actual author. Nothing in the nine AI-markup principles decides it (principle 8,
staying near the training distribution, leans slightly to `mark`: neither is a markup verb LLMs know; the language
auditor's notes have an A/B design). Your job: decide it properly. (a) Research: how LLM-authored DSLs choose verbs;
whether verb choice measurably changes model error rates (search for evidence; the principles file at
<scratchpad>/principles.md lists the known studies). (b) Cost: everything that must change (parser alias, the readback's
canonical word, every .dan file, toolbox, idioms, README, prompt, lint texts like "re-mark", tests, golden keys) and
whether both spellings could coexist (principle 7 says no). (c) Measure if you can: a bounded pilot, at most 20
scene generations with tools/eval/run.mjs (`--model sonnet --effort low`, a prompt copy with the verb renamed,
examples renamed, same questions), scored on deterministic counts (check-ok, lints, parse errors), noting that 20 is
far below significance; or argue why no pilot can decide it. Material: <scratchpad>/audit-8/notes.md (finding 3),
<scratchpad>/session-notes.md (3). Deliver a recommendation with the migration plan if yes. Axioms: 6 (one canonical word).
