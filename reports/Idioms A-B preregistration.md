# Idioms A/B test: pre-registration

Written 2026-10-22, before any run with the idioms.

**Question.** Does putting the mined visual moves (`chat/idioms.md`) in the teaching prompt make the tutor show more of
what it says, without hurting the picture or the wait?

**Arms.** Both use the same working tree (engine, exemplars, prompt) and differ only in the idioms:
- **A:** `chat/prompt.md` as it is.
- **B:** A plus `chat/idioms.md`, appended after the worked examples.

**Runs.**
- 44 questions (`tools/eval/prompts-broad.json`), 2 generation runs per arm.
- A's first run is `eval-refs` (same prompt and engine, generated earlier today), so A gets 1 more run and B gets 2.
- Pairing is by question.

**Primary metric.** The share of claims fully shown at the moment they're said, from `tools/eval/sentences.mjs` (Sonnet
judge, a frame at the end of each sentence), pooled over both runs of each arm.

**Decision rule.** Adopt B only if all of these hold:
1. B − A ≥ +5 points on the primary metric, with the 95% paired-bootstrap interval (resampling questions) above 0.
2. Not worse by more than 0.15 on the scene judge's layout, accuracy or teaching (`tools/eval/judge.mjs --rubric v2`, means).
3. Median time to the first stage line not worse by more than 1 s (`latency.mjs`, n=15 per arm, side by side).
4. The user looks at frames from both arms and agrees.

**Noise floor.** Two runs of the same prompt earlier today gave 50% and 53% (different sentences each run), so a
difference under about 4 points is within run-to-run noise.

**Secondary (reported, not decisive):**
- not-shown share;
- the number and process claim kinds;
- binding lints;
- scene length;
- how often the idioms' moves (enclose, cross, sweep, reveal one by one…) appear in B's scenes.

## Result (same day, after the runs)

No outputs were spoiled by the session limit (checked: 0 replies, scores or sentence verdicts contain a limit message).

| | A (current) | B (with idioms) | B − A |
|---|---|---|---|
| claims fully shown, run 1 / run 2 | 56.5% / 54.0% | 57.4% / 61.4% | |
| **claims fully shown, pooled (primary)** | **55.3%** | **59.4%** | **+4.2 pts, 95% CI [+0.2, +8.2]** |
| claims not shown, pooled | 11.0% | 8.6% | −2.4 |
| layout / accuracy / teaching (scene judge) | 3.48 / 3.66 / 3.74 | 3.44 / 3.70 / 3.78 | −0.03 / +0.05 / +0.05 |
| binding (scene judge) | 3.06 | 3.17 | +0.11 |
| time to first stage line (median, n=15) | 12.95 s | 13.56 s | +0.6 s |
| lines per scene | 26.1 | 26.1 | 0 |

Against the rule:
1. **Not met.** B − A is +4.2 points, below the +5 set beforehand; the interval does exclude 0.
2. Met: nothing is worse by more than 0.15.
3. Met: +0.6 s is under 1 s.
4. Pending: the user's own look.

Secondary: B uses the moves more:
- `enclose` 16 times against 3;
- `send` 12 against 6;
- `hide`, i.e. tidying what's done, 58 against 28.

It uses `at=` pointers less (28 against 43). The scenes are the same length.

**Reading.** A small, probably real gain (the interval excludes zero) at no measured cost. By the rule we set, it falls just
short of "adopt". The decision goes to the user, rather than moving the bar after seeing the result.
