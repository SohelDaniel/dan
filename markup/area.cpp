// The area diagram: a whole split by proportion, so area is the quantity (Cheng's law-encoding
// diagrams; the unit square of conditional probability; the area model of multiplication).
//   mark pop area parts=[1,999] labels=[sick,healthy] colors=[red,-] label="1000 people"
//   mark pop area parts=[1,999] by=[[1,0],[10,989]] bylabels=[…] bycolors=[…]   each strip split again
// Strips run across (pop[i]); by= splits each one top to bottom (pop[i][j]). A change to parts= glides;
// a part too thin to see still gets a sliver.
#include "engine.hpp"
#include <numeric>

namespace mk {
using namespace rm;

static const float kAreaW = 520, kAreaH = 210;

static std::vector<double> numbers(const Value *v);
// parts=, eased while a morph or re-mark changes it (same length: number by number), so the split glides
static std::vector<double> easedParts(const Mark &m) {
  std::vector<double> now = numbers(findProp(m.props, "parts"));
  auto it = m.strBlend.find("parts");
  if (it == m.strBlend.end() || it->second.second >= 1) return now;
  Value old = Value::parse(it->second.first);
  std::vector<double> was = numbers(&old);
  if (was.size() != now.size()) return now;
  float t = rate::smooth(it->second.second);
  for (size_t i = 0; i < now.size(); i++) now[i] = was[i] + (now[i] - was[i]) * t;
  return now;
}

static std::vector<double> numbers(const Value *v) {
  std::vector<double> out;
  if (!v || v->type != Value::LIST) return out;
  for (auto &s : v->list) out.push_back(std::max(0.0, std::atof(s.c_str())));
  return out;
}

// proportional cuts of a length, each at least `least` long (a 1 in 1000 part stays visible)
static std::vector<float> cuts(const std::vector<double> &parts, float length, float least) {
  double total = std::accumulate(parts.begin(), parts.end(), 0.0);
  std::vector<float> w(parts.size(), parts.empty() ? 0 : length / parts.size());
  if (total <= 0) return w;
  float fixed = 0, rest = 0;
  for (size_t i = 0; i < parts.size(); i++) {
    w[i] = (float)(parts[i] / total) * length;
    if (parts[i] > 0 && w[i] < least) { fixed += least - w[i]; w[i] = least; }
    else rest += w[i];
  }
  for (auto &x : w) if (x > least && rest > 0) x -= fixed * x / rest; // the slivers' room comes from the rest
  return w;
}

Vector2 Engine::areaSize(const Mark &m) {
  bool below = false; // strip names that don't fit inside go under the strip
  const Value *lv = findProp(m.props, "labels");
  auto w = cuts(easedParts(m), kAreaW, 6);
  const Value *by = findProp(m.props, "by");
  if (lv && lv->type == Value::LIST)
    for (size_t i = 0; i < lv->list.size() && i < w.size(); i++) // a split strip's name always goes under it
      below |= measure(lv->list[i], 18).x + 10 > w[i] || (by && by->type == Value::LIST && i < by->list.size());
  return {kAreaW, (findProp(m.props, "label") ? 26.f : 0.f) + kAreaH + (below ? 28.f : 0.f)};
}

// strip i (j < 0), or the j-th part of strip i
Rectangle Engine::areaCell(const Mark &m, int i, int j) {
  Vector2 sz = areaSize(m);
  float x = m.pos.x - sz.x / 2, y = m.pos.y - sz.y / 2 + (findProp(m.props, "label") ? 26.f : 0.f);
  auto w = cuts(easedParts(m), kAreaW, 6);
  if (i < 0 || i >= (int)w.size()) return {0, 0, 0, 0};
  for (int k = 0; k < i; k++) x += w[k];
  Rectangle strip{x, y, w[i], kAreaH};
  if (j < 0) return strip;
  const Value *by = findProp(m.props, "by");
  if (!by || by->type != Value::LIST || i >= (int)by->list.size()) return strip;
  Value inner = Value::parse(by->list[i]);
  auto h = cuts(numbers(&inner), kAreaH, 4);
  if (j >= (int)h.size()) return {0, 0, 0, 0};
  for (int k = 0; k < j; k++) strip.y += h[k];
  strip.height = h[j];
  return strip;
}

// "pop[1]" / "pop[1][0]"
bool Engine::areaRef(const Mark &m, const std::string &ref, Rectangle &out) {
  size_t b = ref.find('[');
  if (b == std::string::npos) return false;
  int i = std::atoi(ref.c_str() + b + 1), j = -1;
  if (size_t b2 = ref.find('[', b + 1); b2 != std::string::npos) j = std::atoi(ref.c_str() + b2 + 1);
  out = areaCell(m, i, j);
  return out.width > 0;
}

void Engine::drawArea(const Mark &m, float a) {
  auto listAt = [&](const char *key, size_t k) -> std::string {
    const Value *v = findProp(m.props, key);
    return v && v->type == Value::LIST && k < v->list.size() ? v->list[k] : "";
  };
  auto fill = [&](Rectangle r, const std::string &cn, bool lit) {
    Color c = col::muted;
    bool tinted = colorByName(cn, c);
    DrawRectangleRec(r, fade(c, a * (tinted ? 0.55f : 0.14f) * (lit ? 1 : 0.35f)));
    DrawRectangleLinesEx(r, 2, fade(tinted ? c : col::muted, a * (lit ? 1 : 0.4f)));
  };
  std::string fb = !focus_.empty() && baseId(focus_) == m.id ? focus_ : ""; // focus pop[1]: that part stays lit
  auto lit = [&](int i, int j) {
    if (fb.empty() || fb.find('[') == std::string::npos) return true;
    int fi = std::atoi(fb.c_str() + fb.find('[') + 1), fj = -1;
    if (size_t b2 = fb.find('[', fb.find('[') + 1); b2 != std::string::npos) fj = std::atoi(fb.c_str() + b2 + 1);
    return fi == i && (fj < 0 || fj == j);
  };
  const Value *by = findProp(m.props, "by");
  size_t n = easedParts(m).size(), sub = 0;
  for (size_t i = 0; i < n; i++) {
    Rectangle s = areaCell(m, (int)i, -1);
    std::vector<double> parts;
    if (by && by->type == Value::LIST && i < by->list.size()) { Value v = Value::parse(by->list[i]); parts = numbers(&v); }
    if (parts.empty()) fill(s, listAt("colors", i), lit((int)i, -1));
    for (size_t j = 0; j < parts.size(); j++, sub++) {
      Rectangle c = areaCell(m, (int)i, (int)j);
      fill(c, listAt("bycolors", sub).empty() || listAt("bycolors", sub) == "-" ? listAt("colors", i) : listAt("bycolors", sub), lit((int)i, (int)j));
      std::string t = listAt("bylabels", sub);
      if (!t.empty() && measure(t, 16).x + 8 < c.width && c.height > 20)
        text(t, {c.x + c.width / 2, c.y + c.height / 2}, 16, fade(col::fg, a), 0.5f, 0.5f);
    }
    std::string t = listAt("labels", i);
    if (t.empty()) continue;
    bool inside = measure(t, 18).x + 10 <= s.width;
    if (inside && parts.empty()) text(t, {s.x + s.width / 2, s.y + s.height / 2}, 18, fade(col::fg, a), 0.5f, 0.5f);
    else text(t, {s.x + s.width / 2, s.y + s.height + 6}, 18, fade(col::fg, a), 0.5f, 0);
  }
}

} // namespace mk
