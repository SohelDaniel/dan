#include "raymanim.hpp"
#include "rlgl.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <unordered_map>

namespace rm {

// ================================================================ math
float len(Vector2 a) { return std::sqrt(a.x * a.x + a.y * a.y); }
Vector2 norm(Vector2 a) {
  float l = len(a);
  return l > 1e-6f ? a * (1.f / l) : Vector2{0, 0};
}
Vector2 lerp(Vector2 a, Vector2 b, float t) { return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}; }
float lerpf(float a, float b, float t) { return a + (b - a) * t; }
Vector2 U(float x, float y) { return {W / 2.f + x * (W / 14.222f), H / 2.f - y * (H / 8.f)}; }
float smoothApproach(float cur, float target, float dt, float speed) {
  return target + (cur - target) * std::exp(-dt * speed);
}
Vector2 smoothApproach(Vector2 cur, Vector2 target, float dt, float speed) {
  return {smoothApproach(cur.x, target.x, dt, speed), smoothApproach(cur.y, target.y, dt, speed)};
}

// ================================================================ colors
namespace col {
const Color bg{22, 27, 38, 255};
const Color fg{232, 236, 243, 255};
const Color muted{128, 138, 156, 255};
const Color grid{48, 56, 72, 255};
const Color blue{88, 166, 255, 255};
const Color orange{240, 138, 76, 255};
const Color green{106, 204, 130, 255};
const Color red{238, 98, 98, 255};
const Color yellow{240, 204, 92, 255};
const Color purple{176, 136, 245, 255};
const Color teal{72, 204, 204, 255};
const Color pink{238, 128, 192, 255};
const Color brown{196, 150, 104, 255};
const Color lime{190, 224, 90, 255};
} // namespace col

static const Color kPalette[] = {col::blue, col::orange, col::green, col::purple, col::yellow,
                                 col::teal, col::pink, col::red, col::brown, col::lime};
Color categorical(int i) { return kPalette[((i % 10) + 10) % 10]; }
int categoricalCount() { return 10; }
Color fade(Color c, float a) {
  a = std::clamp(a, 0.f, 1.f);
  return {c.r, c.g, c.b, (unsigned char)(c.a * a)};
}
Color lerpColor(Color a, Color b, float t) {
  t = std::clamp(t, 0.f, 1.f);
  return {(unsigned char)lerpf(a.r, b.r, t), (unsigned char)lerpf(a.g, b.g, t),
          (unsigned char)lerpf(a.b, b.b, t), (unsigned char)lerpf(a.a, b.a, t)};
}
bool colorByName(const std::string &n, Color &out) {
  struct E { const char *n; Color c; };
  static const E table[] = {{"blue", col::blue},     {"orange", col::orange}, {"green", col::green},
                            {"red", col::red},       {"yellow", col::yellow}, {"purple", col::purple},
                            {"teal", col::teal},     {"pink", col::pink},     {"brown", col::brown},
                            {"lime", col::lime},     {"white", col::fg},      {"grey", col::muted},
                            {"gray", col::muted}};
  for (auto &e : table)
    if (n == e.n) { out = e.c; return true; }
  return false;
}

// ================================================================ rate functions
namespace rate {
float linear(float t) { return t; }
float smooth(float t) {
  // manim's smooth: sigmoid-ish, inflection 10
  float s = 1.f / (1.f + std::exp(-10.f * (t - 0.5f)));
  float s0 = 1.f / (1.f + std::exp(5.f)), s1 = 1.f / (1.f + std::exp(-5.f));
  return std::clamp((s - s0) / (s1 - s0), 0.f, 1.f);
}
float rush_into(float t) { return 2 * smooth(t / 2.f); }
float rush_from(float t) { return 2 * smooth(t / 2.f + 0.5f) - 1; }
float slow_into(float t) { return std::sqrt(1 - (1 - t) * (1 - t)); }
float double_smooth(float t) { return t < 0.5f ? 0.5f * smooth(2 * t) : 0.5f * (1 + smooth(2 * t - 1)); }
float there_and_back(float t) { return smooth(t < 0.5f ? 2 * t : 2 * (1 - t)); }
float there_and_back_with_pause(float t) {
  const float a = 1.f / 3;
  if (t < a) return smooth(t / a);
  if (t < 1 - a) return 1;
  return smooth((1 - t) / a);
}
float ease_in_out_cubic(float t) { return t < 0.5f ? 4 * t * t * t : 1 - std::pow(-2 * t + 2, 3.f) / 2; }
float ease_out_back(float t) {
  const float c1 = 1.70158f, c3 = c1 + 1;
  return 1 + c3 * std::pow(t - 1, 3.f) + c1 * std::pow(t - 1, 2.f);
}
float wiggle(float t) { return there_and_back(t) * std::sin(t * 6.2831853f * 2); }
} // namespace rate

RateFunc rateByName(const std::string &n) {
  if (n == "linear") return rate::linear;
  if (n == "rush_into" || n == "in") return rate::rush_into;
  if (n == "rush_from" || n == "out") return rate::rush_from;
  if (n == "slow_into") return rate::slow_into;
  if (n == "double_smooth") return rate::double_smooth;
  if (n == "there_and_back") return rate::there_and_back;
  if (n == "cubic") return rate::ease_in_out_cubic;
  if (n == "back") return rate::ease_out_back;
  if (n == "wiggle") return rate::wiggle;
  return rate::smooth;
}

// ================================================================ text
static Font gFont, gMono;
static bool gFontReady = false, gMonoReady = false;
static void initMono(const std::vector<int> &cps);
static float spacingFor(float size);

static std::vector<int> fontCodepoints() {
  static const char *extra = "→←↔↑↓⇒⇔≈×·−≤≥≠≡∈∉∀∃∑∏∫∂∇∞√∝•…“”‘’—–°±µ"
                             "αβγδεζηθικλμνξπρστυφχψωΓΔΘΛΞΠΣΦΨΩ"
                             "⁰¹²³⁴⁵⁶⁷⁸⁹ⁿ₀₁₂₃₄₅₆₇₈₉ₙ★☆✓✗▲▼●○■□";
  std::vector<int> cps;
  for (int c = 32; c < 127; c++) cps.push_back(c);
  for (int c = 160; c < 256; c++) cps.push_back(c);
  int n = 0;
  int *ex = LoadCodepoints(extra, &n);
  for (int i = 0; i < n; i++) cps.push_back(ex[i]);
  UnloadCodepoints(ex);
  return cps;
}
static const char *kMonoFonts[] = {"/System/Library/Fonts/Supplemental/Andale Mono.ttf",
                                   "/System/Library/Fonts/Supplemental/Courier New.ttf",
                                   "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"};
