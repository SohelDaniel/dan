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

#include "../markup/place/flow.hpp"

// a flow input from names, edges ("a>b") and optional lanes; every box 100 x 50
static FlowInput flowOf(std::vector<std::string> ids, std::vector<std::string> edges, std::vector<int> lanes = {}) {
  FlowInput in;
  in.area = {40, 66, 1200, 494};
  in.id = ids;
  for (size_t i = 0; i < ids.size(); i++) {
    in.size.push_back({100, 50});
    in.lane.push_back(lanes.empty() ? -1 : lanes[i]);
    in.category.push_back(0);
  }
  auto at = [&](const std::string &s) { return (int)(std::find(ids.begin(), ids.end(), s) - ids.begin()); };
  for (auto &e : edges) in.edges.push_back({at(e.substr(0, e.find('>'))), at(e.substr(e.find('>') + 1)), 0});
  return in;
}
static Rect boxOf(const FlowInput &in, const FlowOut &o, int i) { return Rect::around(o.centre[i], in.size[i]); }
// every link drawn as it will be: from the source's right side through its waypoints to the target's left side
static std::vector<Vec> drawn(const FlowInput &in, const FlowOut &o, size_t e) {
  Rect a = boxOf(in, o, in.edges[e].a), b = boxOf(in, o, in.edges[e].b);
  std::vector<Vec> pts{{a.right(), a.center().y + o.route[e].portA}};
  pts.insert(pts.end(), o.route[e].via.begin(), o.route[e].via.end());
  pts.push_back({b.x, b.center().y + o.route[e].portB});
  return smoothRoute(pts);
}

TEST_CASE("flow: a chain reads left to right on one line") {
  auto in = flowOf({"a", "b", "c"}, {"a>b", "b>c"});
  FlowMemo memo;
  auto o = layoutFlow(in, memo);
  CHECK(o.rank == std::vector<int>{0, 1, 2});
  CHECK(o.centre[0].x < o.centre[1].x);
  CHECK(o.centre[1].x < o.centre[2].x);
  CHECK(o.centre[0].y == doctest::Approx(o.centre[1].y).epsilon(0.002));
  CHECK(o.centre[1].y == doctest::Approx(o.centre[2].y).epsilon(0.002));
  // centred on the stage
  float cx = (boxOf(in, o, 0).x + boxOf(in, o, 2).right()) / 2;
  CHECK(cx == doctest::Approx(40 + 600).epsilon(0.002));
  CHECK(o.centre[1].y == doctest::Approx(66 + 247).epsilon(0.002));
}

TEST_CASE("flow: a cycle (backprop's gradients) runs back as arcs; the forward pass decides the columns") {
  auto in = flowOf({"x", "w", "y", "L"}, {"x>y", "w>y", "y>L", "L>y", "y>w"});
  FlowMemo memo;
  auto o = layoutFlow(in, memo);
  CHECK(o.rank[0] == 0);
  CHECK(o.rank[2] == 1);
  CHECK(o.rank[3] == 2);
  CHECK(o.route[3].back);
  CHECK(o.route[4].back);
  CHECK(!o.route[0].back);
}

TEST_CASE("flow: random graphs: no boxes overlap and no link runs through a box") {
  for (uint64_t seed = 1; seed <= 40; seed++) {
    Rng rng(seed);
    int n = 5 + rng.below(14);
    std::vector<std::string> ids;
    for (int i = 0; i < n; i++) ids.push_back("n" + std::to_string(i));
    std::vector<std::string> edges;
    for (int k = 0; k < n + rng.below(n); k++) {
      int a = rng.below(n), b = rng.below(n);
      if (a < b) edges.push_back(ids[a] + ">" + ids[b]); // acyclic, so every link is routed
    }
    auto in = flowOf(ids, edges);
    for (auto &s : in.size) s = {60 + rng.unit() * 60, 36 + rng.unit() * 20};
    FlowMemo memo;
    auto o = layoutFlow(in, memo);
    if (o.overflow) continue; // too big for the stage: flagged, not judged
    for (int i = 0; i < n; i++)
      for (int j = i + 1; j < n; j++) CHECK_MESSAGE(!overlaps(boxOf(in, o, i), boxOf(in, o, j)), "seed " << seed);
    for (size_t e = 0; e < in.edges.size(); e++) {
      auto line = drawn(in, o, e);
      for (int i = 0; i < n; i++) {
        if (i == in.edges[e].a || i == in.edges[e].b) continue;
        for (size_t k = 1; k < line.size(); k++)
          CHECK_MESSAGE(!hits({line[k - 1], line[k]}, inflate(boxOf(in, o, i), -3)), "seed " << seed << " edge " << e);
      }
    }
  }
}

