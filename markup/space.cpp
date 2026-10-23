// The space schema: a 3D world, z up (docs/plan-3d.md, research_notes/3D/explainers.md).
//   schema space x=[-3,3] y=[-3,3] z=[-2,2]       ranges (or found from what is in it); grid=false, axes=false, units=box
// Every point reaches the screen through one mapping (viz::Space::project), so names, the label placer, the
// overlap checker, pointing and notes all work on projected positions exactly as on a plane. A thing in space is
// described once, in data units (spaceShape): its strokes, triangles and points. Layout projects it to find where
// it is on screen; drawing projects it again and paints every piece of every thing back to front (painter's order,
// as ManimCE), surfaces a little translucent so what is behind them still shows. Text is never in the 3D plane:
// names, numbers and formulas are upright screen text with a dark outline (3Blue1Brown's backstroke).
//
// The camera is said in words: camera view=top|front|side|default, orbit=<degrees>, tilt=<degrees>, spin, spin=off,
// zoom= on=, reset. The engine owns the numbers: 3Blue1Brown's tilt, a slow spin of 1.5 degrees a second (the kinetic
// depth effect: shape reads from motion), turns of about 15 degrees a second.
#include "engine.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace mk {

using namespace rm;
using viz::V3;

static constexpr float kSpinRate = 1.5f * (float)viz::kDeg; // radians per second
static constexpr float kTurnRate = 15 * (float)viz::kDeg;   // a big turn, radians per second

static bool rangeOf(const Value *v, double &lo, double &hi) {
  if (!v || v->type != Value::LIST || v->list.size() < 2) return false;
  lo = std::atof(v->list[0].c_str());
  hi = std::atof(v->list[1].c_str());
  return hi > lo;
}

static bool flagOn(const Value *v) {
  if (!v) return false;
  if (v->type == Value::NUM) return v->num != 0;
  return v->type == Value::STR && v->str != "false" && v->str != "off" && v->str != "no" && v->str != "0";
}

// text over a busy 3D picture: a dark outline first (3Blue1Brown's set_backstroke), then the text
void textOutlined(const std::string &s, Vector2 at, float size, Color c, float ax, float ay) {
  Color bg = fade(col::bg, c.a / 255.f * 0.85f);
  for (Vector2 d : {Vector2{-1.2f, 0}, Vector2{1.2f, 0}, Vector2{0, -1.2f}, Vector2{0, 1.2f}})
    text(s, at + d, size, bg, ax, ay);
  text(s, at, size, c, ax, ay);
}

// ---------------------------------------------------------------- geometry, in data units

// the floor the drop lines fall to: the xy-plane if it is in the box, else the box's bottom
static double floorZ(const viz::Space &p) { return std::clamp(0.0, p.lo[2], p.hi[2]); }

// clip a segment to the box (Liang-Barsky): false if nothing of it is inside
static bool clipToBox(V3 &a, V3 &b, const viz::Space &p) {
  double t0 = 0, t1 = 1, d[3] = {b.x - a.x, b.y - a.y, b.z - a.z}, s[3] = {a.x, a.y, a.z};
  for (int i = 0; i < 3; i++) {
    double lo = p.lo[i] - 1e-9, hi = p.hi[i] + 1e-9;
    if (std::fabs(d[i]) < 1e-12) { if (s[i] < lo || s[i] > hi) return false; continue; }
    double ta = (lo - s[i]) / d[i], tb = (hi - s[i]) / d[i];
    if (ta > tb) std::swap(ta, tb);
    t0 = std::max(t0, ta);
    t1 = std::min(t1, tb);
    if (t0 > t1) return false;
  }
  V3 A = a, D{d[0], d[1], d[2]};
  a = A + D * t0;
  b = A + D * t1;
  return true;
}

// the part of a polygon between two heights (Sutherland-Hodgman against z = lo and z = hi): a surface leaving the
// box is cut along a clean line, not dropped cell by cell
static std::vector<V3> clipHeights(std::vector<V3> poly, double lo, double hi) {
  for (int side = 0; side < 2 && !poly.empty(); side++) {
    auto inside = [&](V3 q) { return side == 0 ? q.z >= lo : q.z <= hi; };
    double level = side == 0 ? lo : hi;
    std::vector<V3> out;
    for (size_t i = 0; i < poly.size(); i++) {
      V3 a = poly[i], b = poly[(i + 1) % poly.size()];
      if (inside(a)) out.push_back(a);
      if (inside(a) != inside(b)) out.push_back(a + (b - a) * ((level - a.z) / (b.z - a.z)));
    }
    poly = std::move(out);
  }
  return poly;
}

bool Engine::spacePoint(const Mark &m, V3 &p) {
  std::string k = effectiveKind(m);
  bool hx, hy, hz;
  double x = num(m, "x", &hx), y = num(m, "y", &hy), z = num(m, "z", &hz);
  if (k == "vector") { // its tip: what a vector names
    SpaceShape s = spaceShape(m);
    if (!s.strokes.empty() && !s.strokes[0].pts.empty()) { p = s.strokes[0].pts.back(); return true; }
    return false;
  }
  if (hx && hy) { p = {x, y, hz ? z : 0}; return std::isfinite(x) && std::isfinite(y); }
  SpaceShape s = spaceShape(m); // a shape: the centre of what it draws
  V3 lo{1e300, 1e300, 1e300}, hi{-1e300, -1e300, -1e300};
  auto add = [&](V3 q) {
    lo = {std::min(lo.x, q.x), std::min(lo.y, q.y), std::min(lo.z, q.z)};
    hi = {std::max(hi.x, q.x), std::max(hi.y, q.y), std::max(hi.z, q.z)};
  };
  for (auto &st : s.strokes) for (auto &q : st.pts) add(q);
  for (auto &t : s.tris) { add(t.a); add(t.b); add(t.c); }
  for (auto &q : s.dots) add(q);
  if (lo.x > hi.x) return false;
  p = (lo + hi) * 0.5;
  return true;
}

