// Annotations: marks about other things, the signals explainers use most (3Blue1Brown's Brace,
// SurroundingRectangle, Underline, Cross):
//   mark n brace of=[x1,x2,x3] label="3 inputs"   a brace along them, its name at the tip
//   mark r enclose of=f[top]                       a box around the part of formula f written \textcolor{top}{…}
//   mark u underline of=a[1:3]                     a line under cells 1 to 3
//   mark c cross of=guess        mark k check of=answer
// A target is a thing, an array cell or span (a[k], a[i:j]) or a named part of a formula (f[id]); a
// list covers its targets together. The side comes from the target's shape (below something wide,
// right of something tall, flipped at the stage's edge), and the annotation follows its target.
#include "engine.hpp"
#include <cmath>

namespace mk {
using namespace rm;

static Rectangle unite(Rectangle a, Rectangle b) {
  if (a.width <= 0) return b;
  float x0 = std::min(a.x, b.x), y0 = std::min(a.y, b.y);
  return {x0, y0, std::max(a.x + a.width, b.x + b.width) - x0, std::max(a.y + a.height, b.y + b.height) - y0};
}

// the glyphs of formula m written in \textcolor{part}{…}, where the formula is going this frame
bool Engine::texPartBox(const Mark &m, const std::string &part, Rectangle &out) {
  TexPtr L = texNow(m);
  if (!L || !L->ok) return false;
  float px = texSize(m) / 1000.f;
  Vector2 o = m.tpos - Vector2{L->w * px / 2, L->h * px / 2};
  out = {0, 0, 0, 0};
  for (auto &g : L->glyphs) {
    if (g.color != part || g.shape < 0 || g.shape >= (int)L->shapes.size()) continue;
    const Rectangle &b = L->shapes[g.shape].box;
    out = unite(out, {o.x + (g.at.x + b.x) * px, o.y + (g.at.y + b.y) * px, b.width * px, b.height * px});
  }
  return out.width > 0;
}

bool Engine::annotationTarget(const Mark &m, Rectangle &out) {
  const Value *of = findProp(m.props, "of");
  if (!of) return false;
  std::vector<std::string> refs = of->type == Value::LIST ? of->list : std::vector<std::string>{of->str};
  out = {0, 0, 0, 0};
  for (auto &ref : refs) {
    Mark *t = mark(baseId(ref));
    if (!t || !t->positioned) continue;
    Rectangle r;
    size_t b = ref.find('[');
    if (b != std::string::npos && effectiveKind(*t) == "tex") { // a named part of a formula
      if (!texPartBox(*t, ref.substr(b + 1, ref.find(']') - b - 1), r)) continue;
    } else {
      if (!anchorBox(ref, r)) continue;
      r.x += t->tpos.x - t->pos.x; // where it is going, so the annotation moves with it
      r.y += t->tpos.y - t->pos.y;
      const Value *items = findProp(t->props, "items");
      if (effectiveKind(*t) == "array" && items && items->type == Value::LIST && arrayCols(*t) >= (int)items->list.size())
        r.height += 18; // a row of cells: its index numbers belong to it
    }
    out = unite(out, r);
  }
  return out.width > 0;
}

Engine::Annotation Engine::annotation(const Mark &m) {
  Annotation a;
  std::string k = effectiveKind(m);
  if (!annotationTarget(m, a.target)) return a;
  a.ok = true;
  const Rectangle T = a.target, S = stageRect();
  std::string name = findProp(m.props, "label") ? displayName(m) : "";
  Vector2 ls = name.empty() ? Vector2{0, 0} : measure(name, 19);
  const float gap = 8, depth = 14;
  if (k == "brace" || k == "underline") {
    // below something wide, right of something tall; the other way if that side leaves the stage or
    // more links cross it (a column of inputs gets its brace on the left, away from its arrows)
    bool wide = T.width >= T.height * 0.8f || k == "underline";
    float reach = (k == "brace" ? depth : 0) + gap + 6 + (wide ? ls.y : ls.x);
    auto band = [&](int side) {
      return side == 0 ? Rectangle{T.x, T.y + T.height, T.width, reach} : side == 2 ? Rectangle{T.x, T.y - reach, T.width, reach}
           : side == 1 ? Rectangle{T.x + T.width, T.y, reach, T.height} : Rectangle{T.x - reach, T.y, reach, T.height};
    };
    auto cost = [&](int side) {
      Rectangle r = band(side);
      float c = (r.x < S.x || r.y < S.y || r.x + r.width > S.x + S.width || r.y + r.height > S.y + S.height) ? 100 : 0;
      for (auto &[lid, l] : links_) {
        LinkGeom g = linkGeometryAt(l);
        for (size_t i = 1; i < g.line.size(); i++)
          if (CheckCollisionPointRec(g.line[i], r)) { c += 1; break; }
      }
      return c;
    };
    int first = wide ? 0 : 1, second = wide ? 2 : 3;
    a.side = cost(second) < cost(first) ? second : first;
    Vector2 u = a.side < 2 ? (wide ? Vector2{1, 0} : Vector2{0, 1}) : (a.side == 2 ? Vector2{1, 0} : Vector2{0, 1});
    Vector2 n = a.side == 0 ? Vector2{0, 1} : a.side == 1 ? Vector2{1, 0} : a.side == 2 ? Vector2{0, -1} : Vector2{-1, 0};
    Vector2 from = a.side == 0 ? Vector2{T.x, T.y + T.height + gap}
                 : a.side == 1 ? Vector2{T.x + T.width + gap, T.y}
                 : a.side == 2 ? Vector2{T.x, T.y - gap} : Vector2{T.x - gap, T.y};
    float L = a.side % 2 == 0 ? T.width : T.height;
    Vector2 to = from + u * L;
    if (k == "underline") a.path = {from, to};
    else { // a curly brace: quarter turns at the ends, a point in the middle
      float q = std::min(depth, L / 4);
      Vector2 mid = (from + to) * 0.5f, h = n * (depth / 2);
      auto cubic = [&](Vector2 p0, Vector2 c1, Vector2 c2, Vector2 p1) {
        for (int i = (a.path.empty() ? 0 : 1); i <= 8; i++) {
          float t = i / 8.f, s = 1 - t;
          a.path.push_back(p0 * (s * s * s) + c1 * (3 * s * s * t) + c2 * (3 * s * t * t) + p1 * (t * t * t));
        }
      };
      cubic(from, from + h, from + h, from + u * q + h);
      cubic(from + u * q + h, mid - u * q + h, mid - u * q + h, mid - u * q + h);
      cubic(mid - u * q + h, mid + h, mid + h, mid + n * depth);
      cubic(mid + n * depth, mid + h, mid + h, mid + u * q + h);
      cubic(mid + u * q + h, to - u * q + h, to - u * q + h, to - u * q + h);
      cubic(to - u * q + h, to + h, to + h, to);
    }
    Vector2 tip = (from + to) * 0.5f + n * ((k == "brace" ? depth : 0) + 6);
    a.labelAt = tip;
    a.labelAnchor = a.side == 0 ? Vector2{0.5f, 0} : a.side == 1 ? Vector2{0, 0.5f} : a.side == 2 ? Vector2{0.5f, 1} : Vector2{1, 0.5f};
  } else if (k == "enclose") {
    a.box = {T.x - gap, T.y - gap, T.width + 2 * gap, T.height + 2 * gap};
    // its name goes on the first corner clear of other things: above right, below right, above left, below left
    const Rectangle &B = a.box;
    const std::pair<Vector2, Vector2> spots[] = {{{B.x + B.width, B.y - 4}, {1, 1}}, {{B.x + B.width, B.y + B.height + 4}, {1, 0}},
                                                 {{B.x, B.y - 4}, {0, 1}}, {{B.x, B.y + B.height + 4}, {0, 0}}};
    a.labelAt = spots[0].first;
    a.labelAnchor = spots[0].second;
    for (auto &[at, anchor] : spots) {
      Rectangle lb{at.x - ls.x * anchor.x, at.y - ls.y * anchor.y, ls.x, ls.y};
      bool clear = true;
      for (auto &[oid, o] : marks_) {
        if (&o == &m || !o.positioned || isAnnotationKind(effectiveKind(o)) || ghost(o)) continue;
        Rectangle ob = bodyBounds(o);
        if (findProp(o.props, "label") && isBlockKind(effectiveKind(o))) { // a block: its name on the top edge, then its content
          Rectangle name{ob.x, ob.y, measure(displayName(o), 18).x + 6, 26};
          if (CheckCollisionRecs(lb, name)) { clear = false; break; }
          ob.y += 26;
          ob.height -= 26;
        }
        if (CheckCollisionRecs(lb, ob)) { clear = false; break; }
      }
      if (clear) { a.labelAt = at; a.labelAnchor = anchor; break; }
    }
  } else { // cross over it, check beside it
    a.box = k == "cross" ? Rectangle{T.x, T.y, T.width, T.height}
                         : Rectangle{T.x + T.width + gap, T.y + T.height / 2 - 18, 36, 36};
    a.labelAt = {a.box.x + a.box.width / 2, a.box.y + a.box.height + 6};
    a.labelAnchor = {0.5f, 0};
  }
  a.label = name;
  Rectangle lb{a.labelAt.x - ls.x * a.labelAnchor.x, a.labelAt.y - ls.y * a.labelAnchor.y, ls.x, ls.y};
  if (!name.empty()) a.labelBox = lb;
  a.bounds = a.box.width > 0 ? a.box : Rectangle{0, 0, 0, 0};
  for (auto &p : a.path) a.bounds = unite(a.bounds, {p.x - 2, p.y - 2, 4, 4});
  if (!name.empty()) a.bounds = unite(a.bounds, lb);
  return a;
}

// the room a block in the band needs under it for its braces and underlines (they go below wide things)
float Engine::annotationRoom(const Mark &block) {
  float room = 0;
  for (auto &[id, m] : marks_) {
    std::string k = effectiveKind(m);
    if ((k != "brace" && k != "underline") || alphaOf(m) <= 0.01f) continue;
    const Value *of = findProp(m.props, "of");
    if (!of) continue;
    for (auto &ref : of->type == Value::LIST ? of->list : std::vector<std::string>{of->str})
      if (baseId(ref) == block.id)
        room = std::max(room, (k == "brace" ? 14.f : 0.f) + 14 + (findProp(m.props, "label") ? 22.f : 0.f));
  }
  return room;
}

// placed after everything else, so it sits on where its targets went this frame
void Engine::layoutAnnotations() {
  for (auto &id : order_) {
    Mark *m = mark(id);
    if (!m || !isAnnotationKind(effectiveKind(*m))) continue;
    Annotation a = annotation(*m);
    if (!a.ok) continue;
    m->tpos = m->pos = {a.bounds.x + a.bounds.width / 2, a.bounds.y + a.bounds.height / 2};
    m->positioned = m->placed = m->direct = true;
  }
}

void Engine::drawAnnotation(const Mark &m) {
  Annotation a = annotation(m);
  float al = alphaOf(m);
  if (!a.ok || al <= 0.003f) return;
  std::string k = effectiveKind(m);
  Color c = k == "cross" ? col::red : k == "check" ? col::green : col::yellow;
  if (auto cv = findProp(m.props, "color"); cv && cv->type == Value::STR) colorByName(cv->str, c);
  float grow = std::clamp(m.appear * 1.4f, 0.f, 1.f); // drawn on, like a stroke
  c = fade(c, al);
  if (!a.path.empty()) {
    std::vector<Vector2> part(a.path.begin(), a.path.begin() + std::max<size_t>(2, (size_t)(grow * (a.path.size() - 1)) + 1));
    stroke(part, 3, c);
  } else if (k == "enclose") {
    roundedBox(a.box, 10, 3, c, fade(c, 0.06f * grow));
  } else if (k == "cross") {
    const Rectangle &b = a.box;
    DrawLineEx({b.x, b.y}, {b.x + b.width * std::min(1.f, grow * 2), b.y + b.height * std::min(1.f, grow * 2)}, 5, c);
    if (grow > 0.5f) DrawLineEx({b.x + b.width, b.y}, {b.x + b.width - b.width * (grow - 0.5f) * 2, b.y + b.height * (grow - 0.5f) * 2}, 5, c);
  } else { // check
    const Rectangle &b = a.box;
    stroke({{b.x + 4, b.y + b.height * 0.55f}, {b.x + b.width * 0.4f, b.y + b.height - 4}, {b.x + b.width - 2, b.y + 4}}, 5, c);
  }
  if (!a.label.empty() && grow > 0.6f)
    text(a.label, a.labelAt, 19, fade(c, std::min(1.f, (grow - 0.6f) * 2.5f)), a.labelAnchor.x, a.labelAnchor.y);
}

} // namespace mk
