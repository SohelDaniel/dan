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

TEST_CASE("check: free regions are the empty thirds of the stage") {
  Picture p;
  body(p, "big", {40, 66, 400, 165}); // fills the top-left ninth
  auto r = check(p);
  CHECK(std::find(r.freeRegions.begin(), r.freeRegions.end(), std::string("top-left")) == r.freeRegions.end());
  CHECK(std::find(r.freeRegions.begin(), r.freeRegions.end(), std::string("bottom-right")) != r.freeRegions.end());
}

// ---------------------------------------------------------------- milestone 3: the placer core
#include "../markup/place/regions.hpp"
#include "../markup/place/separate.hpp"
#include "../markup/place/skyline.hpp"
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

TEST_CASE("regions: names and 6x6 slots cover the stage") {
  Rect st{40, 66, 1200, 494};
  auto tl = regionRect("top-left", st), r = regionRect("right", st), a1 = regionRect("a1", st), f6 = regionRect("f6", st);
  REQUIRE(tl); REQUIRE(r); REQUIRE(a1); REQUIRE(f6);
  CHECK(tl->x == 40);
  CHECK(r->right() == doctest::Approx(st.right()));
  CHECK(a1->w == doctest::Approx(200));
  CHECK(f6->bottom() == doctest::Approx(st.bottom()));
  CHECK_FALSE(regionRect("somewhere", st));
}

