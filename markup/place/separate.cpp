#include "separate.hpp"
#include <algorithm>
#include <cmath>
#include <set>

namespace place {

// The weighted least-squares projection onto the constraints, by Dykstra's alternating projections:
// project onto each half-space x_r − x_l ≥ g in turn, carrying a per-constraint correction, which
// (unlike plain cyclic projection) converges to the *closest* feasible point, not just a feasible one.
// The block-merging pass of VPSC ("satisfy") would find a feasible point; this finds the optimum.
std::vector<double> solve1d(const std::vector<double> &d, const std::vector<double> &w, const std::vector<Sep> &cons) {
  std::vector<double> x = d;
  std::vector<double> corr(2 * cons.size(), 0); // per constraint: the correction applied to l and to r
  for (int sweep = 0; sweep < 400; sweep++) { // a hundredth of a pixel is plenty
    double change = 0;
    for (size_t c = 0; c < cons.size(); c++) {
      const Sep &s = cons[c];
      double yl = x[s.l] + corr[2 * c], yr = x[s.r] + corr[2 * c + 1];
      double il = 1 / w[s.l], ir = 1 / w[s.r];          // inverse weights: light things move more
      double viol = s.g - (yr - yl);
      double nl = yl, nr = yr;
      if (viol > 0) { double k = viol / (il + ir); nl = yl - k * il; nr = yr + k * ir; }
      change = std::max({change, std::fabs(nl - x[s.l]), std::fabs(nr - x[s.r])});
      corr[2 * c] = yl - nl;
      corr[2 * c + 1] = yr - nr;
      x[s.l] = nl;
      x[s.r] = nr;
    }
    if (change < 1e-2) break;
  }
  // a last pass of plain projection makes every constraint hold exactly (Dykstra converges in the limit)
  for (int sweep = 0; sweep < 50; sweep++) {
    bool any = false;
    for (const Sep &s : cons) {
      double viol = s.g - (x[s.r] - x[s.l]);
      if (viol > 1e-7) {
        double il = 1 / w[s.l], ir = 1 / w[s.r], k = viol / (il + ir);
        x[s.l] -= k * il; x[s.r] += k * ir; any = true;
      }
    }
    if (!any) break;
  }
  return x;
}

std::vector<Vec> separate(const SeparateInput &in, int *leftOver) {
  size_t n = in.centre.size();
  std::vector<Vec> c = in.centre;
  std::set<std::pair<int, int>> allowed;
  for (auto [a, b] : in.allowed) allowed.insert({std::min(a, b), std::max(a, b)});
  auto overlapXY = [&](size_t i, size_t j, float &ox, float &oy) {
    ox = (in.size[i].x + in.size[j].x) / 2 + in.gap - std::fabs(c[i].x - c[j].x);
    oy = (in.size[i].y + in.size[j].y) / 2 + in.gap - std::fabs(c[i].y - c[j].y);
    return ox > 0.01f && oy > 0.01f;
  };
  auto clampAll = [&] {
    if (in.bounds.empty()) return;
    for (size_t i = 0; i < n; i++) {
      float hw = std::min(in.size[i].x / 2, in.bounds.w / 2), hh = std::min(in.size[i].y / 2, in.bounds.h / 2);
      c[i].x = std::clamp(c[i].x, in.bounds.x + hw, in.bounds.right() - hw);
      c[i].y = std::clamp(c[i].y, in.bounds.y + hh, in.bounds.bottom() - hh);
    }
  };
  clampAll();
  // Each pass: pairs that overlap are pushed apart along their cheaper axis, and every pair that is
  // already apart along that axis (while overlapping along the other) is held apart, so resolving one
  // collision never creates another (the neighbour constraints of VPSC).
  // pairs still overlapping after a round try the other axis next round (a gap too narrow sideways
  // may be easy to clear vertically)
  std::set<std::pair<int, int>> flipped;
  auto pass = [&](bool alongX) {
    std::vector<Sep> cons;
    // how far anything can move this pass: pairs further apart than that can't be pushed together
    float reach = 0;
    for (size_t i = 0; i < n; i++)
      for (size_t j = i + 1; j < n; j++) {
        float ox, oy;
        if (!allowed.count({(int)i, (int)j}) && overlapXY(i, j, ox, oy)) reach = std::max(reach, alongX ? ox : oy);
      }
    reach = 2 * reach + in.gap + 4;
    for (size_t i = 0; i < n; i++)
      for (size_t j = i + 1; j < n; j++) {
        if (allowed.count({(int)i, (int)j})) continue;
        float ox, oy;
        overlapXY(i, j, ox, oy);
        float along = alongX ? ox : oy, across = alongX ? oy : ox;
        if (across <= 0.01f) continue;                  // apart the other way: no conflict along this axis
        bool overlapping = along > 0.01f;
        if (!overlapping && -along > reach) continue; // far apart along this axis: nothing can bring them together
        bool preferX = ox <= oy;
        if (flipped.count({(int)i, (int)j})) preferX = !preferX;
        if (overlapping && preferX != alongX) continue; // fixed along the other axis
        float ci = alongX ? c[i].x : c[i].y, cj = alongX ? c[j].x : c[j].y;
        bool iFirst = ci < cj || (ci == cj && i < j);
        int l = iFirst ? (int)i : (int)j, r = iFirst ? (int)j : (int)i;
        double need = alongX ? (in.size[i].x + in.size[j].x) / 2.0 + in.gap : (in.size[i].y + in.size[j].y) / 2.0 + in.gap;
        cons.push_back({l, r, need});
      }
    for (auto &[a, b, g] : alongX ? in.keepX : in.keepY) cons.push_back({a, b, g}); // relations hold throughout
    if (cons.empty()) return;
    std::vector<double> d(n);
    for (size_t i = 0; i < n; i++) d[i] = alongX ? c[i].x : c[i].y;
    auto nx = solve1d(d, in.weight, cons);
    for (size_t i = 0; i < n; i++) (alongX ? c[i].x : c[i].y) = (float)nx[i];
  };
  for (int round = 0; round < 8; round++) {
    bool any = false;
    for (size_t i = 0; i < n && !any; i++)
      for (size_t j = i + 1; j < n && !any; j++) {
        float ox, oy;
        if (!allowed.count({(int)i, (int)j}) && overlapXY(i, j, ox, oy)) any = true;
      }
    if (!any) break;
    pass(true);
    pass(false);
    clampAll();
    for (size_t i = 0; i < n; i++)
      for (size_t j = i + 1; j < n; j++) {
        float ox, oy;
        if (!allowed.count({(int)i, (int)j}) && overlapXY(i, j, ox, oy)) {
          auto key = std::make_pair((int)i, (int)j);
          if (flipped.count(key)) flipped.erase(key); else flipped.insert(key);
        }
      }
  }
  // compaction: the order-keeping passes can push further than needed; let each thing slide back
  // toward where it wanted to be, as far as it can without touching anything
  auto clear = [&](size_t i, Vec at) {
    for (size_t j = 0; j < n; j++) {
      if (j == i || allowed.count({(int)std::min(i, j), (int)std::max(i, j)})) continue;
      float ox = (in.size[i].x + in.size[j].x) / 2 + in.gap - std::fabs(at.x - c[j].x);
      float oy = (in.size[i].y + in.size[j].y) / 2 + in.gap - std::fabs(at.y - c[j].y);
      if (ox > 0.01f && oy > 0.01f) return false;
    }
    if (!in.bounds.empty() && !contains(in.bounds, Rect::around(at, in.size[i]), 0.5f)) return false;
    return true;
  };
  bool clean = true;
  for (size_t i = 0; i < n && clean; i++)
    for (size_t j = i + 1; j < n && clean; j++) {
      float ox, oy;
      if (!allowed.count({(int)i, (int)j}) && overlapXY(i, j, ox, oy)) clean = false;
    }
  if (clean)
    for (int round = 0; round < 3; round++) {
      std::vector<size_t> order(n);
      for (size_t i = 0; i < n; i++) order[i] = i;
      std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        return length(c[a] - in.centre[a]) * in.weight[a] > length(c[b] - in.centre[b]) * in.weight[b];
      });
      for (size_t i : order) {
        Vec from = c[i], to = in.centre[i];
        if (length(to - from) < 0.5f) continue;
        for (float t : {1.f, 0.8f, 0.6f, 0.4f, 0.25f, 0.12f}) {
          Vec at = from + (to - from) * t;
          if (clear(i, at)) { c[i] = at; break; }
        }
      }
    }
  if (leftOver) {
    int count = 0;
    for (size_t i = 0; i < n; i++)
      for (size_t j = i + 1; j < n; j++) {
        float ox, oy;
        if (!allowed.count({(int)i, (int)j}) && overlapXY(i, j, ox, oy) && ox > in.gap + 0.5f && oy > in.gap + 0.5f) count++;
      }
    *leftOver = count;
  }
  return c;
}

} // namespace place
