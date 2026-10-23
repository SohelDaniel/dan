// History and trails.
//
//   mark p dot x={cos(t)} y={sin(t)} trail=1.5s     a fading tail: where p was over the last 1.5 s
//   mark p dot ... trail=all                        the whole path since p appeared, kept
//
// A trail is recomputed every frame from the recorded history of every number that changed (morphs,
// re-marks, instant sets), not accumulated frame by frame (manim's TracedPath, Unity's TrailRenderer):
// so it is the same at 30 or 144 fps, in --snapshot, after a skip and after appended lines. Samples lie
// on an absolute time grid (they don't slide with the frame time, so the line doesn't shimmer) plus the
// moments motions start and stop (corners stay exact). Fading by time, not length: a faster thing leaves
// a longer tail, so speed shows. Drawn as a tapered, feathered strip.
#include "engine.hpp"
#include "viz/viz.hpp"
#include <algorithm>
#include <cmath>

namespace mk {

using namespace rm;

void Engine::recordActive(const Active &a, const Value &before) {
  HistSeg h;
  h.t0 = clock_ - frameDt_; // the active's own clock starts a frame back: it has run dt by the end of this frame
  h.dur = a.dur;
  h.from = a.from;
  h.to = a.to;
  h.ease = a.ease;
  h.final = a.finalVal;
  h.before = before;
  hist_[a.id + "." + a.prop].push_back(h);
}

void Engine::recordSet(const std::string &id, const std::string &prop, const Value &before, const Value &after) {
  HistSeg h;
  h.t0 = clock_;
  h.set = true;
  h.final = after;
  h.before = before;
  hist_[id + "." + prop].push_back(h);
}

bool Engine::histValue(const std::string &id, const std::string &prop, double t, double &out) {
  auto it = hist_.find(id + "." + prop);
  if (it == hist_.end() || it->second.empty()) return false;
  const auto &segs = it->second;
  double keep = histT_;
  // the latest segment that had started by t; before the first one, the value it started from
  const HistSeg *s = nullptr;
  for (auto &g : segs) if (g.t0 <= t + 1e-9) s = &g;
  if (!s) {
    const HistSeg &f = segs.front();
    out = f.set ? num(f.before) : f.from;
    histT_ = keep;
    return std::isfinite(out);
  }
  if (!s->set && t < s->t0 + s->dur) {
    double k = s->dur > 0 ? (t - s->t0) / s->dur : 1;
    double e = s->ease ? s->ease((float)std::clamp(k, 0.0, 1.0)) : k;
    out = s->from + (s->to - s->from) * e;
  } else {
    out = num(s->final); // an expression is evaluated at t too (its names read history)
  }
  histT_ = keep;
  return true;
}

std::vector<double> Engine::histBreaks(double t0, double t1) {
  std::vector<double> b;
  for (auto &[k, segs] : hist_)
    for (auto &g : segs) {
      if (g.t0 > t0 && g.t0 < t1) b.push_back(g.t0);
      double e = g.t0 + g.dur;
      if (!g.set && e > t0 && e < t1) b.push_back(e);
    }
  return b;
}

bool Engine::trailSpec(const Mark &m, double &window, bool &all) {
  const Value *v = findProp(m.props, "trail");
  if (!v) return false;
  all = false;
  if (v->type == Value::NUM) { window = v->num; return window > 0; }
  std::string s = v->type == Value::STR ? v->str : v->markup();
  if (s == "off" || s == "none" || s == "0") return false;
  if (s == "all" || s == "on" || s == "full" || s == "path") { all = true; return true; }
  char *end = nullptr;
  window = std::strtod(s.c_str(), &end);
  if (end == s.c_str()) { window = 1.5; return true; }
  return window > 0;
}

void Engine::drawTrail(Mark &m) {
  DataScope dataScope(*this, dataViewOf(m));  // its own view's axes, ranges and camera
  for (auto &run : trailRuns(m)) strip(run);
}

// the screen polyline(s) of a mark's trail at the current time; a teleport splits it
std::vector<Vector2> Engine::trailLine(Mark &m) {
  DataScope dataScope(*this, dataViewOf(m));  // its own view's axes, ranges and camera
  std::vector<Vector2> out;
  for (auto &run : trailRuns(m)) {
    // thin for the placer and checker: every ~10 px, and wherever it turns
    for (size_t i = 0; i < run.size(); i++) {
      Vector2 p = run[i].p;
      bool keep = out.empty() || i + 1 == run.size() || len(p - out.back()) > 10;
      if (!keep && i + 1 < run.size() && out.size() >= 1) {
        Vector2 a = p - out.back(), b = run[i + 1].p - p;
        float la = len(a), lb = len(b);
        if (la > 0 && lb > 0 && (a.x * b.x + a.y * b.y) / (la * lb) < 0.94f) keep = true;
      }
      if (keep) out.push_back(p);
    }
  }
  return out;
}

std::vector<std::vector<StripPt>> Engine::trailRuns(Mark &m) {
  std::vector<std::vector<StripPt>> runs;
  double window;
  bool all;
  bool space = isSpace(schemaNow()); // a path in space: re-projected every frame, so it turns with the view
  if (!trailSpec(m, window, all) || !(isAxes(schemaNow()) || space)) return runs;
  if (!findProp(m.props, "x") || !findProp(m.props, "y")) return runs;
  double t1 = clock_, born = bornAt_.count(m.id) ? bornAt_[m.id] : 0;
  double t0 = std::max(born, all ? born : t1 - window);
  if (t1 - t0 < 1e-3) return runs;
  // at most ~1500 samples: long persistent paths step coarser (still on a fixed grid)
  double h = std::max(1.0 / 120, (t1 - t0) / 1500);
  h = std::pow(2.0, std::ceil(std::log2(h))); // powers of two keep the grid fixed as the trail grows
  std::vector<double> ts = viz::trailTimes(t0, t1, h, histBreaks(t0, t1));
  float a = alphaOf(m);
  Color c = colorOf(m);
  const float width = all ? 2.2f : 3.6f;
  std::vector<StripPt> run;
  auto flush = [&] { if (run.size() > 1) runs.push_back(run); run.clear(); };
  Vector2 last{};
  bool have = false;
  double maxJump = std::max(40.0, 2400.0 * h); // a jump this big in one sample is a teleport, not motion
  for (double t : ts) {
    histT_ = t;
    bool hx, hy, hz;
    double x = num(m, "x", &hx), y = num(m, "y", &hy), z = space ? num(m, "z", &hz) : 0;
    histT_ = NAN;
    if (!hx || !hy || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) { flush(); have = false; continue; }
    Vector2 P = axes_.toScreen((float)x, (float)y);
    if (space) { viz::Projected q = spc_.proj.project({x, y, z}); P = {(float)q.x, (float)q.y}; }
    if (have && len(P - last) > maxJump) flush();
    float u = all ? 0.f : (float)std::clamp((t1 - t) / window, 0.0, 1.0); // 0 at the head, 1 at the tail
    float w = all ? width : std::max(0.6f, width * std::sqrt(1 - u));
    float al = a * (all ? 0.7f : std::pow(1 - u, 1.5f));
    run.push_back({P, w, fade(c, al)});
    last = P;
    have = true;
  }
  flush();
  return runs;
}

} // namespace mk
