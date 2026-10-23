// Fields over the plane: what a function of x and y looks like everywhere at once.
//
//   mark E field vx={-y} vy={x}            arrows (a vector at each point), style=lines (streamlines),
//                                          style=flow (streamlines with motion along them); fn={...}
//                                          instead of vx/vy draws the gradient of a function
//   mark S slope fn={x - y}                a slope field for dy/dx = f(x, y)
//   mark s solution of=S from=[0,1]        the curve through a point: an ODE solution, or a particle's path
//   mark L heat fn={x^2 + 4*y^2}           colour for value (viridis; dark-centred diverging if it changes sign)
//   mark C contour fn={...} levels=auto    level curves, labelled
//
// Choices from the research (see docs/plan-camera-fields-trails.md): arrow length L tanh(|v|/L) so zero
// vectors vanish and critical points show; directions and lengths in screen space (unequal axis scales);
// evenly spaced streamlines (Jobard & Lefer) which beat arrow grids in Laidlaw's study; RK4; perceptually
// uniform colour; value ranges and contour levels locked per mark so colours and lines don't flicker when
// a parameter animates. Everything is recomputed from the expressions each frame, so params morph fields.
#include "engine.hpp"
#include "viz/viz.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace mk {

using namespace rm;

// ---------------------------------------------------------------- evaluating f(x, y)
// (a, b: the names of the two variables; u and v for a parametric surface)
std::function<double(double, double)> Engine::fn2(const Mark &m, const char *key, const char *a, const char *b) {
  const Value *v = findProp(m.props, key);
  if (!v) return {};
  if (v->type == Value::NUM) { double k = v->num; return [k](double, double) { return k; }; }
  if (v->type != Value::EXPR || !v->expr) return {};
  const char *ox = xName, *oy = yName;
  xName = a; yName = b;
  auto prog = std::make_shared<Program>(compile(*v->expr, *this));
  xName = ox; yName = oy;
  if (prog->ok) return [prog](double x, double y) { return prog->run(x, y); };
  ExprPtr e = v->expr; // an engine call of the variables: the tree walk, with them set
  return [this, e, a, b](double X, double Y) {
    double ox = this->x, oy = this->y;
    bool ohx = hasX, ohy = hasY;
    const char *on = xName, *om = yName;
    this->x = X; this->y = Y; hasX = hasY = true; xName = a; yName = b;
    double r = eval(*e, *this);
    this->x = ox; this->y = oy; hasX = ohx; hasY = ohy; xName = on; yName = om;
    return r;
  };
}

// the vector field of a field mark: vx/vy, or the gradient of fn (or of the mark named by of=)
std::function<viz::V2(viz::V2)> Engine::vecField(const Mark &m) {
  auto fx = fn2(m, "vx"), fy = fn2(m, "vy");
  if (fx && fy) return [fx, fy](viz::V2 p) { return viz::V2{fx(p.x, p.y), fy(p.x, p.y)}; };
  std::function<double(double, double)> f = fn2(m, "fn");
  if (!f)
    if (const Value *of = findProp(m.props, "of"); of && of->type == Value::STR)
      if (Mark *src = mark(of->str)) f = fn2(*src, "fn");
  if (!f) return {};
  double h = 1e-4 * std::max(1e-9, (double)std::max(axes_.xmax - axes_.xmin, axes_.ymax - axes_.ymin));
  return [f, h](viz::V2 p) {
    return viz::V2{(f(p.x + h, p.y) - f(p.x - h, p.y)) / (2 * h), (f(p.x, p.y + h) - f(p.x, p.y - h)) / (2 * h)};
  };
}

namespace {
std::string sourceOf(const Mark &m) {
  std::string s;
  for (const char *k : {"vx", "vy", "fn", "of"})
    if (const Value *v = findProp(m.props, k)) s += std::string(k) + "=" + v->markup() + ";";
  return s;
}
double percentile(std::vector<double> v, double q) {
  v.erase(std::remove_if(v.begin(), v.end(), [](double x) { return !std::isfinite(x); }), v.end());
  if (v.empty()) return NAN;
  size_t i = std::min(v.size() - 1, (size_t)(q * (v.size() - 1)));
  std::nth_element(v.begin(), v.begin() + (long)i, v.end());
  return v[i];
}
// a lattice of data points about targetPx apart on screen, on round numbers so it doesn't swim
std::vector<double> lattice(double lo, double hi, double px, double targetPx) {
  std::vector<double> out;
  if (!(hi > lo) || !(px > 0)) return out;
  double st = viz::niceStepFloor(targetPx * (hi - lo) / px);
  double half = st / 2;
  for (long k = (long)std::ceil((lo + half * 0.5) / st - 1e-9); k * st <= hi - half * 0.5 + 1e-12; k++) {
    if (out.size() > 200) break;
    out.push_back(k * st);
  }
  return out;
}
Color shade(Color c, float a) { return fade(c, std::clamp(a, 0.f, 1.f)); }
} // namespace