Engine::SpaceShape Engine::spaceShape(const Mark &m) {
  SpaceShape sh;
  const viz::Space &P = spc_.proj;
  std::string k = effectiveKind(m);
  Color c = colorOf(m);
  float grow = std::clamp(m.appear, 0.f, 1.f);
  auto num3 = [&](const char *kx, const char *ky, const char *kz, V3 &out) {
    bool a, b, cz;
    out = {num(m, kx, &a), num(m, ky, &b), num(m, kz, &cz)};
    if (!cz) out.z = 0;
    return a && b && std::isfinite(out.x) && std::isfinite(out.y) && std::isfinite(out.z);
  };
  auto ref = [&](const char *key, V3 &out) { // to=<id>, at=<id>: where that thing is
    const Value *v = findProp(m.props, key);
    Mark *t = v && v->type == Value::STR ? mark(baseId(v->str)) : nullptr;
    return t && t != &m && spacePoint(*t, out);
  };
  auto stroke = [&](std::vector<V3> pts, float width, bool head = false) -> SpaceShape::Stroke & {
    SpaceShape::Stroke s;
    s.pts = std::move(pts);
    s.c = c;
    s.width = width;
    s.head = head;
    sh.strokes.push_back(s);
    return sh.strokes.back();
  };
  // a mesh of points (rows of columns) as shaded triangles, with a few of its lines drawn over it
  auto mesh = [&](const std::vector<std::vector<V3>> &g, float alpha, bool clipZ) {
    int nu = (int)g.size(), nv = nu ? (int)g[0].size() : 0;
    auto finite = [](V3 q) { return std::isfinite(q.x) && std::isfinite(q.y) && std::isfinite(q.z); };
    // a gentle tint by height (darker low, lighter high): with the shading, the shape reads even standing still
    Color low = lerpColor(c, col::bg, 0.35f), high = lerpColor(c, col::fg, 0.25f);
    auto face = [&](std::vector<V3> poly) { // a cell's triangle, cut to the box's heights
      if (clipZ) poly = clipHeights(poly, P.lo[2], P.hi[2]);
      for (size_t t = 1; t + 1 < poly.size(); t++) {
        double z = (poly[0].z + poly[t].z + poly[t + 1].z) / 3;
        float h = (float)std::clamp((z - P.lo[2]) / std::max(1e-9, P.hi[2] - P.lo[2]), 0.0, 1.0);
        sh.tris.push_back({poly[0], poly[t], poly[t + 1], lerpColor(low, high, h), alpha});
      }
    };
    for (int i = 0; i + 1 < nu; i++)
      for (int j = 0; j + 1 < nv; j++) {
        V3 a = g[i][j], b = g[i + 1][j], cc = g[i + 1][j + 1], d = g[i][j + 1];
        if (!finite(a) || !finite(b) || !finite(cc) || !finite(d)) continue;
        face({a, b, cc});
        face({a, cc, d});
      }
    int every = std::max(1, std::min(nu, nv) / 8);
    auto line = [&](const std::vector<V3> &pts) { // a mesh line, cut where the surface leaves the box
      std::vector<V3> run;
      auto flush = [&] {
        if (run.size() > 1) { auto &s = stroke(run, 1); s.alpha = 0.35f; s.ink = false; s.c = lerpColor(c, col::fg, 0.35f); }
        run.clear();
      };
      for (size_t i = 1; i < pts.size(); i++) {
        V3 a = pts[i - 1], b = pts[i];
        if (!finite(a) || !finite(b) || (clipZ && !clipToBox(a, b, P))) { flush(); continue; }
        if (run.empty() || viz::norm(run.back() - a) > 1e-9) { flush(); run.push_back(a); }
        run.push_back(b);
      }
      flush();
    };
    for (int i = 0; i < nu; i += every) line(g[i]);
    for (int j = 0; j < nv; j += every) {
      std::vector<V3> col;
      for (int i = 0; i < nu; i++) col.push_back(g[i][j]);
      line(col);
    }
  };

  if (k == "dot") {
    V3 p;
    if (!num3("x", "y", "z", p)) return sh;
    sh.dots.push_back(p);
    if (flagOn(findProp(m.props, "drop"))) { // a dashed line down to the floor and a shadow: its height, seen
      double fz = floorZ(P);
      auto &d = stroke({p, {p.x, p.y, fz}}, 1.5f);
      d.dashed = true; d.alpha = 0.6f; d.ink = false;
      double r = std::max({P.hi[0] - P.lo[0], P.hi[1] - P.lo[1]}) * 0.03;
      std::vector<V3> ring;
      for (int i = 0; i <= 24; i++) ring.push_back({p.x + r * std::cos(i * 0.2618), p.y + r * std::sin(i * 0.2618), fz});
      auto &s = stroke(ring, 1.5f);
      s.alpha = 0.5f; s.ink = false;
    }
  } else if (k == "vector") { // from x y z (or a thing, at=) by dx dy dz (or to=[x,y,z] | to=<id>); it grows out of its tail
    V3 tail{0, 0, 0}, tip;
    if (!ref("at", tail)) num3("x", "y", "z", tail);
    if (!std::isfinite(tail.x)) tail = {0, 0, 0};
    auto to = listNums(m, "to");
    if (to.size() >= 3) tip = {to[0], to[1], to[2]};
    else if (!ref("to", tip)) {
      bool a, b, cz;
      V3 d{num(m, "dx", &a), num(m, "dy", &b), num(m, "dz", &cz)};
      if (!a) d.x = 0;
      if (!b) d.y = 0;
      if (!cz) d.z = 0;
      tip = tail + d;
    }
    stroke({tail, tail + (tip - tail) * (1 - std::pow(1 - grow, 3.0))}, 3.5f, true);
  } else if (k == "line" || k == "arrow") { // pts=[x1,y1,z1, x2,y2,z2]
    auto p = listNums(m, "pts");
    if (p.size() >= 6) stroke({{p[0], p[1], p[2]}, {p[3], p[4], p[5]}}, 3, k == "arrow");
  } else if (k == "path") { // xt yt zt in t over range=[a,b]: drawn as it grows
    auto fx = fn2(m, "xt", "t", "_"), fy = fn2(m, "yt", "t", "_"), fz = fn2(m, "zt", "t", "_");
    if (!fx || !fy) return sh;
    double t0 = 0, t1 = 6.283185307;
    rangeOf(findProp(m.props, "range"), t0, t1);
    int n = 240, upto = std::max(1, (int)std::ceil(n * grow));
    std::vector<V3> pts;
    for (int i = 0; i <= upto; i++) {
      double t = t0 + (t1 - t0) * i / n;
      V3 q{fx(t, 0), fy(t, 0), fz ? fz(t, 0) : 0};
      if (std::isfinite(q.x) && std::isfinite(q.y) && std::isfinite(q.z)) pts.push_back(q);
    }
    if (pts.size() > 1) stroke(pts, 3);
  } else if (k == "surface") { // z = f(x, y) over the box, or xt yt zt in u, v
    std::vector<std::vector<V3>> g;
    if (findProp(m.props, "xt")) {
      auto fx = fn2(m, "xt", "u", "v"), fy = fn2(m, "yt", "u", "v"), fz = fn2(m, "zt", "u", "v");
      if (!fx || !fy || !fz) return sh;
      double u0 = 0, u1 = 6.283185307, v0 = 0, v1 = 3.141592654;
      rangeOf(findProp(m.props, "u"), u0, u1);
      rangeOf(findProp(m.props, "v"), v0, v1);
      for (int i = 0; i <= 40; i++) {
        g.emplace_back();
        for (int j = 0; j <= 24; j++) {
          double u = u0 + (u1 - u0) * i / 40, v = v0 + (v1 - v0) * j / 24;
          g.back().push_back({fx(u, v), fy(u, v), fz(u, v)});
        }
      }
      mesh(g, 0.8f, false);
    } else {
      auto f = fn2(m, "fn");
      if (!f) return sh;
      int n = 40;
      for (int i = 0; i <= n; i++) {
        g.emplace_back();
        for (int j = 0; j <= n; j++) {
          double x = P.lo[0] + (P.hi[0] - P.lo[0]) * i / n, y = P.lo[1] + (P.hi[1] - P.lo[1]) * j / n;
          g.back().push_back({x, y, f(x, y)});
        }
      }
      mesh(g, 0.8f, true);
    }
  } else if (k == "sphere") {
    V3 o{0, 0, 0};
    num3("x", "y", "z", o);
    double r = num(m, "r");
    if (!std::isfinite(r) || r <= 0) r = 1;
    std::vector<std::vector<V3>> g;
    for (int i = 0; i <= 36; i++) {
      g.emplace_back();
      for (int j = 0; j <= 18; j++) {
        double u = 6.283185307 * i / 36, v = 3.141592654 * j / 18;
        g.back().push_back(o + V3{std::sin(v) * std::cos(u), std::sin(v) * std::sin(u), std::cos(v)} * r);
      }
    }
    mesh(g, 0.8f, false);
  } else if (k == "plane") { // a x + b y + c z = d (normal=[a,b,c] d=), the part of it inside the box
    auto n3 = listNums(m, "normal");
    if (n3.size() < 3) return sh;
    V3 n{n3[0], n3[1], n3[2]};
    double d = num(m, "d");
    if (!std::isfinite(d)) d = 0;
    if (viz::norm(n) < 1e-12) return sh;
    std::vector<V3> poly; // where the plane crosses the box's twelve edges
    for (int axis = 0; axis < 3; axis++)
      for (int e = 0; e < 4; e++) {
        int a1 = (axis + 1) % 3, a2 = (axis + 2) % 3;
        double c1 = e & 1 ? P.hi[a1] : P.lo[a1], c2 = e & 2 ? P.hi[a2] : P.lo[a2];
        double nv[3] = {n.x, n.y, n.z};
        if (std::fabs(nv[axis]) < 1e-12) continue;
        double t = (d - nv[a1] * c1 - nv[a2] * c2) / nv[axis];
        if (t < P.lo[axis] - 1e-9 || t > P.hi[axis] + 1e-9) continue;
        double q[3];
        q[axis] = t; q[a1] = c1; q[a2] = c2;
        poly.push_back({q[0], q[1], q[2]});
      }
    if (poly.size() < 3) return sh;
    V3 ctr{0, 0, 0};
    for (auto &q : poly) ctr = ctr + q;
    ctr = ctr * (1.0 / poly.size());
    V3 e1 = viz::unit(poly[0] - ctr), e2 = viz::cross(viz::unit(n), e1);
    std::sort(poly.begin(), poly.end(), [&](V3 a, V3 b) {
      return std::atan2(viz::dot(a - ctr, e2), viz::dot(a - ctr, e1)) < std::atan2(viz::dot(b - ctr, e2), viz::dot(b - ctr, e1));
    });
    for (size_t i = 0; i < poly.size(); i++) sh.tris.push_back({ctr, poly[i], poly[(i + 1) % poly.size()], c, 0.35f});
    poly.push_back(poly[0]);
    auto &o = stroke(poly, 1.5f);
    o.alpha = 0.7f; o.ink = false;
  } else if (k == "polygon") { // a flat face: pts=[x,y,z, ...]
    auto p = listNums(m, "pts");
    std::vector<V3> pts;
    for (size_t i = 0; i + 2 < p.size(); i += 3) pts.push_back({p[i], p[i + 1], p[i + 2]});
    if (pts.size() < 3) return sh;
    for (size_t i = 1; i + 1 < pts.size(); i++) sh.tris.push_back({pts[0], pts[i], pts[i + 1], c, 0.35f});
    pts.push_back(pts[0]);
    stroke(pts, 2.5f);
  } else if (k == "grid") { // space under a 3x3 matrix m=[...]: the lattice, and the unit cube (its volume is det m)
    auto M = listNums(m, "m");
    if (M.size() < 9) M = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    auto T = [&](double x, double y, double z) { return V3{M[0] * x + M[1] * y + M[2] * z, M[3] * x + M[4] * y + M[5] * z, M[6] * x + M[7] * y + M[8] * z}; };
    double ext = 0;
    for (int i = 0; i < 3; i++) ext = std::max({ext, std::fabs(P.lo[i]), std::fabs(P.hi[i])});
    int N = std::min(6, (int)std::ceil(ext * 2) + 1);
    for (int axis = 0; axis < 3; axis++)
      for (int j = -N; j <= N; j++)
        for (int l = -N; l <= N; l++) {
          double a0[3], a1[3];
          a0[axis] = -N; a1[axis] = N;
          a0[(axis + 1) % 3] = a1[(axis + 1) % 3] = j;
          a0[(axis + 2) % 3] = a1[(axis + 2) % 3] = l;
          V3 A = T(a0[0], a0[1], a0[2]), B = T(a1[0], a1[1], a1[2]);
          if (!clipToBox(A, B, P)) continue;
          auto &s = stroke({A, B}, j == 0 && l == 0 ? 2 : 1);
          s.alpha = j == 0 && l == 0 ? 0.6f : 0.18f; s.ink = false;
        }
    V3 q[8];
    for (int i = 0; i < 8; i++) q[i] = T(i & 1, (i >> 1) & 1, (i >> 2) & 1);
    static const int faces[6][4] = {{0, 1, 3, 2}, {4, 5, 7, 6}, {0, 1, 5, 4}, {2, 3, 7, 6}, {0, 2, 6, 4}, {1, 3, 7, 5}};
    Color cube = col::yellow;
    for (auto &f : faces) {
      sh.tris.push_back({q[f[0]], q[f[1]], q[f[2]], cube, 0.22f});
      sh.tris.push_back({q[f[0]], q[f[2]], q[f[3]], cube, 0.22f});
      auto &s = stroke({q[f[0]], q[f[1]], q[f[2]], q[f[3]], q[f[0]]}, 2);
      s.c = cube; s.alpha = 0.8f; s.ink = false;
    }
  } else {
    return sh;
  }
  sh.ok = true;
  return sh;
}

