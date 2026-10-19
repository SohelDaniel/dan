// Unit and property tests for markup/viz (camera path, ticks, contours, streamlines, colour, trails).
//   make test
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
#include "../markup/viz/viz.hpp"
#include <algorithm>

using namespace viz;

TEST_CASE("zoom path: ends exact, width monotone for a pure zoom, zooms out mid-flight on a long pan") {
  ZoomPath z;
  z.set({0, 0}, 10, {0, 0}, 1); // pure 10x zoom in
  V2 c; double w, prev = 1e9;
  for (int i = 0; i <= 20; i++) {
    z.at(i / 20.0, c, w);
    CHECK(w <= prev + 1e-9);
    prev = w;
  }
  z.at(1, c, w);
  CHECK(w == doctest::Approx(1));
  // log-linear: halfway in time is the geometric mean, not the arithmetic one
  z.at(0.5, c, w);
  CHECK(w == doctest::Approx(std::sqrt(10.0)).epsilon(1e-6));
  CHECK(z.seconds() >= 0.6);
  CHECK(z.seconds() <= 2.0);

  z.set({0, 0}, 1, {20, 0}, 1); // long pan at the same zoom: goes up the ladder and back
  double wmax = 0;
  for (int i = 0; i <= 50; i++) { z.at(i / 50.0, c, w); wmax = std::max(wmax, w); }
  CHECK(wmax > 2);
  z.at(1, c, w);
  CHECK(c.x == doctest::Approx(20));
  CHECK(w == doctest::Approx(1));
  z.at(0, c, w);
  CHECK(c.x == doctest::Approx(0));

  // zooming in on a point in view: it stays in view the whole way
  z.set({0, 0}, 10, {3, 1}, 0.1);
  CHECK(z.anchored);
  for (int i = 0; i <= 40; i++) {
    z.at(i / 40.0, c, w);
    CHECK(std::fabs(3 - c.x) <= w / 2 + 1e-9);
    CHECK(std::fabs(1 - c.y) <= w * 0.5625 / 2 + 1e-9);
  }
  z.set({0, 0}, 1, {0.1, 0}, 1.2); // small move: plain easing
  CHECK(z.linear);
  z.at(0.5, c, w);
  CHECK(c.x == doctest::Approx(0.05));
}

TEST_CASE("ticks: nice steps, the major level has room, the minor one fades in continuously") {
  TickLevel M, m;
  tickLevels(10, 1000, 80, M, m); // 100 px per unit
  CHECK(M.step == doctest::Approx(1));
  CHECK(M.step * 100 >= 80);
  CHECK(m.step == doctest::Approx(0.5));
  CHECK(m.alpha > 0.1f); // 50 px spacing: fading in
  CHECK(m.alpha < 0.5f);
  // continuity: as the view zooms in smoothly, the drawn alpha of each step never jumps
  auto alphaOf = [](double step, double range) {
    TickLevel a, b;
    tickLevels(range, 1000, 80, a, b);
    if (std::fabs(a.step - step) < 1e-9) return 1.0f;
    if (std::fabs(b.step - step) < 1e-9) return b.alpha;
    return 0.0f;
  };
  // every step that is ever drawn stays drawn as we zoom in (lines are only added)
  for (double st : {5.0, 1.0, 0.5, 0.1}) {
    float last = alphaOf(st, 200);
    for (double r = 200; r > 0.5; r *= 0.995) {
      float a = alphaOf(st, r);
      // a step leaves the two drawn levels only when it is coarser than the major one: it is then a
      // multiple of the major step, so its lines are still drawn
      if (a == 0 && last > 0) { TickLevel A, B; tickLevels(r, 1000, 80, A, B); CHECK(std::fmod(st / A.step + 1e-9, 1.0) < 1e-6); }
      else CHECK(a - last > -0.1f);
      last = a;
    }
  }
  CHECK(niceStepAbove(1) == doctest::Approx(2));
  CHECK(niceStepAbove(2) == doctest::Approx(5));
  CHECK(niceStepAbove(5) == doctest::Approx(10));
  CHECK(niceStepFloor(0.37) == doctest::Approx(0.2));
}