static std::vector<const char *> fontCandidates(const char *preferred) {
  return {preferred, "Plex.ttf", "font.ttf", "/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
          "/System/Library/Fonts/Supplemental/Arial.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"};
}

// ---- metrics without a window: the same font file's glyph advances, loaded on the CPU, so headless
// runs (--check, layout reports, evals) measure text exactly as the stage will draw it
struct Metrics {
  bool ready = false;
  std::unordered_map<int, float> adv; // advance at 64 px
  float fallback = 35;
};
static Metrics gMetrics, gMonoMetrics;

static bool loadMetrics(const char *path, const std::vector<int> &cps, Metrics &m) {
  if (!path || !FileExists(path)) return false;
  int size = 0, count = 0;
  unsigned char *data = LoadFileData(path, &size);
  if (!data) return false;
  GlyphInfo *g = LoadFontData(data, size, 64, cps.data(), (int)cps.size(), FONT_DEFAULT, &count);
  UnloadFileData(data);
  if (!g || count == 0) return false;
  for (int i = 0; i < count; i++)
    m.adv[g[i].value] = g[i].advanceX > 0 ? (float)g[i].advanceX : (float)(g[i].image.width + g[i].offsetX);
  if (m.adv.count('?')) m.fallback = m.adv['?'];
  UnloadFontData(g, count);
  m.ready = true;
  return true;
}

void initFontMetrics(const char *preferred) {
  auto cps = fontCodepoints();
  for (const char *c : fontCandidates(preferred)) if (loadMetrics(c, cps, gMetrics)) break;
  for (const char *c : kMonoFonts) if (loadMetrics(c, cps, gMonoMetrics)) break;
}

// MeasureTextEx, from metrics (same rules: widest line, spacing per character of the longest line)
static Vector2 measureWith(const Metrics &m, const std::string &s, float size, float spacing) {
  float w = 0, widest = 0, h = size;
  int chars = 0, mostChars = 0;
  for (size_t i = 0; i < s.size();) {
    int n = 0;
    int cp = GetCodepointNext(s.c_str() + i, &n);
    i += std::max(1, n);
    chars++;
    if (cp == '\n') { widest = std::max(widest, w); w = 0; chars = 0; h += size; continue; }
    auto it = m.adv.find(cp);
    w += it != m.adv.end() ? it->second : m.fallback;
    mostChars = std::max(mostChars, chars);
  }
  widest = std::max(widest, w);
  return {widest * size / 64.f + (float)std::max(0, mostChars - 1) * spacing, h};
}

Vector2 measureMetrics(const std::string &s, float size) {
  return gMetrics.ready ? measureWith(gMetrics, s, size, spacingFor(size)) : Vector2{0, 0};
}

void initFonts(const char *preferred) {
  std::vector<int> cps = fontCodepoints();
  initMono(cps);
  for (const char *c : fontCandidates(preferred)) {
    if (!c || !FileExists(c)) continue;
    gFont = LoadFontEx(c, 64, cps.data(), (int)cps.size());
    if (gFont.texture.id != 0 && gFont.glyphCount > 0) {
      GenTextureMipmaps(&gFont.texture);
      SetTextureFilter(gFont.texture, TEXTURE_FILTER_TRILINEAR);
      gFontReady = true;
      return;
    }
  }
  gFont = GetFontDefault();
  gFontReady = true;
}

// a monospace face for code; falls back to the main font
static void initMono(const std::vector<int> &cps) {
  for (const char *c : kMonoFonts) {
    if (!FileExists(c)) continue;
    gMono = LoadFontEx(c, 64, const_cast<int *>(cps.data()), (int)cps.size());
    if (gMono.texture.id != 0 && gMono.glyphCount > 0) {
      GenTextureMipmaps(&gMono.texture);
      SetTextureFilter(gMono.texture, TEXTURE_FILTER_TRILINEAR);
      gMonoReady = true;
      return;
    }
  }
}
bool fontsReady() { return gFontReady; }

static float spacingFor(float size) { return size / 32.f; }

Vector2 measure(const std::string &s, float size) {
  if (gFontReady) return MeasureTextEx(gFont, s.c_str(), size, spacingFor(size));
  if (gMetrics.ready) return measureWith(gMetrics, s, size, spacingFor(size));
  return {GetCodepointCount(s.c_str()) * size * 0.55f, size};
}

void text(const std::string &s, Vector2 pos, float size, Color c, float ax, float ay) {
  if (!gFontReady || s.empty() || c.a == 0) return;
  Vector2 m = measure(s, size);
  Vector2 p{std::round(pos.x - m.x * ax), std::round(pos.y - m.y * ay)};
  DrawTextEx(gFont, s.c_str(), p, size, spacingFor(size), c);
}

Vector2 measureMono(const std::string &s, float size) {
  if (gMonoReady) return MeasureTextEx(gMono, s.c_str(), size, 0);
  if (!gFontReady && gMonoMetrics.ready) return measureWith(gMonoMetrics, s, size, 0);
  return measure(s, size);
}

void textMono(const std::string &s, Vector2 pos, float size, Color c) {
  if (!gMonoReady) { text(s, pos, size, c, 0, 0); return; }
  if (s.empty() || c.a == 0) return;
  DrawTextEx(gMono, s.c_str(), {std::round(pos.x), std::round(pos.y)}, size, 0, c);
}

std::vector<std::string> wrap(const std::string &s, float size, float maxW) {
  std::vector<std::string> lines;
  std::string line, word;
  auto flushWord = [&] {
    if (word.empty()) return;
    std::string tryLine = line.empty() ? word : line + " " + word;
    if (!line.empty() && measure(tryLine, size).x > maxW) { lines.push_back(line); line = word; }
    else line = tryLine;
    word.clear();
  };
  for (char ch : s) {
    if (ch == ' ') flushWord();
    else if (ch == '\n') { flushWord(); lines.push_back(line); line.clear(); }
    else word += ch;
  }
  flushWord();
  if (!line.empty() || lines.empty()) lines.push_back(line);
  return lines;
}

