// Placement timing budget: 60 labels among 60 things and 20 lines; a 30-thing graph.   make bench
#include "../markup/place/flow.hpp"
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
  std::vector<double> labels;
  for (int run = 0; run < 60; run++) {
    Rng rng(1000 + run);
    std::vector<Vec> pos;
    LabelWorld w;
    std::vector<LabelReq> reqs;
    for (int i = 0; i < 60; i++) {
      pos.push_back({80 + rng.unit() * 1120, 100 + rng.unit() * 420});
      w.obstacles.push_back(Rect::around(pos.back(), {40 + rng.unit() * 90, 24 + rng.unit() * 40}));
      LabelReq r;
      r.anchor = w.obstacles.back();
      r.size = {50 + rng.unit() * 60, 18};
      r.owner = i;
      reqs.push_back(r);
    }
    for (int i = 0; i < 20; i++) w.strokes.push_back({pos[rng.below(60)], pos[rng.below(60)]});
    auto t0 = Clock::now();
    placeLabels(reqs, w);
    auto t1 = Clock::now();
    labels.push_back(ms(t0, t1));
  }
  std::vector<double> flows;
  for (int run = 0; run < 60; run++) { // the graph schema: 30 things, 40 links
    Rng rng(2000 + run);
    FlowInput in;
    in.area = {40, 66, 1200, 494};
    for (int i = 0; i < 30; i++) {
      in.id.push_back("n" + std::to_string(i));
      in.size.push_back({60 + rng.unit() * 60, 36 + rng.unit() * 20});
      in.lane.push_back(i % 3 == 0 ? -1 : i % 2);
      in.category.push_back(0);
    }
    for (int k = 0; k < 40; k++) { // a network: five columns of six, links mostly to the next column
      int a = rng.below(24), b = (a / 6 + 1 + (rng.below(4) == 0)) * 6 + rng.below(6);
      if (b < 30) in.edges.push_back({a, b, 40});
    }
    FlowMemo memo;
    auto t0 = Clock::now();
    layoutFlow(in, memo);
    flows.push_back(ms(t0, Clock::now()));
  }
  double f50 = pct(flows, 0.5), f95 = pct(flows, 0.95);
  std::printf("flow (30 things, ~40 links):    p50 %.2f ms  p95 %.2f ms   budget 2 ms\n", f50, f95);
  double l50 = pct(labels, 0.5), l95 = pct(labels, 0.95);
  std::printf("labels (60 labels, 20 lines):    p50 %.2f ms  p95 %.2f ms   budget 1 ms\n", l50, l95);
  // the target is 1 ms; the pass/fail line allows 3x for timing noise on a busy machine
  bool ok = l95 < 3 && f95 < 6;
  std::printf("%s\n", ok ? (l95 < 1 && f95 < 2 ? "ok: within target" : "ok: within 3x slack (over target)") : "OVER BUDGET");
  return ok ? 0 : 1;
}
