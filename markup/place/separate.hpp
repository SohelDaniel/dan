// place: overlap removal with minimal movement (VPSC: Dwyer, Marriott & Stuckey, "Fast Node Overlap
// Removal", GD 2005). Each overlapping pair is pushed apart along the axis that needs less movement;
// positions solve a weighted least-squares problem under "this one stays left of that one" constraints,
// so things move as little as possible, heavy (fixed) things barely at all, and order is kept.
#pragma once
#include "geom.hpp"
#include <tuple>
#include <utility>
#include <vector>

namespace place {

struct SeparateInput {
  std::vector<Vec> centre, size;
  std::vector<double> weight;                  // how much each resists moving (fixed: 1e6)
  std::vector<std::pair<int, int>> allowed;    // pairs that may overlap (intended)
  float gap = 12;
  Rect bounds{0, 0, 0, 0};                     // keep inside, if non-empty
  // relations that must survive (below / right-of …): extra constraints along x and along y,
  // as (first, second, min distance between centres)
  std::vector<std::tuple<int, int, double>> keepX, keepY;
};

// returns the new centres; overlapping pairs left (if the space is simply too small) go in *left
std::vector<Vec> separate(const SeparateInput &in, int *left = nullptr);

// 1-D core, exposed for tests: minimise Σ w_i (x_i − d_i)² subject to x_r − x_l ≥ g for each constraint
struct Sep { int l, r; double g; };
std::vector<double> solve1d(const std::vector<double> &desired, const std::vector<double> &weight, const std::vector<Sep> &cons);

} // namespace place
