# Copying vs composing: why the tutor imitates examples, and what to do

2026-10-22. Research only, no code. Notes are in `research_notes/Copying vs composing/`:
- `copying.md`: why language models copy examples;
- `training.md`: whether training a model would help;
- `compilers.md`: how other fields compile visuals from meaning.

## What the user saw
A convolution example in the prompt showed only half the move: the kernel slides, but the values never flow into the
output. Claude then copied the half move and dropped the half it used to do. The fear is that Claude imitates instead
of composing the tools, and produces poor work wherever no example covers the topic.

## What our own data says (176 scenes, two prompt versions)
Each scene's structural closeness to its nearest example (pairs of verbs and kinds of thing) set against its quality:

| | least like any example | middle | most like an example |
|---|---|---|---|
| claims fully shown | 54–55% | 53–60% | 56–61% |
| picture (judge) | 3.64–3.68 | 3.57–3.71 | 3.47–3.55 |

- **Not supported:** "uncovered topics come out bad". Recursion, balancing, logarithms and Fourier came out fine with no matching example.
- **Supported:** anchoring. A close example gets copied, including its gaps. Scenes that copy an example closely have slightly worse pictures, a template forced onto a topic it doesn't fit.
- **The real ceiling is everywhere:** about 55–60% of claims are fully shown, whether or not an example covers the topic.

## What the research says
- **Examples teach the shape of an answer, and strong models copy that shape faithfully, flaws included.**
  - Min et al. 2022: the format of demonstrations matters more than whether they are correct.
  - Larger models follow demonstrations over their own priors.
  - Jansson & Smith's "design fixation": human designers copy an example's flaws too.
  - Anthropic's own guidance for Claude: few, varied examples, each with its reasoning, so Claude generalizes from the explanation rather than the surface.
- **Documentation beats examples for using tools in new ways.** Hsieh et al. 2023: documentation alone matched examples on known tasks and beat them on hundreds of new APIs. A wrong example biases usage.
- **Planning before writing makes models compose.** PlanSearch: Claude 3.5 Sonnet went from 41% to 77% by planning in prose before code. Least-to-most and Self-Discover agree. Varied examples beat similar ones for new combinations (Levy 2023).
- **Training:**
  - Supervised fine-tuning on examples teaches imitation (Chu et al. 2025: "SFT memorizes, RL generalizes"), so it would make this worse.
  - Reinforcement learning against our checkers and judge is the kind that transfers. The closest paper (animation code, 2026) reached 94% render success with a 30B model.
  - But RL can't create abilities the base model doesn't already show when sampled several times (Yue et al. 2025).
  - No paper shows a model of 32B or under beating a frontier model on open-ended teaching.
  - Estimated cost: $2–8K and 4–8 weeks, with likely reward hacking (labels on everything, clutter, saying less).
- **Compiling visuals from meaning** (Vega-Lite, Draco, Penrose, Balsa's "interesting events", Gemini's animated transitions):
  - It works inside domains where the rules exist, and gives consistent, correct defaults.
  - It does not invent compositions for things no rule covers.
  - When the language model writes a semantic spec instead of drawing code, results improve (FEYNMAN: 82.5% of Penrose Substance programs compile).

## Recommendation, in order
1. **Measure the real problem first: a held-out "new topics" test set** (about 200 questions on topics no example covers).
   Every change below is judged on it, and it says whether training could help at all: first-try success against best
   of 8, for Claude and an open model.
2. **Restructure the prompt from examples to documentation.** This is cheap and has the strongest evidence.
   - Each primitive gets its purpose: "use it to show X, pairs with Y".
   - The 50 idioms become named patterns: intent, the primitives involved, a short fragment.
   - The gold scenes drop from 9 to 3–5, each complete, varied, and annotated with why each line is there.
   - No partial examples, ever.
3. **Plan, then write.** Claude lists each spoken step with its visual and the primitive that makes it, in its
   thinking. The storyboard test showed this works (+10 points) but cost 2.6 s when written in the visible plan, so
   it goes into the thinking that already happens.
4. **A coverage check as each beat is written.** Every spoken computation must visibly arrive somewhere. Fixes go
   only into beats that haven't played yet; there is time, since Claude writes about 5 times faster than it speaks.
5. **Name the recurring structures as semantic operations the engine compiles:** slide-and-compute, map, reduce, cause,
   compare, scale, transform, part-of. Claude says what happens and the engine draws it consistently. Raw markup
   stays for everything else.
6. **Training only if step 1 shows the gap is learnable,** and then reinforcement learning (not fine-tuning on examples)
   on a narrow job, with a held-out judge to catch reward hacking.
