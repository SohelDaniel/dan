# D1. A thing that rides a curve: `mark q dot on=f x=2` (y derived), `on=S x= y=` (z derived), `on=orbit t=`

Today a point on a curve repeats the function (`x={2+h} y={f(2+h)}`) and walks by morphing a hidden param; the
relation "q is on f" exists only in the author's head. Worse, `on=f` on a dot is accepted today and misplaces it
(the generic "seated on its top edge" attachment for arrays and labels; the dot lands on the curve's bounding box).
The user proposed the rider; the position-rule auditor wrote a full spec with nine tests. Material:
/private/tmp/claude-501/-Users-soheldanial-code-server-testing-visulaizations-tutor/37e07ea3-2a08-44c3-b78a-4c2380bf6126/scratchpad/audit-7/notes.md ("Fix 1" and verdicts about on=), /private/tmp/claude-501/-Users-soheldanial-code-server-testing-visulaizations-tutor/37e07ea3-2a08-44c3-b78a-4c2380bf6126/scratchpad/session-notes.md (finding 7), /private/tmp/claude-501/-Users-soheldanial-code-server-testing-visulaizations-tutor/37e07ea3-2a08-44c3-b78a-4c2380bf6126/scratchpad/audit-6/notes.md (the
on= round trip). Scope: which kinds can ride (dot, label, a vector's tail), what can be ridden (fn path, line,
parametric path with t=, solution, surface), the free coordinate the author gives, no derived property stored (the
readback keeps `on=f x=2`), `{q.y}`/`\val{}` read the derived value (Engine::member), `morph q.x` keeps it on
the curve, trails and camera follow work, the checker allows the shared ink (a declared relation), a lint for kinds
that cannot ride and for a missing free coordinate; the shadow engine (values spoken ahead) must agree. Afterwards:
rewrite chat/examples/derivative.dan and the 3D toolbox move with on=, and the README/prompt text. Axioms to defend:
3 (shared ink by relation), 6 (decodable by construction), position from structure (y from the relation, not typed
twice); 1 and 4 untouched.