static std::vector<float> sample(int nx, int ny, double (*f)(double, double), double x0, double x1, double y0, double y1) {
  std::vector<float> v((size_t)nx * ny);
  for (int j = 0; j < ny; j++)
    for (int i = 0; i < nx; i++) v[(size_t)j * nx + i] = (float)f(x0 + (x1 - x0) * i / (nx - 1), y0 + (y1 - y0) * j / (ny - 1));
  return v;
}

TEST_CASE("contours: a circle is one closed loop at the right radius") {
  int n = 81;
  auto v = sample(n, n, [](double x, double y) { return x * x + y * y; }, -2, 2, -2, 2);
  auto ls = contours(v, n, n, 1.0);
  REQUIRE(ls.size() == 1);
  auto &l = ls[0];
  CHECK(l.size() > 40);
  double h = 4.0 / (n - 1);
  for (V2 p : l) {
    double x = -2 + p.x * h, y = -2 + p.y * h;
    CHECK(std::sqrt(x * x + y * y) == doctest::Approx(1).epsilon(0.01));
  }
  CHECK(norm(l.front() - l.back()) < 1e-9); // closed
}

TEST_CASE("contours: saddles resolve by the centre, lines never cross; NaN is below") {
  int n = 41;
  auto v = sample(n, n, [](double x, double y) { return x * y; }, -1, 1, -1, 1);
  auto pos = contours(v, n, n, 0.25), neg = contours(v, n, n, -0.25);
  CHECK(pos.size() == 2); // two hyperbola branches
  CHECK(neg.size() == 2);
  auto z = contours(v, n, n, 0.0);
  CHECK(!z.empty());
  std::vector<float> w(9, 1.f);
  w[4] = NAN;
  auto hole = contours(w, 3, 3, 0.5);
  CHECK(hole.size() == 1); // a ring around the missing sample
}

TEST_CASE("levels: nice and inside the range; log levels double") {
  auto L = niceLevels(0, 10, 8);
  CHECK(!L.empty());
  CHECK(L.size() <= 13);
  for (double l : L) { CHECK(l > 0); CHECK(l < 10); }
  auto G = logLevels(0, 16, 4);
  REQUIRE(G.size() == 4);
  CHECK(G[1] / G[0] == doctest::Approx(2));
}

TEST_CASE("rk4: a rotation field keeps its radius (Euler would spiral out)") {
  Field rot = [](V2 p) { return V2{-p.y, p.x}; };
  V2 p{1, 0};
  for (int i = 0; i < 1000; i++) p = rk4(rot, p, 0.01);
  CHECK(norm(p) == doctest::Approx(1).epsilon(1e-6));
}

TEST_CASE("streamlines: evenly spaced, inside the domain, never closer than d_test, deterministic") {
  StreamOpts o;
  o.x0 = -1; o.x1 = 1; o.y0 = -1; o.y1 = 1;
  o.dsep = 0.15; o.h = 0.02;
  Field rot = [](V2 p) { return V2{-p.y, p.x}; };
  auto ls = streamlines(rot, o);
  CHECK(ls.size() >= 4);
  double dtest = o.dtest * o.dsep;
  for (size_t a = 0; a < ls.size(); a++) {
    for (V2 p : ls[a]) { CHECK(p.x >= o.x0); CHECK(p.x <= o.x1); CHECK(p.y >= o.y0); CHECK(p.y <= o.y1); }
    for (size_t b = a + 1; b < ls.size(); b++) {
      double dmin = 1e9;
      for (V2 p : ls[a]) for (V2 q : ls[b]) dmin = std::min(dmin, norm(p - q));
      CHECK(dmin >= dtest * 0.9);
    }
  }
  auto again = streamlines(rot, o);
  REQUIRE(again.size() == ls.size());
  CHECK(again[0].size() == ls[0].size());
  // a uniform field gives parallel lines that run with the field
  Field right = [](V2) { return V2{1, 0}; };
  auto par = streamlines(right, o);
  CHECK(par.size() >= 8);
  for (auto &l : par) CHECK(l.back().x > l.front().x);
}

