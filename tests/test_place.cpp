// Unit and property tests for the placement module (markup/place). No window, no raylib:
//   make test
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
#include "../markup/place/check.hpp"
#include "../markup/place/grid.hpp"
#include "../markup/place/rng.hpp"

using namespace place;

TEST_CASE("geometry: overlap, containment, segments") {
  Rect a{0, 0, 10, 10}, b{5, 5, 10, 10}, c{10, 0, 5, 5};
  CHECK(overlapArea(a, b) == doctest::Approx(25));
  CHECK(overlaps(a, b));
  CHECK_FALSE(overlaps(a, c)); // touching edges is not overlapping
  CHECK(contains({0, 0, 100, 100}, a));
  CHECK(hits({{-5, 5}, {20, 5}}, a));
  CHECK_FALSE(hits({{-5, -5}, {-1, 20}}, a));
}

TEST_CASE("rng: same seed, same sequence; fnv is stable") {
  Rng r1(42), r2(42), r3(43);
  for (int i = 0; i < 5; i++) CHECK(r1.next() == r2.next());
  CHECK(Rng(42).next() != r3.next());
  CHECK(fnv1a("tutor") == fnv1a("tutor"));
}

TEST_CASE("grid: queries find what touches, once") {
  Grid g({0, 0, 1280, 720});
  g.insert(0, Rect{10, 10, 40, 40});
  g.insert(1, Rect{600, 300, 80, 20});
  auto q = g.query({30, 30, 100, 100});
  CHECK(q == std::vector<int>{0});
  CHECK(g.query({0, 0, 1280, 720}).size() == 2);
}

static int body(Picture &p, const char *id, Rect r) { return p.add({Ink::Body, id, std::string("bar '") + id + "'", r}); }
static int label(Picture &p, int owner, Rect r) {
  Ink i{Ink::Label, p.ink[owner].id, "name of '" + p.ink[owner].id + "'", r};
  i.owner = owner;
  return p.add(i);
}

TEST_CASE("check: accidental overlaps are found, intended ones are not") {
  Picture p;
  int a = body(p, "alice", {100, 200, 60, 100});
  int b = body(p, "bob", {140, 200, 60, 100}); // overlaps alice
  label(p, a, {100, 305, 50, 20});             // under its own bar: fine
  auto r = check(p);
  REQUIRE(r.overlaps == 1);
  CHECK(r.diags[0].code == Diag::Overlap);
  CHECK(r.score < 100);
  CHECK(describe(p, r, r.diags[0]).find("bar 'alice' overlaps bar 'bob'") == 0);
  p.allow(a, b); // e.g. related things, or on=/in=
  CHECK(check(p).overlaps == 0);
}

TEST_CASE("check: a label on another thing, off the screen, far away, in a zone") {
  Picture p;
  int a = body(p, "a", {200, 200, 50, 50});
  int b = body(p, "b", {300, 200, 50, 50});
  int la = label(p, a, {300, 210, 40, 20}); // a's name sits on b
  p.ink[la].maxDist = 30;
  label(p, b, {1270, 200, 40, 20});          // off the right edge, and far from b
  p.add({Ink::Zone, "subtitle", "subtitle", {120, 592, 1040, 80}});
  body(p, "c", {600, 580, 40, 40});          // dips into the subtitle area
  auto r = check(p);
  int over = 0, off = 0, far = 0, zone = 0;
  for (auto &d : r.diags) {
    over += d.code == Diag::Overlap;
    off += d.code == Diag::OffCanvas;
    far += d.code == Diag::LabelFar;
    zone += d.code == Diag::InZone;
  }
  CHECK(over == 1);
  CHECK(off == 1);
  CHECK(far >= 1);
  CHECK(zone == 1);
}

TEST_CASE("check: links through things; a link's own ends and label are fine") {
  Picture p;
  int a = body(p, "a", {100, 300, 60, 60});
  int b = body(p, "b", {500, 300, 60, 60});
  body(p, "mid", {300, 300, 60, 60});
  Ink l{Ink::Link, "a->b", "link a->b", {130, 329, 400, 2}};
  l.line = {{130, 330}, {530, 330}};
  l.owner = a; l.other = b;
  int li = p.add(l);
  Ink ll{Ink::Label, "a->b", "label of link a->b", {310, 320, 40, 20}};
  ll.owner = li;
  p.add(ll); // the link's label sits on the link: fine (but on 'mid': not)
  auto r = check(p);
  int through = 0;
  for (auto &d : r.diags) through += d.code == Diag::LinkThrough;
  CHECK(through == 1);
}

#include "../markup/place/qp.hpp"
#include "../markup/place/spring.hpp"