TEST_CASE("skyline: neighbours that don't collide share a level; those that do stack") {
  Skyline sky(4);
  CHECK(sky.place(0, 40, 20) == 0);
  CHECK(sky.place(50, 90, 20) == 0);      // clear of the first: same level
  CHECK(sky.place(30, 70, 20) == 24);     // over both: one level down
  CHECK(sky.place(200, 240, 20) == 0);
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

static SeparateInput randomBoxes(uint64_t seed, int n) {
  Rng rng(seed);
  SeparateInput in;
  for (int i = 0; i < n; i++) {
    in.centre.push_back({300 + rng.unit() * 500, 150 + rng.unit() * 300});
    in.size.push_back({30 + rng.unit() * 90, 20 + rng.unit() * 40});
    in.weight.push_back(1);
  }
  in.gap = 6;
  return in;
}
static int overlapsLeft(const SeparateInput &in, const std::vector<Vec> &c) {
  int bad = 0;
  for (size_t i = 0; i < c.size(); i++)
    for (size_t j = i + 1; j < c.size(); j++) {
      bool ok = false;
      for (auto [a, b] : in.allowed) if ((a == (int)i && b == (int)j) || (a == (int)j && b == (int)i)) ok = true;
      if (ok) continue;
      if (overlaps(Rect::around(c[i], in.size[i]), Rect::around(c[j], in.size[j]), 0.5f)) bad++;
    }
  return bad;
}

TEST_CASE("separate: random scenes end with no overlaps (n = 10 … 120)") {
  for (int n : {10, 30, 60, 120})
    for (uint64_t seed = 1; seed <= 5; seed++) {
      auto in = randomBoxes(seed * 7919 + n, n);
      int left = -1;
      auto out = separate(in, &left);
      CAPTURE(n); CAPTURE(seed);
      CHECK(overlapsLeft(in, out) == 0);
      CHECK(left == 0);
    }
}

TEST_CASE("separate: fixed things stay, intended overlaps stay, a clean scene doesn't move") {
  SeparateInput in;
  in.centre = {{400, 300}, {420, 300}, {430, 305}};
  in.size = {{100, 60}, {100, 60}, {20, 20}};
  in.weight = {1e6, 1, 1};
  in.allowed = {{0, 2}, {1, 2}}; // the small one sits on the others on purpose
  auto out = separate(in);
  CHECK(std::fabs(out[0].x - 400) < 0.05f);
  CHECK(std::fabs(out[0].y - 300) < 0.05f);
  CHECK(overlapsLeft(in, out) == 0);
  CHECK(out[2].x == doctest::Approx(430)); // allowed: not pushed

  SeparateInput clean;
  clean.centre = {{100, 100}, {400, 100}};
  clean.size = {{50, 50}, {50, 50}};
  clean.weight = {1, 1};
  auto same = separate(clean);
  CHECK(same[0] == clean.centre[0]);
  CHECK(same[1] == clean.centre[1]);
}

// VPSC keeps things in order (left stays left), which naive pushing doesn't; in very dense random
// scenes that costs some extra movement. Bound the cost, and check the order separately.
TEST_CASE("separate: movement stays within 1.6x of naive pairwise pushing (dense random scenes)") {
  for (uint64_t seed : {99u, 7u, 12345u}) {
    auto in = randomBoxes(seed, 40);
    auto out = separate(in);
    double moved = 0;
    for (size_t i = 0; i < out.size(); i++) moved += length(out[i] - in.centre[i]);
    // naive: push every overlapping pair apart (half each, cheaper axis) until nothing overlaps
    auto c = in.centre;
    for (int it = 0; it < 5000; it++) {
      bool any = false;
      for (size_t i = 0; i < c.size(); i++)
        for (size_t j = i + 1; j < c.size(); j++) {
          float ox = (in.size[i].x + in.size[j].x) / 2 + in.gap - std::fabs(c[i].x - c[j].x);
          float oy = (in.size[i].y + in.size[j].y) / 2 + in.gap - std::fabs(c[i].y - c[j].y);
          if (ox <= 0.01f || oy <= 0.01f) continue;
          any = true;
          if (ox < oy) { float sgn = c[i].x <= c[j].x ? 1 : -1; c[i].x -= sgn * ox / 2; c[j].x += sgn * ox / 2; }
          else { float sgn = c[i].y <= c[j].y ? 1 : -1; c[i].y -= sgn * oy / 2; c[j].y += sgn * oy / 2; }
        }
      if (!any) break;
    }
    double naive = 0, sqV = 0, sqN = 0, maxV = 0, maxN = 0;
    for (size_t i = 0; i < c.size(); i++) {
      naive += length(c[i] - in.centre[i]);
      double dv = length(out[i] - in.centre[i]), dn = length(c[i] - in.centre[i]);
      sqV += dv * dv; sqN += dn * dn; maxV = std::max(maxV, dv); maxN = std::max(maxN, dn);
    }
    MESSAGE("seed " << seed << ": total moved vpsc " << moved << " naive " << naive << " | squared vpsc " << sqV
                    << " naive " << sqN << " | max vpsc " << maxV << " naive " << maxN);
    CAPTURE(seed);
    CHECK(moved <= naive * 1.6 + 1);
  }
}

TEST_CASE("separate: keeps order: things apart along x that share a row stay in that order") {
  for (uint64_t seed : {3u, 4u, 5u}) {
    auto in = randomBoxes(seed, 30);
    auto out = separate(in);
    int flips = 0;
    for (size_t i = 0; i < out.size(); i++)
      for (size_t j = 0; j < out.size(); j++) {
        if (i == j) continue;
        bool rowShared = std::fabs(in.centre[i].y - in.centre[j].y) < (in.size[i].y + in.size[j].y) / 2;
        bool leftBefore = in.centre[i].x + in.size[i].x / 2 < in.centre[j].x - in.size[j].x / 2;
        if (rowShared && leftBefore && out[i].x > out[j].x) flips++;
      }
    CAPTURE(seed);
    CHECK(flips == 0);
  }
}

TEST_CASE("separate: deterministic") {
  auto in = randomBoxes(5, 50);
  auto a = separate(in), b = separate(in);
  for (size_t i = 0; i < a.size(); i++) CHECK(a[i] == b[i]);
}

// ---------------------------------------------------------------- labels
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
#include "../markup/place/regions.hpp"
#include "../markup/place/solve.hpp"

static Item item(const char *id, Vec size, Vec target, Item::Mode mode = Item::Preferred) {
  Item it;
  it.id = id;
  it.size = size;
  it.target = target;
  it.mode = mode;
  return it;
}

TEST_CASE("solve: relations hold; fixed things stay; nothing overlaps") {
  Scene sc;
  sc.items = {item("graph", {500, 300}, {640, 330}, Item::Fixed), item("eq", {240, 50}, {640, 330}),
              item("card", {300, 80}, {640, 330}), item("note", {120, 30}, {200, 200})};
  sc.rels = {{Rel::Above, 1, 0}, {Rel::RightOf, 2, 0}, {Rel::Below, 3, 1}};
  auto p = solveBodies(sc);
  Rect g = Rect::around(p.pos[0], sc.items[0].size), eq = Rect::around(p.pos[1], sc.items[1].size),
       card = Rect::around(p.pos[2], sc.items[2].size);
  CHECK(length(p.pos[0] - Vec{640, 330}) < 0.05f);  // fixed (a weight of 1e6 moves it by far less than a pixel)
  CHECK(eq.bottom() <= g.y - 16 + 3);               // above
  CHECK(card.x >= g.right() + 16 - 3);              // right of (it has to squeeze, the stage is tight)
  for (size_t i = 0; i < p.pos.size(); i++)
    for (size_t j = i + 1; j < p.pos.size(); j++)
      CHECK_FALSE(overlaps(Rect::around(p.pos[i], sc.items[i].size), Rect::around(p.pos[j], sc.items[j].size)));
  for (size_t i = 0; i < p.pos.size(); i++) CHECK(contains(sc.stage, Rect::around(p.pos[i], sc.items[i].size), 1));
}

TEST_CASE("solve: on / in / over may overlap; regions contain") {
  Scene sc;
  sc.items = {item("box", {300, 200}, {640, 300}), item("ball", {40, 40}, {100, 100}), item("tag", {60, 24}, {900, 500}),
              item("key", {200, 60}, {640, 300})};
  sc.items[3].region = regionRect("top-right", sc.stage);
  sc.rels = {{Rel::In, 1, 0}, {Rel::On, 2, 0}};
  auto p = solveBodies(sc);
  Rect box = Rect::around(p.pos[0], sc.items[0].size), ball = Rect::around(p.pos[1], sc.items[1].size),
       tag = Rect::around(p.pos[2], sc.items[2].size), key = Rect::around(p.pos[3], sc.items[3].size);
  CHECK(contains(box, ball, 1));
  CHECK(std::fabs(tag.bottom() - box.y) <= 3);
  CHECK(contains(*sc.items[3].region, key, 1));
  for (auto &n : p.notes) CHECK(n.code != Code::RelationUnsat);
}

TEST_CASE("solve: contradictions are reported, never thrown") {
  Scene sc;
  sc.items = {item("a", {100, 40}, {640, 300}), item("b", {100, 40}, {640, 300})};
  sc.rels = {{Rel::Below, 0, 1}, {Rel::Above, 0, 1}};
  Placement p;
  CHECK_NOTHROW(p = solveBodies(sc));
  int unsat = 0;
  for (auto &n : p.notes) unsat += n.code == Code::RelationUnsat;
  CHECK(unsat >= 1);
}

TEST_CASE("solve: stable: a small change moves the rest little; same input, same output") {
  Scene sc;
  Rng rng(17);
  for (int i = 0; i < 20; i++)
    sc.items.push_back(item("x", {60 + rng.unit() * 80, 30 + rng.unit() * 30}, {200 + rng.unit() * 880, 120 + rng.unit() * 380}));
  auto p1 = solveBodies(sc);
  auto p1b = solveBodies(sc);
  for (size_t i = 0; i < p1.pos.size(); i++) CHECK(p1.pos[i] == p1b.pos[i]);
  for (size_t i = 0; i < sc.items.size(); i++) sc.items[i].prev = p1.pos[i];
  sc.items[5].size.x += 2;
  auto p2 = solveBodies(sc);
  float moved = 0;
  for (size_t i = 0; i < p1.pos.size(); i++) if (i != 5) moved = std::max(moved, length(p2.pos[i] - p1.pos[i]));
  CHECK(moved <= 3);
}

TEST_CASE("solve: a formula below a card in a region never lands on a fixed box (regression)") {
  Scene sc;
  sc.items = {item("sun", {99, 88}, {522, 328}, Item::Fixed), item("earth", {91, 88}, {762, 328}, Item::Fixed),
              item("key", {368, 87}, {640, 313}), item("law", {152, 49}, {640, 313})};
  sc.items[2].region = regionRect("top-right", sc.stage);
  sc.rels = {{Rel::Below, 3, 2}};
  auto p = solveBodies(sc);
  for (size_t i = 0; i < p.pos.size(); i++) MESSAGE(sc.items[i].id << " " << p.pos[i].x << "," << p.pos[i].y);
  for (size_t i = 0; i < p.pos.size(); i++)
    for (size_t j = i + 1; j < p.pos.size(); j++)
      CHECK_FALSE(overlaps(Rect::around(p.pos[i], sc.items[i].size), Rect::around(p.pos[j], sc.items[j].size)));
}

TEST_CASE("solve: fixed things that cross (axis bands at a centred origin) don't make the stage 'crowded'") {
  Scene sc;
  sc.stage = {40, 66, 1200, 494};
  Item xb; xb.mode = Item::Fixed; xb.size = {1156, 22}; xb.target = {648, 328};
  Item yb; yb.mode = Item::Fixed; yb.size = {42, 494}; yb.target = {615, 309};
  Item dot; dot.id = "p"; dot.mode = Item::Fixed; dot.size = {14, 14}; dot.target = {514, 338};
  Item card; card.id = "rule"; card.mode = Item::Preferred; card.size = {152, 49}; card.target = {640, 313};
  card.region = Rect{40, 66, 1200, 165};
  sc.items = {xb, yb, dot, card};
  Placement p = solveBodies(sc);
  for (auto &n : p.notes) CHECK(n.code != Code::Crowded);
  for (int i = 0; i < 3; i++) { CHECK(p.pos[i].x == doctest::Approx(sc.items[i].target.x)); CHECK(p.pos[i].y == doctest::Approx(sc.items[i].target.y)); }
  Rect c = Rect::around(p.pos[3], card.size);
  CHECK_FALSE(overlaps(c, Rect::around(p.pos[0], xb.size)));
  CHECK_FALSE(overlaps(c, Rect::around(p.pos[1], yb.size)));
}