TEST_CASE("colour: viridis ends and monotone lightness; diverging is dark in the middle") {
  RGB a = viridis(0), b = viridis(1);
  CHECK(a.r == doctest::Approx(0.267f));
  CHECK(b.g == doctest::Approx(0.906f));
  auto lum = [](RGB c) { return 0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b; };
  double last = -1;
  for (int i = 0; i <= 20; i++) { double l = lum(viridis(i / 20.0)); CHECK(l >= last - 1e-6); last = l; }
  CHECK(lum(diverging(0.5)) < lum(diverging(0)));
  CHECK(lum(diverging(0.5)) < lum(diverging(1)));
  CHECK(arrowLength(0, 1) == 0);
  CHECK(arrowLength(100, 1) == doctest::Approx(1));
}

TEST_CASE("trail times: absolute grid, exact ends and corners, independent of when you ask") {
  auto a = trailTimes(1.0, 2.0, 0.1, {1.55});
  CHECK(a.front() == doctest::Approx(1.0));
  CHECK(a.back() == doctest::Approx(2.0));
  CHECK(std::find_if(a.begin(), a.end(), [](double t) { return std::fabs(t - 1.55) < 1e-12; }) != a.end());
  // the interior samples are the same grid points whichever window they fall in
  auto b = trailTimes(1.03, 2.0, 0.1, {});
  CHECK(b[1] == doctest::Approx(1.1));
  CHECK(std::is_sorted(a.begin(), a.end()));
}

// ---------------------------------------------------------------- compiled expressions
#include "../markup/expr.hpp"
#include <map>

struct TestCtx : mk::EvalCtx {
  std::map<std::string, double> vars;
  int lookups = 0;
  bool var(const std::string &n, double &out) override {
    lookups++;
    auto it = vars.find(n);
    if (it == vars.end()) return false;
    out = it->second;
    return true;
  }
  bool member(const std::string &, const std::string &, double &) override { return false; }
  bool call(const std::string &, const std::vector<mk::ExprPtr> &, double &, int) override { return false; }
};

TEST_CASE("compile: same values as the tree walk, outside names folded once") {
  TestCtx ctx;
  ctx.vars["k"] = 2.5;
  for (const char *src : {"x^2 + 4*y^2", "sin(x)*cos(y) - k", "-y + k*x", "max(x, y, 0.5)", "clamp(x*y, -1, 1)",
                          "atan2(y, x) + sqrt(abs(x))", "exp(-(x^2+y^2)/k)", "2x + 3y", "pow(x, 2) / (1 + y^2)"}) {
    std::string err;
    auto e = mk::parseExpr(src, err);
    REQUIRE(err.empty());
    ctx.lookups = 0;
    mk::Program p = mk::compile(*e, ctx);
    CHECK(p.ok);
    int compileLookups = ctx.lookups;
    for (double x : {-1.5, 0.0, 0.7, 2.0})
      for (double y : {-2.0, 0.3, 1.0}) {
        ctx.x = x; ctx.hasX = true; ctx.y = y; ctx.hasY = true;
        double want = mk::eval(*e, ctx);
        double got = p.run(x, y);
        if (std::isnan(want)) CHECK(std::isnan(got));
        else CHECK(got == doctest::Approx(want));
      }
    ctx.hasX = ctx.hasY = false;
    CHECK(compileLookups <= 1); // k looked up once at compile time, never per point
  }
  std::string err;
  auto e = mk::parseExpr("f(x) + y", err);
  CHECK_FALSE(mk::compile(*e, ctx).ok); // an engine call of x: fall back to eval
}
