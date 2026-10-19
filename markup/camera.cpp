// The camera on axes and plane scenes. Claude says what to look at (zoom=8 on=p, fit=a,b, follow=p);
// the engine works out the view, the timing and the path:
//   - the view is an override of the schema's ranges (layoutAxes), so marks, labels, the placer and the
//     checker all see exactly what is drawn; text and strokes keep their screen size, curves are
//     re-sampled for the visible range ("zoom in until it's straight" is real);
//   - moves follow van Wijk & Nuij's smooth zoom-and-pan path (log-linear zoom, zooming out mid-flight on
//     long pans), with a duration from the length of the move; following uses a critically damped
//     spring with a dead zone (the camera only moves when the thing nears the edge).
#include "engine.hpp"
#include "viz/viz.hpp"
#include <algorithm>
#include <cmath>

namespace mk {

void Engine::doCamera(const Line &ln) {
  std::string s = schemaNow();
  if (s != "axes" && s != "plane") {
    warn("camera: line " + std::to_string(ln.lineNo) + " needs an axes or plane scene (this one is " + s + ")");
    return;
  }
  if (const Value *f = findProp(ln.props, "follow")) {
    std::string id = f->type == Value::STR ? f->str : f->markup();
    cam_.follow = (id == "none" || id == "off") ? "" : id;
    if (!cam_.follow.empty() && !mark(baseId(cam_.follow))) warn("camera: follow='" + cam_.follow + "' is not on stage");
    if (cam_.follow.empty() && ln.props.size() == 1 && ln.kind.empty()) { // just stop following
      if (cam_.spec.find("follow=") != std::string::npos) cam_.spec.clear();
      return;
    }
  }
  for (const char *k : {"on"})
    if (const Value *v = findProp(ln.props, k); v && !mark(baseId(v->str)))
      warn("camera: " + std::string(k) + "='" + v->str + "' is not on stage");
  // a newer move takes over from where the camera is now
  actives_.erase(std::remove_if(actives_.begin(), actives_.end(), [](const Active &a) { return a.kind == Active::CAM; }),
                 actives_.end());
  cam_.from = cam_.now;
  cam_.req = ln;
  cam_.resolved = false;
  cam_.moving = true;
  cam_.k = 0;
  cam_.on = true;
  cam_.release = ln.kind == "reset" || (ln.kind == "fit" && !findProp(ln.props, "fit"));
  if (cam_.release) {
    cam_.spec.clear();
    cam_.follow.clear();
  } else {
    cam_.spec = "camera";
    if (!ln.kind.empty() && ln.kind != "fit") cam_.spec += " " + ln.kind;
    for (auto &p : ln.props) cam_.spec += " " + p.key + "=" + p.val.markup();
  }
  Active a;
  a.kind = Active::CAM;
  a.dur = opt_.instant ? 0 : (ln.dur >= 0 ? ln.dur : 1.2f); // automatic durations are set once resolved
  a.ease = rm::rateByName(ln.ease.empty() ? "smooth" : ln.ease);
  a.log = cam_.spec.empty() ? "camera back to the whole scene" : cam_.spec;
  actives_.push_back(a);
  caption(cam_.release ? "camera -> back to the whole picture (context returns)"
                       : "camera -> the view moves to what the voice is about (zoom is a pointer)");
}

// where a thing is, in data coordinates: a point, the centre of a shape, or a point on a curve (x=)
bool Engine::camPoint(const std::string &ref, const Props &req, double &x, double &y) {
  Mark *m = mark(baseId(ref));
  if (!m) return false;
  std::string k = effectiveKind(*m);
  const Value *rx = findProp(req, "x");
  if (findProp(m->props, "fn")) {
    bool ok;
    double X = rx ? num(*rx) : num(*m, "x", &ok);
    if (!rx && !ok) X = (axes_.xmin + axes_.xmax) / 2;
    x = X;
    y = pathAt(*m, X);
    return std::isfinite(x) && std::isfinite(y);
  }
  bool hx, hy;
  x = num(*m, "x", &hx);
  y = num(*m, "y", &hy);
  if (hx && hy && std::isfinite(x) && std::isfinite(y)) return true;
  float a, b, c, d;
  if (worldExtent(*m, a, b, c, d)) { x = (a + b) / 2; y = (c + d) / 2; return true; }
  return false;
}

bool Engine::camBox(const std::vector<std::string> &ids, double &x0, double &x1, double &y0, double &y1) {
  x0 = y0 = 1e300; x1 = y1 = -1e300;
  auto add = [&](double x, double y) {
    if (!std::isfinite(x) || !std::isfinite(y)) return;
    x0 = std::min(x0, x); x1 = std::max(x1, x); y0 = std::min(y0, y); y1 = std::max(y1, y);
  };
  for (auto &id : ids) {
    Mark *m = mark(baseId(id));
    if (!m) { warn("camera: fit='" + id + "' is not on stage"); continue; }
    float a, b, c, d;
    if (findProp(m->props, "fn")) { // a curve: over its domain, or what is in view now
      float lo = axes_.xmin, hi = axes_.xmax;
      if (const Value *d = findProp(m->props, "domain"); d && d->type == Value::LIST && d->list.size() >= 2) {
        float a0 = (float)std::atof(d->list[0].c_str()), a1 = (float)std::atof(d->list[1].c_str());
        if (a1 > a0) { lo = a0; hi = a1; }
      }
      for (int i = 0; i <= 60; i++) { double X = lo + (hi - lo) * i / 60.0; add(X, pathAt(*m, X)); }
    } else if (worldExtent(*m, a, b, c, d)) {
      add(a, c); add(b, d);
    } else {
      double x, y;
      if (camPoint(id, {}, x, y)) add(x, y);
    }
  }
  return x1 >= x0 && y1 >= y0;
}

void Engine::camApply(Rectangle area) {
  const double bx0 = axes_.xmin, bx1 = axes_.xmax, by0 = axes_.ymin, by1 = axes_.ymax;
  const double bw = bx1 - bx0, bh = by1 - by0;
  CamView base{(bx0 + bx1) / 2, (by0 + by1) / 2, 1};
  if (!cam_.on || !(bw > 0) || !(bh > 0)) { cam_.now = base; return; }
  // base pixels: the schema's view, where x and y have the same unit (so the zoom path is isotropic)
  const double sx = area.width / bw, sy = area.height / bh;
  auto toPx = [&](const CamView &v, viz::V2 &c, double &w) { c = {(v.cx - bx0) * sx, (v.cy - by0) * sy}; w = area.width / v.z; };
  auto fromPx = [&](viz::V2 c, double w) { return CamView{bx0 + c.x / sx, by0 + c.y / sy, area.width / w}; };
  bool camActive = std::any_of(actives_.begin(), actives_.end(), [](const Active &a) { return a.kind == Active::CAM; });

  if (cam_.moving && !cam_.resolved) {
    const Line &ln = cam_.req;
    CamView T = cam_.now;
    if (cam_.release) {
      T = base;
    } else if (const Value *f = findProp(ln.props, "fit")) {
      std::vector<std::string> ids = f->type == Value::LIST ? f->list : std::vector<std::string>{f->str};
      double x0, x1, y0, y1;
      if (camBox(ids, x0, x1, y0, y1)) {
        const double margin = 56; // px around the fitted things (their names need room)
        // a single point (or a thin thing) still gets a sensible frame: at least a third of the stage tall
        double minPx = area.height / 3;
        double pw = std::max((x1 - x0) * sx, minPx), ph = std::max((y1 - y0) * sy, minPx);
        T.cx = (x0 + x1) / 2;
        T.cy = (y0 + y1) / 2;
        T.z = std::min((area.width - 2 * margin) / pw, (area.height - 2 * margin) / ph);
        T.z = std::clamp(T.z, 0.25, 64.0);
        if (const Value *z = findProp(ln.props, "zoom")) { double zz = num(*z); if (std::isfinite(zz) && zz > 0) T.z = zz; }
      }
    } else {
      const Value *on = findProp(ln.props, "on");
      std::string target = on ? on->str : cam_.follow;
      double x, y;
      if (!target.empty() && camPoint(target, ln.props, x, y)) { T.cx = x; T.cy = y; }
      const Value *vx = findProp(ln.props, "x"), *vy = findProp(ln.props, "y");
      if (target.empty() && vx) T.cx = num(*vx);
      if (target.empty() && vy) T.cy = num(*vy);
      if (const Value *z = findProp(ln.props, "zoom")) {
        double zz = num(*z);
        if (std::isfinite(zz) && zz > 0) T.z = zz;
        else warn("camera: zoom should be a positive number (8 = eight times closer)");
      }
    }
    T.z = std::clamp(T.z, 0.05, 1000.0);
    cam_.to = T;
    cam_.resolved = true;
    if (cam_.req.dur < 0 && !opt_.instant) { // automatic duration: from how far the view travels
      viz::ZoomPath zp;
      viz::V2 c0, c1;
      double w0, w1;
      toPx(cam_.from, c0, w0);
      toPx(cam_.to, c1, w1);
      zp.set(c0, w0, c1, w1, area.height / area.width);
      for (auto &a : actives_) if (a.kind == Active::CAM) a.dur = (float)zp.seconds();
    }
  }

  CamView v = cam_.now;
  if (cam_.moving && cam_.resolved) {
    viz::ZoomPath zp;
    viz::V2 c0, c1, c;
    double w0, w1, w;
    toPx(cam_.from, c0, w0);
    toPx(cam_.to, c1, w1);
    zp.set(c0, w0, c1, w1, area.height / area.width);
    zp.at(cam_.k, c, w);
    v = fromPx(c, w);
    if (cam_.k >= 1 && !camActive) {
      v = cam_.to;
      cam_.moving = false;
      if (cam_.release) { cam_.on = false; cam_.now = base; return; }
    }
  } else if (!cam_.follow.empty()) {
    double x, y;
    if (camPoint(cam_.follow, {}, x, y)) {
      // dead zone: the central 45% of the view; outside it, the camera eases the thing back in
      double hw = bw / (2 * v.z), hh = bh / (2 * v.z), lim = 0.45;
      double gx = v.cx, gy = v.cy;
      if (x > v.cx + lim * hw) gx = x - lim * hw;
      if (x < v.cx - lim * hw) gx = x + lim * hw;
      if (y > v.cy + lim * hh) gy = y - lim * hh;
      if (y < v.cy - lim * hh) gy = y + lim * hh;
      if (opt_.instant) { v.cx = gx; v.cy = gy; }
      else {
        place::Spring spx{(float)v.cx, cam_.vx}, spy{(float)v.cy, cam_.vy};
        spx.step((float)gx, 0.18f, lastDt_);
        spy.step((float)gy, 0.18f, lastDt_);
        v.cx = spx.x; v.cy = spy.x; cam_.vx = spx.v; cam_.vy = spy.v;
      }
    }
  }
  cam_.now = v;
  double hw = bw / (2 * v.z), hh = bh / (2 * v.z);
  axes_.xmin = (float)(v.cx - hw); axes_.xmax = (float)(v.cx + hw);
  axes_.ymin = (float)(v.cy - hh); axes_.ymax = (float)(v.cy + hh);
}

} // namespace mk
