// The engine's side of the placement module: what is on stage, as "ink" the checker understands.
// Bodies and blocks are boxes, curves/lines/links/vectors are polylines, and every outside label is a box
// owned by its thing, so the checker sees exactly what is drawn.
#include "engine.hpp"
#include "place/labels.hpp"
#include "place/regions.hpp"
#include "place/rng.hpp"
#include "place/solve.hpp"
#include <cstdio>
#include <cstdlib>

namespace mk {

using namespace rm;

static place::Rect toRect(Rectangle r) { return {r.x, r.y, r.width, r.height}; }

std::string Engine::inkName(const Mark &m) {
  std::string k = effectiveKind(m);
  if (k == "tex") k = "formula";
  return k + " '" + m.id + "'";
}

place::Picture Engine::picture() {
  place::Picture p;
  std::map<std::string, int> idx; // mark id -> ink
  auto pointerTarget = [&](const Mark &m) -> std::string {
    const Value *at = findProp(m.props, "at");
    return at && at->type == Value::STR && !isWorldKind(effectiveKind(m)) ? baseId(at->str) : "";
  };
  for (int pass = 0; pass < 2; pass++) // things first, then the pointer labels that name them
    for (auto &id : order_) {
      Mark *m = mark(id);
      if (!m || !m->positioned) continue;
      std::string k = effectiveKind(*m);
      if (k == "param" || k == "grid" || k == "path" || k == "region" || k == "line" || k == "arrow" || k == "vector") continue;
      if (isFieldKind(k) || k == "solution") continue;
      if (k == "polygon" || k == "angle" || k == "circle") continue; // outlines, added below
      if (alphaOf(*m) < 0.05f && m->appear >= 1) continue; // hidden: a ghost doesn't take space
      std::string target = pointerTarget(*m);
      if ((pass == 0) != target.empty()) continue;
      Rectangle b = bodyBounds(*m);
      // a thing with no size yet (a bar at zero, still growing) is still a thing: its labels belong to it
      if (b.width <= 0 || b.height <= 0) b = {m->pos.x, m->pos.y, 0.5f, 0.5f};
      place::Ink ink{target.empty() ? place::Ink::Body : place::Ink::Label, id, inkName(*m), toRect(b)};
      if (!target.empty()) {
        auto t = idx.find(target);
        ink.owner = t == idx.end() ? -1 : t->second;
        ink.what = "pointer '" + id + "'";
      }
      if (!focus_.empty() && baseId(focus_) == id) ink.priority = 2;
      idx[id] = p.add(ink);
    }
  auto lineInk = [&](const std::string &id, const std::string &what, const std::vector<Vector2> &pts, int owner, int other) {
    if (pts.size() < 2) return -1;
    place::Ink ink{place::Ink::Link, id, what, {}};
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    for (auto &q : pts) {
      ink.line.push_back({q.x, q.y});
      x0 = std::min(x0, q.x); y0 = std::min(y0, q.y); x1 = std::max(x1, q.x); y1 = std::max(y1, q.y);
    }
    ink.box = {x0, y0, std::max(1.f, x1 - x0), std::max(1.f, y1 - y0)};
    ink.owner = owner;
    ink.other = other;
    return p.add(ink);
  };
  // curves, lines and vectors are strokes, not boxes: polylines (a vector may touch what it's attached to)
  for (auto &id : order_) {
    Mark *m = mark(id);
    if (!m || !m->positioned || alphaOf(*m) < 0.05f || !isAxes(schemaNow())) continue;
    std::string k = effectiveKind(*m);
    if (isCurveKind(k)) {
      auto pts = curvePoints(*m);
      std::vector<Vector2> thin; // every 8th point is plenty for checking
      for (size_t i = 0; i < pts.size(); i += 8) thin.push_back(pts[i]);
      if (!pts.empty()) thin.push_back(pts.back());
      idx[id] = lineInk(id, "curve '" + id + "'", thin, -1, -1);
    } else if (k == "line" || k == "arrow") {
      Vector2 A, B;
      bool hs;
      if (lineEnds(*m, A, B, hs)) idx[id] = lineInk(id, k + " '" + id + "'", {A, B}, -1, -1);
    } else if (k == "polygon" || k == "circle" || k == "angle") { // translucent shapes: only their outline
      std::vector<Vector2> pts;
      if (k == "polygon") {
        auto q = listNums(*m, "pts");
        for (size_t i = 0; i + 1 < q.size(); i += 2) pts.push_back(axes_.toScreen((float)q[i], (float)q[i + 1]));
        if (!pts.empty()) pts.push_back(pts.front());
      } else if (k == "circle") {
        Vector2 ctr = axes_.toScreen((float)num(*m, "x"), (float)num(*m, "y"));
        double r = num(*m, "r");
        float ppy = axes_.area.height / std::max(1e-6f, axes_.ymax - axes_.ymin);
        for (int i = 0; i <= 24; i++) {
          float t = 6.2831853f * i / 24;
          pts.push_back({ctr.x + std::cos(t) * (float)r * ppy, ctr.y + std::sin(t) * (float)r * ppy});
        }
      } else {
        Rectangle b = bodyBounds(*m);
        pts = {{b.x, b.y}, {b.x + b.width, b.y}, {b.x + b.width, b.y + b.height}, {b.x, b.y + b.height}, {b.x, b.y}};
      }
      idx[id] = lineInk(id, k + " '" + id + "'", pts, -1, -1);
    } else if (k == "vector") {
      Vector2 s0, e1;
      if (!vectorEnds(*m, s0, e1)) continue;
      int owner = -1;
      if (const Value *at = findProp(m->props, "at"); at && at->type == Value::STR)
        if (auto t = idx.find(baseId(at->str)); t != idx.end()) owner = t->second;
      idx[id] = lineInk(id, inkName(*m), {s0, e1}, owner, -1);
    }
  }
  for (auto &id : order_) // trails: strokes owned by the thing that leaves them
    if (Mark *m = mark(id); m && m->positioned && findProp(m->props, "trail") && isAxes(schemaNow())) {
      int owner = idx.count(id) ? idx[id] : -1;
      lineInk(id + "~trail", "the trail of '" + id + "'", trailLine(*m), owner, -1);
    }
  // labels: every mark's outside text, owned by the mark (so a name may sit on its own thing)
  for (auto &id : order_) {
    Mark *m = mark(id);
    if (!m || !m->positioned || alphaOf(*m) < 0.05f) continue;
    auto own = idx.find(id);
    for (const TextLabel &t : markLabels(*m)) {
      if (t.text.empty()) continue;
      place::Ink ink{place::Ink::Label, id, (t.role == "inside" ? std::string("name") : t.role) + " of '" + id + "'",
                     toRect(labelBox(t))};
      ink.owner = own == idx.end() ? -1 : own->second;
      ink.maxDist = t.leader ? 120.f : t.role == "category" ? 100.f : 60.f;
      p.add(ink);
    }
  }
  // links: the line as drawn, and its label
  for (auto &id : order_) {
    auto lt = links_.find(id);
    if (lt == links_.end() || alphaOf(lt->second) < 0.05f) continue;
    LinkGeom g = linkGeometry(lt->second);
    if (!g.ok) continue;
    auto a = idx.find(lt->second.a), b = idx.find(lt->second.b);
    int li = lineInk(id, "link " + lt->second.a + "->" + lt->second.b, g.line, a == idx.end() ? -1 : a->second,
                     b == idx.end() ? -1 : b->second);
    if (li < 0 || g.label.empty()) continue;
    Vector2 sz = measure(g.label, 18);
    place::Ink ink{place::Ink::Label, id, "label of link " + lt->second.a + "->" + lt->second.b,
                   {g.mid.x - sz.x / 2 - 6, g.mid.y - sz.y / 2 - 3, sz.x + 12, sz.y + 6}};
    ink.owner = li;
    p.add(ink);
  }
  // labels with no place of their own: the quiet stack in the corner (drawInfo)
  {
    float x = 56, y = 74;
    if (isAxes(schemaNow())) { x = axes_.area.x + 18; y = axes_.area.y + 8; }
    for (auto &id : order_) {
      Mark *m = mark(id);
      if (!m || effectiveKind(*m) != "label" || m->positioned || alphaOf(*m) <= 0.01f) continue;
      std::string t = labelText(*m), mp = magProp(*m);
      if (!mp.empty() && !findProp(m->props, "text")) t += " = " + valueText(*m, mp);
      Vector2 sz = measure(t, 21);
      idx[id] = p.add({place::Ink::Body, id, "label '" + id + "'", {x - 6, y - 3, sz.x + 12, sz.y + 6}});
      y += 30;
    }
  }
  for (auto &id : order_) {
    auto gt = groups_.find(id);
    if (gt == groups_.end()) continue;
    Rectangle r = groupBounds(gt->second);
    if (r.width <= 0) continue;
    place::Ink ink{place::Ink::Group, id, "group '" + id + "'", toRect(r)};
    for (auto &mbr : gt->second.members) if (auto it = idx.find(mbr); it != idx.end()) ink.members.push_back(it->second);
    p.add(ink);
  }
  // data-placed things (axes, numberline, timeline) sit on curves and lines by meaning, not by accident
  {
    std::vector<int> strokes, data;
    for (int i = 0; i < (int)p.ink.size(); i++) {
      const place::Ink &k = p.ink[i];
      if (k.kind == place::Ink::Link && k.owner < 0 && k.other < 0) strokes.push_back(i);
      if (k.kind == place::Ink::Body)
        if (Mark *m = mark(k.id); m && m->direct) data.push_back(i);
    }
    for (int s : strokes) for (int d : data) p.allow(s, d);
    // data-placed things may coincide: shapes in a geometry proof overlap by design, and dots that meet
    // on the axes mean "equal" (that is the semantic check's business, zeroCheck, not layout's)
    for (size_t i = 0; i < data.size(); i++)
      for (size_t j = i + 1; j < data.size(); j++) p.allow(data[i], data[j]);
  }
  // on= / in= / over=: touching on purpose
  for (auto &[id, i] : idx) {
    Mark *m = mark(id);
    if (!m) continue;
    for (const char *k : {"on", "in", "over"})
      if (const Value *v = findProp(m->props, k); v && v->type == Value::STR)
        if (auto t = idx.find(baseId(v->str)); t != idx.end()) p.allow(i, t->second);
  }
  // intended coincidence: related things may touch (a shared category, a link, a group)
  std::vector<std::pair<std::string, int>> bodies(idx.begin(), idx.end());
  for (size_t i = 0; i < bodies.size(); i++)
    for (size_t j = i + 1; j < bodies.size(); j++)
      if (related(bodies[i].first, bodies[j].first)) p.allow(bodies[i].second, bodies[j].second);
  for (const Rectangle &f : furniture()) p.add({place::Ink::Furniture, "axes", "the axis numbers", toRect(f)});
  // reserved screen zones
  if (!title_.empty()) p.add({place::Ink::Zone, "title", "title", {24, 10, measure(title_, 24).x + 4, 32}});
  if (say_.active || say_.lingering > 0) p.add({place::Ink::Zone, "subtitle", "subtitle", {120, 586, 1040, 100}});
  return p;
}

place::Report Engine::layoutCheck(std::vector<std::string> *messages) {
  place::Picture p = picture();
  place::Report r = place::check(p);
  if (std::getenv("TUTOR_DUMP_PICTURE")) { // debugging the checker: every ink, its box and links
    for (size_t i = 0; i < p.ink.size(); i++) {
      auto &k = p.ink[i];
      std::fprintf(stderr, "ink %zu kind=%d %-28s box=%.0f,%.0f %.0fx%.0f owner=%d other=%d members=%zu\n", i, (int)k.kind,
                   k.what.c_str(), k.box.x, k.box.y, k.box.w, k.box.h, k.owner, k.other, k.members.size());
    }
  }
  if (messages)
    for (auto &d : r.diags) messages->push_back(place::describe(p, r, d));
  return r;
}

} // namespace mk

