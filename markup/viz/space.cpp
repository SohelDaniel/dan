#include "space.hpp"
#include <algorithm>

namespace viz {

bool namedView(const std::string &name, Orbit &o) {
  Orbit d; // the default
  if (name == "default") { o.theta = d.theta; o.phi = d.phi; return true; }
  if (name == "top") { o.theta = -90 * kDeg; o.phi = clampTilt(0); return true; }    // x right, y up
  if (name == "front") { o.theta = -90 * kDeg; o.phi = 90 * kDeg; return true; }     // x right, z up
  if (name == "side") { o.theta = 0; o.phi = 90 * kDeg; return true; }               // y right, z up
  return false;
}

double clampTilt(double phi) { return std::clamp(phi, 0.5 * kDeg, 90 * kDeg); }

V3 Space::world(V3 d) const {
  double s[3];
  double widest = std::max({hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2], 1e-9});
  for (int i = 0; i < 3; i++) s[i] = 2 / (equal ? widest : std::max(hi[i] - lo[i], 1e-9));
  V3 c = centre();
  return {(d.x - c.x) * s[0], (d.y - c.y) * s[1], (d.z - c.z) * s[2]};
}

double Space::radius() const {
  V3 a = world({lo[0], lo[1], lo[2]}), b = world({hi[0], hi[1], hi[2]});
  return std::max(1e-9, norm(b - a) / 2);
}

V3 Space::forward() const {
  double t = view.theta, p = clampTilt(view.phi);
  return V3{-std::cos(t) * std::sin(p), -std::sin(t) * std::sin(p), -std::cos(p)};
}

// far enough that the bounding sphere just fills the field of view at zoom 1
V3 Space::eye() const { return world(aim()) - forward() * (radius() / std::sin(fov / 2)); }

// the scale at which the box fits the area from any azimuth: the cylinder it sweeps turning about z (its corners'
// circle, at the bottom and the top of the box), seen from this tilt, projected and fitted to the area
double Space::focal() const {
  double key[12] = {view.theta, view.phi, view.zoom, ax, ay, aw, ah, fov, hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2], equal ? 1.0 : 0.0};
  if (fit_ > 0 && std::equal(key, key + 12, fitKey_)) return fit_;
  std::copy(key, key + 12, fitKey_);
  V3 f = forward(), r = unit(cross(f, {0, 0, 1})), u = cross(r, f), e = eye();
  V3 a = world({lo[0], lo[1], lo[2]}), b = world({hi[0], hi[1], hi[2]}), c = (a + b) * 0.5;
  double rxy = std::sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y)) / 2;
  double sx = 1e-9, sy = 1e-9;
  for (int i = 0; i < 32; i++)
    for (double z : {a.z, b.z}) {
      double t = i * 3.14159265358979 / 16;
      V3 q = V3{c.x + rxy * std::cos(t), c.y + rxy * std::sin(t), z} - e;
      double d = std::max(1e-6, dot(q, f));
      sx = std::max(sx, std::fabs(dot(q, r)) / d);
      sy = std::max(sy, std::fabs(dot(q, u)) / d);
    }
  return fit_ = std::min(aw / 2 / sx, ah / 2 / sy) * 0.96 * view.zoom; // a little air at the edges
}

Projected Space::project(V3 data) const {
  V3 f = forward(), r = unit(cross(f, {0, 0, 1})), u = cross(r, f);
  V3 q = world(data) - eye();
  double depth = std::max(1e-6, dot(q, f));
  double focal = this->focal();
  Projected p;
  p.depth = depth;
  p.scale = focal / depth;
  p.x = ax + aw / 2 + dot(q, r) * p.scale;
  p.y = ay + ah / 2 - dot(q, u) * p.scale;
  return p;
}

double Space::screenDir(V3 dataDir, double &dx, double &dy) const {
  Projected a = project(aim()), b = project(aim() + dataDir);
  dx = b.x - a.x;
  dy = b.y - a.y;
  double n = std::sqrt(dx * dx + dy * dy);
  if (n > 0) { dx /= n; dy /= n; }
  return n;
}

} // namespace viz