// ---------------------------------------------------------------- drawing
void Engine::drawFieldMark(Mark &m) {
  std::string k = effectiveKind(m);
  float a = alphaOf(m);
  if (a <= 0.003f) return;
  const Rectangle &A = axes_.area;
  Color c = colorOf(m);
  BeginScissorMode((int)A.x, (int)A.y, (int)A.width, (int)A.height);
  if (k == "heat") drawHeat(m, a);
  else if (k == "contour") drawContour(m, a, c);
  else if (k == "slope") drawSlope(m, a, c);
  else if (k == "field") drawVecField(m, a, c);
  EndScissorMode();
  for (const TextLabel &l : markLabels(m)) { // a plate under the legend, so the field doesn't run through it
    Rectangle b = labelBox(l);
    DrawRectangleRounded({b.x - 6, b.y - 3, b.width + 12, b.height + 6}, 0.3f, 6, fade(col::bg, 0.85f * a));
  }
  drawLabels(m);
}

void Engine::drawVecField(Mark &m, float a, Color c) {
  auto F = vecField(m);
  if (!F) { warn("field: " + m.id + " needs vx={...} vy={...}, or fn={...} for a gradient"); return; }
  const Rectangle &A = axes_.area;
  double ppx = A.width / (axes_.xmax - axes_.xmin), ppy = A.height / (axes_.ymax - axes_.ymin);
  std::string style;
  if (const Value *s = findProp(m.props, "style"); s && s->type == Value::STR) style = s->str;
  // the field in screen pixels per data unit of time (y flipped): directions and lengths as seen
  auto Fpx = [&](viz::V2 P) {
    viz::V2 d{axes_.xmin + (P.x - A.x) / ppx, axes_.ymin + (A.y + A.height - P.y) / ppy};
    viz::V2 v = F(d);
    return viz::V2{v.x * ppx, -v.y * ppy};
  };
  // the magnitude scale is fixed per mark (95th percentile when first seen), so colour doesn't flicker
  std::string key = m.id + "|" + sourceOf(m);
  auto lat = [&](double target) {
    std::vector<viz::V2> pts;
    for (double y : lattice(axes_.ymin, axes_.ymax, A.height, target))
      for (double x : lattice(axes_.xmin, axes_.xmax, A.width, target)) pts.push_back({x, y});
    return pts;
  };
  if (!fieldScale_.count(key)) {
    std::vector<double> mags;
    for (auto p : lat(60)) { viz::V2 v = F(p); mags.push_back(std::hypot(v.x * ppx, v.y * ppy)); }
    double s = percentile(mags, 0.95);
    fieldScale_[key] = std::isfinite(s) && s > 0 ? s : 1;
  }
  double vmax = fieldScale_[key];

  if (style == "lines" || style == "flow" || style == "streamlines") {
    viz::StreamOpts o;
    o.x0 = A.x; o.x1 = A.x + A.width; o.y0 = A.y; o.y1 = A.y + A.height;
    o.dsep = 38; o.h = 4; o.maxSteps = 500;
    // re-trace only when the view or the field changes (sampled at a few fixed points)
    std::string sig = key + "|" + std::to_string(axes_.xmin) + "," + std::to_string(axes_.xmax) + "," +
                      std::to_string(axes_.ymin) + "," + std::to_string(axes_.ymax);
    for (double u : {0.21, 0.5, 0.79})
      for (double w : {0.23, 0.5, 0.77}) {
        viz::V2 v = Fpx({A.x + u * A.width, A.y + w * A.height});
        char b[48];
        std::snprintf(b, sizeof b, "|%.3g,%.3g", v.x, v.y);
        sig += b;
      }
    auto &cache = streamCache_[m.id];
    if (cache.sig != sig) {
      cache.sig = sig;
      cache.lines = viz::streamlines(Fpx, o);
      cache.mags.clear();
      for (auto &l : cache.lines) {
        std::vector<float> ms;
        for (auto p : l) { viz::V2 v = Fpx(p); ms.push_back((float)std::hypot(v.x, v.y)); }
        cache.mags.push_back(ms);
      }
    }
    bool flow = style == "flow";
    for (size_t li = 0; li < cache.lines.size(); li++) {
      const auto &l = cache.lines[li];
      if (l.size() < 2) continue;
      std::vector<double> s(l.size(), 0);
      for (size_t i = 1; i < l.size(); i++) s[i] = s[i - 1] + viz::norm(l[i] - l[i - 1]);
      double L = s.back();
      std::vector<StripPt> pts;
      for (size_t i = 0; i < l.size(); i++) {
        float mag = (float)std::clamp(cache.mags[li][i] / vmax, 0.0, 1.0);
        float ends = (float)std::min(1.0, std::min(s[i], L - s[i]) / std::min(30.0, L / 3)); // taper both ends
        float al = a * (flow ? 0.28f : 0.35f + 0.6f * mag) * ends;
        pts.push_back({{(float)l[i].x, (float)l[i].y}, 2.f, shade(c, al)});
      }
      strip(pts);
      if (!flow) { // a small head at the middle shows which way it goes
        size_t mi = l.size() / 2;
        if (mi + 1 < l.size()) {
          Vector2 p{(float)l[mi].x, (float)l[mi].y}, q{(float)l[mi + 1].x, (float)l[mi + 1].y};
          Vector2 d = q - p;
          float dl = len(d);
          if (dl > 0) {
            d = d * (1 / dl);
            Vector2 nn{-d.y, d.x}, tip = p + d * 5;
            float al = a * (0.35f + 0.6f * (float)std::clamp(cache.mags[li][mi] / vmax, 0.0, 1.0));
            DrawTriangle(tip, tip - d * 10 + nn * 5, tip - d * 10 - nn * 5, shade(c, al));
            DrawTriangle(tip, tip - d * 10 - nn * 5, tip - d * 10 + nn * 5, shade(c, al));
          }
        }
      } else { // motion along the line: a bright comet that travels with the field
        double win = 90, speed = 110;
        double phase = std::fmod(li * 0.6180339887 * (L + win), L + win);
        double head = std::fmod(clock_ * speed + phase, L + win);
        std::vector<StripPt> comet;
        for (size_t i = 0; i < l.size(); i++) {
          if (s[i] < head - win || s[i] > head) continue;
          float u = (float)((s[i] - (head - win)) / win); // 0 tail .. 1 head
          float mag = (float)std::clamp(cache.mags[li][i] / vmax, 0.0, 1.0);
          comet.push_back({{(float)l[i].x, (float)l[i].y}, 1.5f + 1.5f * u, shade(c, a * u * u * (0.5f + 0.5f * mag))});
        }
        strip(comet);
      }
    }
    return;
  }
  // arrows: one per lattice point, centred on it, length L tanh(|v| / L) in screen space
  double L = 0;
  auto pts = lat(62);
  {
    std::vector<double> xs = lattice(axes_.xmin, axes_.xmax, A.width, 62);
    if (xs.size() > 1) L = 0.85 * (xs[1] - xs[0]) * ppx;
    else L = 50;
  }
  for (auto p : pts) {
    viz::V2 v = F(p);
    if (!std::isfinite(v.x) || !std::isfinite(v.y)) continue;
    viz::V2 vp{v.x * ppx, -v.y * ppy};
    double n = std::hypot(vp.x, vp.y);
    if (n <= 1e-12) continue;
    // lengths relative to the scene's typical magnitude: |v| = vmax draws about 0.8 L
    double drawn = viz::arrowLength(n / vmax * L, L);
    if (drawn < 1.5) continue;
    viz::V2 d = vp * (1 / n);
    Vector2 P = axes_.toScreen((float)p.x, (float)p.y);
    Vector2 h{(float)(d.x * drawn / 2), (float)(d.y * drawn / 2)};
    float mag = (float)std::clamp(n / vmax, 0.0, 1.0);
    float head = (float)std::min(9.0, drawn * 0.45);
    arrow(P - h, P + h, std::max(1.2f, (float)std::min(2.4, drawn / 10)), shade(c, a * (0.4f + 0.6f * mag)), head);
  }
}