namespace mk {

static float easeOutL(float t) { t = std::clamp(t, 0.f, 1.f); return 1 - (1 - t) * (1 - t) * (1 - t); }
static float easeBackL(float t) { return rm::rate::ease_out_back(std::clamp(t, 0.f, 1.f)); }

// ---------------------------------------------------------------- axis furniture
// where the axes put text: the tick numbers under the x axis and left of the y axis, and the titles
std::vector<Rectangle> Engine::furniture() {
  std::vector<Rectangle> out;
  std::string s = schemaNow();
  if (!isAxes(s)) return out;
  if (s == "plane") {
    auto ax = findProp(schemaProps_, "axes");
    if (ax && ax->num == 0) return out;
  }
  float y0 = std::clamp(0.f, axes_.ymin, axes_.ymax), x0 = std::clamp(0.f, axes_.xmin, axes_.xmax);
  Vector2 xo = axes_.toScreen(axes_.xmin, y0), yo = axes_.toScreen(x0, axes_.ymin);
  out.push_back({axes_.area.x, xo.y + 8, axes_.area.width + 16, 22});  // x tick numbers (and the arrow)
  out.push_back({yo.x - 46, axes_.area.y - 8, 42, axes_.area.height + 16}); // y tick numbers
  if (auto xl = findProp(schemaProps_, "xlabel"); xl && xl->type == Value::STR) {
    Vector2 p = axes_.toScreen(axes_.xmax, y0), sz = measure(xl->str, 18);
    out.push_back({p.x - sz.x, p.y + 28, sz.x, sz.y});
  }
  if (auto yl = findProp(schemaProps_, "ylabel"); yl && yl->type == Value::STR) {
    Vector2 p = axes_.toScreen(x0, axes_.ymax), sz = measure(yl->str, 18);
    out.push_back({p.x + 14, p.y + 4, sz.x, sz.y});
  }
  return out;
}

// ---------------------------------------------------------------- curve / line geometry
std::vector<Vector2> Engine::curvePoints(const Mark &m) {
  if (effectiveKind(m) == "solution") return solutionPoints(m);
  std::vector<Vector2> pts;
  if (isParametric(m)) {
    float t0 = 0, t1 = 6.2831853f;
    if (auto rg = findProp(m.props, "range"); rg && rg->type == Value::LIST && rg->list.size() >= 2) {
      t0 = (float)std::atof(rg->list[0].c_str());
      t1 = (float)std::atof(rg->list[1].c_str());
    }
    for (int i = 0; i <= 400; i++) {
      double px, py;
      if (paramAt(m, t0 + (t1 - t0) * i / 400.0, px, py)) pts.push_back(axes_.toScreen((float)px, (float)py));
    }
    return pts;
  }
  float x0 = axes_.xmin, x1 = axes_.xmax;
  if (auto d = findProp(m.props, "domain"); d && d->type == Value::LIST && d->list.size() >= 2) {
    x0 = std::max(x0, (float)std::atof(d->list[0].c_str()));
    x1 = std::min(x1, (float)std::atof(d->list[1].c_str()));
  }
  return axes_.plot([&](double x) { return pathAt(m, x); }, x0, x1, 360);
}

bool Engine::lineEnds(const Mark &m, Vector2 &A, Vector2 &B, bool &hs) {
  bool h1, h2;
  double x1 = num(m, "x1", &h1), y1 = num(m, "y1"), x2 = num(m, "x2", &h2), y2 = num(m, "y2");
  double slope = num(m, "slope", &hs);
  bool extend = findProp(m.props, "extend") && num(m, "extend") != 0;
  if (hs) {
    double px = num(m, "x"), py = num(m, "y");
    x1 = axes_.xmin; y1 = py + slope * (x1 - px);
    x2 = axes_.xmax; y2 = py + slope * (x2 - px);
  } else if (!h1 || !h2) {
    return false;
  } else if (extend && std::fabs(x2 - x1) > 1e-9) {
    double sl = (y2 - y1) / (x2 - x1), ox = x1, oy = y1;
    x1 = axes_.xmin; y1 = oy + sl * (x1 - ox);
    x2 = axes_.xmax; y2 = oy + sl * (x2 - ox);
  }
  if (!std::isfinite(y1) || !std::isfinite(y2)) return false;
  A = axes_.toScreen((float)x1, (float)y1);
  B = axes_.toScreen((float)x2, (float)y2);
  return true;
}

// ---------------------------------------------------------------- outside text of a mark
std::vector<TextLabel> Engine::markLabels(const Mark &m, bool legacy) {
  std::vector<TextLabel> out = markLabelsAt(m);
  if (legacy || !opt_.newPlacer) return out;
  // the placer moved some of them: same labels, in the same order, at the spots it found
  auto it = labelSlots_.find(m.id);
  if (it == labelSlots_.end() || it->second.size() != out.size()) return out;
  for (size_t i = 0; i < out.size(); i++) {
    const LabelSlot &s = it->second[i];
    if (s.side == -1 || !movableLabel(m, out[i])) continue;
    out[i].at = s.at(); // the glide toward s.box
    out[i].ax = 0;
    out[i].ay = 0;
    out[i].leader = false;
    if (!s.shown) out[i].text.clear();
  }
  return out;
}

std::vector<TextLabel> Engine::markLabelsAt(const Mark &m) {
  std::vector<TextLabel> out;
  std::string k = effectiveKind(m), s = schemaNow();
  if (!m.positioned) return out;
  std::string name = displayName(m), mp = magProp(m), cp = catProp(m), cv = catValue(m);
  auto add = [&](const std::string &t, Vector2 at, float size, float ax, float ay, TextLabel::Ink ink,
                 const char *role) -> TextLabel & {
    TextLabel l;
    l.text = t; l.at = at; l.size = size; l.ax = ax; l.ay = ay; l.ink = ink; l.role = role;
    out.push_back(l);
    return out.back();
  };
  auto category = [&](Vector2 at, float ax, float ay) {
    if (!cv.empty()) add(cv, at, 15, ax, ay, TextLabel::Muted, "category").blendKey = cp;
  };

  if (k == "bar" && m.anchorBottom) {
    Rectangle r = bodyBounds(m);
    if (!mp.empty() && num(m, mp) < 0) { // value at the bar's end below zero, name above zero
      add(valueText(m, mp), {m.pos.x, r.y + r.height + 6}, 22, 0.5f, 0, TextLabel::Fg, "value");
      add(name, {m.pos.x, m.pos.y - 8}, 20, 0.5f, 1, TextLabel::Fg, "name");
      category({m.pos.x, m.pos.y - 32}, 0.5f, 1);
      return out;
    }
    if (!mp.empty()) add(valueText(m, mp), {m.pos.x, r.y - 6}, 22, 0.5f, 1, TextLabel::Fg, "value");
    add(name, {m.pos.x, m.pos.y + 10}, 20, 0.5f, 0, TextLabel::Fg, "name");
    category({m.pos.x, m.pos.y + 34}, 0.5f, 0);
    return out;
  }
  if (k == "bar" && isAxes(s)) {
    Rectangle r = bodyBounds(m);
    std::string key = findProp(m.props, "y") ? "y" : mp;
    if (!key.empty() && r.width > 18) add(valueText(m, key), {r.x + r.width / 2, r.y - 4}, 15, 0.5f, 1, TextLabel::Fg, "value");
    return out;
  }
  if (k == "dot") {
    float r = dotRadius(m) * easeBackL(m.appear);
    Vector2 ctr = m.anchorBottom ? Vector2{m.pos.x, m.pos.y - dotRadius(m)} : m.pos;
    if (m.anchorBottom) {
      float y = m.pos.y + 10;
      add(name, {m.pos.x, y}, 20, 0.5f, 0, TextLabel::Fg, "name");
      y += 24;
      if (!mp.empty()) { add(valueText(m, mp), {m.pos.x, y}, 17, 0.5f, 0, TextLabel::Muted, "value"); y += 20; }
      category({m.pos.x, y}, 0.5f, 0);
    } else if (s == "numberline") {
      TextLabel &l = add(name, {ctr.x, ctr.y - r - 8 - m.labelLift}, 17, 0.5f, 1, TextLabel::Fg, "name");
      if (m.labelLift > 0) {
        l.leader = true;
        l.leaderFrom = {ctr.x, ctr.y - r - 2};
        l.leaderTo = {ctr.x, ctr.y - r - 6 - m.labelLift};
      }
    } else if (s == "timeline") { // alternate above / below with a leader line
      int i = m.order % 2;
      Vector2 lp{ctr.x, ctr.y + (i ? 64.f : -64.f)};
      TextLabel &l = add(name, lp, 19, 0.5f, i ? 0.f : 1.f, TextLabel::Fg, "name");
      l.leader = true;
      l.leaderFrom = {ctr.x, ctr.y + (i ? r : -r)};
      l.leaderTo = {lp.x, lp.y - (i ? 6 : -6)};
      l.leaderWidth = 1.5f;
      l.leaderAlpha = 0.7f;
      category(lp + Vector2{0, i ? 22.f : -22.f}, 0.5f, i ? 0.f : 1.f);
    } else if (isAxes(s)) { // the corner layout found free of other labels and dots
      int q = m.labelQuad;
      add(name, {ctr.x + (q % 2 ? -10.f : 10.f), ctr.y + (q >= 2 ? 10.f : -10.f)}, 17, q % 2 ? 1.f : 0.f,
          q >= 2 ? 0.f : 1.f, TextLabel::Fg, "name");
    } else { // graph
      add(name, {ctr.x, ctr.y + r + 6}, 18, 0.5f, 0, TextLabel::Fg, "name");
    }
    return out;
  }
  if (k == "label" && s == "numberline" && !nlProp_.empty() && findProp(m.props, nlProp_)) { // ▲ mean 62
    add(labelText(m) + " " + valueText(m, nlProp_), {m.pos.x, nl_.a.y + 42}, 18, 0.5f, 0, TextLabel::Mark, "label");
    return out;
  }
  if (!isAxes(s)) return out;
  if (isWorldKind(k)) { // names of shapes in the world
    bool named = findProp(m.props, "label") || findProp(m.props, "name");
    float e = easeOutL(m.appear);
    float ppy = axes_.area.height / std::max(1e-6f, axes_.ymax - axes_.ymin);
    auto S = [&](double x, double y) { return axes_.toScreen((float)x, (float)y); };
    if (k == "circle" && named) {
      Vector2 ctr = S(num(m, "x"), num(m, "y"));
      double r = num(m, "r");
      if (!std::isfinite(r) || r <= 0) r = 0.5;
      add(name, {ctr.x, ctr.y + (float)r * ppy + 8}, 18, 0.5f, 0, TextLabel::Fg, "name");
    } else if (k == "rect" && named) { // inside if it fits, else above
      double x = num(m, "x"), y = num(m, "y"), w = num(m, "w"), h = num(m, "h");
      if (!std::isfinite(w)) w = 1;
      if (!std::isfinite(h)) h = 1;
      Vector2 p0 = S(x - w / 2, y + h / 2), p1 = S(x + w / 2, y - h / 2);
      Rectangle r{p0.x, p0.y, p1.x - p0.x, p1.y - p0.y};
      float sc = 0.8f + 0.2f * e;
      Rectangle rr{r.x + r.width * (1 - sc) / 2, r.y + r.height * (1 - sc) / 2, r.width * sc, r.height * sc};
      if (measure(name, 18).x < rr.width - 8 && rr.height > 26)
        add(name, {rr.x + rr.width / 2, rr.y + rr.height * 0.42f}, 18, 0.5f, 0.5f, TextLabel::Fg, "name").role = "inside";
      else
        add(name, {rr.x + rr.width / 2, rr.y - 6}, 18, 0.5f, 1, TextLabel::Fg, "name");
    } else if (k == "polygon" && named) {
      auto p = listNums(m, "pts");
      Vector2 ctr{0, 0};
      int n = 0;
      for (size_t i = 0; i + 1 < p.size(); i += 2, n++) ctr += S(p[i], p[i + 1]);
      if (n >= 2) add(name, ctr * (1.f / n), 18, 0.5f, 0.5f, TextLabel::Fg, "name").role = "inside";
    } else if (k == "angle") { // its name, or its size in degrees, on the bisector
      auto p = listNums(m, "pts");
      if (p.size() >= 6) {
        Vector2 v = S(p[0], p[1]), A = S(p[2], p[3]), B = S(p[4], p[5]);
        float ta = std::atan2(A.y - v.y, A.x - v.x), tb = std::atan2(B.y - v.y, B.x - v.x);
        float d = tb - ta;
        while (d > 3.14159265f) d -= 6.2831853f;
        while (d < -3.14159265f) d += 6.2831853f;
        float deg = std::fabs(d) * 57.29578f, rad = 42, mid = ta + d / 2;
        add(named ? name : fmtNum(std::round(deg)) + "°", {v.x + std::cos(mid) * (rad + 22), v.y + std::sin(mid) * (rad + 22)},
            18, 0.5f, 0.5f, TextLabel::Mark, "name");
      }
    } else if (k == "vector" && named && e > 0.6f) { // past the tip
      Vector2 s0, e1;
      if (vectorEnds(m, s0, e1)) {
        Vector2 dir = norm(e1 - s0);
        add(name, e1 + Vector2{dir.y * 14 + dir.x * 10, -dir.x * 14 + dir.y * 10}, 19, 0.5f, 0.5f, TextLabel::Mark, "name");
      }
    }
    return out;
  }
  if (isFieldKind(k)) { // a field's name: a legend in the top-left corner of the axes, one line per field
    int idx = 0;
    for (auto &id : order_) {
      if (id == m.id) break;
      if (Mark *o = mark(id); o && isFieldKind(effectiveKind(*o)) && findProp(o->props, "label") && opacityOf(*o) > 0.3f) idx++;
    }
    if (findProp(m.props, "label") && opacityOf(m) > 0.3f) add(name, {axes_.area.x + 16, axes_.area.y + 34 + idx * 26.f}, 19, 0, 0, TextLabel::Mark, "name");
    return out;
  }
  if (isCurveKind(k) && m.appear > 0.95f) {
    auto pts = curvePoints(m);
    if (pts.empty()) return out;
    if (isParametric(m) || k == "solution") {
      add(name, pts.back() + Vector2{8, -8}, 19, 0, 1, TextLabel::Mark, "name");
    } else {
      Vector2 e = pts.back();
      e.y = std::clamp(e.y, axes_.area.y + 10, axes_.area.y + axes_.area.height - 10);
      add(name, e + Vector2{-6, -8}, 19, 1, 1, TextLabel::Mark, "name");
    }
    return out;
  }
  if (k == "region") {
    auto of = findProp(m.props, "of");
    Mark *p = of ? mark(of->str) : nullptr;
    if (!p) return out;
    double f = num(m, "from"), t = num(m, "to");
    if (!std::isfinite(f)) f = axes_.xmin;
    if (!std::isfinite(t)) t = axes_.xmax;
    double mid = (f + t) / 2;
    std::string label = name;
    if (!mp.empty()) label += " " + valueText(m, mp);
    if (std::fabs(t - f) > (axes_.xmax - axes_.xmin) * 0.06)
      add(label, axes_.toScreen((float)mid, (float)(pathAt(*p, mid) / 2)), 17, 0.5f, 0.5f, TextLabel::Fg, "name");
    return out;
  }
  if (k == "line" || k == "arrow") {
    Vector2 A, B;
    bool hs;
    if (!lineEnds(m, A, B, hs)) return out;
    Vector2 L = B; // near the visible end
    L.x = std::clamp(L.x, axes_.area.x + 40, axes_.area.x + axes_.area.width - 10);
    L.y = std::clamp(L.y, axes_.area.y + 20, axes_.area.y + axes_.area.height - 10);
    std::string label = name;
    if (hs) label += "  slope " + valueText(m, "slope");
    add(label, L + Vector2{-8, -8}, 17, 1, 1, TextLabel::Mark, "name");
  }
  return out;
}

// a vector's tail (at=<thing>, its side, or x y) and tip (dx dy, to=[x,y] or to=<thing>), on screen
bool Engine::vectorEnds(const Mark &m, Vector2 &s0, Vector2 &e1) {
  float ppx = axes_.area.width / std::max(1e-6f, axes_.xmax - axes_.xmin);
  float ppy = axes_.area.height / std::max(1e-6f, axes_.ymax - axes_.ymin);
  double sx = num(m, "x"), sy = num(m, "y");
  const Value *at = findProp(m.props, "at");
  Rectangle box;
  if (at && at->type == Value::STR && anchorBox(at->str, box)) {
    s0 = {box.x + box.width / 2, box.y + box.height / 2};
    if (const Value *sd = findProp(m.props, "side"); sd && sd->type == Value::STR) {
      if (sd->str == "left") s0.x = box.x;
      if (sd->str == "right") s0.x = box.x + box.width;
      if (sd->str == "top") s0.y = box.y;
      if (sd->str == "bottom") s0.y = box.y + box.height;
    }
  } else if (std::isfinite(sx) && std::isfinite(sy)) {
    s0 = axes_.toScreen((float)sx, (float)sy);
  } else {
    return false;
  }
  const Value *to = findProp(m.props, "to");
  if (to && to->type == Value::LIST && to->list.size() >= 2) {
    auto t = listNums(m, "to");
    e1 = axes_.toScreen((float)t[0], (float)t[1]);
  } else if (to && to->type == Value::STR && anchorBox(to->str, box)) {
    e1 = {box.x + box.width / 2, box.y + box.height / 2};
  } else {
    double dx = num(m, "dx"), dy = num(m, "dy");
    if (!std::isfinite(dx)) dx = 0;
    if (!std::isfinite(dy)) dy = 0;
    e1 = {s0.x + (float)dx * ppx, s0.y - (float)dy * ppy};
  }
  return true;
}

Rectangle Engine::labelBox(const TextLabel &t) {
  Vector2 sz = measure(t.text, t.size);
  return {t.at.x - sz.x * t.ax, t.at.y - sz.y * t.ay, sz.x, sz.y};
}

// ---------------------------------------------------------------- link geometry
LinkGeom Engine::linkGeometry(const Link &l, bool legacy) {
  LinkGeom g = linkGeometryAt(l);
  if (!legacy && opt_.newPlacer && g.ok)
    if (auto it = linkSlots_.find(l.id); it != linkSlots_.end() && it->second.side != -1)
      g.mid = {it->second.at().x + it->second.box.width / 2, it->second.at().y + it->second.box.height / 2};
  return g;
}

LinkGeom Engine::linkGeometryAt(const Link &l) {
  LinkGeom g;
  Rectangle ra, rb;
  Mark *ma = mark(l.a), *mb = mark(l.b);
  auto boundsOf = [&](const std::string &id, Mark *m, Rectangle &out) {
    if (m) { if (!m->positioned) return false; out = bodyBounds(*m); return true; }
    auto gr = groups_.find(id);
    if (gr == groups_.end()) return false;
    out = groupBounds(gr->second);
    return out.width > 0;
  };
  if (!boundsOf(l.a, ma, ra) || !boundsOf(l.b, mb, rb)) return g;
  g.ok = true;
  // label text: label=…, else the properties
  if (auto v = findProp(l.props, "label"); v && v->type == Value::STR) {
    g.label = v->str;
    // a named value: label="w₁₁" w=0.5 reads "w₁₁ = 0.5", and the number follows a morph
    for (auto &p : l.props)
      if (p.key != "label" && p.key != "color" && p.key != "bend" && p.key != "opacity" && p.val.isNumeric()) { g.label += " = " + fmtNum(num(p.val)); break; }
  } else
    for (auto &p : l.props) {
      if (p.key == "color" || p.key == "opacity" || p.key == "bend") continue; // style, not content
      if (!g.label.empty()) g.label += "  ";
      if (p.val.type == Value::STR) g.label += p.val.str;
      else g.label += p.key + " " + (p.val.isNumeric() ? fmtNum(num(p.val)) : p.val.markup());
    }
  if (l.kind == "similar") g.label = g.label.empty() ? "≈" : "≈ " + g.label;
  if (l.kind == "equals") g.label = g.label.empty() ? "=" : "= " + g.label;

  g.overBars = ma && mb && ma->anchorBottom && mb->anchorBottom && effectiveKind(*ma) == "bar" && effectiveKind(*mb) == "bar";
  if (g.overBars) {
    // between things on a baseline: an arc over the top, both ends one height above the taller mark,
    // so the arc never cuts through value labels
    float y = std::min(std::min(ra.y, ma->pos.y), std::min(rb.y, mb->pos.y)) - 40;
    g.pa = {ra.x + ra.width / 2, y};
    g.pb = {rb.x + rb.width / 2, y};
    g.bend = (g.pb.x >= g.pa.x ? 1 : -1) * std::clamp(std::fabs(g.pb.x - g.pa.x) * 0.4f, 40.f, 120.f);
    g.mid = arcPoint(g.pa, g.pb, g.bend, 0.5f) - Vector2{0, 16};
  } else {
    Vector2 ca{ra.x + ra.width / 2, ra.y + ra.height / 2}, cb{rb.x + rb.width / 2, rb.y + rb.height / 2};
    Vector2 pa = rectEdgePoint(ra, cb), pb = rectEdgePoint(rb, ca);
    if (ma && effectiveKind(*ma) == "dot") pa = ca + norm(cb - ca) * (ra.width / 2);
    if (mb && effectiveKind(*mb) == "dot") pb = cb + norm(ca - cb) * (rb.width / 2);
    Vector2 d = norm(pb - pa);
    g.pa = pa + d * 6.f;
    g.pb = pb - d * 6.f;
    // bend=… curves a link; a link with a partner running the other way bends on its own
    if (auto bv = findProp(l.props, "bend")) g.bend = (float)num(*bv);
    else
      for (auto &[oid, o] : links_)
        if (o.a == l.b && o.b == l.a && oid != l.id) { g.bend = 34; break; }
    g.mid = std::fabs(g.bend) > 0.5f ? arcPoint(g.pa, g.pb, g.bend, 0.5f) : lerp(g.pa, g.pb, 0.5f);
  }
  if (std::fabs(g.bend) > 0.5f)
    for (int i = 0; i <= 24; i++) g.line.push_back(arcPoint(g.pa, g.pb, g.bend, i / 24.f));
  else
    g.line = {g.pa, g.pb};
  return g;
}

} // namespace mk

