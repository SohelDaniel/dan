// Schemas: each owns its layout, so there is no general constraint solver.
//   free        things left-to-right in declaration order, bars on a shared zero baseline
//   numberline  one magnitude -> x
//   axes        two magnitudes -> x, y (functions, regions, lines, points)
//   timeline    order in time -> x
//   graph       things + connections, no axes (layered if acyclic, force-directed otherwise)
#include "engine.hpp"
#include <algorithm>
#include <cmath>
#include <functional>

namespace mk {

using namespace rm;

static const Rectangle kStage{40, 66, 1200, 494};

std::string Engine::schemaNow() const { return schema_ != "auto" ? schema_ : autoSchema_; }

static bool listRange(const Value *v, float &lo, float &hi) {
  if (!v || v->type != Value::LIST || v->list.size() < 2) return false;
  lo = (float)std::atof(v->list[0].c_str());
  hi = (float)std::atof(v->list[1].c_str());
  return hi > lo;
}

static bool kPositionalForBox(const std::string &k) {
  if (isPlacementKey(k)) return true;
  static const std::set<std::string> skip = {"x",    "y",       "t",    "at",   "year", "r",  "w",  "fn",    "of",
                                             "domain", "pos",  "shape", "bend", "range", "xt", "yt", "opacity", "side",
                                             "to",   "dx",     "dy",    "h",    "size", "line", "n", "m"};
  return skip.count(k) > 0;
}

static float easeOut(float t) { return 1 - (1 - t) * (1 - t) * (1 - t); }

// ================================================================ sizes
float Engine::dotRadius(const Mark &m) {
  std::string s = schemaNow();
  if (auto r = findProp(m.props, "r")) return std::max(2.0, num(*r));
  if (s == "numberline" || s == "timeline") return 10;
  if (isAxes(s)) return 7;
  std::string k = magProp(m);
  if (k.empty()) return 16;
  double mx = std::max(1e-9, maxAbs_.count(k) ? maxAbs_[k] : 1.0);
  double v = std::fabs(num(m, k));
  if (!std::isfinite(v)) v = 0;
  return (float)std::max(s == "graph" ? 14.0 : 5.0, std::sqrt(std::min(v, mx * 4) / mx) * 44.0); // area ∝ magnitude
}

std::string Engine::valueText(const Mark &m, const std::string &key) {
  double v = num(m, key);
  if (!std::isfinite(v)) return "?";
  bool isInt = propIsInt_.count(key) ? propIsInt_[key] : false;
  std::string s = fmtNum(isInt ? std::round(v) : v);
  if (auto u = findProp(m.props, "unit"); u && u->type == Value::STR) {
    if (u->str == "$" || u->str == "€" || u->str == "£") s = u->str + s;
    else s += " " + u->str;
  }
  return s;
}

Vector2 Engine::boxSize(const Mark &m) {
  float w = measure(displayName(m), 22).x;
  int lines = 0;
  for (auto &p : m.props) {
    if (p.key == "label" || p.key == "name" || p.key == "color" || p.key == "unit") continue;
    if (kPositionalForBox(p.key)) continue;
    std::string t = p.key + " " + (p.val.isNumeric() ? valueText(m, p.key) : p.val.markup());
    w = std::max(w, measure(t, 17).x);
    lines++;
  }
  if (!m.category.empty()) { w = std::max(w, measure(m.category, 15).x); lines++; }
  return {w + 32, 44.f + 22.f * lines};
}

Rectangle Engine::bodyBounds(const Mark &m) {
  std::string k = effectiveKind(m), s = schemaNow();
  Vector2 p = m.pos;
  if (k == "bar" && m.anchorBottom) {
    std::string mp = magProp(m);
    double v = mp.empty() ? 0 : num(m, mp);
    if (!std::isfinite(v)) v = 0;
    float h = (float)v * m.barScale * easeOut(m.appear);
    return h >= 0 ? Rectangle{p.x - 34, p.y - h, 68, h} : Rectangle{p.x - 34, p.y, 68, -h};
  }
  if (k == "bar" && isAxes(s)) {
    float x = (float)num(m, "x");
    std::string mp = findProp(m.props, "y") ? "y" : magProp(m);
    float y = mp.empty() ? 0 : (float)num(m, mp) * easeOut(m.appear);
    float w = findProp(m.props, "w") ? (float)num(m, "w") : (axes_.xmax - axes_.xmin) / 40.f;
    Vector2 a = axes_.toScreen(x - w / 2, std::max(0.f, y)), b = axes_.toScreen(x + w / 2, std::min(0.f, y));
    return {a.x, a.y, b.x - a.x, b.y - a.y};
  }
  if (k == "dot") {
    float r = dotRadius(m);
    Vector2 c = m.anchorBottom ? Vector2{p.x, p.y - r} : p;
    return {c.x - r, c.y - r, 2 * r, 2 * r};
  }
  if (k == "box" || k == "bar") {
    Vector2 sz = boxSize(m);
    if (m.anchorBottom) return {p.x - sz.x / 2, p.y - sz.y, sz.x, sz.y};
    return {p.x - sz.x / 2, p.y - sz.y / 2, sz.x, sz.y};
  }
  if (k == "label") {
    if (s == "numberline" && !nlProp_.empty() && findProp(m.props, nlProp_)) // a ▲ marker under the line
      return {p.x - 8, nl_.a.y + 22, 16, 14};
    Vector2 sz = measure(displayName(m), 20);
    return {p.x - sz.x / 2, p.y - sz.y / 2, sz.x, sz.y};
  }
  if (k == "tex") {
    float px = texSize(m) / 1000.f;
    TexPtr L = texNow(m);
    Vector2 sz = L && L->ok ? Vector2{L->w * px, L->h * px} : Vector2{texSize(m) * 4, texSize(m) * 1.3f};
    return {p.x - sz.x / 2, p.y - sz.y / 2, sz.x, sz.y};
  }
  if (isBlockKind(k)) {
    Vector2 sz = blockSize(m);
    return {p.x - sz.x / 2, p.y - sz.y / 2, sz.x, sz.y};
  }
  if (isWorldKind(k) && isAxes(s)) {
    float x0, x1, y0, y1;
    if (!worldExtent(m, x0, x1, y0, y1)) return {p.x, p.y, 0, 0};
    Vector2 a = axes_.toScreen(x0, y1), b = axes_.toScreen(x1, y0);
    return {a.x, a.y, std::max(1.f, b.x - a.x), std::max(1.f, b.y - a.y)};
  }
  return {p.x, p.y, 0, 0};
}

Rectangle Engine::fullBounds(const Mark &m) {
  Rectangle r = bodyBounds(m);
  std::string k = effectiveKind(m);
  if (m.anchorBottom) {
    float below = 34 + (catValue(m).empty() ? 0 : 18) + (k == "dot" && !magProp(m).empty() ? 20 : 0);
    float top = std::min(r.y - (k == "bar" ? 30 : 0), m.pos.y - (k == "bar" ? 30 : 0));
    float w = std::max(r.width, measure(displayName(m), 20).x + 8);
    float bot = std::max(m.pos.y + below, r.y + r.height + 28);
    return {m.pos.x - w / 2, top, w, bot - top};
  }
  if (k == "dot") {
    float w = std::max(r.width, measure(displayName(m), 18).x + 8);
    return {m.pos.x - w / 2, r.y, w, r.height + 28};
  }
  return r;
}

Rectangle Engine::groupBounds(const Group &g, int depth) {
  float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
  for (auto &id : g.members) {
    Rectangle r{0, 0, 0, 0};
    bool ok = false;
    if (Mark *m = mark(id)) { r = fullBounds(*m); ok = m->positioned || m->placed; }
    else if (auto it = groups_.find(id); it != groups_.end() && depth < 8) { r = groupBounds(it->second, depth + 1); ok = r.width > 0; }
    if (!ok) continue;
    x0 = std::min(x0, r.x); y0 = std::min(y0, r.y); x1 = std::max(x1, r.x + r.width); y1 = std::max(y1, r.y + r.height);
  }
  if (x0 > x1) return {0, 0, 0, 0};
  const float pad = 14;
  return {x0 - pad, y0 - pad - 24, (x1 - x0) + 2 * pad, (y1 - y0) + 2 * pad + 24};
}

float Engine::footprint(const Mark &m) {
  std::string k = effectiveKind(m);
  float name = measure(displayName(m), 20).x;
  if (k == "bar") return std::max(104.f, name + 24);
  if (k == "dot") return std::max(2 * dotRadius(m), name) + 30;
  if (k == "box") return boxSize(m).x + 26;
  return name + 30;
}

// ================================================================ world shapes and blocks
std::vector<double> Engine::listNums(const Mark &m, const std::string &key) {
  std::vector<double> out;
  const Value *v = findProp(m.props, key);
  if (!v || v->type != Value::LIST) return out;
  for (auto &s : v->list) out.push_back(num(Value::parse(s)));
  // mid-morph: ease from the old numbers (same length) to the new ones
  if (auto it = m.strBlend.find(key); it != m.strBlend.end() && it->second.second < 1) {
    Value old = Value::parse(it->second.first);
    if (old.type == Value::LIST && old.list.size() == out.size()) {
      float t = it->second.second;
      for (size_t i = 0; i < out.size(); i++) {
        double a = num(Value::parse(old.list[i]));
        if (std::isfinite(a)) out[i] = a + (out[i] - a) * t;
      }
    }
  }
  return out;
}

// the box a world shape covers, in world units (false if it has none yet)
bool Engine::worldExtent(const Mark &m, float &x0, float &x1, float &y0, float &y1) {
  std::string k = m.kind;
  bool hx, hy;
  double x = num(m, "x", &hx), y = num(m, "y", &hy);
  auto box = [&](double a0, double a1, double b0, double b1) {
    if (!std::isfinite(a0 + a1 + b0 + b1)) return false;
    x0 = (float)std::min(a0, a1); x1 = (float)std::max(a0, a1);
    y0 = (float)std::min(b0, b1); y1 = (float)std::max(b0, b1);
    return true;
  };
  if (k == "circle" && hx && hy) { double r = num(m, "r"); return box(x - r, x + r, y - r, y + r); }
  if (k == "rect" && hx && hy) {
    double w = num(m, "w"), h = num(m, "h");
    if (!std::isfinite(w)) w = 1;
    if (!std::isfinite(h)) h = 1;
    return box(x - w / 2, x + w / 2, y - h / 2, y + h / 2);
  }
  if (k == "polygon" || k == "angle") {
    auto p = listNums(m, "pts");
    if (p.size() < 4) return false;
    double a0 = 1e18, a1 = -1e18, b0 = 1e18, b1 = -1e18;
    for (size_t i = 0; i + 1 < p.size(); i += 2) {
      a0 = std::min(a0, p[i]); a1 = std::max(a1, p[i]);
      b0 = std::min(b0, p[i + 1]); b1 = std::max(b1, p[i + 1]);
    }
    return box(a0, a1, b0, b1);
  }
  if (k == "vector") {
    double sx = x, sy = y;
    if (auto at = findProp(m.props, "at"); at && at->type == Value::STR) {
      Mark *t = mark(baseId(at->str));
      if (!t) return false;
      sx = num(*t, "x", &hx); sy = num(*t, "y", &hy);
    }
    if (!hx || !hy) return false;
    double ex = sx + num(m, "dx"), ey = sy + num(m, "dy");
    if (auto to = findProp(m.props, "to"); to && to->type == Value::LIST && to->list.size() >= 2) {
      auto t = listNums(m, "to");
      ex = t[0]; ey = t[1];
    }
    return box(sx, ex, sy, ey);
  }
  return false;
}

static std::vector<std::string> codeLines(const std::string &s) {
  std::vector<std::string> out;
  size_t i = 0;
  for (size_t p; (p = s.find("\\n", i)) != std::string::npos; i = p + 2) out.push_back(s.substr(i, p - i));
  out.push_back(s.substr(i));
  return out;
}

static const float kCellH = 58;

Vector2 Engine::blockSize(const Mark &m) {
  std::string k = effectiveKind(m);
  const Value *tv = findProp(m.props, "text");
  std::string txt = tv ? (tv->type == Value::STR ? tv->str : labelText(m)) : "";
  const Value *lv = findProp(m.props, "label");
  float head = lv ? 26.f : 0.f;
  if (k == "card") {
    auto lines = wrap(txt, 24, 520);
    float w = lv ? measure(lv->str, 17).x : 0;
    for (auto &l : lines) w = std::max(w, measure(l, 24).x);
    return {w + 40, head + lines.size() * 31.f + 30};
  }
  if (k == "code") {
    auto lines = codeLines(txt);
    float w = lv ? measure(lv->str, 17).x : 0;
    for (auto &l : lines) w = std::max(w, measureMono(l, 20).x);
    return {w + 74, head + lines.size() * 27.f + 26};
  }
  if (k == "array") {
    const Value *iv = findProp(m.props, "items");
    size_t n = iv && iv->type == Value::LIST ? iv->list.size() : 0;
    float cw = 58;
    if (iv) for (auto &s : iv->list) cw = std::max(cw, measure(s, 24).x + 26);
    // room underneath for the pointer labels aimed at its cells (at=a[k]), stacked if several share one
    std::map<std::string, int> per;
    int depth = 0;
    for (auto &[pid, pm] : marks_)
      if (auto at = findProp(pm.props, "at"); at && at->type == Value::STR && baseId(at->str) == m.id)
        depth = std::max(depth, ++per[at->str]);
    return {std::max(1.f, n * cw), head + kCellH + 26 + (depth ? 52.f : 0)};
  }
  if (k == "table") {
    const Value *cv = findProp(m.props, "cells");
    int cols = std::max(1, (int)num(m, "cols"));
    size_t n = cv && cv->type == Value::LIST ? cv->list.size() : 0;
    int rows = (int)((n + cols - 1) / cols);
    float w = 0;
    for (int c = 0; c < cols; c++) {
      float cw = 40;
      for (int r = 0; r < rows; r++)
        if (size_t i = (size_t)(r * cols + c); i < n) cw = std::max(cw, measure(cv->list[i], 20).x + 28);
      w += cw;
    }
    return {w, head + rows * 38.f};
  }
  return {0, 0};
}

Rectangle Engine::cellRect(const Mark &m, int k) {
  Vector2 sz = blockSize(m);
  const Value *iv = findProp(m.props, "items");
  size_t n = iv && iv->type == Value::LIST ? std::max<size_t>(1, iv->list.size()) : 1;
  float cw = sz.x / n, head = findProp(m.props, "label") ? 26.f : 0.f;
  return {m.pos.x - sz.x / 2 + k * cw, m.pos.y - sz.y / 2 + head, cw, kCellH};
  // (the pointer room, if any, is below the cells: cells stay at the top of the block)
}

bool Engine::anchorBox(const std::string &ref, Rectangle &out) {
  int cell;
  Mark *t = mark(baseId(ref, &cell));
  if (!t || !t->positioned) return false;
  if (cell >= 0 && effectiveKind(*t) == "array") {
    out = cellRect(*t, cell);
    if (size_t c = ref.find(':'); c != std::string::npos) { // a span: a[1:3] covers cells 1 to 3
      int last = std::atoi(ref.c_str() + c + 1);
      for (int k = cell + 1; k <= last && k < cell + 64; k++) {
        Rectangle r = cellRect(*t, k);
        out.width = r.x + r.width - out.x;
      }
    }
  } else {
    out = bodyBounds(*t);
  }
  return true;
}

// ================================================================ ranges (eased, stable)
void Engine::computeRanges(float dt) {
  std::string s = schemaNow();
  std::string sig = s + "|" + std::to_string(marks_.size());
  bool recompute = sig != rangeSig_;
  rangeSig_ = sig;
  float k = rangesInit_ ? std::min(1.f, dt * 5) : 1.f;

  if (s == "numberline") {
    if (auto p = findProp(schemaProps_, "prop"); p && p->type == Value::STR) nlProp_ = p->str;
    else {
      std::map<std::string, int> counts;
      for (auto &[id, m] : marks_) { std::string mp = magProp(m); if (!mp.empty()) counts[mp]++; }
      int best = 0;
      for (auto &[kk, c] : counts) if (c > best) { best = c; nlProp_ = kk; }
    }
    float &tlo = nlTarget_[0], &thi = nlTarget_[1];
    if (recompute || !rangesInit_) {
      float lo, hi;
      if (!listRange(findProp(schemaProps_, "range"), lo, hi)) {
        lo = 1e9f; hi = -1e9f;
        if (minVal_.count(nlProp_)) { lo = (float)minVal_[nlProp_]; hi = (float)maxVal_[nlProp_]; }
        for (auto &[id, m] : marks_) {
          bool ok;
          double v = num(m, nlProp_, &ok);
          if (ok && std::isfinite(v)) { lo = std::min(lo, (float)v); hi = std::max(hi, (float)v); }
        }
        if (lo > hi) { lo = 0; hi = 10; }
        auto z = findProp(schemaProps_, "zero");
        niceRange(lo, hi, z && z->num != 0);
      }
      tlo = lo; thi = hi;
    }
    nlMin_ = lerpf(nlMin_, tlo, k);
    nlMax_ = lerpf(nlMax_, thi, k);
  }

  if (isAxes(s)) {
    float &tx0 = axTarget_[0], &tx1 = axTarget_[1], &ty0 = axTarget_[2], &ty1 = axTarget_[3];
    if (recompute || !rangesInit_) {
      float x0, x1, y0, y1;
      bool hx = listRange(findProp(schemaProps_, "x"), x0, x1);
      bool hy = listRange(findProp(schemaProps_, "y"), y0, y1);
      if (!hx) {
        x0 = 1e9f; x1 = -1e9f;
        for (const char *key : {"x", "x1", "x2", "from", "to"})
          if (minVal_.count(key)) { x0 = std::min(x0, (float)minVal_[key]); x1 = std::max(x1, (float)maxVal_[key]); }
        for (auto &[id, m] : marks_) {
          for (const char *key : {"x", "x1", "x2", "from", "to"}) {
            bool ok;
            double v = num(m, key, &ok);
            if (ok && std::isfinite(v)) { x0 = std::min(x0, (float)v); x1 = std::max(x1, (float)v); }
          }
          float a, b;
          if (listRange(findProp(m.props, "domain"), a, b)) { x0 = std::min(x0, a); x1 = std::max(x1, b); }
          float wx0, wx1, wy0, wy1;
          if (worldExtent(m, wx0, wx1, wy0, wy1)) { x0 = std::min(x0, wx0); x1 = std::max(x1, wx1); }
          if (isParametric(m)) {
            float t0 = 0, t1 = 6.2831853f;
            listRange(findProp(m.props, "range"), t0, t1);
            for (int i = 0; i <= 120; i++) {
              double px, py;
              if (paramAt(m, t0 + (t1 - t0) * i / 120.0, px, py)) { x0 = std::min(x0, (float)px); x1 = std::max(x1, (float)px); }
            }
          }
        }
        if (x0 > x1) { x0 = -1; x1 = 5; }
        niceRange(x0, x1, true);
      }
      if (!hy) {
        y0 = 1e9f; y1 = -1e9f;
        for (const char *key : {"y", "y1", "y2"})
          if (minVal_.count(key)) { y0 = std::min(y0, (float)minVal_[key]); y1 = std::max(y1, (float)maxVal_[key]); }
        for (auto &[id, m] : marks_) {
          std::string kind = effectiveKind(m);
          float wx0, wx1, wy0, wy1;
          if (worldExtent(m, wx0, wx1, wy0, wy1)) { y0 = std::min(y0, wy0); y1 = std::max(y1, wy1); }
          for (const char *key : {"y", "y1", "y2"}) {
            bool ok;
            double v = num(m, key, &ok);
            if (ok && std::isfinite(v)) { y0 = std::min(y0, (float)v); y1 = std::max(y1, (float)v); }
          }
          if (isParametric(m)) {
            float t0 = 0, t1 = 6.2831853f;
            listRange(findProp(m.props, "range"), t0, t1);
            for (int i = 0; i <= 120; i++) {
              double px, py;
              if (paramAt(m, t0 + (t1 - t0) * i / 120.0, px, py)) { y0 = std::min(y0, (float)py); y1 = std::max(y1, (float)py); }
            }
          } else if (kind == "path") {
            float a = x0, b = x1;
            listRange(findProp(m.props, "domain"), a, b);
            std::vector<float> ys;
            for (int i = 0; i <= 120; i++) {
              double y = pathAt(m, a + (b - a) * i / 120.0);
              if (std::isfinite(y)) ys.push_back((float)y);
            }
            if (!ys.empty()) {
              std::sort(ys.begin(), ys.end()); // ignore asymptotes: 2%..98%
              y0 = std::min(y0, ys[ys.size() * 2 / 100]);
              y1 = std::max(y1, ys[ys.size() * 98 / 100]);
            }
          }
        }
        if (y0 > y1) { y0 = -1; y1 = 5; }
        auto z = findProp(schemaProps_, "zero");
        niceRange(y0, y1, !(z && z->num == 0));
      }
      tx0 = x0; tx1 = x1; ty0 = y0; ty1 = y1;
    }
    axX0_ = lerpf(axX0_, tx0, k); axX1_ = lerpf(axX1_, tx1, k);
    axY0_ = lerpf(axY0_, ty0, k); axY1_ = lerpf(axY1_, ty1, k);
  }

  if (s == "timeline") {
    float &tlo = tlTarget_[0], &thi = tlTarget_[1];
    if (recompute || !rangesInit_) {
      float lo, hi;
      if (!listRange(findProp(schemaProps_, "range"), lo, hi)) {
        lo = 1e9f; hi = -1e9f;
        for (const char *key : {"t", "at", "year"}) {
          if (minVal_.count(key)) { lo = std::min(lo, (float)minVal_[key]); hi = std::max(hi, (float)maxVal_[key]); }
          for (auto &[id, m] : marks_) {
            bool ok;
            double v = num(m, key, &ok);
            if (ok && std::isfinite(v)) { lo = std::min(lo, (float)v); hi = std::max(hi, (float)v); }
          }
        }
        if (lo > hi) { lo = 0; hi = 10; }
        float pad = std::max(0.5f, (hi - lo) * 0.08f);
        lo -= pad; hi += pad;
      }
      tlo = lo; thi = hi;
    }
    tlMin_ = lerpf(tlMin_, tlo, k);
    tlMax_ = lerpf(tlMax_, thi, k);
  }
  rangesInit_ = true;
}

// ================================================================ main layout
void Engine::layout(float dt) {
  lastDt_ = dt;
  for (auto &[id, m] : marks_) { m.positioned = false; m.anchorBottom = false; m.direct = false; m.culled = false; }
  computeRanges(dt);
  std::string s = schemaNow();

  std::vector<std::string> body; // marks that need a place in free/graph layouts
  for (auto &id : order_) {
    Mark *m = mark(id);
    if (!m) continue;
    std::string k = effectiveKind(*m);
    if (k == "label" || k == "path" || k == "region" || k == "line" || k == "arrow" || k == "param") continue;
    if (isFieldKind(k) || k == "solution") continue;
    if (isBlockKind(k) || isWorldKind(k)) continue;
    if (findProp(m->props, "pos") || hasPlacement(*m)) continue; // placed by hand / by relations
    if (auto at = findProp(m->props, "at"); at && at->type == Value::STR) continue; // placed at another thing
    body.push_back(id);
  }

  // blocks (formulas, cards, code, arrays, tables) with no place of their own stack in a band at the
  // top, and the schema gets the rest. With nothing else on a free stage, the stack is centred.
  Rectangle stage = kStage;
  {
    std::vector<Mark *> blocks;
    float total = 0;
    for (auto &id : order_) {
      Mark *m = mark(id);
      if (!m || !isBlockKind(effectiveKind(*m)) || findProp(m->props, "pos") || hasPlacement(*m)) continue;
      if (isAxes(s) && findProp(m->props, "x") && findProp(m->props, "y")) continue;
      blocks.push_back(m);
      total += bodyBounds(*m).height + 14;
    }
    bool alone = body.empty() && (s == "free" || s == "graph");
    float y = alone ? kStage.y + std::max(4.f, (kStage.height - total) / 2) : kStage.y + 4;
    for (Mark *m : blocks) {
      float h = bodyBounds(*m).height;
      m->tpos = {kStage.x + kStage.width / 2, y + h / 2};
      m->positioned = true;
      y += h + 14;
    }
    if (!blocks.empty() && !alone) { stage.y += total + 6; stage.height -= total + 6; }
  }

  if (s == "free") layoutFree(stage, body);
  else if (s == "graph") layoutGraph(stage, body);
  else if (s == "numberline") layoutNumberline(stage);
  else if (isAxes(s)) layoutAxes(stage);
  else if (s == "timeline") layoutTimeline(stage);

  // pos=[fx,fy]: a hand placement in stage fractions, in any schema
  for (auto &[id, m] : marks_) {
    const Value *pv = findProp(m.props, "pos");
    if (!pv || pv->type != Value::LIST || pv->list.size() < 2) continue;
    float fx = (float)std::atof(pv->list[0].c_str()), fy = (float)std::atof(pv->list[1].c_str());
    m.tpos = {kStage.x + fx * kStage.width, kStage.y + fy * kStage.height};
    m.anchorBottom = false;
    m.positioned = true;
    m.direct = false;
    if (effectiveKind(m) == "bar" && !isAxes(s)) { // still a bar: pos is the foot of its zero line
      std::string mp = magProp(m);
      auto bs = barScale_.find(mp);
      double mx = std::max(1e-9, maxAbs_.count(mp) ? maxAbs_[mp] : 1.0);
      m.barScale = bs != barScale_.end() ? bs->second : (float)(kStage.height * 0.45 / mx);
      m.anchorBottom = true;
    }
  }

  // at=<thing> or at=a[3]: a pointer label sits under what it points at (and glides when re-aimed);
  // several on one target stand side by side
  std::map<std::string, int> total, placedAt;
  for (auto &[id, m] : marks_)
    if (auto at = findProp(m.props, "at"); at && at->type == Value::STR && !isWorldKind(effectiveKind(m))) total[at->str]++;
  for (auto &id : order_) {
    Mark *m = mark(id);
    if (!m || findProp(m->props, "pos") || hasPlacement(*m) || isWorldKind(effectiveKind(*m))) continue;
    const Value *at = findProp(m->props, "at");
    Rectangle box;
    if (!at || at->type != Value::STR || !anchorBox(at->str, box)) continue;
    int n = placedAt[at->str]++, cnt = total[at->str];
    float w = 0;
    for (auto &[oid, om] : marks_)
      if (auto oat = findProp(om.props, "at"); oat && oat->type == Value::STR && oat->str == at->str)
        w = std::max(w, measure(labelText(om), 19).x + 14);
    m->tpos = {box.x + box.width / 2 + (n - (cnt - 1) / 2.f) * w, box.y + box.height + 52};
    m->anchorBottom = false;
    m->positioned = true;
    m->direct = false;
  }

  placeRelated(); // marks placed by words (region=, below=, …): solved against everything else on stage

  for (auto &[id, m] : marks_) {
    if (!m.positioned) continue;
    if (!m.placed || m.direct) { m.pos = m.tpos; m.placed = true; }
    else m.pos = smoothApproach(m.pos, m.tpos, dt, 9);
  }
  for (auto &[id, m] : marks_) if (m.culled) m.positioned = false; // out of the camera's view: not drawn, no labels
  if (opt_.newPlacer) placeAllLabels(opt_.instant ? 1.f : dt);
}

// ---------------------------------------------------------------- free
void Engine::layoutFree(Rectangle area, const std::vector<std::string> &ids) {
  if (ids.empty()) return;
  std::set<std::string> eligible(ids.begin(), ids.end());
  struct Item { std::string id; bool group = false; std::vector<Item> kids; float w = 0; };
  const float gap = 26, pad = 18;

  std::function<std::string(const std::string &)> topGroup = [&](const std::string &id) {
    std::string g = groupOf(id), last;
    int guard = 0;
    while (!g.empty() && guard++ < 16) { last = g; g = groupOf(g); }
    return last;
  };
  auto propOf = [&](const Item &it) -> std::string {
    if (it.group) return "";
    Mark &m = marks_[it.id];
    return effectiveKind(m) == "bar" ? magProp(m) : "";
  };
  // extra room between neighbours: a new bar scale starts, or a straight link (and its label) joins them
  auto pairGap = [&](const Item &prev, const Item &cur) -> float {
    float extra = 0;
    if (propOf(cur) != propOf(prev) && !propOf(cur).empty()) extra += 40;
    for (auto &[lid, l] : links_) {
      if (!((l.a == prev.id && l.b == cur.id) || (l.a == cur.id && l.b == prev.id))) continue;
      if (propOf(cur) != "" && propOf(prev) != "") continue; // bars use an arc over the top
      float lw = 60;
      if (auto v = findProp(l.props, "label"); v && v->type == Value::STR) lw = measure(v->str, 18).x + 70;
      else for (auto &p : l.props) lw = std::max(lw, measure(p.val.markup(), 18).x + 110);
      extra = std::max(extra, lw - gap);
    }
    return extra;
  };

  std::function<Item(const std::string &, int)> build = [&](const std::string &gid, int depth) {
    Item it;
    it.id = gid;
    it.group = true;
    float w = 0;
    for (auto &mid : groups_[gid].members) {
      if (eligible.count(mid)) {
        Item k;
        k.id = mid;
        k.w = footprint(marks_[mid]);
        it.kids.push_back(k);
      } else if (groups_.count(mid) && depth < 8) {
        it.kids.push_back(build(mid, depth + 1));
      } else continue;
      w += it.kids.back().w;
      if (it.kids.size() > 1) w += pairGap(it.kids[it.kids.size() - 2], it.kids.back());
    }
    it.w = w + gap * std::max(0, (int)it.kids.size() - 1) + 2 * pad;
    return it;
  };

  std::vector<Item> items;
  std::set<std::string> emitted;
  for (auto &id : ids) {
    std::string g = topGroup(id);
    if (g.empty()) {
      Item it;
      it.id = id;
      it.w = footprint(marks_[id]);
      items.push_back(it);
    } else if (!emitted.count(g)) {
      emitted.insert(g);
      items.push_back(build(g, 0));
    }
  }

  // bars: a shared zero, one scale per property (fees don't borrow the balance scale)
  bool hasBars = false;
  for (auto &id : ids) if (effectiveKind(marks_[id]) == "bar") hasBars = true;
  // positive and negative extents per property (whole script + now); zero sits where they meet
  std::map<std::string, double> posMax, negMax;
  for (auto &id : ids) {
    Mark &m = marks_[id];
    if (effectiveKind(m) != "bar") continue;
    std::string mp = magProp(m);
    if (mp.empty()) continue;
    double v = num(m, mp);
    double hi = std::max({0.0, maxVal_.count(mp) ? maxVal_[mp] : 0.0, std::isfinite(v) ? v : 0.0});
    double lo = std::max({0.0, minVal_.count(mp) ? -minVal_[mp] : 0.0, std::isfinite(v) ? -v : 0.0});
    // max= / min= : the author's promise of the largest value an expression will reach (keeps the scale fixed)
    if (const Value *mv = findProp(m.props, "max"); mv && mv->isNumeric()) hi = std::max(hi, num(*mv));
    if (const Value *mv = findProp(m.props, "min"); mv && mv->isNumeric()) lo = std::max(lo, -num(*mv));
    posMax[mp] = std::max(posMax[mp], hi);
    negMax[mp] = std::max(negMax[mp], lo);
  }
  float negFrac = 0;
  for (auto &[k, p] : posMax) negFrac = std::max(negFrac, (float)(negMax[k] / std::max(1e-9, p + negMax[k])));
  float top = area.y + 64 + (groups_.empty() ? 0 : 30), bottom = area.y + area.height - 52;
  if (negFrac > 0) bottom -= 30; // names of negative bars sit above zero; leave room for values below
  float avail = bottom - top;
  float baseline = hasBars ? top + avail * (1 - negFrac) : area.y + area.height * 0.62f;

  auto extraGap = [&](size_t i) -> float { return i ? pairGap(items[i - 1], items[i]) : 0.f; };
  float total = 0;
  for (size_t i = 0; i < items.size(); i++) total += items[i].w + (i ? gap : 0) + extraGap(i);
  float squeeze = total > area.width ? area.width / total : 1;
  bool grouped = std::any_of(items.begin(), items.end(), [](const Item &it) { return it.group; });
  if (squeeze < 0.75f && items.size() >= 4 && !grouped && area.height >= 300) { // too crowded: two rows
    size_t half = (items.size() + 1) / 2;
    std::vector<std::string> a, b;
    for (size_t i = 0; i < items.size(); i++) (i < half ? a : b).push_back(items[i].id);
    layoutFree({area.x, area.y, area.width, area.height / 2}, a);
    layoutFree({area.x, area.y + area.height / 2, area.width, area.height / 2}, b);
    return;
  }
  if (squeeze < 0.6f) warn("layout: " + std::to_string(ids.size()) + " things crowd one row; group them, or use a table/array");
  float x = area.x + area.width / 2 - total * squeeze / 2;

  std::function<void(const Item &, float)> assign = [&](const Item &it, float x0) {
    if (!it.group) {
      Mark &m = marks_[it.id];
      m.tpos = {x0 + it.w * squeeze / 2, baseline};
      m.anchorBottom = true;
      m.positioned = true;
      std::string mp = magProp(m);
      if (!mp.empty()) {
        double up = posMax.count(mp) ? posMax[mp] : 0, down = negMax.count(mp) ? negMax[mp] : 0;
        if (up + down < 1e-9) up = std::max(1e-9, maxAbs_.count(mp) ? maxAbs_[mp] : 1.0);
        float target = (float)std::min(up > 0 ? avail * (1 - negFrac) / up : 1e9, down > 0 ? avail * negFrac / down : 1e9);
        auto bs = barScale_.find(mp);
        if (bs == barScale_.end()) bs = barScale_.emplace(mp, target).first;
        else bs->second = lerpf(bs->second, target, 0.15f);
        m.barScale = bs->second;
      }
      return;
    }
    float cx = x0 + pad * squeeze;
    for (size_t i = 0; i < it.kids.size(); i++) {
      if (i) cx += pairGap(it.kids[i - 1], it.kids[i]) * squeeze;
      assign(it.kids[i], cx);
      cx += (it.kids[i].w + gap) * squeeze;
    }
  };
  for (size_t i = 0; i < items.size(); i++) {
    x += extraGap(i) * squeeze;
    assign(items[i], x);
    x += (items[i].w + gap) * squeeze;
  }
}

// ---------------------------------------------------------------- graph
void Engine::layoutGraph(Rectangle area, const std::vector<std::string> &ids) {
  if (ids.empty()) return;
  std::vector<std::pair<int, int>> edges;
  std::map<std::string, int> idx;
  for (size_t i = 0; i < ids.size(); i++) idx[ids[i]] = (int)i;
  std::string sig;
  for (auto &id : ids) sig += id + ",";
  for (auto &id : order_) {
    auto it = links_.find(id);
    if (it == links_.end()) continue;
    if (idx.count(it->second.a) && idx.count(it->second.b)) {
      edges.push_back({idx[it->second.a], idx[it->second.b]});
      sig += it->second.a + ">" + it->second.b + ",";
    }
  }
  for (auto &[gid, g] : groups_) { sig += gid + ":"; for (auto &m : g.members) sig += m + ","; }
  int n = (int)ids.size();
  std::vector<Vector2> sizes(n);
  for (int i = 0; i < n; i++) {
    Rectangle b = bodyBounds(marks_[ids[i]]);
    sizes[i] = {std::max(b.width, 40.f) + 70, std::max(b.height, 40.f) + 60}; // room for link labels
  }

  sig += "|" + std::to_string((int)area.y) + "x" + std::to_string((int)area.height);
  if (sig != graphSig_) {
    graphSig_ = sig;
    std::vector<Vector2> pos(n);
    // acyclic? -> layered left-to-right (call stacks, org charts, who-owes-whom)
    std::vector<int> indeg(n, 0), rank(n, 0);
    std::vector<std::vector<int>> out(n), in(n);
    for (auto [a, b] : edges) if (a != b) { out[a].push_back(b); in[b].push_back(a); indeg[b]++; }
    std::vector<int> q, topo;
    for (int i = 0; i < n; i++) if (!indeg[i]) q.push_back(i);
    while (!q.empty()) {
      int v = q.front();
      q.erase(q.begin());
      topo.push_back(v);
      for (int w : out[v]) {
        rank[w] = std::max(rank[w], rank[v] + 1);
        if (--indeg[w] == 0) q.push_back(w);
      }
    }
    // Nodes without links yet (parts introduced before they're wired up) still read in a flow:
    // categories form columns in the order they were declared (input -> hidden -> output), so the
    // picture doesn't start as a ring and reshuffle when the arrows arrive. A lone node joins the
    // column of a linked node of its category.
    std::vector<int> deg(n, 0), catCol(n, 0);
    for (auto [a, b] : edges) if (a != b) { deg[a]++; deg[b]++; }
    std::map<std::string, int> cats;
    for (int i = 0; i < n; i++) {
      auto [it, fresh] = cats.insert({marks_[ids[i]].category, (int)cats.size()});
      catCol[i] = it->second;
    }
    int K = (int)cats.size();
    bool acyclic = (int)topo.size() == n;
    bool flow = acyclic && (!edges.empty() || K > 1 || n <= 6);
    if (flow) {
      int maxRank = 0;
      for (int i = 0; i < n; i++) if (deg[i]) maxRank = std::max(maxRank, rank[i]);
      for (int i = 0; i < n; i++) {
        if (deg[i]) continue;
        int r = -1;
        for (int j = 0; j < n && r < 0; j++)
          if (deg[j] && catCol[j] == catCol[i]) r = rank[j];
        if (r < 0) r = edges.empty() ? (K > 1 ? catCol[i] : i) : std::min(catCol[i], maxRank + 1);
        rank[i] = r;
      }
    }
    bool dag = flow;
    if (dag) {
      int R = *std::max_element(rank.begin(), rank.end()) + 1;
      std::vector<std::vector<int>> layers(R);
      for (int v : topo) layers[rank[v]].push_back(v);
      for (auto &L : layers) std::sort(L.begin(), L.end());
      std::vector<float> slot(n, 0);
      for (int pass = 0; pass < 4; pass++) {
        for (int r = 0; r < R; r++) {
          for (size_t i = 0; i < layers[r].size(); i++) slot[layers[r][i]] = (float)i;
          if (r == 0) continue;
          std::vector<std::pair<float, int>> bc;
          for (int v : layers[r]) {
            float s = 0;
            for (int u : in[v]) s += slot[u];
            bc.push_back({in[v].empty() ? slot[v] : s / in[v].size(), v});
          }
          std::stable_sort(bc.begin(), bc.end());
          for (size_t i = 0; i < bc.size(); i++) layers[r][i] = bc[i].second;
        }
      }
      for (int r = 0; r < R; r++)
        for (size_t i = 0; i < layers[r].size(); i++) {
          int v = layers[r][i];
          pos[v] = {area.x + area.width * (r + 0.5f) / R,
                    area.y + 20 + (area.height - 40) * (i + 0.5f) / layers[r].size()};
        }
    } else {
      // force-directed (Fruchterman–Reingold), deterministic start
      Vector2 c{area.x + area.width / 2, area.y + area.height / 2};
      for (int i = 0; i < n; i++) {
        auto it = graphPos_.find(ids[i]);
        float a = 6.2831853f * i / n - 1.5707963f;
        pos[i] = it != graphPos_.end() ? it->second : c + Vector2{std::cos(a) * area.width * 0.3f, std::sin(a) * area.height * 0.32f};
      }
      if (n > 1) {
        float kk = std::sqrt(area.width * area.height / n) * 0.8f;
        // two links between the same pair pull no harder than one
        std::set<std::pair<int, int>> seen;
        std::vector<std::pair<int, int>> springs;
        for (auto [a, b] : edges)
          if (a != b && seen.insert({std::min(a, b), std::max(a, b)}).second) springs.push_back({a, b});
        float temp = area.width * 0.08f;
        for (int iter = 0; iter < 400; iter++) {
          std::vector<Vector2> d(n, {0, 0});
          for (int i = 0; i < n; i++)
            for (int j = i + 1; j < n; j++) {
              Vector2 dv = pos[i] - pos[j];
              float L = std::max(1.f, len(dv));
              Vector2 f = dv * (kk * kk / (L * L));
              d[i] += f;
              d[j] += f * -1.f;
            }
          for (auto [a, b] : springs) {
            Vector2 dv = pos[a] - pos[b];
            float L = std::max(1.f, len(dv));
            Vector2 f = dv * (L / kk);
            d[a] += f * -1.f;
            d[b] += f;
          }
          for (auto &[gid, g] : groups_) { // members of a group pull together (part-of)
            Vector2 gc{0, 0};
            std::vector<int> mem;
            for (auto &m : g.members) if (idx.count(m)) { mem.push_back(idx[m]); gc += pos[idx[m]]; }
            if (mem.size() < 2) continue;
            gc = gc * (1.f / mem.size());
            for (int v : mem) d[v] += (gc - pos[v]) * 0.6f;
          }
          for (int i = 0; i < n; i++) {
            d[i] += (c - pos[i]) * 0.02f;
            float L = len(d[i]);
            if (L > 0) pos[i] += d[i] * (std::min(L, temp) / L);
            pos[i].x = std::clamp(pos[i].x, area.x + 80, area.x + area.width - 80);
            pos[i].y = std::clamp(pos[i].y, area.y + 50, area.y + area.height - 50);
          }
          temp *= 0.985f;
        }
      } else {
        pos[0] = c;
      }
    }
    // Zero: unrelated nodes never touch by accident
    for (int iter = 0; iter < 80; iter++) {
      bool moved = false;
      for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++) {
          Vector2 dv = pos[j] - pos[i];
          float ox = (sizes[i].x + sizes[j].x) / 2 - std::fabs(dv.x);
          float oy = (sizes[i].y + sizes[j].y) / 2 - std::fabs(dv.y);
          if (ox > 0 && oy > 0) {
            moved = true;
            if (ox < oy) { float s = (dv.x >= 0 ? 1 : -1) * ox / 2 + 0.5f; pos[i].x -= s; pos[j].x += s; }
            else { float s = (dv.y >= 0 ? 1 : -1) * oy / 2 + 0.5f; pos[i].y -= s; pos[j].y += s; }
          }
        }
      if (!moved) break;
    }
    if (!dag && n > 1) { // centre the drawing on the stage
      Vector2 mean{0, 0};
      for (auto &p : pos) mean += p;
      mean = mean * (1.f / n);
      Vector2 shift = Vector2{area.x + area.width / 2, area.y + area.height / 2} - mean;
      for (auto &p : pos) p += shift;
    }
    { // fit: the overlap push can carry nodes off the stage; scale about the centre, then shift inside
      float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
      for (int i = 0; i < n; i++) {
        x0 = std::min(x0, pos[i].x - sizes[i].x / 2); x1 = std::max(x1, pos[i].x + sizes[i].x / 2);
        y0 = std::min(y0, pos[i].y - sizes[i].y / 2); y1 = std::max(y1, pos[i].y + sizes[i].y / 2);
      }
      float sx = (x1 - x0) > area.width ? area.width / (x1 - x0) : 1, sy = (y1 - y0) > area.height ? area.height / (y1 - y0) : 1;
      float k = std::min(sx, sy);
      Vector2 c0{(x0 + x1) / 2, (y0 + y1) / 2};
      if (k < 1) for (auto &p : pos) p = c0 + (p - c0) * k;
      x0 = 1e9f; y0 = 1e9f; x1 = -1e9f; y1 = -1e9f;
      for (int i = 0; i < n; i++) {
        x0 = std::min(x0, pos[i].x - sizes[i].x / 2); x1 = std::max(x1, pos[i].x + sizes[i].x / 2);
        y0 = std::min(y0, pos[i].y - sizes[i].y / 2); y1 = std::max(y1, pos[i].y + sizes[i].y / 2);
      }
      Vector2 d{0, 0};
      if (x0 < area.x) d.x = area.x - x0; else if (x1 > area.x + area.width) d.x = area.x + area.width - x1;
      if (y0 < area.y) d.y = area.y - y0; else if (y1 > area.y + area.height) d.y = area.y + area.height - y1;
      for (auto &p : pos) p += d;
    }
    for (int i = 0; i < n; i++) graphPos_[ids[i]] = pos[i];
  }
  for (auto &id : ids) {
    Mark &m = marks_[id];
    m.tpos = graphPos_[id];
    m.positioned = true;
  }
}