// ---------------------------------------------------------------- ranges

// the box: the schema's ranges, or what its things reach (surfaces over x and y count for z), nice numbers,
// worked out when things are added and eased there (a scale never jumps)
void Engine::spaceRanges() {
  SpaceView &v = spc_;
  std::string sig = std::to_string(marks_.size());
  for (auto &p : schemaProps_) sig += "|" + p.key + "=" + p.val.markup();
  if (sig != v.rangeSig || !v.rangesInit) {
    v.rangeSig = sig;
    double lo[3] = {1e300, 1e300, 1e300}, hi[3] = {-1e300, -1e300, -1e300};
    bool given[3];
    const char *axis[3] = {"x", "y", "z"};
    for (int i = 0; i < 3; i++) given[i] = rangeOf(findProp(schemaProps_, axis[i]), lo[i], hi[i]);
    auto add = [&](V3 q) {
      double c[3] = {q.x, q.y, q.z};
      for (int i = 0; i < 3; i++)
        if (!given[i] && std::isfinite(c[i])) { lo[i] = std::min(lo[i], c[i]); hi[i] = std::max(hi[i], c[i]); }
    };
    for (const char *key : {"x", "x1", "x2"}) if (minVal_.count(key)) { add({minVal_[key], NAN, NAN}); add({maxVal_[key], NAN, NAN}); }
    for (const char *key : {"y", "y1", "y2"}) if (minVal_.count(key)) { add({NAN, minVal_[key], NAN}); add({NAN, maxVal_[key], NAN}); }
    for (const char *key : {"z", "z1", "z2"}) if (minVal_.count(key)) { add({NAN, NAN, minVal_[key]}); add({NAN, NAN, maxVal_[key]}); }
    for (int pass = 0; pass < 2; pass++) { // surfaces over x and y need those first
      for (int i = 0; i < 2; i++)
        if (pass == 1 && lo[i] > hi[i]) { lo[i] = -3; hi[i] = 3; }
      if (pass == 1)
        for (int i = 0; i < 2; i++) { v.proj.lo[i] = lo[i]; v.proj.hi[i] = hi[i]; }
      for (auto &id : order_) {
        Mark *m = mark(id);
        if (!m || !inDataView(*m)) continue;
        bool overXY = effectiveKind(*m) == "surface" && !findProp(m->props, "xt");
        if (overXY != (pass == 1)) continue;
        if (effectiveKind(*m) == "plane" || effectiveKind(*m) == "grid") continue; // they fill whatever box there is
        SpaceShape s = spaceShape(*m);
        for (auto &st : s.strokes) if (st.ink) for (auto &q : st.pts) add(q);
        for (auto &t : s.tris) { add(t.a); add(t.b); add(t.c); }
        for (auto &q : s.dots) add(q);
      }
    }
    for (int i = 0; i < 3; i++) {
      if (lo[i] > hi[i]) { lo[i] = -3; hi[i] = 3; }
      if (!given[i]) {
        float a = (float)lo[i], b = (float)hi[i];
        niceRange(a, b, true);
        lo[i] = a; hi[i] = b;
      }
      v.lo[i] = lo[i];
      v.hi[i] = hi[i];
    }
  }
  float k = v.rangesInit && !opt_.instant ? std::min(1.f, lastDt_ * 5) : 1.f;
  for (int i = 0; i < 3; i++) {
    v.proj.lo[i] += (v.lo[i] - v.proj.lo[i]) * k;
    v.proj.hi[i] += (v.hi[i] - v.proj.hi[i]) * k;
  }
  v.rangesInit = true;
  // a world (spheres round, right angles right) unless one range is far from the others (a loss surface): then each
  // axis gets the same length, a box (units=equal | box says which)
  double w[3];
  for (int i = 0; i < 3; i++) w[i] = v.proj.hi[i] - v.proj.lo[i];
  const Value *u = findProp(schemaProps_, "units");
  std::string units = u && u->type == Value::STR ? u->str : "";
  v.proj.equal = units == "equal" || (units != "box" && *std::max_element(w, w + 3) <= 3 * *std::min_element(w, w + 3));
}