namespace mk {

// Which labels the placer may move. Names beside dots on axes and graphs, curve and line names,
// vector names and the outside names of shapes: the ones fixed offsets put on top of each other.
// Bar names under their baseline, number-line and timeline labels keep their schema's place.
bool Engine::movableLabel(const Mark &m, const TextLabel &l) {
  std::string k = effectiveKind(m), s = schemaNow();
  if (l.role == "inside") return false;
  if (k == "dot") return !m.anchorBottom && (isAxes(s) || s == "graph");
  if (isCurveKind(k) || k == "line" || k == "arrow") return true;
  if (k == "vector" || k == "circle" || k == "rect") return true;
  return false;
}

void Engine::placeAllLabels(float dt) {
  place::LabelWorld world;
  world.bounds = {16, 44, 1248, 540};
  auto R = [](Rectangle r) { return place::Rect{r.x, r.y, r.width, r.height}; };
  std::map<std::string, int> obstacle;
  // things are obstacles; a label may sit on its own thing
  for (auto &id : order_) {
    Mark *m = mark(id);
    if (!m || !m->positioned || alphaOf(*m) < 0.05f) continue;
    std::string k = effectiveKind(*m);
    if (k == "path" || k == "region" || k == "line" || k == "arrow" || k == "param" || k == "grid" || k == "vector") continue;
    if (isFieldKind(k) || k == "solution") continue;
    Rectangle b = bodyBounds(*m);
    if (b.width <= 0 || b.height <= 0) continue;
    obstacle[id] = (int)world.obstacles.size();
    world.obstacles.push_back(R(b));
  }
  // labels that keep their place are obstacles too; strokes (curves, lines, vectors, links) shouldn't be covered
  struct Pending { std::string id; size_t index; Rectangle anchor; Vector2 size; std::vector<place::Side> sides; std::vector<place::Vec> along; int priority; };
  std::vector<Pending> pending;
  auto thin = [](const std::vector<Vector2> &pts) {
    std::vector<place::Vec> o;
    for (size_t i = 0; i < pts.size(); i += 8) o.push_back({pts[i].x, pts[i].y});
    if (!pts.empty()) o.push_back({pts.back().x, pts.back().y});
    return o;
  };
  using S = place::Side;
  for (auto &id : order_) {
    Mark *m = mark(id);
    if (!m || !m->positioned || alphaOf(*m) < 0.05f) continue;
    std::string k = effectiveKind(*m);
    std::vector<Vector2> stroke;
    if (isAxes(schemaNow())) {
      if (isCurveKind(k)) stroke = curvePoints(*m);
      else if (k == "line" || k == "arrow") { Vector2 A, B; bool hs; if (lineEnds(*m, A, B, hs)) stroke = {A, B}; }
      else if (k == "vector") { Vector2 a, b; if (vectorEnds(*m, a, b)) stroke = {a, b}; }
    }
    if (!stroke.empty()) world.strokes.push_back(thin(stroke));
    if (findProp(m->props, "trail") && isAxes(schemaNow())) { // labels keep off trails too
      std::vector<place::Vec> tl;
      for (auto &q : trailLine(*m)) tl.push_back({q.x, q.y});
      if (tl.size() > 1) world.strokes.push_back(tl);
    }
    auto labels = markLabelsAt(*m);
    for (size_t i = 0; i < labels.size(); i++) {
      const TextLabel &l = labels[i];
      if (l.text.empty()) continue;
      Rectangle box = labelBox(l);
      if (!movableLabel(*m, l)) { world.obstacles.push_back(R(box)); continue; }
      Pending p{id, i, box, {box.width, box.height}, {}, {}, 2};
      if (k == "dot") {
        p.anchor = bodyBounds(*m);
        p.sides = isAxes(schemaNow()) ? std::vector<S>{S::NE, S::NW, S::SE, S::SW, S::E, S::W, S::N, S::S}
                                      : std::vector<S>{S::S, S::N, S::E, S::W, S::SE, S::SW, S::NE, S::NW};
      } else if (isCurveKind(k) && !stroke.empty()) { // near the end of the curve (inside the axes), above or below it
        Vector2 e = stroke.back();
        e.x = std::clamp(e.x, axes_.area.x, axes_.area.x + axes_.area.width);
        e.y = std::clamp(e.y, axes_.area.y + 10, axes_.area.y + axes_.area.height - 10);
        p.anchor = {e.x - 3, e.y - 3, 6, 6};
        p.sides = {S::NW, S::SW, S::N, S::S, S::W, S::NE, S::SE};
        for (float f : {0.92f, 0.82f, 0.7f}) {
          Vector2 q = stroke[(size_t)(f * (stroke.size() - 1))];
          p.along.push_back({q.x, q.y - box.height / 2 - 10});
          p.along.push_back({q.x, q.y + box.height / 2 + 10});
        }
      } else if ((k == "line" || k == "arrow" || k == "vector") && stroke.size() == 2) { // by its far end, or its middle
        Vector2 e = stroke[1];
        if (k != "vector") { // a line's far end can be off the axes: use where it leaves them
          e.x = std::clamp(e.x, axes_.area.x + 40, axes_.area.x + axes_.area.width - 10);
          e.y = std::clamp(e.y, axes_.area.y + 20, axes_.area.y + axes_.area.height - 10);
        }
        p.anchor = {e.x - 4, e.y - 4, 8, 8};
        p.sides = {S::NE, S::NW, S::SE, S::SW, S::E, S::W, S::N, S::S};
        Vector2 mid = lerp(stroke[0], stroke[1], 0.5f);
        p.along = {{mid.x, mid.y - box.height / 2 - 10}, {mid.x, mid.y + box.height / 2 + 10}};
      } else {
        p.anchor = bodyBounds(*m);
        p.sides = {S::S, S::N, S::E, S::W};
      }
      if (!focus_.empty() && baseId(focus_) == id) p.priority = 3;
      pending.push_back(p);
    }
  }
  // link labels: at the middle of the link, or a little along it
  struct PendingLink { std::string id; Vector2 size; place::Vec mid; std::vector<place::Vec> along; };
  std::vector<PendingLink> plinks;
  for (auto &id : order_) {
    auto lt = links_.find(id);
    if (lt == links_.end() || alphaOf(lt->second) < 0.05f) continue;
    LinkGeom g = linkGeometryAt(lt->second);
    if (!g.ok) continue;
    std::vector<place::Vec> line;
    for (auto &q : g.line) line.push_back({q.x, q.y});
    world.strokes.push_back(line);
    if (g.label.empty()) continue;
    Vector2 sz = measure(g.label, 18);
    PendingLink pl{id, {sz.x + 12, sz.y + 6}, {g.mid.x, g.mid.y}, {}};
    auto at = [&](float t) {
      if (g.line.size() < 2) return Vector2{g.mid.x, g.mid.y};
      float f = t * (g.line.size() - 1);
      size_t i0 = std::min((size_t)f, g.line.size() - 2);
      return lerp(g.line[i0], g.line[i0 + 1], f - i0);
    };
    for (float t : {0.5f, 0.35f, 0.65f, 0.25f, 0.75f}) {
      Vector2 q = at(t);
      pl.along.push_back({q.x, q.y});
    }
    plinks.push_back(pl);
  }
  // axis furniture: tick numbers along the axes and the axis titles
  for (const Rectangle &f : furniture()) world.obstacles.push_back(R(f));
  // reserved screen areas
  if (!title_.empty()) world.zones.push_back({24, 10, measure(title_, 24).x + 4, 32});
  world.zones.push_back({(float)W - 380, 4, 376, 24}); // key hints
  if (say_.active || say_.lingering > 0) world.zones.push_back({120, 586, 1040, 100});

  std::vector<place::LabelReq> reqs;
  for (auto &p : pending) {
    place::LabelReq r;
    r.anchor = R(p.anchor);
    r.size = {p.size.x, p.size.y};
    r.priority = p.priority;
    r.sides = p.sides;
    r.along = p.along;
    if (auto it = obstacle.find(p.id); it != obstacle.end()) r.owner = it->second;
    if (auto s = labelSlots_.find(p.id); s != labelSlots_.end() && p.index < s->second.size()) r.prevSide = s->second[p.index].side;
    reqs.push_back(r);
  }
  for (auto &pl : plinks) {
    place::LabelReq r;
    r.anchor = {pl.mid.x - 2, pl.mid.y - 2, 4, 4};
    r.size = {pl.size.x, pl.size.y};
    r.priority = 1;
    // on the link if there's room; beside its middle if not (a very short link, or one over things)
    r.sides = {place::Side::C, place::Side::N, place::Side::S, place::Side::E, place::Side::W};
    r.along = pl.along;
    if (auto s = linkSlots_.find(pl.id); s != linkSlots_.end()) r.prevSide = s->second.side;
    reqs.push_back(r);
  }
  auto out = place::placeLabels(reqs, world);
  // glide to the new spots (critically damped springs: a side change is a short move, not a jump)
  bool snap = opt_.instant || dt <= 0 || dt >= 0.5f;
  auto settle = [&](LabelSlot &s, const LabelSlot *prev) {
    float ox = s.box.x - s.anchor.x, oy = s.box.y - s.anchor.y; // where it should sit, relative to its thing
    if (prev && !snap) {
      s.x = prev->x;
      s.y = prev->y;
      s.x.step(ox, 0.08f, dt);
      s.y.step(oy, 0.08f, dt);
    } else {
      s.x = {ox, 0};
      s.y = {oy, 0};
    }
  };
  std::map<std::string, std::vector<LabelSlot>> slots;
  for (size_t i = 0; i < pending.size(); i++) {
    auto &v = slots[pending[i].id];
    auto labels = markLabelsAt(*mark(pending[i].id));
    if (v.size() < labels.size()) v.resize(labels.size());
    LabelSlot s;
    s.box = {out[i].box.x, out[i].box.y, out[i].box.w, out[i].box.h};
    s.side = out[i].side;
    s.shown = out[i].shown;
    s.anchor = {pending[i].anchor.x, pending[i].anchor.y};
    const LabelSlot *prev = nullptr;
    if (auto it = labelSlots_.find(pending[i].id); it != labelSlots_.end() && pending[i].index < it->second.size() &&
                                                   it->second[pending[i].index].side != -1)
      prev = &it->second[pending[i].index];
    settle(s, prev);
    v[pending[i].index] = s;
    if (reqs[i].prevSide != -1 && reqs[i].prevSide != out[i].side) labelFlips++;
  }
  std::map<std::string, LabelSlot> lslots;
  for (size_t i = 0; i < plinks.size(); i++) {
    const auto &o = out[pending.size() + i];
    LabelSlot s;
    s.box = {o.box.x, o.box.y, o.box.w, o.box.h};
    s.side = o.side;
    s.shown = o.shown;
    s.anchor = {plinks[i].mid.x, plinks[i].mid.y};
    auto it = linkSlots_.find(plinks[i].id);
    settle(s, it != linkSlots_.end() && it->second.side != -1 ? &it->second : nullptr);
    lslots[plinks[i].id] = s;
  }
  labelSlots_ = std::move(slots);
  linkSlots_ = std::move(lslots);
}

} // namespace mk