// ---------------------------------------------------------------- number line
void Engine::layoutNumberline(Rectangle area) {
  float lineY = area.y + area.height * 0.66f;
  nl_.a = {area.x + 90, lineY};
  nl_.b = {area.x + area.width - 90, lineY};
  nl_.min = nlMin_;
  nl_.max = nlMax_;
  nl_.step = 0;
  std::vector<std::pair<float, std::string>> pts;
  std::vector<std::string> rest;
  for (auto &id : order_) {
    Mark *m = mark(id);
    if (!m) continue;
    std::string k = effectiveKind(*m);
    bool ok;
    double v = num(*m, nlProp_, &ok);
    if (isBlockKind(k)) continue;
    if (ok && std::isfinite(v) && k != "path" && k != "region") {
      m->tpos = {nl_.toScreen((float)v).x, lineY};
      m->positioned = true;
      m->direct = true;
      if (k != "label") pts.push_back({m->tpos.x, id});
    } else if (k != "label" && k != "path" && k != "region") {
      rest.push_back(id);
    }
  }
  // equal values stack upward instead of hiding each other
  std::stable_sort(pts.begin(), pts.end());
  std::vector<std::pair<float, int>> placed;
  for (auto &[x, id] : pts) {
    Mark &m = marks_[id];
    float r = dotRadius(m);
    int level = 0;
    for (bool clash = true; clash;) {
      clash = false;
      for (auto &[px, pl] : placed)
        if (pl == level && std::fabs(px - x) < 2 * r + 4) { clash = true; level++; break; }
    }
    placed.push_back({x, level});
    m.tpos.y = lineY - level * (2 * r + 6);
  }
  // name labels: lift any that would overlap a neighbour's
  std::vector<Rectangle> labels;
  for (auto &[x, id] : pts) {
    Mark &m = marks_[id];
    float w = measure(displayName(m), 17).x + 8, r = dotRadius(m);
    float base = m.tpos.y - r - 8 - 18;
    float lift = 0;
    for (bool clash = true; clash && lift < 200;) {
      clash = false;
      Rectangle rc{x - w / 2, base - lift, w, 18};
      for (auto &o : labels) if (CheckCollisionRecs(rc, o)) { clash = true; lift += 20; break; }
    }
    m.labelLift = smoothApproach(m.labelLift, lift, 1.f / 60, 12);
    labels.push_back({x - w / 2, base - lift, w, 18});
  }
  if (!rest.empty()) layoutFree({area.x, area.y, area.width, area.height * 0.42f}, rest);
}