// ---------------------------------------------------------------- camera

void Engine::doSpaceCamera(const Line &ln) {
  SpaceView &v = spc_;
  auto line = [&] {
    std::string s = "camera";
    if (!ln.kind.empty()) s += " " + ln.kind;
    for (auto &p : ln.props) s += " " + p.key + (p.val.type == Value::STR && p.val.str.empty() ? "" : "=" + p.val.markup());
    return s;
  };
  // spin: a slow turn that goes on (through the narration) until spin=off; it is not a move
  if (ln.kind == "spin" || findProp(ln.props, "spin")) {
    const Value *s = findProp(ln.props, "spin");
    v.spin = !s || flagOn(s) || (s->type == Value::STR && s->str == "on");
    v.spec.erase(std::remove_if(v.spec.begin(), v.spec.end(), [](const std::string &l) { return l.find("spin") != std::string::npos; }), v.spec.end());
    if (v.spin) v.spec.push_back("camera spin");
    caption(v.spin ? "camera spin -> a slow turn: the shape reads from the motion" : "camera spin=off -> the view holds still");
    return;
  }
  viz::Orbit now = v.proj.view, to = now;
  bool absolute = ln.kind == "reset";
  if (ln.kind == "reset") to = viz::Orbit{};
  if (const Value *vw = findProp(ln.props, "view")) {
    std::string name = vw->type == Value::STR ? vw->str : vw->markup();
    viz::Orbit o = to;
    if (!viz::namedView(name, o)) warn("camera: view=" + name + " is not a view (default, top, front, side)");
    // the nearest turn to the named view, not the long way round
    double d = std::remainder(o.theta - now.theta, 2 * 3.141592653589793);
    to.theta = now.theta + d;
    to.phi = o.phi;
    absolute = true;
  }
  if (const Value *o = findProp(ln.props, "orbit")) to.theta += num(*o) * viz::kDeg;
  if (const Value *t = findProp(ln.props, "tilt")) to.phi = viz::clampTilt(to.phi - num(*t) * viz::kDeg); // tilt up: look more from above
  if (const Value *z = findProp(ln.props, "zoom")) to.zoom = std::clamp(num(*z), 0.3, 20.0);
  if (const Value *on = findProp(ln.props, "on")) {
    V3 p;
    Mark *t = mark(baseId(on->type == Value::STR ? on->str : on->markup()));
    if (t && spacePoint(*t, p)) { to.aimed = true; to.target = p; }
    else warn("camera: on='" + on->markup() + "' is not on stage");
  }
  if (absolute) v.spec.erase(std::remove_if(v.spec.begin(), v.spec.end(), [](const std::string &l) { return l.find("spin") == std::string::npos; }), v.spec.end());
  if (ln.kind != "reset") v.spec.insert(v.spec.end() - (v.spin ? 1 : 0), line());
  actives_.erase(std::remove_if(actives_.begin(), actives_.end(), [&](const Active &a) { return a.kind == Active::ORBIT && a.id == curData_; }),
                 actives_.end());
  v.from = now;
  v.to = to;
  v.k = 0;
  double turn = std::max(std::fabs(to.theta - now.theta), std::fabs(to.phi - now.phi));
  Active a;
  a.kind = Active::ORBIT;
  a.id = curData_;
  a.dur = opt_.instant ? 0 : (ln.dur >= 0 ? ln.dur : std::clamp((float)turn / kTurnRate, 1.5f, 12.f));
  a.ease = rm::rateByName(ln.ease.empty() ? "smooth" : ln.ease);
  a.log = line();
  actives_.push_back(a);
  caption("camera -> the view turns to show the shape (motion is how 3D reads)");
}

