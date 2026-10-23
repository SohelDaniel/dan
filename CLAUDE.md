# The visual tutor

The user opens this with `tutor claude`: the tutor runs in the background and they talk to you about what they
saw in it. Expect "this looked wrong", a screenshot, or a feature request.

Start every session by reading the map below. It holds the axioms, the code map, how we work, the gotchas and the
open list.

@docs/HANDOFF.md

## Working on a complaint

1. Reproduce it. Find the chat (`chat/sessions/`, newest first), replay it (`tools/audit-sessions.mjs --last 1
   --frames DIR`) or snapshot the scene, and look at the frame yourself.
2. Find the cause before touching code. For anything beyond a small fix, research (other fields too), propose a
   fix, say which axioms it touches, and wait for the user's yes.
3. Fix with a test that fails on the old engine. Run the regressions (`make test check golden append`, plus
   `node tests/toolbox.mjs` after prompt or language changes).
4. `tutor restart`, so the user's next try runs the new code. Tell them what changed and how to see it.
5. Commit.

## Ending a session: update the handoff

When a piece of work is committed, and when the user says they're done (or the conversation is winding down),
update the handoff before you stop:
- `docs/HANDOFF.md`: fix any part of the map the work changed (code, commands, gotchas), move finished items out
  of "Open", add new ones, and set "Last updated".
- `docs/history.md`: add a dated entry at the end: what the user asked, what was found, what was built, what was
  measured (numbers with their noise), what was tried and dropped, and why.
- Commit both with the work.

Don't wait for a goodbye: sessions can end without one, so update the handoff with each commit.