// ---------------------------------------------------------------- axes
void Engine::layoutAxes(Rectangle area) {
  axes_.area = {area.x + 70, area.y + 8, area.width - 130, area.height - 44};
  axes_.xmin = axX0_; axes_.xmax = axX1_; axes_.ymin = axY0_; axes_.ymax = axY1_;
  // plane (or equal=1): one unit is the same length across and up, so circles are round and
  // angles true. Widen the shorter range about its centre.
  auto eq = findProp(schemaProps_, "equal");
  if ((schemaNow() == "plane" && !(eq && eq->num == 0)) || (eq && eq->num != 0)) {
    if (schemaNow() == "plane") axes_.area = {area.x + 30, area.y + 4, area.width - 60, area.height - 16};
    float dx = axes_.xmax - axes_.xmin, dy = axes_.ymax - axes_.ymin;
    float ppu = std::min(axes_.area.width / dx, axes_.area.height / dy);
    float cx = (axes_.xmin + axes_.xmax) / 2, cy = (axes_.ymin + axes_.ymax) / 2;
    float hw = axes_.area.width / ppu / 2, hh = axes_.area.height / ppu / 2;
    axes_.xmin = cx - hw; axes_.xmax = cx + hw; axes_.ymin = cy - hh; axes_.ymax = cy + hh;
  }
  camApply(axes_.area); // the camera's view, if one is in force
  // under a camera, points outside the view are not drawn (curves and shapes are clipped to the axes)
  auto outside = [&](Vector2 p) {
    const Rectangle &r = axes_.area;
    return cam_.on && (p.x < r.x - 2 || p.x > r.x + r.width + 2 || p.y < r.y - 2 || p.y > r.y + r.height + 2);
  };
  std::vector<std::string> rest;
  for (auto &id : order_) {
    Mark *m = mark(id);
    if (!m) continue;
    std::string k = effectiveKind(*m);
    bool hx, hy;
    double x = num(*m, "x", &hx), y = num(*m, "y", &hy);
    if (k == "path" || k == "region" || k == "line" || k == "arrow" || isWorldKind(k) || isFieldKind(k) || k == "solution") {
      m->positioned = m->direct = true;
      if (hx && hy) m->tpos = axes_.toScreen((float)x, (float)y);
      continue;
    }
    if (k == "bar" && hx) {
      m->tpos = axes_.toScreen((float)x, 0);
      m->positioned = m->direct = true;
      continue;
    }
    if (hx && hy) {
      m->tpos = axes_.toScreen((float)x, (float)y);
      m->positioned = m->direct = true;
      m->culled = outside(m->tpos);
    } else if (k != "label" && k != "param" && !isBlockKind(k) && !findProp(m->props, "at")) {
      rest.push_back(id);
    }
  }
  if (!rest.empty()) layoutFree({area.x + area.width - 360, area.y, 340, 150}, rest);

  // dot names: each takes the first corner (NE, NW, SE, SW) that doesn't cover another name or dot
  std::vector<Rectangle> taken;
  for (auto &id : order_)
    if (Mark *m = mark(id); m && m->positioned && effectiveKind(*m) == "dot") {
      float r = dotRadius(*m);
      taken.push_back({m->tpos.x - r, m->tpos.y - r, 2 * r, 2 * r});
    }
  for (auto &id : order_) {
    Mark *m = mark(id);
    if (!m || !m->positioned || effectiveKind(*m) != "dot") continue;
    Vector2 sz = measure(displayName(*m), 17);
    int best = 0;
    for (int q = 0; q < 4; q++) {
      float x = q % 2 ? m->tpos.x - 10 - sz.x : m->tpos.x + 10, y = q >= 2 ? m->tpos.y + 10 : m->tpos.y - 10 - sz.y;
      Rectangle rc{x, y, sz.x, sz.y};
      bool clash = false;
      for (auto &t : taken) if (CheckCollisionRecs(rc, t)) { clash = true; break; }
      if (!clash) { best = q; break; }
    }
    m->labelQuad = best;
    float x = best % 2 ? m->tpos.x - 10 - sz.x : m->tpos.x + 10, y = best >= 2 ? m->tpos.y + 10 : m->tpos.y - 10 - sz.y;
    taken.push_back({x, y, sz.x, sz.y});
  }
}