namespace mk {

bool Engine::hasPlacement(const Mark &m) const {
  for (auto &p : m.props) if (isPlacementKey(p.key)) return true;
  return false;
}

// Marks placed by words. Everything else on stage is a fixed obstacle; these are solved by the
// placement module (regions and relations with kiwi, then overlap removal) whenever the scene's
// structure changes, and eased to their spots like any other move.
void Engine::placeRelated() {
  std::vector<std::string> movable;
  for (auto &id : order_)
    if (Mark *m = mark(id); m && hasPlacement(*m) && !isWorldKind(effectiveKind(*m)) && effectiveKind(*m) != "param")
      movable.push_back(id);
  if (movable.empty()) { relatedPos_.clear(); return; }
  place::Scene sc;
  sc.stage = {40, 66, 1200, 494};
  std::map<std::string, int> idx;
  auto boxOf = [&](Mark &m) { // where layout puts it this frame (tpos), not where it is drawn yet
    Mark at = m;
    at.pos = m.tpos;
    Rectangle b = bodyBounds(at);
    if (b.width <= 0 || b.height <= 0) b = {m.tpos.x - 20, m.tpos.y - 10, 40, 20};
    return b;
  };
  // everything that has a place already: fixed
  for (auto &id : order_) {
    Mark *m = mark(id);
    if (!m || !m->positioned || hasPlacement(*m) || alphaOf(*m) < 0.05f) continue;
    std::string k = effectiveKind(*m);
    if (k == "path" || k == "region" || k == "line" || k == "arrow" || k == "param" || k == "grid" || k == "vector") continue;
    if (isFieldKind(k) || k == "solution") continue;
    Rectangle b = boxOf(*m);
    place::Item it;
    it.id = id;
    it.size = {b.width, b.height};
    it.mode = place::Item::Fixed;
    it.target = {b.x + b.width / 2, b.y + b.height / 2};
    idx[id] = (int)sc.items.size();
    sc.items.push_back(it);
  }
  for (const Rectangle &f : furniture()) { // axis numbers and titles are in the way too
    place::Item it;
    it.size = {f.width, f.height};
    it.mode = place::Item::Fixed;
    it.target = {f.x + f.width / 2, f.y + f.height / 2};
    sc.items.push_back(it);
  }
  // the marks placed by words
  for (auto &id : movable) {
    Mark &m = *mark(id);
    Rectangle b = boxOf(m);
    place::Item it;
    it.id = id;
    it.size = {b.width, b.height};
    it.mode = place::Item::Preferred;
    auto prev = relatedPos_.find(id);
    it.target = prev != relatedPos_.end() ? place::Vec{prev->second.x, prev->second.y} : place::Vec{640, 313};
    if (prev != relatedPos_.end()) it.prev = it.target;
    if (const Value *r = findProp(m.props, "region"); r && r->type == Value::STR) {
      it.region = place::regionRect(r->str, sc.stage);
      if (!it.region) warn("layout: '" + id + "' region=" + r->str + " is not a region (top, bottom, left, right, center, top-left … bottom-right, or a1 … f6)");
    }
    idx[id] = (int)sc.items.size();
    sc.items.push_back(it);
  }
  static const std::pair<const char *, place::Rel> kRel[] = {
      {"below", place::Rel::Below}, {"above", place::Rel::Above}, {"left-of", place::Rel::LeftOf},
      {"right-of", place::Rel::RightOf}, {"near", place::Rel::Near}, {"align", place::Rel::Align},
      {"on", place::Rel::On}, {"in", place::Rel::In}, {"over", place::Rel::Over}};
  for (auto &id : movable) {
    Mark &m = *mark(id);
    for (auto &[key, rel] : kRel) {
      const Value *v = findProp(m.props, key);
      if (!v || v->type != Value::STR) continue;
      auto t = idx.find(baseId(v->str));
      if (t == idx.end()) { warn("layout: '" + id + "' " + key + "=" + v->str + ": no such thing on stage"); continue; }
      sc.rels.push_back({rel, idx[id], t->second, 16, 2});
      if (rel == place::Rel::On || rel == place::Rel::In || rel == place::Rel::Over)
        sc.mayOverlap.push_back({idx[id], t->second});
    }
  }
  // solve only when the structure changed (what is where, how big, related how)
  uint64_t sig = 1469598103934665603ull;
  for (auto &it : sc.items) {
    sig = place::fnv1a(it.id + "|" + std::to_string((int)(it.size.x / 4)) + "x" + std::to_string((int)(it.size.y / 4)) + "@" +
                           (it.mode == place::Item::Fixed ? std::to_string((int)(it.target.x / 4)) + "," + std::to_string((int)(it.target.y / 4)) : ""),
                       sig);
  }
  for (auto &r : sc.rels) sig = place::fnv1a(std::to_string((int)r.kind) + ":" + std::to_string(r.a) + ">" + std::to_string(r.b), sig);
  if (sig != relatedSig_) {
    relatedSig_ = sig;
    place::Placement p = place::solveBodies(sc);
    if (std::getenv("TUTOR_DUMP_PLACE")) {
      for (size_t i = 0; i < sc.items.size(); i++)
        std::fprintf(stderr, "place %-6s %s size %.0fx%.0f target %.0f,%.0f -> %.0f,%.0f\n", sc.items[i].id.c_str(),
                     sc.items[i].mode == place::Item::Fixed ? "fixed" : "free ", sc.items[i].size.x, sc.items[i].size.y,
                     sc.items[i].target.x, sc.items[i].target.y, p.pos[i].x, p.pos[i].y);
      for (auto &r : sc.rels) std::fprintf(stderr, "  rel %d: %d -> %d\n", (int)r.kind, r.a, r.b);
    }
    for (auto &id : movable) relatedPos_[id] = {p.pos[idx[id]].x, p.pos[idx[id]].y};
    for (auto &n : p.notes)
      if (n.code == place::Code::RelationUnsat && n.a >= 0 && n.b >= 0)
        warn("layout: '" + sc.items[n.a].id + "' can't keep its place relative to '" + sc.items[n.b].id +
             "' (the relations conflict, or there is no room); try a region= instead");
      else if (n.code == place::Code::Crowded)
        warn("layout: the stage is too crowded to keep everything apart; hide what is done, or use a new scene");
  }
  for (auto &id : movable) {
    Mark &m = *mark(id);
    Vector2 c = relatedPos_[id];
    Rectangle b = boxOf(m);
    // tpos is the centre for most kinds; anchorBottom things stand on their foot
    m.tpos = c;
    if (effectiveKind(m) == "bar") { m.anchorBottom = true; m.tpos = {c.x, c.y + b.height / 2}; }
    m.positioned = true;
    m.direct = false;
  }
}

} // namespace mk