// ---------------------------------------------------------------- layout

void Engine::layoutSpace(Rectangle area, float dt) {
  SpaceView &v = spc_;
  v.proj.ax = area.x + 24;
  v.proj.ay = area.y + 8;
  v.proj.aw = area.width - 48;
  v.proj.ah = area.height - 16;
  spaceRanges();
  // the camera: a move eases from -> to; the spin adds a slow turn on top (not in an instant run)
  auto mix = [](double a, double b, double k) { return a + (b - a) * k; };
  if (!opt_.instant) {
    v.spinRate = smoothApproach(v.spinRate, v.spin ? kSpinRate : 0.f, dt, 2);
    v.from.theta += v.spinRate * dt;
    v.to.theta += v.spinRate * dt;
  }
  viz::Orbit &o = v.proj.view;
  o.theta = mix(v.from.theta, v.to.theta, v.k);
  o.phi = mix(v.from.phi, v.to.phi, v.k);
  o.zoom = std::exp(mix(std::log(v.from.zoom), std::log(v.to.zoom), v.k));
  V3 a = v.from.aimed ? v.from.target : v.proj.centre(), b = v.to.aimed ? v.to.target : v.proj.centre();
  o.aimed = v.from.aimed || v.to.aimed;
  o.target = a + (b - a) * v.k;

  for (auto &id : order_) {
    Mark *m = mark(id);
    if (!m || !inDataView(*m)) continue;
    std::string k = effectiveKind(*m);
    if (k != "dot" && k != "vector" && k != "line" && k != "arrow" && k != "path" && k != "polygon" && k != "grid" && !isSpaceKind(k)) continue;
    SpaceShape s = spaceShape(*m);
    // a thing the box doesn't hold leaves the picture: say so (against the ranges it is easing to)
    auto outside = [&](V3 q) {
      double c[3] = {q.x, q.y, q.z};
      for (int i = 0; i < 3; i++) {
        double pad = (v.hi[i] - v.lo[i]) * 0.02;
        if (c[i] < v.lo[i] - pad || c[i] > v.hi[i] + pad) {
          const char *axis[3] = {"x", "y", "z"};
          warn("space: '" + m->id + "' reaches " + axis[i] + " = " + fmtNum(c[i]) + ", outside the box (" + axis[i] + "=[" +
               fmtNum(v.lo[i]) + ", " + fmtNum(v.hi[i]) + "]): widen the schema's " + axis[i] + "=[…] so it stays in view");
          return true;
        }
      }
      return false;
    };
    bool out = false;
    for (auto &st : s.strokes) if (st.ink) for (auto &q : st.pts) if (!out) out = outside(q);
    for (auto &q : s.dots) if (!out) out = outside(q);
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    auto add = [&](V3 q) {
      viz::Projected p = v.proj.project(q);
      x0 = std::min(x0, (float)p.x); y0 = std::min(y0, (float)p.y);
      x1 = std::max(x1, (float)p.x); y1 = std::max(y1, (float)p.y);
    };
    for (auto &st : s.strokes) if (st.ink || s.tris.empty()) for (auto &q : st.pts) add(q);
    for (auto &t : s.tris) { add(t.a); add(t.b); add(t.c); }
    for (auto &q : s.dots) add(q);
    if (x0 > x1) continue;
    m->spaceBox = {x0, y0, x1 - x0, y1 - y0};
    if (k == "dot" && !s.dots.empty()) {
      viz::Projected p = v.proj.project(s.dots[0]);
      m->tpos = {(float)p.x, (float)p.y};
    } else {
      m->tpos = {x0 + (x1 - x0) / 2, y0 + (y1 - y0) / 2};
    }
    m->positioned = m->direct = true;
  }
}