void Engine::drawSlope(Mark &m, float a, Color c) {
  auto f = fn2(m, "fn");
  if (!f) { warn("slope: " + m.id + " needs fn={...} (dy/dx in terms of x and y)"); return; }
  const Rectangle &A = axes_.area;
  double ppx = A.width / (axes_.xmax - axes_.xmin), ppy = A.height / (axes_.ymax - axes_.ymin);
  std::vector<double> xs = lattice(axes_.xmin, axes_.xmax, A.width, 46), ys = lattice(axes_.ymin, axes_.ymax, A.height, 46);
  double sp = xs.size() > 1 ? (xs[1] - xs[0]) * ppx : 46;
  if (ys.size() > 1) sp = std::min(sp, (ys[1] - ys[0]) * ppy);
  float half = (float)(0.33 * sp);
  for (double y : ys)
    for (double x : xs) {
      double s = f(x, y);
      if (!std::isfinite(s)) continue;
      Vector2 d{(float)ppx, (float)(-s * ppy)}; // direction (1, f) on screen
      float l = len(d);
      if (l <= 0) continue;
      d = d * (half / l);
      Vector2 P = axes_.toScreen((float)x, (float)y);
      DrawLineEx(P - d, P + d, 2.f, shade(c, a * 0.6f));
    }
}

