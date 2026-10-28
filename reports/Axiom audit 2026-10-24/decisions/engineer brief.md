# Common brief for the decision engineers (read this first, then your decision's text)

You are one of two independent engineers given the same decision. You do not know who the other is and you must
not look for their work: never read any folder named `dec-D*` other than your own. At the end a reviewer compares
the two solutions and picks the more correct one, so your job is to be right, tested, and honest about limits.

## The system
A live tutor in the spirit of 3Blue1Brown: Claude writes a declarative markup called Dan (files `*.dan`); a C++20 +
raylib engine (`./tutor`) plays it on a stage window while a real voice (Inworld TTS) speaks the `Say:` lines and
the picture moves on the cued words. Claude never gives pixels; the engine owns layout, colour, timing. It must be
able to explain anything from any field. Read `docs/HANDOFF.md` first (the map: code, commands, gotchas, how we
work), then README.md's section "## The language" (the reference; it is part of Claude's prompt) and "## Axioms,
enforced", then `chat/prompt.md` (the teaching protocol). The axioms are the law; every change must keep them and
say which ones it touches:
1 Identity (no delete; re-mark updates in place; a morph from a wrong value animates from the real one) ·
2 Structure (shared baselines and scales fixed by a prescan; they never jump) · 3 Zero (nothing without a
meaning; unrelated things never share a colour; shared ink only when related) · 4 Continuity (every change an
eased morph) · 5 Bounded focus (focus lights one thing for one sentence) · 6 Decodability (--readback prints the
scene in the same vocabulary; --check re-parses it to the same scene) · and the derived rule: position comes from
structure (no placement words; links make columns, groups lanes, order is order).

## Where you work
You are in YOUR OWN git worktree of the repo (confirm with `git rev-parse --show-toplevel`; it is not
<repo>). Work only there. Never edit, build in, or commit to
the main repo. Do not commit in your worktree either: leave your changes in the working tree and export them.
Before building: `ln -s <repo>/tools/tex/node_modules tools/tex/node_modules`
(MathJax for formulas; otherwise --check reports TeX errors). Then `make` (first build 1-3 min), `make test`.
The user's data (chat/sessions, chat/memory, chat/library, chat/settings.json) lives only in the main repo: you may
READ `<repo>/chat/sessions/*/turns/*.txt|*.dan` to see what
Claude really writes; never write there. Never open a visible window: use `--snapshot`, `--layout-report`,
`--check`, `--readback`. Keep scratch files (test scenes, frames) in YOUR_DIR (given below), not in the worktree.

## How to work (software engineering, as this project does it; HANDOFF §5)
1. Understand before touching: read the auditors' material named in your decision (their notes, reproducers,
   proposed fix and tests) and the code paths involved. Reproduce the problem yourself first.
2. Research before designing: the project's own reports/ and research_notes/, the code's comments, and the real
   chats. Compare at least two designs; say why you chose one (simplicity, locality, no new syntax unless the
   language genuinely lacks the concept, decodability, what Claude would have to learn).
3. Only when stuck on a design question: look outside software for a solved analogue (cartography, typesetting,
   film editing, music notation, perception research, control theory, accounting, architecture, biology...). Say
   what you borrowed and why it transfers. Do not do this for its own sake.
4. Build small: the minimal change that solves the decision, in the project's style (short comments that say why,
   small named functions, no dead code, match the surrounding code). Add the test FIRST and watch it fail, then make
   it pass. A test that fails on the old engine is mandatory for any behaviour change.
5. Prove nothing broke: `make test`, `make check` (incl. `node tests/toolbox.mjs`), `make golden`, `make append`.
   Golden "WORSE" or append "DIFFERS" must be explained line by line; re-baseline (`node tests/golden.mjs --save`)
   only if the change is the intent, and say so. Known pre-existing: `make append` reports space-downhill and
   space-saddle DIFFERS (a stale lint comment; model identical). Look at frames (`--snapshot`, view the PNG): the
   checker's score is not the user's judgement.
6. If the language changes (a new key, verb, or meaning): update README's language section, chat/prompt.md and/or
   chat/toolbox.md in the same change, keep the toolbox test green, and show one worked example before/after.
7. Report plainly: what works, what you measured (numbers with their noise), what you did not do, what could go
   wrong. Don't oversell.

## Deliverables (all under YOUR_DIR; the reviewer reads only these)
- `notes.md` with these headings, in order: **Problem** (reproduced, with the command and what happened) ·
  **Designs considered** (2+, with the reasons) · **Chosen design** (what changes, which axioms it touches and why
  each still holds, what Claude's markup looks like before/after if the language changes) · **Outside the field**
  (only if used) · **Implementation** (files, functions, roughly how many lines) · **Tests** (names; the failing
  output on the old engine; the passing output after) · **Regressions** (the exact output tails of make test /
  check / golden / append / toolbox, with every difference explained) · **Frames** (what you looked at, what you
  saw) · **Limits and risks** (what it does not solve; what could regress; migration for existing scripts/chats) ·
  **Verdict** (ship / ship with caveats / do not ship, and why).
- `patch.diff`: `git diff` of your worktree (plus `git diff --stat`), and `git status --short`.
- Any test scenes and frames you refer to.
Write notes.md early and append as you go; if you are cut off, the notes are what survives.