// ---------------------------------------------------------------- what labels and the checker see

std::vector<Vector2> Engine::spaceStroke(const Mark &m) {
  std::vector<Vector2> out;
  SpaceShape s = spaceShape(m);
  for (auto &st : s.strokes) {
    if (!st.ink) continue;
    for (auto &q : st.pts) {
      viz::Projected p = spc_.proj.project(q);
      out.push_back({(float)p.x, (float)p.y});
    }
    break; // its first line: a vector's shaft, a curve
  }
  return out;
}

// a vector's name past its tip, a curve's at its end, a surface's or a shape's above it; a dot's is placed by the
// label placer like any dot's
bool Engine::spaceName(const Mark &m, Vector2 &at, float &ax, float &ay) {
  std::string k = effectiveKind(m);
  if (k == "dot" || k == "grid") return false;
  std::vector<Vector2> st = spaceStroke(m);
  if ((k == "vector" || k == "line" || k == "arrow" || k == "path") && st.size() >= 2) {
    Vector2 e = st.back(), d = norm(st.back() - st[st.size() - 2]);
    at = e + d * 14;
    ax = d.x >= 0 ? 0 : 1;
    ay = d.y >= 0 ? 0 : 1;
    return true;
  }
  const Rectangle &b = m.spaceBox;
  if (b.width <= 0) return false;
  at = {b.x + b.width * 0.85f, b.y - 6};
  ax = 0.5f;
  ay = 1;
  return true;
}

