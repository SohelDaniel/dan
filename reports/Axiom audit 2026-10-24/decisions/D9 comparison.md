# Final decisions, pair by pair

## D9. A path between two things → **B**
Both engineers independently chose the same design: fail-soft acceptance as the existing link, read back only as
`link`, with a `link:` lint naming the canonical line; no README/prompt change (one spelling taught).
B is the more correct implementation:
- B accepts the near-miss only when both ends are single-word ids; A would turn `from="Box 1"` into a link whose
  end has a space, which cannot read back (axiom 6).
- B leaves any stroke with its own geometry (x, y, x1..y2, slope, pts, fn, xt/yt/zt) a world shape; A checks only
  fn/xt, so a coordinate line that also carried from=/to= would silently become a link.
- B proved byte-identical --check and --readback output against the old binary on all 93 repo scenes and all 63
  real chat turns, and streamed the new forms 16/16 identically. A ran the standard suites only.
- Same view from both on the mirror question: keep the word `path` for curves.
Integration: apply dec-D9-B/patch.diff, re-run the regressions.
