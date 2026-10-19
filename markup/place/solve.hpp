// place: where bodies go. Relations (below, right-of, on, in, near …), regions and the schema's own
// proposal become linear constraints of different strengths, solved by Cassowary (kiwi, the solver
// behind Apple's Auto Layout): stage bounds are required, relations strong, the proposal medium and
// the previous position weak, so a small change in the scene makes a small change on screen. Then
// overlap removal (separate) clears whatever still collides, except overlaps that were asked for.
#pragma once
#include "types.hpp"

namespace place {
Placement solveBodies(const Scene &scene);
}