void textBg(const std::string &s, Vector2 pos, float size, Color c, Color bg, float ax, float ay, float pad) {
  Vector2 m = measure(s, size);
  Rectangle r{pos.x - m.x * ax - pad, pos.y - m.y * ay - pad / 2, m.x + 2 * pad, m.y + pad};
  DrawRectangleRounded(r, 0.3f, 6, bg);
  text(s, pos, size, c, ax, ay);
}

std::string fmtNum(double v) {
  char buf[64];
  if (std::fabs(v - std::round(v)) < 1e-6) {
    std::snprintf(buf, sizeof buf, "%.0f", std::round(v) == 0 ? 0.0 : std::round(v));
  } else if (std::fabs(v) < 0.1) {
    std::snprintf(buf, sizeof buf, "%.3g", v);
  } else {
    std::snprintf(buf, sizeof buf, "%.2f", v);
    std::string s = buf;
    while (!s.empty() && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
    return s;
  }
  return buf;
}

// ================================================================ primitives
void initGraphics() { rlDisableBackfaceCulling(); }

void tri(Vector2 a, Vector2 b, Vector2 c, Color col) { DrawTriangle(a, b, c, col); }

void stroke(const std::vector<Vector2> &pts, float thick, Color c, bool closed) {
  if (pts.size() < 2 || c.a == 0 || thick <= 0) return;
  if (closed) {
    std::vector<Vector2> p = pts;
    p.push_back(pts[0]);
    if (p.size() > 2) p.push_back(pts[1]); // overlap one segment so the join is mitred
    DrawSplineLinear(p.data(), (int)p.size(), thick, c);
  } else {
    DrawSplineLinear(pts.data(), (int)pts.size(), thick, c);
  }
}

void strokePartial(const std::vector<Vector2> &pts, float frac, float thick, Color c, bool closed) {
  if (frac >= 0.999f) { stroke(pts, thick, c, closed); return; }
  if (frac <= 0 || pts.size() < 2) return;
  std::vector<Vector2> p = pts;
  if (closed) p.push_back(pts[0]);
  float total = 0;
  for (size_t i = 1; i < p.size(); i++) total += len(p[i] - p[i - 1]);
  float want = total * frac, acc = 0;
  std::vector<Vector2> out{p[0]};
  for (size_t i = 1; i < p.size(); i++) {
    float l = len(p[i] - p[i - 1]);
    if (acc + l >= want) {
      out.push_back(lerp(p[i - 1], p[i], l > 0 ? (want - acc) / l : 0));
      break;
    }
    out.push_back(p[i]);
    acc += l;
  }
  stroke(out, thick, c, false);
}

static float signedArea(const std::vector<Vector2> &p) {
  float a = 0;
  for (size_t i = 0, j = p.size() - 1; i < p.size(); j = i++) a += (p[j].x * p[i].y - p[i].x * p[j].y);
  return a / 2;
}
static bool pointInTri(Vector2 p, Vector2 a, Vector2 b, Vector2 c) {
  auto s = [](Vector2 p1, Vector2 p2, Vector2 p3) {
    return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
  };
  float d1 = s(p, a, b), d2 = s(p, b, c), d3 = s(p, c, a);
  bool neg = (d1 < 0) || (d2 < 0) || (d3 < 0), pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
  return !(neg && pos);
}

void fillPolygon(const std::vector<Vector2> &in, Color c) {
  if (in.size() < 3 || c.a == 0) return;
  std::vector<Vector2> p = in;
  if (signedArea(p) < 0) std::reverse(p.begin(), p.end());
  std::vector<int> idx(p.size());
  for (size_t i = 0; i < p.size(); i++) idx[i] = (int)i;
  int guard = 0;
  while (idx.size() > 3 && guard++ < 10000) {
    bool clipped = false;
    for (size_t i = 0; i < idx.size(); i++) {
      int ia = idx[(i + idx.size() - 1) % idx.size()], ib = idx[i], ic = idx[(i + 1) % idx.size()];
      Vector2 a = p[ia], b = p[ib], cc = p[ic];
      float cross = (b.x - a.x) * (cc.y - a.y) - (b.y - a.y) * (cc.x - a.x);
      if (cross <= 0) continue; // reflex
      bool inside = false;
      for (int j : idx)
        if (j != ia && j != ib && j != ic && pointInTri(p[j], a, b, cc)) { inside = true; break; }
      if (inside) continue;
      tri(a, b, cc, c);
      idx.erase(idx.begin() + i);
      clipped = true;
      break;
    }
    if (!clipped) { // self-intersecting (e.g. mid-morph): fall back to a fan from the centroid
      Vector2 ctr{0, 0};
      for (int j : idx) ctr += p[j];
      ctr = ctr * (1.f / idx.size());
      for (size_t i = 0; i < idx.size(); i++) tri(ctr, p[idx[i]], p[idx[(i + 1) % idx.size()]], c);
      return;
    }
  }
  if (idx.size() == 3) tri(p[idx[0]], p[idx[1]], p[idx[2]], c);
}

void arrow(Vector2 a, Vector2 b, float thick, Color c, float head) {
  Vector2 d = norm(b - a);
  if (len(b - a) < 1) return;
  head = std::min(head, len(b - a) * 0.6f);
  Vector2 base = b - d * head;
  Vector2 n{-d.y, d.x};
  DrawLineEx(a, base + d * 1.f, thick, c);
  tri(b, base + n * (head * 0.5f), base - n * (head * 0.5f), c);
}

Vector2 arcPoint(Vector2 a, Vector2 b, float bulge, float t) {
  Vector2 mid = lerp(a, b, 0.5f);
  Vector2 d = b - a;
  Vector2 n = norm(Vector2{d.y, -d.x});
  Vector2 ctrl = mid + n * bulge;
  float u = 1 - t;
  return a * (u * u) + ctrl * (2 * u * t) + b * (t * t);
}

void arcArrow(Vector2 a, Vector2 b, float bulge, float thick, Color c, float frac, bool head) {
  const int N = 48;
  std::vector<Vector2> pts;
  int last = std::max(1, (int)(N * std::clamp(frac, 0.f, 1.f)));
  for (int i = 0; i <= last; i++) pts.push_back(arcPoint(a, b, bulge, (float)i / N));
  if (pts.size() < 2) return;
  if (head && frac > 0.98f) {
    Vector2 tip = pts.back();
    Vector2 d = norm(tip - pts[pts.size() - 4]);
    float hl = 14;
    while (pts.size() > 2 && len(pts.back() - tip) < hl * 0.8f) pts.pop_back();
    stroke(pts, thick, c);
    Vector2 base = tip - d * hl, n{-d.y, d.x};
    tri(tip, base + n * 7.f, base - n * 7.f, c);
  } else {
    stroke(pts, thick, c);
  }
}

void dashed(Vector2 a, Vector2 b, float thick, Color c, float dash, float gap) {
  float L = len(b - a);
  Vector2 d = norm(b - a);
  for (float s = 0; s < L; s += dash + gap) DrawLineEx(a + d * s, a + d * std::min(L, s + dash), thick, c);
}

void roundedBox(Rectangle r, float radius, float thick, Color s, Color f) {
  float rn = std::min(1.f, 2 * radius / std::max(1.f, std::min(r.width, r.height)));
  if (f.a) DrawRectangleRounded(r, rn, 10, f);
  if (s.a && thick > 0) DrawRectangleRoundedLinesEx(r, rn, 10, thick, s);
}

Vector2 rectEdgePoint(Rectangle r, Vector2 toward) {
  Vector2 c{r.x + r.width / 2, r.y + r.height / 2};
  Vector2 d = toward - c;
  if (std::fabs(d.x) < 1e-4f && std::fabs(d.y) < 1e-4f) return c;
  float tx = d.x != 0 ? (r.width / 2) / std::fabs(d.x) : 1e9f;
  float ty = d.y != 0 ? (r.height / 2) / std::fabs(d.y) : 1e9f;
  float t = std::min(tx, ty);
  return c + d * t;
}

float niceStep(float range, int target) {
  if (range <= 0) return 1;
  float raw = range / std::max(1, target);
  float mag = std::pow(10.f, std::floor(std::log10(raw)));
  float r = raw / mag;
  float nice = r < 1.5f ? 1 : r < 3 ? 2 : r < 7 ? 5 : 10;
  return nice * mag;
}

void niceRange(float &lo, float &hi, bool includeZero) {
  if (includeZero) { lo = std::min(lo, 0.f); hi = std::max(hi, 0.f); }
  if (hi - lo < 1e-6f) { lo -= 1; hi += 1; }
  float pad = (hi - lo) * 0.08f;
  if (!(includeZero && lo == 0)) lo -= pad;
  if (!(includeZero && hi == 0)) hi += pad;
  float st = niceStep(hi - lo, 8);
  lo = std::floor(lo / st) * st;
  hi = std::ceil(hi / st) * st;
}

// ================================================================ coordinate systems
Vector2 NumberLine::toScreen(float v) const { return lerp(a, b, (v - min) / std::max(1e-6f, max - min)); }
float NumberLine::fromScreen(Vector2 p) const {
  float t = (p.x - a.x) / std::max(1e-6f, b.x - a.x);
  return min + t * (max - min);
}
void NumberLine::draw(Color c, float alpha, bool labels) const {
  Color cc = fade(c, alpha);
  DrawLineEx(a - Vector2{12, 0}, b + Vector2{12, 0}, 2.5f, cc);
  float st = step > 0 ? step : niceStep(max - min, 10);
  float first = std::ceil(min / st - 1e-4f) * st;
  for (float v = first; v <= max + st * 1e-3f; v += st) {
    Vector2 p = toScreen(v);
    DrawLineEx(p - Vector2{0, 7}, p + Vector2{0, 7}, 2, cc);
    if (labels) text(fmtNum(v), p + Vector2{0, 14}, 18, fade(col::muted, alpha), 0.5f, 0);
  }
}

Vector2 Axes::toScreen(float x, float y) const {
  float tx = (x - xmin) / std::max(1e-6f, xmax - xmin);
  float ty = (y - ymin) / std::max(1e-6f, ymax - ymin);
  return {area.x + tx * area.width, area.y + area.height - ty * area.height};
}
Vector2 Axes::toWorld(Vector2 p) const {
  return {xmin + (p.x - area.x) / area.width * (xmax - xmin),
          ymin + (area.y + area.height - p.y) / area.height * (ymax - ymin)};
}
void Axes::draw(Color c, float alpha, bool labels, bool grid) const {
  Color cc = fade(c, alpha), gc = fade(col::grid, alpha), lc = fade(col::muted, alpha);
  float sx = niceStep(xmax - xmin, 10), sy = niceStep(ymax - ymin, 7);
  float x0 = std::clamp(0.f, xmin, xmax), y0 = std::clamp(0.f, ymin, ymax);
  for (float x = std::ceil(xmin / sx - 1e-4f) * sx; x <= xmax + 1e-4f; x += sx) {
    Vector2 p = toScreen(x, y0);
    if (grid) DrawLineEx(toScreen(x, ymin), toScreen(x, ymax), 1, gc);
    DrawLineEx(p - Vector2{0, 5}, p + Vector2{0, 5}, 2, cc);
    if (labels && std::fabs(x) > 1e-6f) text(fmtNum(x), p + Vector2{0, 10}, 16, lc, 0.5f, 0);
  }
  for (float y = std::ceil(ymin / sy - 1e-4f) * sy; y <= ymax + 1e-4f; y += sy) {
    Vector2 p = toScreen(x0, y);
    if (grid) DrawLineEx(toScreen(xmin, y), toScreen(xmax, y), 1, gc);
    DrawLineEx(p - Vector2{5, 0}, p + Vector2{5, 0}, 2, cc);
    if (labels && std::fabs(y) > 1e-6f) text(fmtNum(y), p - Vector2{10, 0}, 16, lc, 1, 0.5f);
  }
  arrow(toScreen(xmin, y0), toScreen(xmax, y0) + Vector2{14, 0}, 2.5f, cc, 12);
  arrow(toScreen(x0, ymin), toScreen(x0, ymax) - Vector2{0, 14}, 2.5f, cc, 12);
}
std::vector<Vector2> Axes::plot(const std::function<double(double)> &f, float x0, float x1, int n) const {
  std::vector<Vector2> pts;
  for (int i = 0; i <= n; i++) {
    double x = x0 + (x1 - x0) * i / n;
    double y = f(x);
    if (!std::isfinite(y)) continue;
    y = std::clamp(y, (double)ymin - (ymax - ymin), (double)ymax + (ymax - ymin));
    pts.push_back(toScreen((float)x, (float)y));
  }
  return pts;
}
void Axes::fillUnder(const std::function<double(double)> &f, float x0, float x1, Color c, int n) const {
  if (x1 < x0) std::swap(x0, x1);
  float yb = std::clamp(0.f, ymin, ymax);
  for (int i = 0; i < n; i++) {
    float xa = x0 + (x1 - x0) * i / n, xb = x0 + (x1 - x0) * (i + 1) / n;
    float ya = std::clamp((float)f(xa), ymin, ymax), yb2 = std::clamp((float)f(xb), ymin, ymax);
    if (!std::isfinite(ya) || !std::isfinite(yb2)) continue;
    Vector2 p1 = toScreen(xa, yb), p2 = toScreen(xb, yb), p3 = toScreen(xb, yb2), p4 = toScreen(xa, ya);
    tri(p1, p2, p3, c);
    tri(p1, p3, p4, c);
  }
}

// ================================================================ mobjects
void Mobject::draw() {
  if (opacity <= 0) return;
  for (auto &p : paths) {
    if (closed && fillOpacity > 0 && p.size() >= 3) fillPolygon(p, fade(fillColor, fillOpacity * opacity));
    strokePartial(p, drawFrac, strokeWidth, fade(strokeColor, opacity), closed);
  }
  for (auto &s : submobjects) s->draw();
}
MobPtr Mobject::copy() const {
  auto m = std::make_shared<Mobject>(*this);
  m->submobjects.clear();
  for (auto &s : submobjects) m->submobjects.push_back(s->copy());
  m->updaters.clear();
  return m;
}
void Mobject::update(float dt) {
  for (auto &u : updaters) u(*this, dt);
  for (auto &s : submobjects) s->update(dt);
}
void Mobject::family(const std::function<void(Mobject &)> &fn) {
  fn(*this);
  for (auto &s : submobjects) s->family(fn);
}
void Mobject::allPaths(std::vector<std::vector<Vector2>> &out) const {
  for (auto &p : paths) out.push_back(p);
  for (auto &s : submobjects) s->allPaths(out);
}
Rectangle Mobject::bounds() const {
  float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
  std::vector<std::vector<Vector2>> all;
  allPaths(all);
  for (auto &p : all)
    for (auto &q : p) { x0 = std::min(x0, q.x); y0 = std::min(y0, q.y); x1 = std::max(x1, q.x); y1 = std::max(y1, q.y); }
  if (auto t = dynamic_cast<const Text *>(this)) {
    Vector2 m = measure(t->str, t->size);
    return {t->pos.x - m.x / 2, t->pos.y - m.y / 2, m.x, m.y};
  }
  if (x0 > x1) return {0, 0, 0, 0};
  return {x0, y0, x1 - x0, y1 - y0};
}
Vector2 Mobject::center() const {
  Rectangle b = bounds();
  return {b.x + b.width / 2, b.y + b.height / 2};
}
Mobject &Mobject::shift(Vector2 d) {
  family([&](Mobject &m) {
    for (auto &p : m.paths) for (auto &q : p) q += d;
    if (auto t = dynamic_cast<Text *>(&m)) t->pos += d;
  });
  return *this;
}
Mobject &Mobject::moveTo(Vector2 p) { return shift(p - center()); }
Mobject &Mobject::scale(float s, Vector2 about) {
  family([&](Mobject &m) {
    for (auto &p : m.paths) for (auto &q : p) q = about + (q - about) * s;
    if (auto t = dynamic_cast<Text *>(&m)) { t->pos = about + (t->pos - about) * s; t->size *= s; }
  });
  return *this;
}
Mobject &Mobject::rotate(float a, Vector2 about) {
  float c = std::cos(a), s = std::sin(a);
  family([&](Mobject &m) {
    for (auto &p : m.paths)
      for (auto &q : p) {
        Vector2 d = q - about;
        q = about + Vector2{d.x * c - d.y * s, d.x * s + d.y * c};
      }
  });
  return *this;
}
Mobject &Mobject::setColor(Color c) {
  family([&](Mobject &m) { m.strokeColor = c; if (m.fillOpacity > 0) m.fillColor = c; });
  return *this;
}
Mobject &Mobject::setFill(Color c, float op) { fillColor = c; fillOpacity = op; return *this; }
Mobject &Mobject::setStroke(Color c, float w) { strokeColor = c; strokeWidth = w; return *this; }
Mobject &Mobject::nextTo(const Mobject &o, Vector2 dir, float buff) {
  Rectangle a = o.bounds(), b = bounds();
  Vector2 oc{a.x + a.width / 2, a.y + a.height / 2};
  Vector2 target = oc + Vector2{dir.x * (a.width / 2 + b.width / 2 + buff), dir.y * (a.height / 2 + b.height / 2 + buff)};
  if (dir.x == 0) target.x = oc.x;
  if (dir.y == 0) target.y = oc.y;
  return moveTo(target);
}

static std::vector<Vector2> ellipsePts(Vector2 c, float rx, float ry, int n) {
  std::vector<Vector2> p;
  for (int i = 0; i < n; i++) {
    float a = -1.5707963f + 6.2831853f * i / n;
    p.push_back({c.x + rx * std::cos(a), c.y + ry * std::sin(a)});
  }
  return p;
}
MobPtr Circle(Vector2 c, float r, Color cc) {
  auto m = std::make_shared<Mobject>();
  m->paths.push_back(ellipsePts(c, r, r, 96));
  m->strokeColor = cc;
  return m;
}
MobPtr Dot(Vector2 c, float r, Color cc) {
  auto m = Circle(c, r, cc);
  m->setFill(cc, 1);
  m->strokeWidth = 0;
  return m;
}
MobPtr Rect(Rectangle r, Color cc) {
  auto m = std::make_shared<Mobject>();
  m->paths.push_back({{r.x, r.y}, {r.x + r.width, r.y}, {r.x + r.width, r.y + r.height}, {r.x, r.y + r.height}});
  m->strokeColor = cc;
  return m;
}
MobPtr Square(Vector2 c, float s, Color cc) { return Rect({c.x - s / 2, c.y - s / 2, s, s}, cc); }
MobPtr RegularPolygon(Vector2 c, float r, int n, Color cc) {
  auto m = std::make_shared<Mobject>();
  m->paths.push_back(ellipsePts(c, r, r, n));
  m->strokeColor = cc;
  return m;
}
MobPtr Line(Vector2 a, Vector2 b, Color cc) {
  auto m = std::make_shared<Mobject>();
  m->paths.push_back({a, b});
  m->closed = false;
  m->strokeColor = cc;
  return m;
}
MobPtr ArrowMob(Vector2 a, Vector2 b, Color cc) {
  auto shaft = Line(a, b - norm(b - a) * 12.f, cc);
  Vector2 d = norm(b - a), n{-d.y, d.x}, base = b - d * 16.f;
  auto head = std::make_shared<Mobject>();
  head->paths.push_back({b, base + n * 8.f, base - n * 8.f});
  head->strokeColor = cc;
  head->strokeWidth = 1;
  head->setFill(cc, 1);
  shaft->add(head);
  return shaft;
}
MobPtr FunctionGraph(const Axes &ax, std::function<double(double)> f, float x0, float x1, Color cc) {
  auto m = std::make_shared<Mobject>();
  m->paths.push_back(ax.plot(f, x0, x1));
  m->closed = false;
  m->strokeColor = cc;
  return m;
}
MobPtr VGroup(std::vector<MobPtr> items) {
  auto g = std::make_shared<Mobject>();
  g->submobjects = std::move(items);
  return g;
}
void arrange(Mobject &g, Vector2 dir, float buff) {
  if (g.submobjects.empty()) return;
  Vector2 c = g.center();
  Vector2 cursor = g.submobjects[0]->center();
  for (size_t i = 1; i < g.submobjects.size(); i++) g.submobjects[i]->nextTo(*g.submobjects[i - 1], dir, buff);
  (void)cursor;
  g.moveTo(c);
}

Text::Text(std::string s, Vector2 p, float sz, Color c) : str(std::move(s)), size(sz), pos(p) {
  strokeColor = c;
  closed = false;
}
void Text::draw() {
  if (opacity <= 0) return;
  std::string shown = str;
  if (revealed < 1) {
    int n = GetCodepointCount(str.c_str());
    int keep = (int)std::round(n * std::clamp(revealed, 0.f, 1.f));
    const char *s = str.c_str();
    int bytes = 0;
    for (int i = 0; i < keep; i++) { int sz; GetCodepointNext(s + bytes, &sz); bytes += sz; }
    shown = str.substr(0, bytes);
    Vector2 full = measure(str, size);
    text(shown, {pos.x - full.x / 2, pos.y}, size, fade(strokeColor, opacity), 0, 0.5f);
  } else {
    text(shown, pos, size, fade(strokeColor, opacity), 0.5f, 0.5f);
  }
  for (auto &s2 : submobjects) s2->draw();
}
MobPtr Text::copy() const { return std::make_shared<Text>(*this); }
std::shared_ptr<Text> TextMob(const std::string &s, Vector2 p, float size, Color c) {
  return std::make_shared<Text>(s, p, size, c);
}

// ================================================================ animations
namespace {

struct FnAnim : Animation {
  std::function<void()> onBegin;
  std::function<void(float)> fn;
  std::vector<MobPtr> intro, rem;
  void begin() override { if (onBegin) onBegin(); }
  void interpolate(float a) override { fn(a); }
  std::vector<MobPtr> introduces() override { return intro; }
  std::vector<MobPtr> removes() override { return rem; }
};
std::shared_ptr<FnAnim> mk(float rt) {
  auto a = std::make_shared<FnAnim>();
  a->runTime = rt;
  return a;
}

std::vector<Vector2> resample(const std::vector<Vector2> &p, int n, bool closed) {
  std::vector<Vector2> src = p;
  if (src.empty()) src.push_back({W / 2.f, H / 2.f});
  if (closed && src.size() > 1) src.push_back(src[0]);
  std::vector<float> cum{0};
  for (size_t i = 1; i < src.size(); i++) cum.push_back(cum.back() + len(src[i] - src[i - 1]));
  float total = cum.back();
  std::vector<Vector2> out;
  for (int i = 0; i < n; i++) {
    float want = total * i / std::max(1, closed ? n : n - 1);
    size_t j = 1;
    while (j < cum.size() && cum[j] < want) j++;
    if (j >= src.size()) { out.push_back(src.back()); continue; }
    float seg = cum[j] - cum[j - 1];
    out.push_back(lerp(src[j - 1], src[j], seg > 0 ? (want - cum[j - 1]) / seg : 0));
  }
  return out;
}

struct TransformAnim : Animation {
  MobPtr from, to;
  bool replace = false;
  std::vector<std::vector<Vector2>> a, b;
  Color sa, sb, fa, fb;
  float swa, swb, foa, fob, opa, opb;
  bool textMode = false;
  void begin() override {
    textMode = dynamic_cast<Text *>(from.get()) || dynamic_cast<Text *>(to.get());
    if (textMode) return;
    std::vector<std::vector<Vector2>> pa, pb;
    from->allPaths(pa);
    to->allPaths(pb);
    size_t n = std::max(pa.size(), pb.size());
    auto pad = [&](std::vector<std::vector<Vector2>> &v, const Mobject &m) {
      Vector2 c = m.center();
      while (v.size() < n) v.push_back(v.empty() ? std::vector<Vector2>{c} : std::vector<Vector2>{v.back().empty() ? c : v.back()[0]});
    };
    pad(pa, *from);
    pad(pb, *to);
    a.clear(); b.clear();
    for (size_t i = 0; i < n; i++) {
      int pts = (int)std::max<size_t>({pa[i].size(), pb[i].size(), 2}) * 2;
      pts = std::min(pts, 400);
      a.push_back(resample(pa[i], pts, from->closed));
      b.push_back(resample(pb[i], pts, to->closed));
    }
    sa = from->strokeColor; sb = to->strokeColor;
    fa = from->fillColor; fb = to->fillColor;
    swa = from->strokeWidth; swb = to->strokeWidth;
    foa = from->fillOpacity; fob = to->fillOpacity;
    opa = from->opacity; opb = to->opacity;
    from->submobjects.clear();
  }
  void interpolate(float t) override {
    if (textMode) {
      auto ft = dynamic_cast<Text *>(from.get());
      auto tt = dynamic_cast<Text *>(to.get());
      if (ft && tt) {
        // cross-fade: from fades as to's text takes over at the same place
        if (t < 0.5f) { ft->opacity = 1 - t * 2; }
        else { ft->str = tt->str; ft->opacity = (t - 0.5f) * 2; }
        ft->pos = lerp(ft->pos, tt->pos, t);
        ft->size = lerpf(ft->size, tt->size, t);
        ft->strokeColor = lerpColor(ft->strokeColor, tt->strokeColor, t);
      } else {
        from->opacity = 1 - t;
      }
      return;
    }
    from->paths.resize(a.size());
    for (size_t i = 0; i < a.size(); i++) {
      from->paths[i].resize(a[i].size());
      for (size_t j = 0; j < a[i].size(); j++) from->paths[i][j] = lerp(a[i][j], b[i][j], t);
    }
    from->strokeColor = lerpColor(sa, sb, t);
    from->fillColor = lerpColor(fa, fb, t);
    from->strokeWidth = lerpf(swa, swb, t);
    from->fillOpacity = lerpf(foa, fob, t);
    from->opacity = lerpf(opa, opb, t);
    from->closed = t < 0.5f ? from->closed : to->closed;
  }
  void finish() override {
    interpolate(1);
    if (!textMode) {
      std::vector<std::vector<Vector2>> pb;
      to->allPaths(pb);
      from->paths = pb;
      from->closed = to->closed;
    }
  }
  std::vector<MobPtr> introduces() override { return replace ? std::vector<MobPtr>{from} : std::vector<MobPtr>{from}; }
  std::vector<MobPtr> removes() override { return replace ? std::vector<MobPtr>{from} : std::vector<MobPtr>{}; }
};

struct GroupAnim : Animation {
  std::vector<AnimPtr> kids;
  std::vector<float> starts;
  std::vector<bool> begun, done;
  void begin() override { begun.assign(kids.size(), false); done.assign(kids.size(), false); }
  void interpolate(float alpha) override {
    float T = alpha * runTime;
    for (size_t i = 0; i < kids.size(); i++) {
      if (T < starts[i] && alpha < 1) continue;
      if (!begun[i]) { kids[i]->begin(); begun[i] = true; }
      if (done[i]) continue;
      float local = kids[i]->runTime > 0 ? (T - starts[i]) / kids[i]->runTime : 1;
      if (local >= 1 || alpha >= 1) { kids[i]->finish(); done[i] = true; }
      else kids[i]->interpolate(kids[i]->rateFunc(std::clamp(local, 0.f, 1.f)));
    }
  }
  void finish() override { interpolate(1); }
  std::vector<MobPtr> introduces() override {
    std::vector<MobPtr> v;
    for (auto &k : kids) for (auto &m : k->introduces()) v.push_back(m);
    return v;
  }
  std::vector<MobPtr> removes() override {
    std::vector<MobPtr> v;
    for (auto &k : kids) for (auto &m : k->removes()) v.push_back(m);
    return v;
  }
};

} // namespace

AnimPtr Create(MobPtr m, float rt) {
  auto a = mk(rt);
  auto fills = std::make_shared<std::vector<float>>();
  a->onBegin = [m, fills] {
    fills->clear();
    m->family([&](Mobject &x) { fills->push_back(x.fillOpacity); x.drawFrac = 0; x.opacity = 1; });
  };
  a->fn = [m, fills](float t) {
    size_t i = 0;
    m->family([&](Mobject &x) {
      x.drawFrac = std::clamp(t * 1.4f, 0.f, 1.f);
      float f = i < fills->size() ? (*fills)[i] : 0;
      x.fillOpacity = f * std::clamp((t - 0.5f) * 2, 0.f, 1.f);
      if (auto tx = dynamic_cast<Text *>(&x)) tx->revealed = t;
      i++;
    });
  };
  a->intro = {m};
  return a;
}
AnimPtr Uncreate(MobPtr m, float rt) {
  auto a = mk(rt);
  a->fn = [m](float t) { m->family([&](Mobject &x) { x.drawFrac = 1 - t; if (auto tx = dynamic_cast<Text *>(&x)) tx->revealed = 1 - t; }); };
  a->rem = {m};
  return a;
}
AnimPtr FadeIn(MobPtr m, float rt, Vector2 sh) {
  auto a = mk(rt);
  auto last = std::make_shared<float>(0);
  a->onBegin = [m, sh, last] { m->shift(sh * -1.f); *last = 0; m->family([](Mobject &x) { x.opacity = 0; }); };
  a->fn = [m, sh, last](float t) {
    m->shift(sh * (t - *last));
    *last = t;
    m->family([&](Mobject &x) { x.opacity = t; });
  };
  a->intro = {m};
  return a;
}
AnimPtr FadeOut(MobPtr m, float rt, Vector2 sh) {
  auto a = mk(rt);
  auto last = std::make_shared<float>(0);
  a->onBegin = [last] { *last = 0; };
  a->fn = [m, sh, last](float t) {
    m->shift(sh * (t - *last));
    *last = t;
    m->family([&](Mobject &x) { x.opacity = 1 - t; });
  };
  a->rem = {m};
  return a;
}
AnimPtr Transform(MobPtr from, MobPtr to, float rt) {
  auto a = std::make_shared<TransformAnim>();
  a->from = from; a->to = to; a->runTime = rt;
  return a;
}
AnimPtr ReplacementTransform(MobPtr from, MobPtr to, float rt) {
  // morph `from`, then swap in `to` so later code can keep using `to`
  auto t = std::make_shared<TransformAnim>();
  t->from = from; t->to = to; t->runTime = rt; t->replace = true;
  auto swap = mk(0);
  swap->fn = [](float) {};
  swap->intro = {to};
  return Succession({t, swap});
}
AnimPtr MoveTo(MobPtr m, Vector2 p, float rt) {
  auto a = mk(rt);
  auto start = std::make_shared<Vector2>();
  a->onBegin = [m, start] { *start = m->center(); };
  a->fn = [m, p, start](float t) { m->moveTo(lerp(*start, p, t)); };
  return a;
}
AnimPtr Shift(MobPtr m, Vector2 d, float rt) {
  auto a = mk(rt);
  auto last = std::make_shared<float>(0);
  a->onBegin = [last] { *last = 0; };
  a->fn = [m, d, last](float t) { m->shift(d * (t - *last)); *last = t; };
  return a;
}
AnimPtr ScaleBy(MobPtr m, float s, float rt) {
  auto a = mk(rt);
  auto cur = std::make_shared<float>(1);
  a->onBegin = [cur] { *cur = 1; };
  a->fn = [m, s, cur](float t) {
    float want = lerpf(1, s, t);
    m->scale(want / *cur);
    *cur = want;
  };
  return a;
}
AnimPtr Rotate(MobPtr m, float r, float rt) {
  auto a = mk(rt);
  auto last = std::make_shared<float>(0);
  a->onBegin = [last] { *last = 0; };
  a->fn = [m, r, last](float t) { m->rotate(r * (t - *last)); *last = t; };
  return a;
}
AnimPtr Indicate(MobPtr m, Color c, float rt) {
  auto a = mk(rt);
  auto orig = std::make_shared<Color>();
  auto cur = std::make_shared<float>(1);
  a->onBegin = [m, orig, cur] { *orig = m->strokeColor; *cur = 1; };
  a->fn = [m, c, orig, cur](float t) {
    float k = rate::there_and_back(t);
    float want = 1 + 0.2f * k;
    m->scale(want / *cur);
    *cur = want;
    m->setColor(lerpColor(*orig, c, k));
  };
  a->rateFunc = rate::linear;
  return a;
}
AnimPtr Write(std::shared_ptr<Text> t, float rt) {
  auto a = mk(rt);
  a->onBegin = [t] { t->revealed = 0; };
  a->fn = [t](float k) { t->revealed = k; };
  a->intro = {t};
  a->rateFunc = rate::linear;
  return a;
}
AnimPtr ChangeValue(TrackerPtr tr, float to, float rt) {
  auto a = mk(rt);
  auto from = std::make_shared<float>();
  a->onBegin = [tr, from] { *from = tr->value; };
  a->fn = [tr, to, from](float t) { tr->value = lerpf(*from, to, t); };
  return a;
}
AnimPtr Wait(float rt) {
  auto a = mk(rt);
  a->fn = [](float) {};
  return a;
}
AnimPtr Group(std::vector<AnimPtr> anims, float lag) {
  auto g = std::make_shared<GroupAnim>();
  g->rateFunc = rate::linear;
  float t = 0, end = 0;
  for (auto &k : anims) {
    g->starts.push_back(t);
    end = std::max(end, t + k->runTime);
    t += k->runTime * lag;
  }
  g->kids = std::move(anims);
  g->runTime = end;
  return g;
}
AnimPtr Succession(std::vector<AnimPtr> anims) { return Group(std::move(anims), 1.f); }

// ================================================================ recorder
bool Recorder::start(const char *path, int fps) {
  path_ = path ? path : "";
  fps_ = fps;
  return !path_.empty();
}
void Recorder::frame() {
  if (path_.empty()) return;
  Image img = LoadImageFromScreen();
  if (!pipe_) {
    w_ = img.width; h_ = img.height;
    char cmd[1024];
    std::snprintf(cmd, sizeof cmd,
                  "ffmpeg -y -loglevel error -f rawvideo -pixel_format rgba -video_size %dx%d -framerate %d -i - "
                  "-vf \"scale=trunc(iw/2)*2:trunc(ih/2)*2\" -c:v libx264 -pix_fmt yuv420p -crf 18 \"%s\"",
                  w_, h_, fps_, path_.c_str());
    pipe_ = popen(cmd, "w");
    if (!pipe_) { path_.clear(); UnloadImage(img); return; }
  }
  if (img.width == w_ && img.height == h_) fwrite(img.data, 4, (size_t)w_ * h_, pipe_);
  UnloadImage(img);
}
void Recorder::stop() {
  if (pipe_) { pclose(pipe_); pipe_ = nullptr; std::printf("recorded %s\n", path_.c_str()); }
  path_.clear();
}

// ================================================================ scene
Scene::Scene(SceneConfig cfg) : cfg_(cfg) {}
Scene::~Scene() = default;

void Scene::render() {
  SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI);
  InitWindow(W, H, cfg_.title);
  SetTargetFPS(cfg_.fps);
  initGraphics();
  initFonts();
  if (cfg_.recordPath) rec_.start(cfg_.recordPath, cfg_.fps);
  try {
    construct();
    wait(0.5f);
  } catch (WindowClosed &) {
  }
  rec_.stop();
  CloseWindow();
}