TEST_CASE("flow: lanes keep their members together and their stages line up") {
  // two pipelines and a shared source: the dense net on top, the transformer below
  auto in = flowOf({"img", "px", "hid", "out", "p1", "p2", "p3"}, {"img>px", "px>hid", "hid>out", "img>p1", "p1>p2", "p1>p3"},
                   {-1, 0, 0, 0, 1, 1, 1});
  FlowMemo memo;
  auto o = layoutFlow(in, memo);
  CHECK(o.centre[1].x == o.centre[4].x); // pixels and patch 1: the same stage
  CHECK(o.centre[2].x == o.centre[5].x);
  float bottomDense = 0, topTf = 1e9f;
  for (int i : {1, 2, 3}) bottomDense = std::max(bottomDense, boxOf(in, o, i).bottom());
  for (int i : {4, 5, 6}) topTf = std::min(topTf, boxOf(in, o, i).y);
  CHECK(topTf - bottomDense >= 52); // room for the second lane's box and name
  // the fan from patch 1 leaves from distinct points, in the order of its targets
  CHECK(o.route[4].portA != o.route[5].portA);
  CHECK((o.route[4].portA < o.route[5].portA) == (o.centre[5].y < o.centre[6].y));
}

TEST_CASE("flow: streaming: a newcomer is inserted, the things on stage keep their order; same input, same output") {
  auto in = flowOf({"a", "b", "c", "d"}, {"a>b", "a>c", "a>d"});
  FlowMemo memo;
  auto o1 = layoutFlow(in, memo);
  auto in2 = flowOf({"a", "b", "c", "d", "e"}, {"a>b", "a>c", "a>d", "a>e"});
  auto o2 = layoutFlow(in2, memo);
  CHECK(o2.centre[1].y < o2.centre[2].y);
  CHECK(o2.centre[2].y < o2.centre[3].y);
  CHECK(o1.centre[1].y < o1.centre[2].y);
  FlowMemo m1, m2;
  auto p1 = layoutFlow(in2, m1), p2 = layoutFlow(in2, m2);
  for (size_t i = 0; i < p1.centre.size(); i++) CHECK(p1.centre[i] == p2.centre[i]);
}

TEST_CASE("flow: model order: without links, one category keeps declaration order; categories form columns") {
  auto in = flowOf({"a", "b", "c"}, {});
  FlowMemo memo;
  auto o = layoutFlow(in, memo);
  CHECK(o.centre[0].x < o.centre[1].x);
  CHECK(o.centre[1].x < o.centre[2].x);
  in.category = {0, 1, 1};
  FlowMemo m2;
  auto o2 = layoutFlow(in, m2);
  CHECK(o2.rank == std::vector<int>{0, 1, 1});
  CHECK(o2.centre[1].y < o2.centre[2].y);
}

TEST_CASE("flow: a long label widens its gap; a source sits next to what it feeds") {
  auto in = flowOf({"a", "b", "c"}, {"a>b", "b>c"});
  in.area.w = 700; // tight, so little slack is shared out
  in.edges[0].labelW = 200;
  FlowMemo memo;
  auto o = layoutFlow(in, memo);
  float g0 = boxOf(in, o, 1).x - boxOf(in, o, 0).right(), g1 = boxOf(in, o, 2).x - boxOf(in, o, 1).right();
  CHECK(g0 >= 240 - 0.5f);
  CHECK(g0 > g1);
  auto bias = flowOf({"x", "h", "o", "b"}, {"x>h", "h>o", "b>o"});
  FlowMemo m2;
  CHECK(layoutFlow(bias, m2).rank[3] == 1);
}
