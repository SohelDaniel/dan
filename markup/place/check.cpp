#include "check.hpp"
#include "grid.hpp"
#include <algorithm>
#include <cmath>

namespace place {

// Which pairs are meant to touch: a label and what it names, a link and its ends, a group and what
// it encloses, furniture with furniture, and anything the engine declared related.
static bool intended(const Picture &p, int i, int j) {
  const Ink &a = p.ink[i], &b = p.ink[j];
  if (p.allowed.count({std::min(i, j), std::max(i, j)})) return true;
  auto owns = [&](const Ink &l, int x) { return l.owner == x || l.other == x; };
  if ((a.kind == Ink::Label && owns(a, j)) || (b.kind == Ink::Label && owns(b, i))) return true;
  if ((a.kind == Ink::Link && owns(a, j)) || (b.kind == Ink::Link && owns(b, i))) return true;
  // a link's own label sits on the link
  if (a.kind == Ink::Label && b.kind == Ink::Link && a.owner == j) return true;
  if (b.kind == Ink::Label && a.kind == Ink::Link && b.owner == i) return true;
  auto encloses = [&](const Ink &g, int x) {
    if (g.kind != Ink::Group) return false;
    auto member = [&](int m) { return std::find(g.members.begin(), g.members.end(), m) != g.members.end(); };
    const Ink &k = p.ink[x];
    if (member(x) || (k.kind == Ink::Label && member(k.owner))) return true;
    // a link between two members (and its label) is inside the group too
    int ln = k.kind == Ink::Link ? x : k.kind == Ink::Label && k.owner >= 0 && p.ink[k.owner].kind == Ink::Link ? k.owner : -1;
    return ln >= 0 && member(p.ink[ln].owner) && member(p.ink[ln].other);
  };
  if (encloses(a, j) || encloses(b, i)) return true;
  if (a.kind == Ink::Furniture && b.kind == Ink::Furniture) return true;
  // furniture is text (ticks, axis titles): data marks may sit on it, labels may not
  if ((a.kind == Ink::Furniture && b.kind == Ink::Body) || (b.kind == Ink::Furniture && a.kind == Ink::Body)) return true;
  // links crossing links is a matter of taste, not an overlap
  if (a.kind == Ink::Link && b.kind == Ink::Link) return true;
  // group boxes against group boxes: nesting is fine (members checked separately)
  if (a.kind == Ink::Group && b.kind == Ink::Group) return true;
  // zones only matter for what can be moved out of them
  if (a.kind == Ink::Zone && b.kind == Ink::Zone) return true;
  if ((a.kind == Ink::Zone && (b.kind == Ink::Furniture || b.kind == Ink::Link)) ||
      (b.kind == Ink::Zone && (a.kind == Ink::Furniture || a.kind == Ink::Link)))
    return true;
  return false;
}

// how long two polylines run on top of each other: nearly parallel pieces within 2.5 px
static float sharedLength(const std::vector<Vec> &a, const std::vector<Vec> &b) {
  float shared = 0;
  for (size_t i = 1; i < a.size(); i++) {
    Vec p = a[i - 1], d = a[i] - p;
    float L = length(d);
    if (L < 0.5f) continue;
    Vec u = d * (1 / L), nrm{-u.y, u.x};
    for (size_t j = 1; j < b.size(); j++) {
      Vec q0 = b[j - 1] - p, q1 = b[j] - p;
      float e = length(q1 - q0);
      if (e < 0.5f) continue;
      float dot = ((q1.x - q0.x) * u.x + (q1.y - q0.y) * u.y) / e;
      if (std::fabs(dot) < 0.9986f) continue; // more than 3 degrees apart
      if (std::fabs(q0.x * nrm.x + q0.y * nrm.y) > 2.5f || std::fabs(q1.x * nrm.x + q1.y * nrm.y) > 2.5f) continue;
      float t0 = q0.x * u.x + q0.y * u.y, t1 = q1.x * u.x + q1.y * u.y;
      shared += std::max(0.f, std::min(L, std::max(t0, t1)) - std::max(0.f, std::min(t0, t1)));
    }
  }
  return shared;
}

Report check(const Picture &p) {
  Report r;
  Grid grid(p.canvas, 25);
  for (int i = 0; i < (int)p.ink.size(); i++) if (!p.ink[i].box.empty() || p.ink[i].kind == Ink::Link) grid.insert(i, p.ink[i].box);
  float penalty = 0;
  for (int i = 0; i < (int)p.ink.size(); i++) {
    const Ink &a = p.ink[i];
    if (a.box.empty() && a.kind != Ink::Link) continue;
    // off the canvas (labels and bodies only; furniture may bleed)
    if ((a.kind == Ink::Body || a.kind == Ink::Label) && !contains(p.canvas, a.box)) {
      float over = std::max({p.canvas.x - a.box.x, a.box.right() - p.canvas.right(), p.canvas.y - a.box.y,
                             a.box.bottom() - p.canvas.bottom(), 0.f});
      r.diags.push_back({Diag::OffCanvas, i, -1, over});
      penalty += 8;
    }
    // a label too far from what it names
    if (a.kind == Ink::Label && a.owner >= 0 && p.ink[a.owner].kind != Ink::Link) {
      const Rect &o = p.ink[a.owner].box;
      float dx = std::max({o.x - a.box.right(), a.box.x - o.right(), 0.f});
      float dy = std::max({o.y - a.box.bottom(), a.box.y - o.bottom(), 0.f});
      float d = std::sqrt(dx * dx + dy * dy);
      if (!o.empty() && d > a.maxDist) { r.diags.push_back({Diag::LabelFar, i, a.owner, d}); penalty += 2; }
    }
    for (int j : grid.query(a.box)) {
      if (j <= i) continue;
      const Ink &b = p.ink[j];
      if (b.box.empty() && b.kind != Ink::Link) continue;
      // two links on one track can't be told apart: which arrow carries which label?
      if (a.kind == Ink::Link && b.kind == Ink::Link) { // (relations only: a tangent lies on its curve by design)
        bool relations = a.owner >= 0 && a.other >= 0 && b.owner >= 0 && b.other >= 0;
        if (relations && sharedLength(a.line, b.line) > 24) { r.diags.push_back({Diag::LinksAlong, i, j, 0}); penalty += 6; }
        continue;
      }
      if (intended(p, i, j)) continue;
      // links: test the drawn line against boxes, not the line's bounding box
      if (a.kind == Ink::Link || b.kind == Ink::Link) {
        const Ink &ln = a.kind == Ink::Link ? a : b;
        int li = a.kind == Ink::Link ? i : j, oi = a.kind == Ink::Link ? j : i;
        const Ink &o = p.ink[oi];
        if (o.kind != Ink::Body && o.kind != Ink::Label) continue;
        Rect inner = inflate(o.box, -3);
        if (inner.empty()) continue;
        bool cut = false;
        for (size_t k = 1; k < ln.line.size() && !cut; k++) cut = hits({ln.line[k - 1], ln.line[k]}, inner);
        if (cut) {
          r.diags.push_back({o.kind == Ink::Body ? Diag::LinkThrough : Diag::Overlap, li, oi, 0});
          penalty += o.kind == Ink::Body ? 5 : 3;
          if (o.kind != Ink::Body) r.overlaps++;
        }
        continue;
      }
      if (a.kind == Ink::Zone || b.kind == Ink::Zone) {
        const Ink &z = a.kind == Ink::Zone ? a : b;
        int ti = a.kind == Ink::Zone ? j : i;
        float area = overlapArea(z.box, p.ink[ti].box);
        if (area > 4) { r.diags.push_back({Diag::InZone, ti, a.kind == Ink::Zone ? i : j, area}); penalty += 8; }
        continue;
      }
      float area = overlapArea(a.box, b.box);
      if (area <= 4) continue; // a hair is not an overlap
      float smaller = std::max(1.f, std::min(a.box.w * a.box.h, b.box.w * b.box.h));
      r.diags.push_back({Diag::Overlap, i, j, area});
      r.overlaps++;
      penalty += 30 * std::min(1.f, area / smaller) * std::max(1, std::max(a.priority, b.priority));
    }
  }
  r.score = std::clamp(100 - penalty, 0.f, 100.f);
  Rect ext{0, 0, 0, 0};
  int bodies = 0;
  for (const Ink &a : p.ink) {
    if (a.kind != Ink::Body && a.kind != Ink::Label && a.kind != Ink::Group && a.kind != Ink::Link) continue;
    bodies += a.kind == Ink::Body;
    float x0 = std::max(a.box.x, p.stage.x), y0 = std::max(a.box.y, p.stage.y);
    float x1 = std::min(a.box.right(), p.stage.right()), y1 = std::min(a.box.bottom(), p.stage.bottom());
    if (x1 <= x0 || y1 <= y0) continue;
    if (ext.empty()) ext = {x0, y0, x1 - x0, y1 - y0};
    else {
      float ex1 = std::max(ext.right(), x1), ey1 = std::max(ext.bottom(), y1);
      ext.x = std::min(ext.x, x0); ext.y = std::min(ext.y, y0);
      ext.w = ex1 - ext.x; ext.h = ey1 - ext.y;
    }
  }
  if (bodies >= 2 && !ext.empty()) {
    r.fill = ext.w * ext.h / (p.stage.w * p.stage.h);
    r.offset = std::max(std::fabs(ext.center().x - p.stage.center().x) / p.stage.w, std::fabs(ext.center().y - p.stage.center().y) / p.stage.h);
  }
  return r;
}

std::string describe(const Picture &p, const Report &r, const Diag &d) {
  auto name = [&](int i) { return i >= 0 && i < (int)p.ink.size() ? p.ink[i].what : std::string("?"); };
  switch (d.code) {
  case Diag::Overlap: return name(d.a) + " overlaps " + name(d.b);
  case Diag::OffCanvas: return name(d.a) + " runs " + std::to_string((int)d.amount) + "px off the screen";
  case Diag::InZone: return name(d.a) + " sits in the " + p.ink[d.b].what + " area";
  case Diag::LabelFar: return name(d.a) + " is " + std::to_string((int)d.amount) + "px from " + name(d.b);
  case Diag::LinkThrough: return name(d.a) + " passes through " + name(d.b);
  case Diag::LinksAlong: return name(d.a) + " runs along " + name(d.b);
  }
  return "";
}

} // namespace place