// the axes' numbers and names, where drawSpace writes them (the checker's furniture)
std::vector<std::pair<Rectangle, std::string>> Engine::spaceTicks() {
  std::vector<std::pair<Rectangle, std::string>> out;
  const viz::Space &P = spc_.proj;
  const Value *ax = findProp(schemaProps_, "axes");
  if (ax && !flagOn(ax)) return out;
  double origin[3];
  for (int i = 0; i < 3; i++) origin[i] = std::clamp(0.0, P.lo[i], P.hi[i]);
  const char *names[3] = {"x", "y", "z"}, *keys[3] = {"xlabel", "ylabel", "zlabel"};
  for (int i = 0; i < 3; i++) {
    double dx, dy;
    V3 dir{i == 0 ? 1.0 : 0, i == 1 ? 1.0 : 0, i == 2 ? 1.0 : 0};
    P.screenDir(dir, dx, dy);
    // numbers sit beside the axis, on the side away from it (perpendicular on screen)
    Vector2 off{(float)-dy * 16, (float)dx * 16};
    if (off.y < 0) off = off * -1.f;
    for (double t : {P.lo[i], P.hi[i]}) { // its two ends: the scale, without clutter
      if (std::fabs(t) < (P.hi[i] - P.lo[i]) * 1e-6) continue;
      double q[3] = {origin[0], origin[1], origin[2]};
      q[i] = t;
      viz::Projected p = P.project({q[0], q[1], q[2]});
      std::string s = fmtNum(t);
      Vector2 sz = measure(s, 14);
      out.push_back({{(float)p.x + off.x - sz.x / 2, (float)p.y + off.y - sz.y / 2, sz.x, sz.y}, s});
    }
    double q[3] = {origin[0], origin[1], origin[2]};
    q[i] = P.hi[i];
    viz::Projected p = P.project({q[0], q[1], q[2]});
    const Value *l = findProp(schemaProps_, keys[i]);
    std::string s = l && l->type == Value::STR ? l->str : names[i];
    Vector2 sz = measure(s, 19), at{(float)(p.x + dx * 22), (float)(p.y + dy * 22)};
    out.push_back({{at.x - sz.x / 2, at.y - sz.y / 2, sz.x, sz.y}, "\x01" + s}); // \x01: an axis name
  }
  return out;
}

// two things meet in space only if they are at one point in 3D: in projection, anything can line up
int Engine::spaceCoincide(const Mark &a, const Mark &b) {
  if (!isSpace(schemaOf(a)) || !isSpace(schemaOf(b))) return -1;
  DataScope ds(*this, dataViewOf(a));
  V3 p, q;
  if (!spacePoint(a, p) || !spacePoint(b, q)) return 0;
  double diag = std::sqrt(std::pow(spc_.proj.hi[0] - spc_.proj.lo[0], 2) + std::pow(spc_.proj.hi[1] - spc_.proj.lo[1], 2) +
                          std::pow(spc_.proj.hi[2] - spc_.proj.lo[2], 2));
  return viz::norm(p - q) < diag * 0.015 ? 1 : 0;
}

// ---------------------------------------------------------------- drawing

namespace {
struct Piece {
  enum Kind : unsigned char { Tri, Seg, Disc, Head } kind = Seg;
  float depth = 0;
  Vector2 a{}, b{}, c{};
  Color col{};
  float w = 1;
  bool dashed = false;
  bool thing = false; // a point, an arrowhead or a line of a thing: drawn again faintly on top, so what goes behind a
                      // surface is still seen (identity: nothing the voice names may vanish)
};
} // namespace

