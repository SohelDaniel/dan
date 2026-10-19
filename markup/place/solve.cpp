#include "solve.hpp"
#include "separate.hpp"
#include "../../third_party/kiwi/kiwi/kiwi.h"
#include <cmath>
#include <set>

namespace place {

Placement solveBodies(const Scene &sc) {
  using namespace kiwi;
  Placement out;
  size_t n = sc.items.size();
  out.pos.resize(n);
  Solver solver;
  std::vector<Variable> cx(n), cy(n);
  auto add = [&](const Constraint &c) {
    try { solver.addConstraint(c); return true; }
    catch (const std::exception &) { return false; } // unsatisfiable / duplicate: drop it, never throw
  };
  std::vector<bool> related(n, false); // placed by words: the words decide, not the default target
  for (const Relation &r : sc.rels) if (r.a >= 0 && r.a < (int)n) related[r.a] = true;
  for (size_t i = 0; i < n; i++) {
    const Item &it = sc.items[i];
    Vec tgt = it.target;
    if (it.region) tgt = it.region->center(); // a named region replaces the schema's proposal
    if (it.mode == Item::Fixed) { // data positions are facts: constants
      add(cx[i] == it.target.x);
      add(cy[i] == it.target.y);
      continue;
    }
    float hw = it.size.x / 2, hh = it.size.y / 2;
    double inside = hw * 2 <= sc.stage.w && hh * 2 <= sc.stage.h ? strength::required : strength::strong;
    add((cx[i] - hw >= sc.stage.x) | inside);
    add((cx[i] + hw <= sc.stage.right()) | inside);
    add((cy[i] - hh >= sc.stage.y) | inside);
    add((cy[i] + hh <= sc.stage.bottom()) | inside);
    if (it.region) {
      const Rect &r = *it.region;
      add((cx[i] - hw >= r.x) | strength::strong);
      add((cx[i] + hw <= r.right()) | strength::strong);
      add((cy[i] - hh >= r.y) | strength::strong);
      add((cy[i] + hh <= r.bottom()) | strength::strong);
    }
    double pull = related[i] && !it.region ? strength::weak : strength::medium;
    add((cx[i] == tgt.x) | pull);
    add((cy[i] == tgt.y) | pull);
    if (it.prev) {
      add((cx[i] == it.prev->x) | strength::weak);
      add((cy[i] == it.prev->y) | strength::weak);
    }
  }
  std::set<std::pair<int, int>> allowed;
  for (auto [a, b] : sc.mayOverlap) allowed.insert({std::min(a, b), std::max(a, b)});
  for (const Relation &r : sc.rels) {
    if (r.a < 0 || r.b < 0 || r.a >= (int)n || r.b >= (int)n || r.a == r.b) continue;
    const Item &A = sc.items[r.a], &B = sc.items[r.b];
    double st = r.strength >= 2 ? strength::strong : r.strength == 1 ? strength::medium : strength::weak;
    int a = r.a, b = r.b;
    float wa = A.size.x / 2, ha = A.size.y / 2, wb = B.size.x / 2, hb = B.size.y / 2;
    bool ok = true;
    switch (r.kind) {
    case Rel::Below:
      ok = add((cy[a] - ha >= cy[b] + hb + r.gap) | st);
      add((cx[a] == cx[b]) | strength::medium);
      break;
    case Rel::Above:
      ok = add((cy[a] + ha <= cy[b] - hb - r.gap) | st);
      add((cx[a] == cx[b]) | strength::medium);
      break;
    case Rel::RightOf:
      ok = add((cx[a] - wa >= cx[b] + wb + r.gap) | st);
      add((cy[a] == cy[b]) | strength::medium);
      break;
    case Rel::LeftOf:
      ok = add((cx[a] + wa <= cx[b] - wb - r.gap) | st);
      add((cy[a] == cy[b]) | strength::medium);
      break;
    case Rel::Align: ok = add((cy[a] == cy[b]) | st); break;
    case Rel::Near: // within reach on any side; pulled close, and overlap removal decides which side
      ok = add((cx[a] - cx[b] <= wa + wb + r.gap + 40) | st) && add((cx[b] - cx[a] <= wa + wb + r.gap + 40) | st) &&
           add((cy[a] - cy[b] <= ha + hb + r.gap + 40) | st) && add((cy[b] - cy[a] <= ha + hb + r.gap + 40) | st);
      add((cx[a] == cx[b]) | strength::medium);
      add((cy[a] == cy[b]) | strength::medium);
      break;
    case Rel::On: // sitting on top of b, touching
      ok = add((cy[a] + ha == cy[b] - hb) | st) && add((cx[a] == cx[b]) | st);
      allowed.insert({std::min(a, b), std::max(a, b)});
      break;
    case Rel::In:
      ok = add((cx[a] - wa >= cx[b] - wb) | st) && add((cx[a] + wa <= cx[b] + wb) | st) &&
           add((cy[a] - ha >= cy[b] - hb) | st) && add((cy[a] + ha <= cy[b] + hb) | st);
      add((cx[a] == cx[b]) | strength::medium);
      add((cy[a] == cy[b]) | strength::medium);
      allowed.insert({std::min(a, b), std::max(a, b)});
      break;
    case Rel::Over:
      ok = add((cx[a] == cx[b]) | st) && add((cy[a] == cy[b]) | st);
      allowed.insert({std::min(a, b), std::max(a, b)});
      break;
    }
    if (!ok) out.notes.push_back({Code::RelationUnsat, a, b});
  }
  try { solver.updateVariables(); } catch (const std::exception &) {}
  SeparateInput sep;
  for (size_t i = 0; i < n; i++) {
    sep.centre.push_back({(float)cx[i].value(), (float)cy[i].value()});
    sep.size.push_back(sc.items[i].size);
    sep.weight.push_back(sc.items[i].mode == Item::Fixed ? 1e6 : 1.0);
  }
  // two fixed things that overlap (the x and y axis-number bands crossing at a centred origin) are facts,
  // not ours to separate: only free things move
  for (size_t i = 0; i < n; i++)
    for (size_t j = i + 1; j < n; j++)
      if (sc.items[i].mode == Item::Fixed && sc.items[j].mode == Item::Fixed) allowed.insert({(int)i, (int)j});
  sep.allowed.assign(allowed.begin(), allowed.end());
  for (const Relation &r : sc.rels) { // the ordering relations survive overlap removal
    if (r.a < 0 || r.b < 0 || r.a >= (int)n || r.b >= (int)n || r.a == r.b) continue;
    const Item &A = sc.items[r.a], &B = sc.items[r.b];
    double gy = (A.size.y + B.size.y) / 2.0 + r.gap, gx = (A.size.x + B.size.x) / 2.0 + r.gap;
    if (r.kind == Rel::Below) sep.keepY.push_back({r.b, r.a, gy});
    if (r.kind == Rel::Above) sep.keepY.push_back({r.a, r.b, gy});
    if (r.kind == Rel::RightOf) sep.keepX.push_back({r.b, r.a, gx});
    if (r.kind == Rel::LeftOf) sep.keepX.push_back({r.a, r.b, gx});
  }
  sep.gap = sc.gap;
  sep.bounds = sc.stage;
  int left = 0;
  out.pos = separate(sep, &left);
  for (size_t i = 0; i < n; i++) // and they stay exactly where they are
    if (sc.items[i].mode == Item::Fixed) out.pos[i] = sc.items[i].target;
  if (left) out.notes.push_back({Code::Crowded, -1, -1});
  // report every relation the final layout doesn't keep (conflicting strong relations are traded off
  // by the solver, not thrown, so this is where the author learns about them)
  for (const Relation &r : sc.rels) {
    if (r.a < 0 || r.b < 0 || r.a >= (int)n || r.b >= (int)n || r.a == r.b) continue;
    Rect A = Rect::around(out.pos[r.a], sc.items[r.a].size), B = Rect::around(out.pos[r.b], sc.items[r.b].size);
    const float tol = 3;
    bool kept = true;
    switch (r.kind) {
    case Rel::Below: kept = A.y >= B.bottom() + r.gap - tol; break;
    case Rel::Above: kept = A.bottom() <= B.y - r.gap + tol; break;
    case Rel::RightOf: kept = A.x >= B.right() + r.gap - tol; break;
    case Rel::LeftOf: kept = A.right() <= B.x - r.gap + tol; break;
    case Rel::Align: kept = std::fabs(A.center().y - B.center().y) <= tol; break;
    case Rel::On: kept = std::fabs(A.bottom() - B.y) <= tol; break;
    case Rel::In: kept = contains(B, A, tol); break;
    case Rel::Over: kept = length(A.center() - B.center()) <= tol; break;
    case Rel::Near: kept = length(A.center() - B.center()) <= (A.w + B.w + A.h + B.h) / 2 + r.gap + 60; break;
    }
    bool noted = false;
    for (auto &nt : out.notes) noted |= nt.code == Code::RelationUnsat && nt.a == r.a && nt.b == r.b;
    if (!kept && !noted) out.notes.push_back({Code::RelationUnsat, r.a, r.b});
  }
  return out;
}

} // namespace place
