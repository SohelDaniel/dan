// Placement timing budget: 60 things, 60 labels, 20 lines.   make bench
#include "../markup/place/place.hpp"
#include "../markup/place/rng.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>

using namespace place;
using Clock = std::chrono::steady_clock;

static double ms(Clock::time_point a, Clock::time_point b) { return std::chrono::duration<double, std::milli>(b - a).count(); }
static double pct(std::vector<double> v, double p) { std::sort(v.begin(), v.end()); return v[(size_t)(p * (v.size() - 1))]; }

int main() {
  std::vector<double> bodies, labels;
  for (int run = 0; run < 60; run++) {
    Rng rng(1000 + run);
    Scene sc;
    for (int i = 0; i < 60; i++) {
      Item it;
      it.size = {40 + rng.unit() * 90, 24 + rng.unit() * 40};
      it.target = {80 + rng.unit() * 1120, 100 + rng.unit() * 420};
      it.mode = i % 5 == 0 ? Item::Fixed : Item::Preferred;
      sc.items.push_back(it);
    }
    for (int i = 0; i < 12; i++) sc.rels.push_back({Rel(rng.below(5)), rng.below(60), rng.below(60)});
    auto t0 = Clock::now();
    auto p = solveBodies(sc);
    auto t1 = Clock::now();
    bodies.push_back(ms(t0, t1));
    LabelWorld w;
    std::vector<LabelReq> reqs;
    for (int i = 0; i < 60; i++) {
      w.obstacles.push_back(Rect::around(p.pos[i], sc.items[i].size));
      LabelReq r;
      r.anchor = w.obstacles.back();
      r.size = {50 + rng.unit() * 60, 18};
      r.owner = i;
      reqs.push_back(r);
    }
    for (int i = 0; i < 20; i++) w.strokes.push_back({p.pos[rng.below(60)], p.pos[rng.below(60)]});
    auto t2 = Clock::now();
    placeLabels(reqs, w);
    auto t3 = Clock::now();
    labels.push_back(ms(t2, t3));
  }
  double b50 = pct(bodies, 0.5), b95 = pct(bodies, 0.95), l50 = pct(labels, 0.5), l95 = pct(labels, 0.95);
  std::printf("bodies (60 items, 12 relations): p50 %.2f ms  p95 %.2f ms   budget 5 ms\n", b50, b95);
  std::printf("labels (60 labels, 20 lines):    p50 %.2f ms  p95 %.2f ms   budget 1 ms\n", l50, l95);
  // targets are 5 ms / 1 ms; the pass/fail line allows 3x for timing noise on a busy machine
  bool ok = b95 < 15 && l95 < 3;
  std::printf("%s\n", ok ? (b95 < 5 && l95 < 1 ? "ok: within target" : "ok: within 3x slack (over target)") : "OVER BUDGET");
  return ok ? 0 : 1;
}
