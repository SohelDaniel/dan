# Prompt size: idioms in the system prompt? (2026-10-22)

## Measured size (claude -p "hi" --output-format json, sonnet, per-part append minus bare baseline)
- Claude Code baseline (no append): 2,895 tokens
- chat/prompt.md (no examples): ~6,460 tokens (17.7k chars)
- 9 exemplars: ~4,430 tokens (~490 each; markup ~2.2 chars/token)
- language reference: ~6,810 tokens (17.7k chars)
- Total appended: ~17,680 tokens; full input 20,578
- Caching: the 2nd identical call showed cache_creation 0, cache_read 20,576. -p caches the appended prompt across processes.

## Latency, n=15 each, run side by side (jobs 2), time to first scene line
- plain: median 13.4 s (p25 11.3, p75 17.0); total 18.6 s
- +16k tokens (31 exemplar-style idioms, "## More idioms"): median 14.4 s (12.0, 18.9); total 20.5 s
- paired by question: +0.2 s mean, SE 0.76 s, 8 of 15 slower. This is noise. Plan length unchanged (405 vs 410 chars).

## Sources
- Agarwal 2024 many-shot ICL https://arxiv.org/abs/2404.11018 (gains with many shots; MATH peaks about 125 shots, XSum about 50)
- Bertsch 2024 https://arxiv.org/html/2405.00200 (retrieval beats random by 51.5 points at 1 shot, by 4.9 at 1,500)
- Baek 2024 https://arxiv.org/abs/2412.16926 (selection hardly matters at long context)
- KATE https://arxiv.org/html/2101.06804 (ToTTo 28.4 to 40.3 BLEU; NQ 28.6 to 41.6)
- EPR https://arxiv.org/abs/2112.08633
- Lost in the Middle https://arxiv.org/html/2307.03172v3 (75.8% to 53.8%)
- Chroma Context Rot https://www.trychroma.com/research/context-rot (focused ~300 tokens beats ~113k for all models)
- NoLiMa https://arxiv.org/abs/2502.05167 (11 of 13 models below 50% of baseline at 32k)
- IFScale https://arxiv.org/abs/2507.11538 (claude-sonnet-4 decays linearly with instruction count; bias toward early instructions)
- Prompt caching https://platform.claude.com/docs/en/build-with-claude/prompt-caching (read 0.1x, write 1.25x/2x, TTL 5 min or 1 h)
- Claude Code caching https://code.claude.com/docs/en/prompt-caching (automatic, -p included, 1-hour TTL on a subscription; a changed system prompt invalidates the cache)
- Agent Skills https://www.anthropic.com/engineering/equipping-agents-for-the-real-world-with-agent-skills
- Context engineering https://www.anthropic.com/engineering/effective-context-engineering-for-ai-agents

## Recommendation
- Idiom size: about 500 tokens per full scene, or about 150-250 for a short idiom of one narration line plus 3-8 markup lines. That gives roughly 50 short idioms ≈ 10k tokens, 300 ≈ 50-75k.
- Up to ~50 short idioms (~10-15k tokens, prompt ~35k total): put them in the static system prompt. Measured latency cost is about zero and the cache keeps it cheap. This is still far below the 32k-113k ranges where degradation is reported, and at ~35k we are near the edge of NoLiMa's range.
- 100-300 idioms (30-75k tokens): retrieve per question rather than including all of them. Have server.mjs pick the 5-10 most relevant (embedding or BM25 on the narration-kind description against the learner's question plus stage state). Inject them into the user message, not the system prompt, so the cached system prefix stays intact.
- Engine verbs: promote recurring idioms into verbs. That shrinks both the prompt and the output.
- Gate: compare a static set and a retrieved set on the eval rubric before committing.