void Engine::drawSpace() {
  const viz::Space &P = spc_.proj;
  std::vector<Piece> pieces;
  auto proj = [&](V3 q, float &depth) {
    viz::Projected p = P.project(q);
    depth = (float)p.depth;
    return Vector2{(float)p.x, (float)p.y};
  };
  // a polyline: each segment a piece of its own, so a surface can come between its parts
  auto addLine = [&](const std::vector<V3> &pts, Color c, float w, bool dashed, bool thing = false) {
    for (size_t i = 1; i < pts.size(); i++) {
      Piece pc;
      float d0, d1;
      pc.a = proj(pts[i - 1], d0);
      pc.b = proj(pts[i], d1);
      pc.depth = (d0 + d1) / 2;
      pc.col = c; pc.w = w; pc.dashed = dashed; pc.thing = thing;
      pieces.push_back(pc);
    }
  };
  // light from above and a little left of the camera: a surface's shape shows in its shading, and the light turns
  // with the view, so a spin doesn't make it flicker
  V3 f = P.forward(), r = viz::unit(viz::cross(f, {0, 0, 1})), u = viz::cross(r, f);
  V3 light = viz::unit(f * -1.0 + u * 0.8 + r * -0.4);
  auto addTri = [&](const SpaceShape::Tri &t, float alpha) {
    V3 n = viz::unit(viz::cross(P.world(t.b) - P.world(t.a), P.world(t.c) - P.world(t.a)));
    float shade = 0.3f + 0.8f * (float)std::fabs(viz::dot(n, light));
    Piece pc;
    pc.kind = Piece::Tri;
    float d0, d1, d2;
    pc.a = proj(t.a, d0); pc.b = proj(t.b, d1); pc.c = proj(t.c, d2);
    pc.depth = (d0 + d1 + d2) / 3;
    Color c = t.col;
    auto lit = [&](unsigned char v) { return (unsigned char)std::min(255.f, v * shade); };
    pc.col = fade({lit(c.r), lit(c.g), lit(c.b), 255}, t.alpha * alpha);
    pieces.push_back(pc);
  };

  // furniture: the floor grid, then the three axes with their arrows
  const float F = stageFade_;
  const Value *gv = findProp(schemaProps_, "grid"), *av = findProp(schemaProps_, "axes");
  double origin[3];
  for (int i = 0; i < 3; i++) origin[i] = std::clamp(0.0, P.lo[i], P.hi[i]);
  if (!gv || flagOn(gv)) {
    double fz = floorZ(P);
    for (int i = 0; i < 2; i++) {
      double span = P.hi[i] - P.lo[i], step = viz::niceStepFloor(span / 4);
      for (double t = std::ceil(P.lo[i] / step) * step; t <= P.hi[i] + 1e-9; t += step) {
        V3 a = i == 0 ? V3{t, P.lo[1], fz} : V3{P.lo[0], t, fz}, b = i == 0 ? V3{t, P.hi[1], fz} : V3{P.hi[0], t, fz};
        addLine({a, b}, fade(col::grid, 0.55f * F), 1, false);
      }
    }
  }
  if (!av || flagOn(av))
    for (int i = 0; i < 3; i++) {
      double a[3] = {origin[0], origin[1], origin[2]}, b[3] = {origin[0], origin[1], origin[2]};
      a[i] = P.lo[i];
      b[i] = P.hi[i];
      addLine({{a[0], a[1], a[2]}, {b[0], b[1], b[2]}}, fade(col::muted, 0.9f * F), 2, false);
      Piece h;
      h.kind = Piece::Head;
      float d0, d1;
      double before[3] = {b[0], b[1], b[2]};
      before[i] -= (P.hi[i] - P.lo[i]) * 0.04;
      Vector2 tip = proj({b[0], b[1], b[2]}, d0), back = proj({before[0], before[1], before[2]}, d1);
      Vector2 dir = norm(tip - back), side{-dir.y, dir.x};
      h.a = tip + dir * 6; h.b = tip - dir * 8 + side * 5; h.c = tip - dir * 8 - side * 5;
      h.depth = d0;
      h.col = fade(col::muted, 0.9f * F);
      pieces.push_back(h);
    }

  // the things of this view
  for (auto &id : order_) {
    Mark *m = mark(id);
    if (!m || !m->positioned || !inDataView(*m)) continue;
    float a = alphaOf(*m);
    if (a <= 0.003f) continue;
    SpaceShape s = spaceShape(*m);
    if (!s.ok) continue;
    for (auto &t : s.tris) addTri(t, a);
    for (auto &st : s.strokes) {
      addLine(st.pts, fade(st.c, st.alpha * a), st.width, st.dashed, st.ink);
      if (st.head && st.pts.size() >= 2) { // a cone-like head: a triangle along the direction on screen
        float d0, d1;
        Vector2 tip = proj(st.pts.back(), d0), back = proj(st.pts[st.pts.size() - 2], d1);
        if (len(tip - back) < 1) continue;
        Vector2 dir = norm(tip - back), side{-dir.y, dir.x};
        Piece h;
        h.kind = Piece::Head;
        h.a = tip + dir * 4; h.b = tip - dir * 14 + side * 7; h.c = tip - dir * 14 - side * 7;
        h.depth = d0 - 0.001f;
        h.col = fade(st.c, a);
        h.thing = true;
        pieces.push_back(h);
      }
    }
    for (auto &q : s.dots) {
      Piece d;
      d.kind = Piece::Disc;
      d.a = proj(q, d.depth);
      d.depth -= 0.002f; // a dot on a surface is in front of it
      d.col = fade(colorOf(*m), a);
      d.w = dotRadius(*m) * std::clamp(m->appear * 1.4f, 0.f, 1.f);
      d.thing = true;
      pieces.push_back(d);
    }
  }

  std::stable_sort(pieces.begin(), pieces.end(), [](const Piece &x, const Piece &y) { return x.depth > y.depth; });
  bool anySurface = std::any_of(pieces.begin(), pieces.end(), [](const Piece &p) { return p.kind == Piece::Tri; });
  for (auto &p : pieces) {
    switch (p.kind) {
    case Piece::Tri: tri(p.a, p.b, p.c, p.col); break;
    case Piece::Head: tri(p.a, p.b, p.c, p.col); break;
    case Piece::Disc:
      DrawCircleV(p.a, p.w + 1.5f, fade(col::bg, p.col.a / 255.f));
      DrawCircleV(p.a, p.w, p.col);
      break;
    case Piece::Seg:
      if (p.dashed) dashed(p.a, p.b, p.w, p.col, 6, 5);
      else {
        DrawLineEx(p.a, p.b, p.w, p.col);
        if (p.w >= 2.5f) { DrawCircleV(p.a, p.w / 2, p.col); DrawCircleV(p.b, p.w / 2, p.col); }
      }
      break;
    }
  }
  if (anySurface) // things hidden by a surface show through it faintly (in front they only brighten a little)
    for (auto &p : pieces) {
      if (!p.thing) continue;
      Color g = fade(p.col, 0.3f);
      if (p.kind == Piece::Disc) DrawCircleV(p.a, p.w, g);
      else if (p.kind == Piece::Head) tri(p.a, p.b, p.c, g);
      else if (!p.dashed) DrawLineEx(p.a, p.b, p.w, g);
    }
  // numbers and axis names last, upright, readable over anything
  for (auto &[box, s] : spaceTicks()) {
    bool name = !s.empty() && s[0] == '\x01';
    textOutlined(name ? s.substr(1) : s, {box.x + box.width / 2, box.y + box.height / 2}, name ? 19.f : 14.f,
                 fade(name ? col::fg : col::muted, F), 0.5f, 0.5f);
  }
}

} // namespace mk
