#include "qp.hpp"
#include <algorithm>
#include <cmath>

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

} // namespace place