void Scene::add(MobPtr m) {
  if (std::find(mobjects.begin(), mobjects.end(), m) == mobjects.end()) mobjects.push_back(m);
}
void Scene::remove(MobPtr m) { mobjects.erase(std::remove(mobjects.begin(), mobjects.end(), m), mobjects.end()); }
void Scene::clear() { mobjects.clear(); }

void Scene::frame(float dt) {
  if (WindowShouldClose()) throw WindowClosed{};
  time_ += dt;
  for (auto &m : mobjects) m->update(dt);
  std::stable_sort(mobjects.begin(), mobjects.end(), [](const MobPtr &a, const MobPtr &b) { return a->zIndex < b->zIndex; });
  BeginDrawing();
  ClearBackground(cfg_.background);
  for (auto &m : mobjects) m->draw();
  drawOverlay();
  EndDrawing();
  rec_.frame();
}

void Scene::play(std::vector<AnimPtr> anims) {
  float rt = 0;
  for (auto &a : anims) {
    a->begin();
    for (auto &m : a->introduces()) add(m);
    rt = std::max(rt, a->runTime);
  }
  float t = 0;
  const float fixed = 1.f / cfg_.fps;
  while (t < rt) {
    float dt = rec_.active() || cfg_.recordPath ? fixed : std::min(GetFrameTime(), 0.05f);
    if (dt <= 0) dt = fixed;
    t += dt;
    for (auto &a : anims) {
      float local = a->runTime > 0 ? std::clamp(t / a->runTime, 0.f, 1.f) : 1;
      a->interpolate(a->rateFunc(local));
    }
    frame(dt);
  }
  for (auto &a : anims) {
    a->finish();
    for (auto &m : a->removes()) remove(m);
  }
}

void Scene::wait(float s) {
  float t = 0;
  const float fixed = 1.f / cfg_.fps;
  do {
    float dt = cfg_.recordPath ? fixed : std::min(GetFrameTime(), 0.05f);
    if (dt <= 0) dt = fixed;
    t += dt;
    frame(dt);
  } while (t < s);
}

} // namespace rm
