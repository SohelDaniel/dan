# D9. `path from=a to=b` between two things: fail-soft acceptance, a synonym, or a rejection?

The user's intuition: a path has a start and an end; between two boxes it should be a path. The language has the
concept as `link <id> arrow a -> b` (a relation, routed by the layout) and `send a -> b` (a copy travelling). Today
`path from= to=` is not accepted (and a `path` under a schema that cannot draw it vanishes silently: a separate
Tier-1 lint fix). Principle 7 (one way per thing) says no synonyms; principle 6 (fail soft) says a near-miss should
still do something sensible, visibly. Claude never wrote `path from=` (0 of 143 files). Decide: reject with a
helpful lint ("a path between things is a link: link w arrow a -> b"), or accept it as a link (readback canonical
form = link; a lint says so), or leave it. Consider also the mirror case the user raised: is a curve really a
"path", or should `fn=` curves be called something else (`curve`)? That is a rename with the same costs as D8; give
a view, not a project. Material: <scratchpad>/audit-8/notes.md (path between boxes), <scratchpad>/session-notes.md (1, the user's
comment in the plan: <plan file> Part A row 1). Axioms: 6
(one canonical form), position from structure (a path between things must be routed, never placed).