// level-set and heat-map sampling of fn over the view
bool Engine::sampleGrid(const Mark &m, int step, std::vector<float> &vals, int &nx, int &ny) {
  auto f = fn2(m, "fn");
  if (!f) return false;
  const Rectangle &A = axes_.area;
  nx = std::max(8, (int)(A.width / step) + 1);
  ny = std::max(8, (int)(A.height / step) + 1);
  vals.resize((size_t)nx * ny);
  for (int j = 0; j < ny; j++) {
    double y = axes_.ymin + (axes_.ymax - axes_.ymin) * j / (ny - 1);
    for (int i = 0; i < nx; i++) {
      double x = axes_.xmin + (axes_.xmax - axes_.xmin) * i / (nx - 1);
      vals[(size_t)j * nx + i] = (float)f(x, y);
    }
  }
  return true;
}

void Engine::drawHeat(Mark &m, float a) {
  std::vector<float> v;
  int nx, ny;
  if (!sampleGrid(m, 6, v, nx, ny)) { warn("heat: " + m.id + " needs fn={...} in terms of x and y"); return; }
  std::string key = m.id + "|" + sourceOf(m);
  if (!fieldLevels_.count(key)) {
    std::vector<double> d(v.begin(), v.end());
    double lo = percentile(d, 0.02), hi = percentile(d, 0.98);
    if (!std::isfinite(lo)) lo = 0;
    if (!(hi > lo)) hi = lo + 1;
    fieldLevels_[key] = {lo, hi};
  }
  double lo = fieldLevels_[key][0], hi = fieldLevels_[key][1];
  std::string cmap = "auto";
  if (const Value *cm = findProp(m.props, "cmap"); cm && cm->type == Value::STR) cmap = cm->str;
  bool div = cmap == "diverging" || (cmap == "auto" && lo < 0 && hi > 0 && std::min(-lo, hi) > 0.15 * std::max(-lo, hi));
  double M = std::max(std::fabs(lo), std::fabs(hi));
  std::vector<Color> px((size_t)nx * ny);
  for (int j = 0; j < ny; j++)
    for (int i = 0; i < nx; i++) {
      float val = v[(size_t)j * nx + i];
      Color &o = px[(size_t)(ny - 1 - j) * nx + i]; // image rows run top-down
      if (!std::isfinite(val)) { o = BLANK; continue; }
      viz::RGB c = div ? viz::diverging(0.5 + 0.5 * val / M) : viz::viridis((val - lo) / (hi - lo));
      o = {(unsigned char)(c.r * 255), (unsigned char)(c.g * 255), (unsigned char)(c.b * 255), 255};
    }
  auto &t = heatTex_[m.id];
  if (t.id == 0 || t.width != nx || t.height != ny) {
    if (t.id) UnloadTexture(t);
    Image im{px.data(), nx, ny, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    t = LoadTextureFromImage(im);
    SetTextureFilter(t, TEXTURE_FILTER_BILINEAR);
  } else {
    UpdateTexture(t, px.data());
  }
  bool ho;
  double op = num(m, "opacity", &ho);
  float alpha = a * (ho ? 1.f : 0.72f); // keep text and lines on top readable
  (void)op;
  const Rectangle &A = axes_.area;
  // sample centres sit on the edges of the view: inset the source by half a texel
  DrawTexturePro(t, {0.5f, 0.5f, (float)nx - 1, (float)ny - 1}, A, {0, 0}, 0, fade(WHITE, alpha));
}

void Engine::drawContour(Mark &m, float a, Color c) {
  std::vector<float> v;
  int nx, ny;
  if (!sampleGrid(m, 5, v, nx, ny)) { warn("contour: " + m.id + " needs fn={...} in terms of x and y"); return; }
  std::string key = m.id + "|" + sourceOf(m);
  if (!fieldLevels_.count(key)) {
    std::vector<double> d(v.begin(), v.end());
    double lo = percentile(d, 0.0), hi = percentile(d, 0.98);
    std::vector<double> levels;
    const Value *lv = findProp(m.props, "levels");
    if (lv && lv->type == Value::LIST) {
      for (auto &s : lv->list) levels.push_back(std::atof(s.c_str()));
    } else if (lv && lv->type == Value::STR && lv->str == "log") {
      levels = viz::logLevels(lo, hi, 8);
    } else {
      int n = lv && lv->type == Value::NUM ? std::clamp((int)lv->num, 1, 30) : 9;
      levels = viz::niceLevels(lo, hi, n);
    }
    fieldLevels_[key] = levels;
  }
  const auto &levels = fieldLevels_[key];
  const Rectangle &A = axes_.area;
  auto toScreen = [&](viz::V2 g) {
    return Vector2{A.x + (float)(g.x / (nx - 1)) * A.width, A.y + A.height - (float)(g.y / (ny - 1)) * A.height};
  };
  bool labels = !(findProp(m.props, "labels") && num(m, "labels") == 0);
  std::vector<Rectangle> taken;
  for (size_t li = 0; li < levels.size(); li++) {
    auto lines = viz::contours(v, nx, ny, levels[li]);
    float w = (levels.size() > 6 && li % 5 == 4) ? 2.6f : 1.7f; // index contours, as on maps
    for (auto &l : lines) {
      std::vector<StripPt> pts;
      for (auto g : l) pts.push_back({toScreen(g), w, shade(c, a * 0.85f)});
      strip(pts);
    }
    if (!labels || lines.empty() || (levels.size() > 8 && li % 2)) continue;
    // a label on the straightest stretch of the longest line (matplotlib's rule), upright, never overlapping
    std::string tx = fmtNum(levels[li]);
    Vector2 sz = measure(tx, 15);
    auto &l = *std::max_element(lines.begin(), lines.end(), [](auto &p, auto &q) { return p.size() < q.size(); });
    if (l.size() < 12) continue;
    std::vector<Vector2> sp;
    for (auto g : l) sp.push_back(toScreen(g));
    float best = 1e9f;
    Rectangle bestR{};
    int win = 6;
    for (size_t i = win; i + win < sp.size(); i += 3) {
      Vector2 p = sp[i];
      Rectangle r{p.x - sz.x / 2 - 4, p.y - sz.y / 2 - 2, sz.x + 8, sz.y + 4};
      if (r.x < A.x + 4 || r.y < A.y + 4 || r.x + r.width > A.x + A.width - 4 || r.y + r.height > A.y + A.height - 4) continue;
      bool hit = false;
      for (auto &t : taken) if (CheckCollisionRecs(r, {t.x - 30, t.y - 30, t.width + 60, t.height + 60})) hit = true;
      if (hit) continue;
      Vector2 c0 = sp[i - win], c1 = sp[i + win];
      Vector2 ch = c1 - c0;
      float cl = len(ch);
      if (cl < 1) continue;
      float dev = 0;
      for (size_t j = i - win; j <= i + win; j++) {
        Vector2 q = sp[j] - c0;
        dev += std::fabs(q.x * ch.y - q.y * ch.x) / cl;
      }
      if (dev < best) { best = dev; bestR = r; }
    }
    if (best < 1e9f) {
      taken.push_back(bestR);
      DrawRectangleRounded(bestR, 0.4f, 6, fade(col::bg, 0.92f * a));
      text(tx, {bestR.x + bestR.width / 2, bestR.y + bestR.height / 2}, 15, shade(c, a), 0.5f, 0.5f);
    }
  }
}

// ---------------------------------------------------------------- solution curves
// of=<slope>: y(x) through from=[x0,y0] across the view; of=<field>: the path a particle takes from there
std::vector<Vector2> Engine::solutionPoints(const Mark &m) {
  DataScope dataScope(*this, dataViewOf(m));  // its own view's axes, ranges and camera
  std::vector<Vector2> out;
  const Value *of = findProp(m.props, "of");
  Mark *src = of && of->type == Value::STR ? mark(of->str) : nullptr;
  double x0 = NAN, y0 = NAN;
  if (auto f = listNums(m, "from"); f.size() >= 2) { // eased while it morphs: the curve follows its start
    x0 = f[0];
    y0 = f[1];
  } else {
    bool hx, hy;
    x0 = num(m, "x", &hx);
    y0 = num(m, "y", &hy);
    if (!hx || !hy) return out;
  }
  if (!src || !std::isfinite(x0) || !std::isfinite(y0)) return out;
  double W = axes_.xmax - axes_.xmin, H = axes_.ymax - axes_.ymin;
  auto inRange = [&](double x, double y) {
    return std::isfinite(x) && std::isfinite(y) && x >= axes_.xmin - W && x <= axes_.xmax + W && y >= axes_.ymin - 2 * H &&
           y <= axes_.ymax + 2 * H;
  };
  std::string sk = effectiveKind(*src);
  if (sk == "slope") {
    auto f = fn2(*src, "fn");
    if (!f) return out;
    viz::Field g = [&f](viz::V2 p) { return viz::V2{1, f(p.x, p.y)}; }; // (dx, dy) per unit x
    int n = 500;
    double h = W / n;
    std::vector<viz::V2> back, fwd;
    viz::V2 p{x0, y0};
    for (int i = 0; i < 2 * n && p.x > axes_.xmin; i++) { p = viz::rk4(g, p, -h); if (!inRange(p.x, p.y)) break; back.push_back(p); }
    p = {x0, y0};
    for (int i = 0; i < 2 * n && p.x < axes_.xmax; i++) { p = viz::rk4(g, p, h); if (!inRange(p.x, p.y)) break; fwd.push_back(p); }
    for (auto it = back.rbegin(); it != back.rend(); ++it) out.push_back(axes_.toScreen((float)it->x, (float)it->y));
    out.push_back(axes_.toScreen((float)x0, (float)y0));
    for (auto &q : fwd) out.push_back(axes_.toScreen((float)q.x, (float)q.y));
    return out;
  }
  if (sk == "field") {
    auto F = vecField(*src);
    if (!F) return out;
    // the particle's own path forward in time: unit-speed steps in screen space, so it is evenly sampled
    double ppx = axes_.area.width / W, ppy = axes_.area.height / H;
    viz::Field g = [&](viz::V2 p) {
      viz::V2 v = F(p);
      double n = std::hypot(v.x * ppx, v.y * ppy);
      return n > 1e-12 ? viz::V2{v.x / n, v.y / n} : viz::V2{0, 0};
    };
    viz::V2 p{x0, y0};
    out.push_back(axes_.toScreen((float)x0, (float)y0));
    double s = 0, step = 3; // px
    for (int i = 0; i < 1500; i++) {
      viz::V2 q = viz::rk4(g, p, step);
      if (!inRange(q.x, q.y) || (q.x == p.x && q.y == p.y)) break;
      p = q;
      s += step;
      Vector2 P = axes_.toScreen((float)p.x, (float)p.y);
      out.push_back(P);
      if (s > 40 && len(P - out.front()) < step * 0.9f) { out.push_back(out.front()); break; } // a closed orbit
    }
    return out;
  }
  return out;
}

void Engine::clearFieldCaches() {
  fieldScale_.clear();
  fieldLevels_.clear();
  streamCache_.clear();
  for (auto &[id, t] : heatTex_) if (t.id) UnloadTexture(t);
  heatTex_.clear();
}

} // namespace mk
