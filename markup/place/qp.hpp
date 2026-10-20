// place: 1-D least squares under separation constraints, the core of VPSC (Dwyer, Marriott & Stuckey,
// "Fast Node Overlap Removal", GD 2005): things move as little as possible, heavy ones barely at all,
// and "this one stays at least g before that one" always holds. The flow layout spaces rows with it.
#pragma once
#include <vector>

namespace place {

// minimise Σ w_i (x_i − d_i)² subject to x_r − x_l ≥ g for each constraint
struct Sep { int l, r; double g; };
std::vector<double> solve1d(const std::vector<double> &desired, const std::vector<double> &weight, const std::vector<Sep> &cons);

} // namespace place