TEST_CASE("spring: reaches the goal, never overshoots, same result for one big or two small steps") {
  Spring s{0, 0};
  for (int i = 0; i < 120; i++) { s.step(100, 0.12f, 1.f / 60); CHECK(s.x <= 100.001f); }
  CHECK(s.x == doctest::Approx(100).epsilon(0.001));
  Spring a{0, 0}, b{0, 0};
  a.step(50, 0.1f, 1.f / 30);
  b.step(50, 0.1f, 1.f / 60);
  b.step(50, 0.1f, 1.f / 60);
  CHECK(a.x == doctest::Approx(b.x).epsilon(1e-3));
  Spring big{0, 0};
  big.step(10, 0.1f, 1.0f); // a one-second hitch: stable, lands on the goal
  CHECK(std::fabs(big.x - 10) < 0.01f);
}

TEST_CASE("solve1d: constraints hold with least movement") {
  // two at the same spot, must be 10 apart: they split the difference
  auto x = solve1d({5, 5}, {1, 1}, {{0, 1, 10}});
  CHECK(x[1] - x[0] == doctest::Approx(10));
  CHECK(x[0] == doctest::Approx(0));
  // a heavy one barely moves
  auto y = solve1d({5, 5}, {1e6, 1}, {{0, 1, 10}});
  CHECK(y[0] == doctest::Approx(5).epsilon(1e-4));
  CHECK(y[1] == doctest::Approx(15).epsilon(1e-4));
  // a chain stays ordered
  auto z = solve1d({0, 1, 2, 3}, {1, 1, 1, 1}, {{0, 1, 10}, {1, 2, 10}, {2, 3, 10}});
  for (int i = 0; i < 3; i++) CHECK(z[i + 1] - z[i] >= 10 - 1e-6);
}

#include "../markup/place/labels.hpp"

TEST_CASE("labels: a dense grid of points with long names: shown labels never overlap") {
  std::vector<LabelReq> reqs;
  LabelWorld w;
  for (int i = 0; i < 40; i++) {
    Rect dot{200.f + (i % 8) * 110, 150.f + (i / 8) * 80, 12, 12};
    w.obstacles.push_back(dot);
    LabelReq r;
    r.anchor = dot;
    r.size = {70 + (i % 3) * 20.f, 18};
    r.owner = i;
    r.priority = i % 4 == 0 ? 2 : 1;
    reqs.push_back(r);
  }
  auto out = placeLabels(reqs, w);
  int shown = 0, overl = 0;
  for (size_t i = 0; i < out.size(); i++) {
    if (!out[i].shown) { CHECK(reqs[i].priority < 2); continue; } // important ones are never dropped
    shown++;
    for (size_t j = i + 1; j < out.size(); j++)
      if (out[j].shown && overlaps(out[i].box, out[j].box)) overl++;
    for (size_t k = 0; k < w.obstacles.size(); k++)
      if (k != i && overlaps(out[i].box, w.obstacles[k])) overl++;
    // within reach of what it names
    const Rect &a = reqs[i].anchor, &b = out[i].box;
    float dx = std::max({a.x - b.right(), b.x - a.right(), 0.f}), dy = std::max({a.y - b.bottom(), b.y - a.bottom(), 0.f});
    CHECK(std::sqrt(dx * dx + dy * dy) <= 31);
  }
  CHECK(overl == 0);
  CHECK(shown >= 30);
}

TEST_CASE("labels: avoid a reserved zone and other things; keep a previous side when it's still good") {
  LabelWorld w;
  w.zones.push_back({0, 600, 1280, 120});      // subtitles
  Rect thing{600, 560, 40, 30};
  w.obstacles.push_back(thing);
  w.obstacles.push_back({600, 520, 120, 36});  // something right above
  LabelReq r;
  r.anchor = thing;
  r.size = {60, 18};
  r.owner = 0;
  auto out = placeLabels({r}, w);
  CHECK_FALSE(overlaps(out[0].box, w.zones[0]));
  CHECK_FALSE(overlaps(out[0].box, w.obstacles[1]));
  // hysteresis: a free scene, previous side W (index 3 of the default order) is kept over S (index 0)
  LabelWorld free;
  LabelReq q;
  q.anchor = {400, 300, 20, 20};
  q.size = {40, 16};
  q.prevSide = 3;
  CHECK(placeLabels({q}, free)[0].side == 0); // S is three preference steps better: worth switching…
  q.sides = {Side::S, Side::W};
  q.prevSide = 1;
  CHECK(placeLabels({q}, free)[0].side == 1); // …but with two near-equal choices, it stays put
}

// ---------------------------------------------------------------- bodies: relations, regions, stability
