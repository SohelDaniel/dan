// Drawing: every mark kind, links, enclosures, the voice's subtitles and the dictionary caption.
#include "engine.hpp"
#include <algorithm>
#include <cmath>

namespace mk {

using namespace rm;

static float easeOut(float t) { return 1 - (1 - t) * (1 - t) * (1 - t); }
static float easeBack(float t) { return rate::ease_out_back(std::clamp(t, 0.f, 1.f)); }

float Engine::opacityOf(const Obj &o) {
  const Value *v = findProp(o.props, "opacity");
  if (!v) return 1;
  double x = num(*v);
  return std::isfinite(x) ? std::clamp((float)x, 0.f, 1.f) : 1.f;
}
float Engine::alphaOf(const Obj &o) {
  return easeOut(std::clamp(o.appear, 0.f, 1.f)) * o.lit * stageFade_ * opacityOf(o);
}

// shape= : the second categorical channel for things drawn as dots
static void drawShape(const std::string &shape, Vector2 c, float r, Color col) {
  if (shape == "square") { DrawRectangleRec({c.x - r * 0.88f, c.y - r * 0.88f, r * 1.76f, r * 1.76f}, col); return; }
  if (shape == "diamond") { DrawPoly(c, 4, r * 1.15f, 0, col); return; }
  if (shape == "triangle") { DrawPoly({c.x, c.y + r * 0.2f}, 3, r * 1.3f, -90, col); return; }
  if (shape == "hexagon") { DrawPoly(c, 6, r * 1.05f, 30, col); return; }
  if (shape == "star") {
    std::vector<Vector2> pts;
    for (int i = 0; i < 10; i++) {
      float a = -1.5707963f + i * 0.6283185f, rr = i % 2 ? r * 0.5f : r * 1.25f;
      pts.push_back({c.x + std::cos(a) * rr, c.y + std::sin(a) * rr});
    }
    fillPolygon(pts, col);
    return;
  }
  if (shape == "ring") { DrawRing(c, r * 0.62f, r, 0, 360, 48, col); return; }
  DrawCircleV(c, r, col);
}

// a category label that cross-fades while its value morphs
static void blendText(const Obj &o, const std::string &key, const std::string &now, Vector2 p, float size, Color c,
                      float ax, float ay) {
  auto it = o.strBlend.find(key);
  if (it == o.strBlend.end()) { text(now, p, size, c, ax, ay); return; }
  float t = it->second.second;
  text(it->second.first, p + Vector2{0, -8 * t}, size, fade(c, 1 - t), ax, ay);
  text(now, p + Vector2{0, 8 * (1 - t)}, size, fade(c, t), ax, ay);
}

// a label's text: text= (its value, if it is an expression), else its name
std::string Engine::labelText(const Mark &m) {
  const Value *v = findProp(m.props, "text");
  if (!v) return displayName(m);
  if (v->type == Value::EXPR) return valueText(m, "text");
  return v->str;
}

// ---------------------------------------------------------------- formulas
// A formula is a row of glyphs. It writes itself in left to right; a term written
// \textcolor{id}{…} takes that thing's color and stays lit while the voice is about it;
// when the formula changes, glyphs the two formulas share glide to their new places.
void Engine::drawTex(const Mark &m) {
  float a0 = easeOut(std::clamp(m.appear, 0.f, 1.f)) * stageFade_;
  if (a0 <= 0.003f) return;
  TexPtr now = texNow(m);
  if (!now) { text("…", m.pos, 22, fade(col::muted, a0), 0.5f, 0.5f); return; }
  if (!now->ok) { textBg("TeX: " + now->error, m.pos, 16, fade(col::red, a0), fade(col::bg, 0.9f)); return; }
  // over a field or heat map, a formula gets a plate (the field covers the whole plane: there is no free spot)
  if (isAxes(schemaNow()))
    for (auto &id : order_)
      if (Mark *f = mark(id); f && isFieldKind(effectiveKind(*f)) && alphaOf(*f) > 0.2f) {
        Rectangle b = bodyBounds(m);
        DrawRectangleRounded({b.x - 10, b.y - 6, b.width + 20, b.height + 12}, 0.2f, 8, fade(col::bg, 0.88f * a0));
        break;
      }
  const float px = texSize(m) / 1000.f;
  bool inked = catValue(m).empty() && !findProp(m.props, "color") && !findProp(m.props, "of");
  Color base = inked ? col::fg : colorOf(m);

  auto glyphColor = [&](const TexGlyph &g, float alpha) {
    Color c = base;
    float lit = m.lit;
    if (!g.color.empty()) {
      if (Mark *ref = mark(g.color)) {
        c = colorOf(*ref);
        if (!focus_.empty() && focus_ == g.color) lit = 1;
      } else if (auto lk = links_.find(g.color); lk != links_.end()) { // a link: its color=, or the link ink
        c = col::fg;
        if (auto cv = findProp(lk->second.props, "color"); cv && cv->type == Value::STR) colorByName(cv->str, c);
      } else {
        colorByName(g.color, c);
      }
    }
    return fade(c, a0 * lit * alpha);
  };
  auto originOf = [&](const TexLayout &L) { return m.pos - Vector2{L.w * px / 2, L.h * px / 2}; };

  // the formula it is morphing from, if any
  TexPtr old;
  float t = 1;
  if (auto it = m.strBlend.find("tex"); it != m.strBlend.end() && it->second.second < 1) {
    t = it->second.second;
    old = tex::request(texSource(it->second.first));
    if (old && (!old->ok || old.get() == now.get())) old.reset();
  }

  if (!old) {
    // write-in: glyphs fade up one after another as the mark appears
    const float lag = 0.7f;
    size_t n = now->glyphs.size();
    for (size_t i = 0; i < n; i++) {
      const TexGlyph &g = now->glyphs[i];
      float gt = std::clamp(m.appear * (1 + lag) - lag * (float)i / std::max<size_t>(1, n), 0.f, 1.f);
      if (gt <= 0) continue;
      float e = easeOut(gt);
      tex::drawGlyph(*now, g, originOf(*now) + Vector2{0, (1 - e) * 10}, px, glyphColor(g, e));
    }
    return;
  }

  // matching transform: the k-th glyph with a given character pairs with the k-th one in the other formula
  std::map<std::string, std::vector<size_t>> fromIx, toIx;
  for (size_t i = 0; i < old->glyphs.size(); i++) fromIx[old->glyphs[i].key].push_back(i);
  for (size_t i = 0; i < now->glyphs.size(); i++) toIx[now->glyphs[i].key].push_back(i);
  Vector2 o0 = originOf(*old), o1 = originOf(*now);
  for (auto &[key, from] : fromIx) {
    const auto &to = toIx[key];
    for (size_t j = 0; j < from.size(); j++) {
      const TexGlyph &ga = old->glyphs[from[j]];
      if (j >= to.size()) { // nothing to become: fade out, drifting up
        tex::drawGlyph(*old, ga, o0 + Vector2{0, -10 * t}, px, glyphColor(ga, 1 - t));
        continue;
      }
      const TexGlyph &gb = now->glyphs[to[j]];
      Vector2 pa = o0 + ga.at * px, pb = o1 + gb.at * px, p = lerp(pa, pb, t);
      bool same = old->shapes[ga.shape].id == now->shapes[gb.shape].id;
      if (same) { tex::drawGlyph(*now, gb, p - gb.at * px, px, glyphColor(gb, 1)); continue; }
      tex::drawGlyph(*old, ga, p - ga.at * px, px, glyphColor(ga, 1 - t));
      tex::drawGlyph(*now, gb, p - gb.at * px, px, glyphColor(gb, t));
    }
  }
  for (auto &[key, to] : toIx) {
    size_t have = fromIx.count(key) ? fromIx[key].size() : 0;
    for (size_t j = have; j < to.size(); j++) { // new symbols fade in, settling down
      const TexGlyph &gb = now->glyphs[to[j]];
      tex::drawGlyph(*now, gb, o1 + Vector2{0, 10 * (1 - t)}, px, glyphColor(gb, t));
    }
  }
}

// ---------------------------------------------------------------- world shapes (axes / plane units)
void Engine::drawWorld(const Mark &m) {
  if (!isAxes(schemaNow())) return; // linted at load
  float a = alphaOf(m);
  if (a <= 0.003f) return;
  std::string k = effectiveKind(m);
  Color c = colorOf(m);
  float e = easeOut(std::clamp(m.appear, 0.f, 1.f));
  float ppx = axes_.area.width / std::max(1e-6f, axes_.xmax - axes_.xmin);
  float ppy = axes_.area.height / std::max(1e-6f, axes_.ymax - axes_.ymin);
  auto S = [&](double x, double y) { return axes_.toScreen((float)x, (float)y); };

  if (k == "grid") { // the plane's grid carried by a 2x2 matrix m=[a,b,c,d]: morph m and the plane deforms
    auto M = listNums(m, "m");
    if (M.size() < 4) M = {1, 0, 0, 1};
    double step = num(m, "step");
    if (!std::isfinite(step) || step <= 0) step = 1;
    double ext = std::max({std::fabs(axes_.xmin), std::fabs(axes_.xmax), std::fabs(axes_.ymin), std::fabs(axes_.ymax)});
    int N = std::min(80, (int)std::ceil(ext * 3 / step) + 2);
    double R = N * step;
    auto T = [&](double x, double y) { return S(M[0] * x + M[1] * y, M[2] * x + M[3] * y); };
    BeginScissorMode((int)axes_.area.x, (int)axes_.area.y, (int)axes_.area.width, (int)axes_.area.height);
    for (int i = -N; i <= N; i++) {
      double p = i * step;
      float w = i == 0 ? 2.5f : 1.2f, al = a * (i == 0 ? 0.85f : 0.4f) * e;
      DrawLineEx(T(p, -R), T(p, R), w, fade(c, al));
      DrawLineEx(T(-R, p), T(R, p), w, fade(c, al));
    }
    EndScissorMode();
    return;
  }
  if (k == "circle") {
    Vector2 ctr = S(num(m, "x"), num(m, "y"));
    double r = num(m, "r");
    if (!std::isfinite(r) || r <= 0) r = 0.5;
    std::vector<Vector2> pts;
    for (int i = 0; i <= 96; i++) {
      float t = -1.5707963f + 6.2831853f * i / 96;
      pts.push_back({ctr.x + std::cos(t) * (float)r * ppx, ctr.y + std::sin(t) * (float)r * ppy});
    }
    if (pts.size() > 3) fillPolygon(pts, fade(c, a * 0.18f * e));
    strokePartial(pts, e, 3, fade(c, a), false);
    drawLabels(m);
    return;
  }
  if (k == "rect") {
    double x = num(m, "x"), y = num(m, "y"), w = num(m, "w"), h = num(m, "h");
    if (!std::isfinite(w)) w = 1;
    if (!std::isfinite(h)) h = 1;
    Vector2 p0 = S(x - w / 2, y + h / 2), p1 = S(x + w / 2, y - h / 2);
    Rectangle r{p0.x, p0.y, p1.x - p0.x, p1.y - p0.y};
    float sc = 0.8f + 0.2f * e;
    Rectangle rr{r.x + r.width * (1 - sc) / 2, r.y + r.height * (1 - sc) / 2, r.width * sc, r.height * sc};
    const Value *sh = findProp(m.props, "shape");
    std::string look = sh && sh->type == Value::STR ? sh->str : "block";
    if (look == "cart") { // a box on two wheels
      float wr = std::min(rr.height * 0.22f, rr.width * 0.12f);
      Rectangle body{rr.x, rr.y, rr.width, rr.height - wr};
      roundedBox(body, 6, 2.5f, fade(c, a), fade(c, a * 0.55f));
      for (float fx : {0.25f, 0.75f}) {
        Vector2 wc{rr.x + rr.width * fx, rr.y + rr.height - wr};
        DrawCircleV(wc, wr, fade(col::bg, a));
        DrawRing(wc, wr - 2.5f, wr, 0, 360, 32, fade(c, a));
      }
    } else {
      roundedBox(rr, 6, 2.5f, fade(c, a), fade(c, a * 0.55f));
    }
    drawLabels(m);
    return;
  }
  if (k == "polygon") {
    auto p = listNums(m, "pts");
    std::vector<Vector2> pts;
    for (size_t i = 0; i + 1 < p.size(); i += 2) pts.push_back(S(p[i], p[i + 1]));
    if (pts.size() < 2) return;
    if (pts.size() >= 3) fillPolygon(pts, fade(c, a * 0.22f * e));
    strokePartial(pts, e, 3, fade(c, a), true);
    drawLabels(m);
    return;
  }
  if (k == "angle") { // pts = vertex, a point on each ray
    auto p = listNums(m, "pts");
    if (p.size() < 6) return;
    Vector2 v = S(p[0], p[1]), A = S(p[2], p[3]), B = S(p[4], p[5]);
    float ta = std::atan2(A.y - v.y, A.x - v.x), tb = std::atan2(B.y - v.y, B.x - v.x);
    float d = tb - ta;
    while (d > 3.14159265f) d -= 6.2831853f;
    while (d < -3.14159265f) d += 6.2831853f;
    float deg = std::fabs(d) * 57.29578f, rad = 42;
    if (std::fabs(deg - 90) < 0.5f) { // a right angle is a little square
      Vector2 ua = norm(A - v) * 22, ub = norm(B - v) * 22;
      std::vector<Vector2> sq{v + ua, v + ua + ub, v + ub};
      strokePartial(sq, e, 2.5f, fade(c, a), false);
    } else {
      std::vector<Vector2> arc;
      for (int i = 0; i <= 40; i++) {
        float t = ta + d * i / 40;
        arc.push_back({v.x + std::cos(t) * rad, v.y + std::sin(t) * rad});
      }
      std::vector<Vector2> fan{v};
      fan.insert(fan.end(), arc.begin(), arc.end());
      fillPolygon(fan, fade(c, a * 0.2f * e));
      strokePartial(arc, e, 3, fade(c, a), false);
    }
    drawLabels(m);
    return;
  }
  if (k == "vector") {
    Vector2 s0, e1;
    if (!vectorEnds(m, s0, e1)) return;
    Vector2 tip = lerp(s0, e1, e);
    if (len(e1 - s0) > 3) arrow(s0, tip, 4, fade(c, a), 16);
    drawLabels(m);
    return;
  }
}

// ---------------------------------------------------------------- blocks
void Engine::drawBlock(const Mark &m) {
  float a = alphaOf(m);
  if (a <= 0.003f) return;
  std::string k = effectiveKind(m);
  Rectangle r = bodyBounds(m);
  float e = easeOut(std::clamp(m.appear, 0.f, 1.f));
  r.y += (1 - e) * 10;
  bool inked = catValue(m).empty() && !findProp(m.props, "color") && !findProp(m.props, "of");
  Color c = inked ? col::muted : colorOf(m);
  const Value *lv = findProp(m.props, "label");
  float y = r.y;
  if (lv && k != "card" && k != "code") { text(lv->str, {r.x, y}, 17, fade(col::muted, a), 0, 0); y += 26; }
  const Value *tv = findProp(m.props, "text");
  std::string txt = tv ? (tv->type == Value::STR ? tv->str : labelText(m)) : "";

  if (k == "card") {
    roundedBox(r, 12, 2, fade(c, a * 0.9f), fade(col::bg, a * 0.92f));
    DrawRectangleRec({r.x + 2, r.y + 10, 4, r.height - 20}, fade(c, a));
    y = r.y + 14;
    if (lv) { text(lv->str, {r.x + 20, y}, 17, fade(c, a), 0, 0); y += 26; }
    for (auto &l : wrap(txt, 24, 520)) { text(l, {r.x + 20, y}, 24, fade(col::fg, a), 0, 0); y += 31; }
    return;
  }
  if (k == "code") {
    roundedBox(r, 10, 1.5f, fade(c, a * 0.6f), fade(Color{14, 16, 22, 255}, a));
    y = r.y + 12;
    if (lv) { text(lv->str, {r.x + 14, y}, 17, fade(col::muted, a), 0, 0); y += 26; }
    std::vector<std::string> lines;
    size_t i = 0;
    for (size_t p; (p = txt.find("\\n", i)) != std::string::npos; i = p + 2) lines.push_back(txt.substr(i, p - i));
    lines.push_back(txt.substr(i));
    bool hasLine;
    double cur = num(m, "line", &hasLine); // 1-based, eased when morphed: the highlight glides
    if (hasLine && std::isfinite(cur) && cur >= 1) {
      float hy = y + (float)(cur - 1) * 27 - 2;
      DrawRectangleRounded({r.x + 6, hy, r.width - 12, 27}, 0.3f, 6, fade(col::yellow, a * 0.16f));
      DrawRectangleRec({r.x + 6, hy, 3, 27}, fade(col::yellow, a));
    }
    for (size_t li = 0; li < lines.size(); li++) {
      bool on = hasLine && std::fabs(cur - (double)(li + 1)) < 0.5;
      text(std::to_string(li + 1), {r.x + 34, y + 2}, 15, fade(col::muted, a * 0.7f), 1, 0);
      textMono(lines[li], {r.x + 46, y}, 20, fade(on ? col::fg : lerpColor(col::fg, col::muted, 0.35f), a));
      y += 27;
    }
    return;
  }
  if (k == "array") {
    const Value *iv = findProp(m.props, "items");
    if (!iv || iv->type != Value::LIST) return;
    int focusCell = -1;
    if (!focus_.empty() && baseId(focus_, &focusCell) == m.id) {} else focusCell = -1;
    const Value *ix = findProp(m.props, "index");
    bool showIndex = !(ix && ix->num == 0);
    // mid-morph (a swap, a shift): each value travels from its old cell on a little arc
    std::vector<int> from(iv->list.size(), -1);
    float bt = 1;
    if (auto it = m.strBlend.find("items"); it != m.strBlend.end() && it->second.second < 1) {
      Value old = Value::parse(it->second.first);
      bt = rate::smooth(it->second.second);
      std::vector<bool> used(old.list.size(), false);
      for (size_t j = 0; j < iv->list.size(); j++) {
        for (size_t i = 0; i < old.list.size(); i++)
          if (!used[i] && old.list[i] == iv->list[j]) { if (i == j) { from[j] = (int)i; used[i] = true; break; } }
        if (from[j] >= 0) continue;
        for (size_t i = 0; i < old.list.size(); i++)
          if (!used[i] && old.list[i] == iv->list[j]) { from[j] = (int)i; used[i] = true; break; }
      }
    }
    for (size_t i = 0; i < iv->list.size(); i++) {
      Rectangle cell = cellRect(m, (int)i);
      cell.y += (1 - e) * 10;
      float ca = a * (focusCell < 0 || focusCell == (int)i ? 1.f : 0.3f);
      Color cc = c; // colors=[teal,-,teal,…]: cells that belong together share a colour
      bool tinted = false;
      if (const Value *cl = findProp(m.props, "colors"); cl && cl->type == Value::LIST && i < cl->list.size())
        tinted = colorByName(cl->list[i], cc);
      DrawRectangleRec(cell, fade(cc, ca * (tinted ? 0.38f : 0.16f)));
      DrawRectangleLinesEx(cell, 2, fade(cc, ca));
      if (showIndex) text(std::to_string(i), {cell.x + cell.width / 2, cell.y + cell.height + 4}, 14, fade(col::muted, ca * 0.8f), 0.5f, 0);
    }
    for (size_t j = 0; j < iv->list.size(); j++) {
      Rectangle cj = cellRect(m, (int)j);
      Vector2 to{cj.x + cj.width / 2, cj.y + (1 - e) * 10 + cj.height / 2}, p = to;
      float va = a * (focusCell < 0 || focusCell == (int)j ? 1.f : 0.3f);
      if (bt < 1 && from[j] >= 0 && from[j] != (int)j) {
        Rectangle ci = cellRect(m, from[j]);
        Vector2 fr{ci.x + ci.width / 2, to.y};
        p = lerp(fr, to, bt);
        p.y -= std::sin(bt * 3.14159265f) * (from[j] < (int)j ? 34.f : -34.f); // swaps pass each other
        va = a;
      } else if (bt < 1 && from[j] < 0) {
        va *= bt; // a new value fades in
      }
      text(iv->list[j], p, 24, fade(col::fg, va), 0.5f, 0.5f);
    }
    return;
  }
  if (k == "table") {
    const Value *cv = findProp(m.props, "cells");
    if (!cv || cv->type != Value::LIST) return;
    int cols = std::max(1, (int)num(m, "cols"));
    size_t n = cv->list.size();
    int rows = (int)((n + cols - 1) / cols);
    std::vector<float> cw(cols, 40);
    for (size_t i = 0; i < n; i++) cw[i % cols] = std::max(cw[i % cols], measure(cv->list[i], 20).x + 28);
    for (int row = 0; row < rows; row++) {
      float x = r.x;
      if (row == 0) DrawRectangleRec({r.x, y, r.width, 38}, fade(c, a * 0.18f));
      for (int col = 0; col < cols; col++) {
        size_t i = (size_t)(row * cols + col);
        if (i < n) text(cv->list[i], {x + cw[col] / 2, y + 19}, 20, fade(row == 0 ? c : col::fg, a), 0.5f, 0.5f);
        x += cw[col];
      }
      DrawLineEx({r.x, y + 38}, {r.x + r.width, y + 38}, row == 0 ? 2.f : 1.f, fade(col::muted, a * (row == 0 ? 0.8f : 0.35f)));
      y += 38;
    }
    return;
  }
}

// indicate: a ring that swells and fades around the thing (or the one cell) the voice points at
void Engine::drawPulses() {
  for (auto &[id, m] : marks_) {
    if (m.pulse <= 0.01f) continue;
    Rectangle b;
    if (!anchorBox(m.pulseRef.empty() ? id : m.pulseRef, b)) continue;
    float g = 6 + 10 * m.pulse;
    Rectangle rr{b.x - g, b.y - g, b.width + 2 * g, b.height + 2 * g};
    DrawRectangleRoundedLinesEx(rr, 0.25f, 8, 3.5f, fade(col::yellow, m.pulse * stageFade_));
    DrawRectangleRounded(rr, 0.25f, 8, fade(col::yellow, 0.08f * m.pulse * stageFade_));
  }
}

// a mark's outside text, exactly where layout put it (markLabels)
void Engine::drawLabels(const Mark &m) {
  float a = alphaOf(m);
  Color c = colorOf(m);
  for (const TextLabel &l : markLabels(m)) {
    if (l.leader) DrawLineEx(l.leaderFrom, l.leaderTo, l.leaderWidth, fade(c, a * l.leaderAlpha));
    Color tc = l.ink == TextLabel::Fg ? fade(col::fg, a) : l.ink == TextLabel::Muted ? fade(col::muted, a) : fade(c, a);
    if (!l.blendKey.empty()) blendText(m, l.blendKey, l.text, l.at, l.size, tc, l.ax, l.ay);
    else text(l.text, l.at, l.size, tc, l.ax, l.ay);
  }
}

void Engine::drawMark(const Mark &mc) {
  Mark &m = const_cast<Mark &>(mc);
  if (!m.positioned && effectiveKind(m) != "label") return;
  std::string k = effectiveKind(m), s = schemaNow();
  if (k == "tex") { drawTex(m); return; }
  if (isWorldKind(k)) {
    // under a camera, shapes are clipped to the axes like curves are
    if (cam_.on) BeginScissorMode((int)axes_.area.x - 4, (int)axes_.area.y - 4, (int)axes_.area.width + 8, (int)axes_.area.height + 8);
    drawWorld(m);
    if (cam_.on) EndScissorMode();
    return;
  }
  if (isBlockKind(k)) { drawBlock(m); return; }
  float a = alphaOf(m);
  if (a <= 0.003f) return;
  Color c = colorOf(m);
  Color fg = fade(col::fg, a), mutedC = fade(col::muted, a);
  std::string name = displayName(m);
  std::string mp = magProp(m);
  std::string cp = catProp(m), cv = catValue(m);

  if (k == "bar") {
    if (m.anchorBottom) {
      DrawRectangleRec(bodyBounds(m), fade(c, a));
      drawLabels(m);
      return;
    }
    if (isAxes(s)) {
      Rectangle r = bodyBounds(m);
      DrawRectangleRec(r, fade(c, a * 0.85f));
      DrawRectangleLinesEx(r, 1.5f, fade(c, a));
      drawLabels(m);
      return;
    }
    // graph: a box that also shows its magnitude as a little bar on a shared scale
    Rectangle r = bodyBounds(m);
    roundedBox(r, 10, 2.5f, fade(c, a), fade(c, a * 0.14f));
    text(name, {r.x + r.width / 2, r.y + 10}, 21, fg, 0.5f, 0);
    if (!mp.empty()) {
      double mx = std::max(1e-9, maxAbs_.count(mp) ? maxAbs_[mp] : 1.0);
      float w = (float)(std::clamp(num(m, mp) / mx, 0.0, 1.0) * (r.width - 24));
      DrawRectangleRec({r.x + 12, r.y + r.height - 18, w, 8}, fade(c, a));
      text(valueText(m, mp), {r.x + r.width / 2, r.y + 36}, 17, mutedC, 0.5f, 0);
    }
    return;
  }

  if (k == "dot") {
    float r = dotRadius(m) * easeBack(m.appear);
    Vector2 ctr = m.anchorBottom ? Vector2{m.pos.x, m.pos.y - dotRadius(m)} : m.pos;
    const Value *sh = findProp(m.props, "shape");
    drawShape(sh && sh->type == Value::STR ? sh->str : "circle", ctr, r, fade(c, a));
    drawLabels(m);
    if (!m.anchorBottom && s != "numberline" && s != "timeline" && !isAxes(s) && !mp.empty()) // graph: value inside
      text(valueText(m, mp), {ctr.x, ctr.y}, 15, fade(col::bg, a), 0.5f, 0.5f);
    return;
  }

  if (k == "box") {
    Rectangle r = bodyBounds(m);
    float sc = 0.85f + 0.15f * easeOut(m.appear);
    Rectangle rr{r.x + r.width * (1 - sc) / 2, r.y + r.height * (1 - sc) / 2, r.width * sc, r.height * sc};
    roundedBox(rr, 10, 2.5f, fade(c, a), fade(c, a * 0.14f));
    float y = rr.y + 10;
    text(name, {rr.x + rr.width / 2, y}, 22, fg, 0.5f, 0);
    y += 30;
    if (!m.category.empty()) { blendText(m, "@category", m.category, {rr.x + rr.width / 2, y}, 15, mutedC, 0.5f, 0); y += 22; }
    for (auto &p : m.props) {
      static const std::set<std::string> skip = {
          "label", "name",  "color", "unit", "x",     "y",     "t",     "at",    "year",  "r",     "w",    "pos",
          "shape", "bend",  "range", "opacity", "side", "to",  "dx",    "dy",    "h",     "size",  "line", "items",
          "cells", "pts",   "cols",  "grid", "axes",  "equal", "index", "n",     "m",     "xlabel", "ylabel", "tex", "text"};
      if (skip.count(p.key) || isPlacementKey(p.key)) continue;
      std::string v = p.val.isNumeric() ? valueText(m, p.key) : p.val.type == Value::STR ? p.val.str : p.val.markup();
      text(p.key, {rr.x + 14, y}, 17, mutedC, 0, 0);
      if (p.val.type == Value::STR) blendText(m, p.key, v, {rr.x + rr.width - 14, y}, 17, fg, 1, 0);
      else text(v, {rr.x + rr.width - 14, y}, 17, fg, 1, 0);
      y += 22;
    }
    return;
  }

  if (k == "label") {
    if (!m.positioned) return; // info stack draws it
    std::string t = labelText(m);
    if (s == "numberline" && !nlProp_.empty() && findProp(m.props, nlProp_)) {
      // a marker under the line: ▲ mean 62
      Vector2 p{m.pos.x, nl_.a.y + 34};
      tri(p + Vector2{0, -12}, p + Vector2{-8, 2}, p + Vector2{8, 2}, fade(c, a));
      DrawLineEx({m.pos.x, nl_.a.y - 70}, {m.pos.x, nl_.a.y + 20}, 2, fade(c, a * 0.6f));
      drawLabels(m);
      return;
    }
    if (!mp.empty() && !findProp(m.props, "text")) t += " = " + valueText(m, mp);
    // a pointer: at=<thing> / at=a[3] draws a short arrow up to what it names
    Rectangle box;
    if (auto at = findProp(m.props, "at"); at && at->type == Value::STR && anchorBox(at->str, box)) {
      // up to the target, clear of the index row; glides with the label, stays within the target
      Vector2 tip{std::clamp(m.pos.x, box.x + 8, box.x + box.width - 8), box.y + box.height + 22};
      if (at->str.find(':') != std::string::npos) { // a span: a bracket under the cells it covers
        float y = box.y + box.height + 26;
        DrawLineEx({box.x + 4, y}, {box.x + box.width - 4, y}, 2.5f, fade(c, a));
        DrawLineEx({box.x + 4, y - 8}, {box.x + 4, y}, 2.5f, fade(c, a));
        DrawLineEx({box.x + box.width - 4, y - 8}, {box.x + box.width - 4, y}, 2.5f, fade(c, a));
        tip = {box.x + box.width / 2, y + 4};
      }
      if (m.pos.y - 12 > tip.y + 6) arrow({m.pos.x, m.pos.y - 12}, tip, 2.5f, fade(c, a), 10);
    }
    text(t, m.pos, 19, fade(c, a), 0.5f, 0.5f);
    return;
  }

  if (!isAxes(s)) return; // the remaining kinds live on axes / plane

  if (isFieldKind(k)) { drawFieldMark(m); return; }
  if (isCurveKind(k)) {
    auto pts = curvePoints(m);
    BeginScissorMode((int)axes_.area.x - 4, (int)axes_.area.y - 4, (int)axes_.area.width + 8, (int)axes_.area.height + 8);
    strokePartial(pts, easeOut(m.appear), 3.5f, fade(c, m.lit * stageFade_ * opacityOf(m)), false);
    EndScissorMode();
    drawLabels(m);
    return;
  }

  if (k == "region") {
    auto of = findProp(m.props, "of");
    Mark *p = of ? mark(of->str) : nullptr;
    if (!p) return;
    double f = num(m, "from"), t = num(m, "to");
    if (!std::isfinite(f)) f = axes_.xmin;
    if (!std::isfinite(t)) t = axes_.xmax;
    bool hn;
    double nn = num(m, "n", &hn);
    if (cam_.on) BeginScissorMode((int)axes_.area.x, (int)axes_.area.y, (int)axes_.area.width, (int)axes_.area.height);
    if (hn && nn >= 1) { // n slices: the Riemann picture of an integral (midpoint heights)
      int n = std::min(400, (int)std::round(nn));
      double w = (t - f) / n;
      for (int i = 0; i < n; i++) {
        double x0 = f + i * w, h = pathAt(*p, x0 + w / 2);
        if (!std::isfinite(h)) continue;
        Vector2 A = axes_.toScreen((float)x0, (float)std::max(0.0, h)), B = axes_.toScreen((float)(x0 + w), (float)std::min(0.0, h));
        Rectangle rr{A.x, A.y, B.x - A.x, B.y - A.y};
        float grow = std::clamp(easeOut(m.appear) * n - i * 0.6f, 0.f, 1.f);
        rr.y += rr.height * (1 - grow); rr.height *= grow;
        DrawRectangleRec(rr, fade(c, a * 0.32f));
        DrawRectangleLinesEx(rr, 1.5f, fade(c, a * 0.9f));
      }
    } else {
      axes_.fillUnder([&](double x) { return pathAt(*p, x); }, (float)std::max(f, (double)axes_.xmin),
                      (float)std::min(t, (double)axes_.xmax), fade(c, a * 0.32f), 160);
    }
    if (cam_.on) EndScissorMode();
    drawLabels(m);
    return;
  }

  if (k == "line" || k == "arrow") {
    Vector2 A, B;
    bool hs;
    if (!lineEnds(m, A, B, hs)) return;
    BeginScissorMode((int)axes_.area.x, (int)axes_.area.y, (int)axes_.area.width, (int)axes_.area.height);
    Vector2 Bp = lerp(A, B, easeOut(m.appear));
    if (k == "arrow") arrow(A, Bp, 3.5f, fade(c, a), 16);
    else DrawLineEx(A, Bp, 3, fade(c, a));
    EndScissorMode();
    drawLabels(m);
  }
}

void Engine::drawLink(const Link &l) {
  float a = alphaOf(l);
  Mark *ma = mark(l.a), *mb = mark(l.b);
  if (ma) a *= opacityOf(*ma); // a relation fades with what it connects
  if (mb) a *= opacityOf(*mb);
  if (a <= 0.003f) return;
  LinkGeom g = linkGeometry(l);
  if (!g.ok) return;
  Color c = fade(col::fg, a * 0.85f);
  if (auto cv = findProp(l.props, "color"); cv && cv->type == Value::STR) { Color cc; if (colorByName(cv->str, cc)) c = fade(cc, a); }
  float frac = easeOut(std::clamp(l.appear * 1.2f, 0.f, 1.f));
  bool arrowHead = l.kind == "arrow";
  bool dashedK = l.kind == "similar" || l.kind == "equals";
  if (g.overBars || std::fabs(g.bend) > 0.5f) { // an arc: over baseline bars, or a bent link
    if (dashedK) {
      for (int i = 0; i < 24; i += 2)
        if (i / 24.f < frac) DrawLineEx(arcPoint(g.pa, g.pb, g.bend, i / 24.f), arcPoint(g.pa, g.pb, g.bend, (i + 1) / 24.f), 2.5f, c);
    } else {
      arcArrow(g.pa, g.pb, g.bend, 3, c, frac, arrowHead);
    }
  } else {
    Vector2 end = lerp(g.pa, g.pb, frac);
    if (dashedK) dashed(g.pa, end, 2.5f, c);
    else if (arrowHead && frac > 0.98f) arrow(g.pa, g.pb, 3, c, 16);
    else DrawLineEx(g.pa, end, 3, c);
  }
  if (!g.label.empty() && frac > 0.6f) textBg(g.label, g.mid, 18, fade(col::fg, a), fade(col::bg, a * 0.9f), 0.5f, 0.5f, 6);
}

void Engine::drawGroup(const Group &g) {
  float a = alphaOf(g);
  if (a <= 0.003f) return;
  Rectangle r = groupBounds(g);
  if (r.width <= 0) return;
  Color c = col::muted;
  if (auto cv = findProp(g.props, "color"); cv && cv->type == Value::STR) colorByName(cv->str, c);
  roundedBox(r, 14, 2, fade(c, a * 0.9f), fade(c, a * 0.07f));
  text(displayName(g), {r.x + 12, r.y + 6}, 18, fade(c, a), 0, 0);
}

void Engine::drawInfo() {
  // labels the schema did not place: a quiet stack, top-left (parameters, exact values)
  float x = 56, y = 74;
  if (isAxes(schemaNow())) { x = axes_.area.x + 18; y = axes_.area.y + 8; }
  for (auto &id : order_) {
    Mark *m = mark(id);
    if (!m || effectiveKind(*m) != "label" || m->positioned) continue;
    float a = alphaOf(*m);
    if (a <= 0.01f) continue; // hidden
    std::string t = labelText(*m);
    std::string mp = magProp(*m);
    if (!mp.empty() && !findProp(m->props, "text")) t += " = " + valueText(*m, mp);
    textBg(t, {x, y}, 21, fade(colorOf(*m), a), fade(col::bg, 0.85f * a), 0, 0, 6);
    y += 30;
  }
}

void Engine::drawSubtitle() {
  float alpha = say_.active ? 1.f : std::clamp(say_.lingering / 0.6f, 0.f, 1.f) * 0.75f;
  if (alpha <= 0 || say_.words.empty()) return;
  const float size = 26, maxW = 1040, space = measure(" ", size).x;
  std::vector<std::vector<size_t>> lines(1);
  float w = 0;
  for (size_t i = 0; i < say_.words.size(); i++) {
    float ww = measure(say_.words[i].text, size).x;
    if (w + ww > maxW && !lines.back().empty()) { lines.emplace_back(); w = 0; }
    lines.back().push_back(i);
    w += ww + space;
  }
  float y = 592;
  if (lines.size() > 2) y -= 16 * (lines.size() - 2);
  for (auto &L : lines) {
    float lw = 0;
    for (size_t i : L) lw += measure(say_.words[i].text, size).x + space;
    float x = W / 2.f - lw / 2;
    for (size_t i : L) {
      const SayWord &sw = say_.words[i];
      bool spoken = !say_.active || i < say_.nextWord;
      bool current = say_.active && i + 1 == say_.nextWord;
      Color c = spoken ? col::fg : fade(col::fg, 0.32f);
      if (current) c = col::yellow;
      text(sw.text, {x, y}, size, fade(c, alpha), 0, 0);
      float ww = measure(sw.text, size).x;
      if (!sw.hooks.empty() && spoken) DrawLineEx({x, y + size + 2}, {x + ww, y + size + 2}, 2, fade(col::yellow, alpha * 0.6f));
      x += ww + space;
    }
    y += size + 8;
  }
}

void Engine::drawHud() {
  if (showDict_ && !caption_.empty()) {
    float a = captionT_ < 0.3f ? captionT_ / 0.3f : captionT_ > 8 ? 0.45f : 1.f;
    text("dictionary  " + caption_, {W / 2.f, 694}, 16, fade(col::muted, a), 0.5f, 0.5f);
  }
  text("space pause   → skip   R restart   D dictionary   S screenshot", {W - 14.f, 12}, 14, fade(col::muted, 0.7f), 1, 0);
  // progress
  float p = script_.steps.empty() ? 1.f : (float)cursor_ / script_.steps.size();
  DrawRectangle(0, H - 3, (int)(W * p), 3, fade(col::blue, 0.6f));
  if (paused_) textBg("paused", {W / 2.f, 40}, 22, col::fg, fade(col::grid, 0.9f));
  else if (waitingPause_) textBg("press → to continue", {W / 2.f, 40}, 20, col::fg, fade(col::grid, 0.9f));
}

void Engine::draw() {
  ClearBackground(col::bg);
  std::string s = schemaNow();
  if (!title_.empty()) text(title_, {24, 14}, 24, col::muted, 0, 0);

  // schema furniture
  const float F = stageFade_;
  if (s == "numberline") nl_.draw(col::fg, F, true);
  if (s == "timeline") {
    DrawLineEx(nl_.a - Vector2{20, 0}, nl_.b + Vector2{24, 0}, 3, fade(col::muted, F));
    arrow(nl_.b, nl_.b + Vector2{30, 0}, 3, fade(col::muted, F), 12);
    float st = niceStep(nl_.max - nl_.min, 10);
    for (float t = std::ceil(nl_.min / st) * st; t <= nl_.max; t += st) {
      Vector2 p = nl_.toScreen(t);
      DrawLineEx(p - Vector2{0, 5}, p + Vector2{0, 5}, 2, fade(col::muted, F));
      text(fmtNum(t), p + Vector2{0, 10}, 15, fade(col::muted, 0.8f * F), 0.5f, 0);
    }
  }
  if (isAxes(s)) // heat maps lie under the axes and grid
    for (auto &id : order_)
      if (Mark *m = mark(id); m && m->positioned && effectiveKind(*m) == "heat") drawMark(*m);
  if (s == "axes") axes_.draw(col::muted, F, true, true);
  if (isAxes(s)) { // axis titles: xlabel= ylabel= on the schema
    auto xl = findProp(schemaProps_, "xlabel"), yl = findProp(schemaProps_, "ylabel");
    float y0 = std::clamp(0.f, axes_.ymin, axes_.ymax), x0 = std::clamp(0.f, axes_.xmin, axes_.xmax);
    if (xl && xl->type == Value::STR) {
      Vector2 p = axes_.toScreen(axes_.xmax, y0);
      text(xl->str, {p.x, p.y + 28}, 18, fade(col::fg, 0.8f * F), 1, 0);
    }
    if (yl && yl->type == Value::STR) {
      Vector2 p = axes_.toScreen(x0, axes_.ymax);
      text(yl->str, {p.x + 14, p.y + 4}, 18, fade(col::fg, 0.8f * F), 0, 0);
    }
  }
  if (s == "plane") { // a clean world: a faint grid and axes unless turned off
    auto g = findProp(schemaProps_, "grid"), ax = findProp(schemaProps_, "axes");
    bool grid = !(g && g->num == 0), axesOn = !(ax && ax->num == 0);
    if (axesOn) axes_.draw(fade(col::muted, 0.8f), F, true, grid);
    else if (grid) axes_.draw(fade(col::muted, 0.35f), F * 0.6f, false, true);
  }

  // shared zero baselines, one per run of bars with the same property
  {
    std::vector<Mark *> bars;
    for (auto &id : order_)
      if (Mark *m = mark(id); m && m->anchorBottom && m->positioned && effectiveKind(*m) == "bar") bars.push_back(m);
    // a row is bars of one property on one zero line (a crowded stage wraps into two rows)
    std::stable_sort(bars.begin(), bars.end(), [](Mark *a, Mark *b) {
      return std::fabs(a->pos.y - b->pos.y) > 1 ? a->pos.y < b->pos.y : a->pos.x < b->pos.x;
    });
    size_t i = 0;
    while (i < bars.size()) {
      std::string p = magProp(*bars[i]);
      size_t j = i;
      while (j + 1 < bars.size() && magProp(*bars[j + 1]) == p && std::fabs(bars[j + 1]->pos.y - bars[i]->pos.y) < 1) j++;
      float x0 = bars[i]->pos.x - 46, x1 = bars[j]->pos.x + 46, y = bars[i]->pos.y;
      float a = 0;
      for (size_t k = i; k <= j; k++) a = std::max(a, alphaOf(*bars[k]) / std::max(0.01f, bars[k]->lit * F) * F);
      DrawLineEx({x0, y}, {x1, y}, 2, fade(col::muted, a));
      text("0", {x0 - 6, y}, 14, fade(col::muted, a * 0.8f), 1, 0.5f);
      i = j + 1;
    }
  }

  for (auto &id : order_) if (auto g = groups_.find(id); g != groups_.end()) drawGroup(g->second);
  for (auto &id : order_) // contours, slope and vector fields: over the grid, under everything else
    if (Mark *m = mark(id); m && isFieldKind(effectiveKind(*m)) && effectiveKind(*m) != "heat") drawMark(*m);
  for (int pass = 0; pass < 2; pass++) // regions under curves
    for (auto &id : order_)
      if (Mark *m = mark(id)) {
        std::string k = effectiveKind(*m);
        bool under = k == "region";
        bool curve = isCurveKind(k) || k == "line" || k == "arrow";
        if ((pass == 0 && under) || (pass == 1 && curve)) drawMark(*m);
      }
  for (auto &id : order_) // grids, then world shapes, then the vectors drawn over them
    if (Mark *m = mark(id); m && effectiveKind(*m) == "grid") drawMark(*m);
  for (auto &id : order_)
    if (Mark *m = mark(id); m && isWorldKind(effectiveKind(*m)) && effectiveKind(*m) != "vector" && effectiveKind(*m) != "grid") drawMark(*m);
  for (auto &id : order_) if (auto l = links_.find(id); l != links_.end()) drawLink(l->second);
  for (auto &id : order_)
    if (Mark *m = mark(id); m && effectiveKind(*m) == "vector") drawMark(*m);
  if (isAxes(s)) { // trails under the things that leave them
    BeginScissorMode((int)axes_.area.x, (int)axes_.area.y, (int)axes_.area.width, (int)axes_.area.height);
    for (auto &id : order_)
      if (Mark *m = mark(id); m && findProp(m->props, "trail")) drawTrail(*m);
    EndScissorMode();
  }
  for (auto &id : order_)
    if (Mark *m = mark(id)) {
      std::string k = effectiveKind(*m);
      if (k == "dot" || k == "bar" || k == "box" || k == "label" || isBlockKind(k)) drawMark(*m);
    }
  drawPulses();
  drawInfo();
  drawSubtitle();
  drawHud();
}

} // namespace mk
