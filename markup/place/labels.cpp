#include "labels.hpp"
#include "grid.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace place {

static Rect candidate(const Rect &a, Vec sz, Side s, float d) {
  float dg = d * 0.7f; // diagonals sit a little closer
  switch (s) {
  case Side::N: return {a.center().x - sz.x / 2, a.y - d - sz.y, sz.x, sz.y};
  case Side::S: return {a.center().x - sz.x / 2, a.bottom() + d, sz.x, sz.y};
  case Side::E: return {a.right() + d, a.center().y - sz.y / 2, sz.x, sz.y};
  case Side::W: return {a.x - d - sz.x, a.center().y - sz.y / 2, sz.x, sz.y};
  case Side::NE: return {a.right() + dg, a.y - dg - sz.y, sz.x, sz.y};
  case Side::NW: return {a.x - dg - sz.x, a.y - dg - sz.y, sz.x, sz.y};
  case Side::SE: return {a.right() + dg, a.bottom() + dg, sz.x, sz.y};
  case Side::SW: return {a.x - dg - sz.x, a.bottom() + dg, sz.x, sz.y};
  case Side::C: return Rect::around(a.center(), sz);
  }
  return {};
}

std::vector<LabelOut> placeLabels(const std::vector<LabelReq> &reqs, const LabelWorld &w) {
  static const std::vector<Side> kDefault = {Side::S, Side::E, Side::N, Side::W, Side::SE, Side::NE, Side::SW, Side::NW};
  static const float kRings[] = {4, 14, 30};
  static const float kFar[] = {44, 62}; // crowded spots: important labels may step further out
  std::vector<LabelOut> out(reqs.size());
  // obstacles and strokes go in a grid once; placed labels are added as we go
  Grid grid(inflate(w.bounds, 200));
  std::vector<Rect> boxes = w.obstacles;
  for (int i = 0; i < (int)w.obstacles.size(); i++) grid.insert(i, w.obstacles[i]);
  int strokeBase = (int)boxes.size();
  std::vector<Seg> segs;
  for (auto &s : w.strokes)
    for (size_t k = 1; k < s.size(); k++) {
      segs.push_back({s[k - 1], s[k]});
      grid.insert(strokeBase + (int)segs.size() - 1, segs.back());
    }
  int labelBase = strokeBase + (int)segs.size() + 1000000; // placed labels get ids from here
  std::vector<Rect> placed;

  auto cost = [&](const LabelReq &r, const Rect &box, int rank, float dist, bool same) {
    float c = rank * 2.0f + dist * 0.15f;
    if (!contains(w.bounds, box, 0.5f)) c += 10000;
    for (auto &z : w.zones) c += overlapArea(z, box) > 1 ? 4000 : 0;
    float boxArea = std::max(1.f, box.w * box.h);
    for (int id : grid.query(box)) {
      if (id < strokeBase) {
        if (id == r.owner) continue; // a label may sit on its own thing
        // covering a thing hides it: far worse than crossing a thin line
        c += 3000 * std::min(1.f, overlapArea(box, boxes[id]) / boxArea) + (overlaps(box, boxes[id]) ? 1500 : 0);
      } else if (id < labelBase) {
        if (hits(segs[id - strokeBase], inflate(box, -2))) c += 120;
      } else {
        const Rect &o = inflate(placed[id - labelBase], 3); // labels keep a little air between them
        if (overlaps(box, o)) c += 2500 + 3000 * std::min(1.f, overlapArea(box, o) / boxArea);
      }
    }
    if (same) c -= 3 + 0.2f * c + r.sticky; // hysteresis: a step of preference (and a crossing line), never an overlap
    return c;
  };

  std::vector<int> order(reqs.size());
  std::iota(order.begin(), order.end(), 0);
  std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return reqs[a].priority > reqs[b].priority; });
  for (int i : order) {
    const LabelReq &r = reqs[i];
    const auto &sides = r.sides.empty() ? kDefault : r.sides;
    float best = 1e30f;
    Rect bestBox{};
    int bestSide = -1;
    for (float d : kRings)
      for (int si = 0; si < (int)sides.size(); si++) {
        Rect box = candidate(r.anchor, r.size, sides[si], d);
        float c = cost(r, box, si, d, si == r.prevSide);
        if (c < best) { best = c; bestBox = box; bestSide = si; }
      }
    if (best >= 500 && r.priority >= 2) // nothing clean nearby: try further out before settling for an overlap
      for (float d : kFar)
        for (int si = 0; si < (int)sides.size(); si++) {
          Rect box = candidate(r.anchor, r.size, sides[si], d);
          float c = cost(r, box, si + 4, d, si == r.prevSide);
          if (c < best) { best = c; bestBox = box; bestSide = si; }
        }
    for (size_t k = 0; k < r.along.size(); k++) {
      Rect box = Rect::around(r.along[k], r.size);
      float c = cost(r, box, (int)k + 6, 6, r.prevSide == -2); // along a line: when the ends are taken
      if (c < best) { best = c; bestBox = box; bestSide = -2; }
    }
    bool clean = best < 500;
    out[i] = {bestBox, bestSide, clean || r.priority >= 2, clean};
    if (out[i].shown) {
      placed.push_back(bestBox);
      grid.insert(labelBase + (int)placed.size() - 1, bestBox);
    }
  }
  return out;
}

} // namespace place