// ---------------------------------------------------------------- timeline
void Engine::layoutTimeline(Rectangle area) {
  float lineY = area.y + area.height * 0.66f;
  nl_.a = {area.x + 70, lineY};
  nl_.b = {area.x + area.width - 70, lineY};
  nl_.min = tlMin_;
  nl_.max = tlMax_;
  std::vector<std::string> rest;
  for (auto &id : order_) {
    Mark *m = mark(id);
    if (!m) continue;
    std::string k = effectiveKind(*m);
    if (k == "path" || k == "region" || k == "label" || isBlockKind(k)) continue;
    bool ok = false;
    double t = 0;
    for (const char *key : {"t", "at", "year"}) {
      t = num(*m, key, &ok);
      if (ok) break;
    }
    if (ok && std::isfinite(t)) {
      m->tpos = {nl_.toScreen((float)t).x, lineY};
      m->positioned = m->direct = true;
    } else {
      rest.push_back(id);
    }
  }
  if (!rest.empty()) layoutFree({area.x, area.y, area.width, area.height * 0.42f}, rest);
}

// ================================================================ Zero check
void Engine::zeroCheck() {
  std::string s = schemaNow();
  std::vector<Mark *> ms;
  for (auto &id : order_)
    if (Mark *m = mark(id)) {
      std::string k = effectiveKind(*m);
      if (k == "dot" || k == "bar" || k == "box") ms.push_back(m);
    }
  for (size_t i = 0; i < ms.size(); i++)
    for (size_t j = i + 1; j < ms.size(); j++) {
      Mark &a = *ms[i], &b = *ms[j];
      if (related(a.id, b.id)) continue;
      auto ca = findProp(a.props, "color"), cb = findProp(b.props, "color");
      if (ca && cb && ca->str == cb->str)
        warn("zero: " + a.id + " and " + b.id + " share color '" + ca->str + "' but are unrelated (same color reads as same kind)");
      if (!a.positioned || !b.positioned) continue;
      Rectangle ra = bodyBounds(a), rb = bodyBounds(b);
      if (ra.width <= 0 || rb.width <= 0) continue;
      if (CheckCollisionRecs(ra, rb)) {
        if (a.direct && b.direct)
          warn("zero: " + a.id + " and " + b.id + " coincide on the " + s + " but are unrelated; coinciding reads as 'equal'");
        else
          warn("zero: " + a.id + " and " + b.id + " overlap by accident in the " + s + " layout");
      }
    }
}

} // namespace mk
