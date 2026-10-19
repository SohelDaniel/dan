#include "viz.hpp"
#include <algorithm>
#include <deque>
#include <unordered_map>

namespace viz {

// ================================================================ camera
static const double kRho = 1.4142135623730951, kRho2 = 2, kRho4 = 4;

void ZoomPath::set(V2 from, double wFrom, V2 to, double wTo, double aspect) {
  c0 = from; c1 = to; w0 = wFrom; w1 = wTo;
  V2 dv = c1 - c0;
  d = norm(dv);
  double ratio = std::max(w0, w1) / std::max(1e-12, std::min(w0, w1));
  linear = ratio < 1.5 && d < 0.3 * std::max(w0, w1);
  auto inView = [&](V2 p, V2 c, double w) { return std::fabs(p.x - c.x) <= w / 2 && std::fabs(p.y - c.y) <= w * aspect / 2; };
  anchored = false;
  if (!linear && w1 < w0 && inView(c1, c0, w0)) { anchored = true; anchor = c1; }      // zoom in on what we see
  else if (!linear && w1 > w0 && inView(c0, c1, w1)) { anchored = true; anchor = c0; } // step back from here
  if (d < 1e-9 * std::max(w0, w1)) {
    d = 0;
    S = std::log(w1 / w0) / kRho;
    return;
  }
  double b0 = (w1 * w1 - w0 * w0 + kRho4 * d * d) / (2 * w0 * kRho2 * d);
  double b1 = (w1 * w1 - w0 * w0 - kRho4 * d * d) / (2 * w1 * kRho2 * d);
  r0 = std::log(std::sqrt(b0 * b0 + 1) - b0);
  double r1 = std::log(std::sqrt(b1 * b1 + 1) - b1);
  S = (r1 - r0) / kRho;
  if (linear) S = std::log(ratio) / kRho + d / std::max(w0, w1); // for the duration only
}

void ZoomPath::at(double t, V2 &c, double &w) const {
  t = std::clamp(t, 0.0, 1.0);
  if (anchored) {
    // the anchor's place on screen, as a fraction of the view width, moves in a straight line
    w = w0 * std::exp(std::log(w1 / w0) * t);
    V2 s0 = (anchor - c0) * (1 / w0), s1 = (anchor - c1) * (1 / w1);
    V2 s = s0 + (s1 - s0) * t;
    c = anchor - s * w;
    if (t >= 1) { c = c1; w = w1; }
    return;
  }
  if (linear || d == 0) {
    c = c0 + (c1 - c0) * t;
    w = w0 * std::exp(std::log(w1 / w0) * t);
    return;
  }
  double s = t * S;
  double u = w0 / (kRho2 * d) * (std::cosh(r0) * std::tanh(kRho * s + r0) - std::sinh(r0));
  c = c0 + (c1 - c0) * (u / d);
  w = w0 * std::cosh(r0) / std::cosh(kRho * s + r0);
  if (t >= 1) { c = c1; w = w1; }
}

double ZoomPath::seconds() const { return std::clamp(std::fabs(S) / 0.9, 0.6, 2.0); }

// ================================================================ ticks
double niceStepFloor(double raw) {
  if (!(raw > 0)) return 1;
  double mag = std::pow(10.0, std::floor(std::log10(raw)));
  double r = raw / mag;
  return (r >= 5 ? 5 : r >= 2 ? 2 : 1) * mag;
}
double niceStepAbove(double s) {
  double mag = std::pow(10.0, std::floor(std::log10(s) + 1e-9));
  double r = std::round(s / mag);
  return (r < 1.5 ? 2 : r < 3.5 ? 5 : 10) * mag;
}

// Steps come from the chain ... 10, 5, 1, 0.5, 0.1 ...: each divides the one above it, so as the view
// zooms, lines are only ever added (fading in), never moved or removed. (1-2-5 would pop: 0.5 is not a
// multiple of 0.2.)
static double chainFloor(double raw) {
  double mag = std::pow(10.0, std::floor(std::log10(raw)));
  return (raw / mag >= 5 ? 5 : 1) * mag;
}
static double chainFiner(double s) {
  double mag = std::pow(10.0, std::floor(std::log10(s) + 1e-9));
  return std::round(s / mag) >= 5 ? mag : mag / 2;
}
static double chainCoarser(double s) {
  double mag = std::pow(10.0, std::floor(std::log10(s) + 1e-9));
  return std::round(s / mag) >= 5 ? mag * 10 : mag * 5;
}

void tickLevels(double range, double px, double minPx, TickLevel &major, TickLevel &minor) {
  if (!(range > 0) || !(px > 0)) { major = {1, 1, 1}; minor = {1, 0, 0}; return; }
  double ppu = px / range;
  double s = chainFloor(minPx / ppu); // largest chain step <= minPx: one finer than we want
  while (s * ppu < minPx * 0.999) s = chainCoarser(s);
  major = {s, 1, 1};
  double finer = chainFiner(s);
  double sp = finer * ppu;
  minor.step = finer;
  minor.alpha = (float)smoothstep(minPx * 0.5, minPx, sp);
  minor.labelAlpha = (float)smoothstep(minPx * 0.75, minPx, sp);
}

// ================================================================ contours
namespace {
// edges of cell (i, j): 0 bottom, 1 right, 2 top, 3 left; ids are shared with the neighbouring cell
inline long edgeId(int i, int j, int e, int nx) {
  switch (e) {
  case 0: return ((long)j * nx + i) * 2;           // horizontal edge at row j
  case 2: return ((long)(j + 1) * nx + i) * 2;     // horizontal edge at row j+1
  case 3: return ((long)j * nx + i) * 2 + 1;       // vertical edge at column i
  default: return ((long)j * nx + i + 1) * 2 + 1;  // vertical edge at column i+1
  }
}
} // namespace

std::vector<std::vector<V2>> contours(const std::vector<float> &values, int nx, int ny, double level) {
  std::vector<std::vector<V2>> out;
  if (nx < 2 || ny < 2 || (int)values.size() < nx * ny) return out;
  auto val = [&](int i, int j) {
    float v = values[(size_t)j * nx + i];
    return std::isfinite(v) ? (double)v : -1e300;
  };
  struct Seg { long a, b; V2 pa, pb; };
  std::vector<Seg> segs;
  auto point = [&](int i, int j, int e) -> V2 {
    int ai = i, aj = j, bi = i, bj = j;
    switch (e) {
    case 0: bi = i + 1; break;
    case 1: ai = i + 1; bi = i + 1; bj = j + 1; break;
    case 2: aj = j + 1; bi = i + 1; bj = j + 1; break;
    default: bj = j + 1; break;
    }
    double va = val(ai, aj), vb = val(bi, bj);
    double t = (va <= -1e299 || vb <= -1e299) ? 0.5 : (level - va) / (vb - va);
    t = std::clamp(t, 0.0, 1.0);
    return {ai + (bi - ai) * t, aj + (bj - aj) * t};
  };
  auto add = [&](int i, int j, int e1, int e2) {
    segs.push_back({edgeId(i, j, e1, nx), edgeId(i, j, e2, nx), point(i, j, e1), point(i, j, e2)});
  };
  for (int j = 0; j + 1 < ny; j++)
    for (int i = 0; i + 1 < nx; i++) {
      double a = val(i, j), b = val(i + 1, j), c = val(i + 1, j + 1), dd = val(i, j + 1);
      int k = (a >= level) | (b >= level) << 1 | (c >= level) << 2 | (dd >= level) << 3;
      switch (k) {
      case 0: case 15: break;
      case 1: case 14: add(i, j, 3, 0); break;
      case 2: case 13: add(i, j, 0, 1); break;
      case 3: case 12: add(i, j, 3, 1); break;
      case 4: case 11: add(i, j, 1, 2); break;
      case 6: case 9: add(i, j, 0, 2); break;
      case 7: case 8: add(i, j, 3, 2); break;
      case 5: case 10: {
        bool centreIn = (a + b + c + dd) / 4 >= level;
        bool aIn = k == 5;
        // centre on a's side: a and c are joined through the middle, the line cuts off b and d
        if (centreIn == aIn) { add(i, j, 0, 1); add(i, j, 2, 3); }
        else { add(i, j, 3, 0); add(i, j, 1, 2); }
        break;
      }
      }
    }
  // stitch segments that share an edge into polylines
  std::unordered_map<long, std::vector<int>> byEdge;
  for (int s = 0; s < (int)segs.size(); s++) { byEdge[segs[s].a].push_back(s); byEdge[segs[s].b].push_back(s); }
  std::vector<char> used(segs.size(), 0);
  auto other = [&](long e, int s) {
    for (int t : byEdge[e]) if (t != s && !used[t]) return t;
    return -1;
  };
  for (int s0 = 0; s0 < (int)segs.size(); s0++) {
    if (used[s0]) continue;
    used[s0] = 1;
    std::deque<V2> line{segs[s0].pa, segs[s0].pb};
    // grow forward from b, then backward from a
    for (int dir = 0; dir < 2; dir++) {
      long e = dir == 0 ? segs[s0].b : segs[s0].a;
      int cur = s0;
      for (;;) {
        int nxt = other(e, cur);
        if (nxt < 0) break;
        used[nxt] = 1;
        const Seg &g = segs[nxt];
        long far = g.a == e ? g.b : g.a;
        V2 p = g.a == e ? g.pb : g.pa;
        if (dir == 0) line.push_back(p); else line.push_front(p);
        e = far;
        cur = nxt;
      }
    }
    out.emplace_back(line.begin(), line.end());
  }
  return out;
}

std::vector<double> niceLevels(double lo, double hi, int n) {
  std::vector<double> out;
  if (!(hi > lo) || n < 1) return out;
  double raw = (hi - lo) / n;
  double s = niceStepFloor(raw);
  if ((hi - lo) / s > n * 1.6) s = niceStepAbove(s);
  for (double v = std::ceil(lo / s - 1e-9) * s; v < hi - 1e-9 * s; v += s)
    if (v > lo + 1e-9 * s) out.push_back(std::fabs(v) < 1e-12 * s ? 0 : v);
  return out;
}

std::vector<double> logLevels(double lo, double hi, int n) {
  std::vector<double> out;
  if (!(hi > lo) || n < 1) return out;
  double base = std::max(lo, 0.0), span = hi - base;
  double first = niceStepFloor(span / std::pow(2.0, n - 1)); // doubling from a round number: 0.1, 0.2, 0.4 ...
  for (int k = 0; k < n; k++) {
    double v = base + first * std::pow(2.0, k);
    if (v < hi) out.push_back(v);
  }
  return out;
}

// ================================================================ fields
V2 rk4(const Field &f, V2 p, double h) {
  V2 k1 = f(p), k2 = f(p + k1 * (h / 2)), k3 = f(p + k2 * (h / 2)), k4 = f(p + k3 * h);
  return p + (k1 + k2 * 2 + k3 * 2 + k4) * (h / 6);
}

namespace {
struct HashGrid {
  double cell, x0, y0;
  std::unordered_map<long long, std::vector<V2>> m;
  long long key(int i, int j) const { return ((long long)i << 32) ^ (unsigned)j; }
  void add(V2 p) { m[key((int)std::floor((p.x - x0) / cell), (int)std::floor((p.y - y0) / cell))].push_back(p); }
  bool near(V2 p, double r) const {
    int ci = (int)std::floor((p.x - x0) / cell), cj = (int)std::floor((p.y - y0) / cell);
    for (int dj = -1; dj <= 1; dj++)
      for (int di = -1; di <= 1; di++) {
        auto it = m.find(key(ci + di, cj + dj));
        if (it == m.end()) continue;
        for (V2 q : it->second) if (norm(q - p) < r) return true;
      }
    return false;
  }
};
} // namespace

std::vector<std::vector<V2>> streamlines(const Field &f, const StreamOpts &o) {
  std::vector<std::vector<V2>> lines;
  HashGrid grid{o.dsep, o.x0, o.y0, {}};
  double dtest = o.dtest * o.dsep, minLen = o.minLength > 0 ? o.minLength : 2 * o.dsep;
  auto inside = [&](V2 p) { return p.x >= o.x0 && p.x <= o.x1 && p.y >= o.y0 && p.y <= o.y1; };
  // unit-speed direction field: lines advance by arc length, so spacing is even along them too
  auto dirField = [&](double sign) {
    return [&f, sign](V2 p) -> V2 {
      V2 v = f(p);
      double n = norm(v);
      if (!(n > 1e-12) || !std::isfinite(n)) return {0, 0};
      return v * (sign / n);
    };
  };
  Field fwd = dirField(1), bwd = dirField(-1);
  auto grow = [&](V2 seed, const Field &g) {
    std::vector<V2> pts;
    V2 p = seed;
    double len = 0;
    for (int s = 0; s < o.maxSteps; s++) {
      V2 v = g(p);
      if (v.x == 0 && v.y == 0) break; // critical point
      V2 q = rk4(g, p, o.h);
      if (!inside(q) || !std::isfinite(q.x) || !std::isfinite(q.y)) break;
      if (grid.near(q, dtest)) break;
      len += norm(q - p);
      if (len > 4 * o.h && norm(q - seed) < o.h * 0.75) { pts.push_back(seed); break; } // closed orbit
      // a line must not run into its own earlier part either
      bool selfHit = false;
      for (size_t k = 0; k + 8 < pts.size() && !selfHit; k++) if (norm(pts[k] - q) < dtest) selfHit = true;
      if (selfHit) break;
      pts.push_back(q);
      p = q;
    }
    return pts;
  };
  std::deque<V2> seeds;
  seeds.push_back({(o.x0 + o.x1) / 2, (o.y0 + o.y1) / 2});
  // a coarse lattice backs up the queue, so regions the first lines never reach still get lines
  std::vector<V2> lattice;
  for (double y = o.y0 + o.dsep / 2; y < o.y1; y += o.dsep * 2)
    for (double x = o.x0 + o.dsep / 2; x < o.x1; x += o.dsep * 2) lattice.push_back({x, y});
  size_t li = 0;
  while (!seeds.empty() || li < lattice.size()) {
    V2 s;
    if (!seeds.empty()) { s = seeds.front(); seeds.pop_front(); }
    else s = lattice[li++];
    if (!inside(s) || grid.near(s, o.dsep)) continue;
    std::vector<V2> back = grow(s, bwd), front = grow(s, fwd);
    std::vector<V2> line(back.rbegin(), back.rend());
    line.push_back(s);
    line.insert(line.end(), front.begin(), front.end());
    double len = 0;
    for (size_t k = 1; k < line.size(); k++) len += norm(line[k] - line[k - 1]);
    if (len < minLen) continue;
    for (V2 p : line) grid.add(p);
    // new seeds one separation to either side
    double acc = 0;
    for (size_t k = 1; k < line.size(); k++) {
      acc += norm(line[k] - line[k - 1]);
      if (acc < o.dsep * 0.5) continue;
      acc = 0;
      V2 t = line[k] - line[k - 1];
      double n = norm(t);
      if (n <= 0) continue;
      V2 nrm{-t.y / n, t.x / n};
      seeds.push_back(line[k] + nrm * o.dsep);
      seeds.push_back(line[k] - nrm * o.dsep);
    }
    lines.push_back(std::move(line));
  }
  return lines;
}

// ================================================================ colour
namespace {
struct Stop { double t; float r, g, b; };
RGB ramp(const Stop *s, int n, double t) {
  if (!std::isfinite(t)) t = 0;
  t = std::clamp(t, 0.0, 1.0);
  for (int i = 1; i < n; i++)
    if (t <= s[i].t) {
      double u = (t - s[i - 1].t) / (s[i].t - s[i - 1].t);
      return {(float)(s[i - 1].r + (s[i].r - s[i - 1].r) * u), (float)(s[i - 1].g + (s[i].g - s[i - 1].g) * u),
              (float)(s[i - 1].b + (s[i].b - s[i - 1].b) * u)};
    }
  return {s[n - 1].r, s[n - 1].g, s[n - 1].b};
}
// matplotlib viridis, sampled every 32 of 256 entries
const Stop kViridis[] = {{0.000, 0.267f, 0.005f, 0.329f}, {0.125, 0.282f, 0.141f, 0.458f}, {0.250, 0.254f, 0.265f, 0.530f},
                         {0.375, 0.207f, 0.372f, 0.553f}, {0.500, 0.164f, 0.471f, 0.558f}, {0.625, 0.128f, 0.567f, 0.551f},
                         {0.750, 0.135f, 0.659f, 0.518f}, {0.875, 0.267f, 0.749f, 0.441f}, {0.940, 0.560f, 0.835f, 0.270f},
                         {1.000, 0.993f, 0.906f, 0.144f}};
// dark-centred diverging (in the spirit of Crameri's berlin): blue below, near-black at 0, warm above
const Stop kDiverging[] = {{0.00, 0.62f, 0.73f, 1.00f}, {0.25, 0.22f, 0.45f, 0.72f}, {0.50, 0.08f, 0.08f, 0.09f},
                           {0.75, 0.62f, 0.25f, 0.13f}, {1.00, 1.00f, 0.70f, 0.55f}};
} // namespace

RGB viridis(double t) { return ramp(kViridis, 10, t); }
RGB diverging(double t) { return ramp(kDiverging, 5, t); }

// ================================================================ trails
std::vector<double> trailTimes(double t0, double t1, double h, const std::vector<double> &breaks) {
  std::vector<double> ts;
  if (!(t1 > t0) || !(h > 0)) { ts.push_back(t1); return ts; }
  ts.push_back(t0);
  for (double k = std::ceil(t0 / h + 1e-9); k * h < t1 - 1e-9; k += 1) ts.push_back(k * h);
  for (double b : breaks) if (b > t0 && b < t1) ts.push_back(b);
  ts.push_back(t1);
  std::sort(ts.begin(), ts.end());
  ts.erase(std::unique(ts.begin(), ts.end(), [](double a, double b) { return std::fabs(a - b) < 1e-9; }), ts.end());
  return ts;
}

} // namespace viz
