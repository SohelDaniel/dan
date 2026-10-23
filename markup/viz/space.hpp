// viz: a 3D view (no raylib, no Engine, unit-tested). Every point of a `space` scene reaches the screen
// through Space::project, the one mapping: labels, the placer and the checker then agree with the pixels,
// and everything is drawn as 2D strokes and triangles sorted by depth (research_notes/3D/explainers.md).
//
// The data box is seen by a camera orbiting its centre: azimuth theta around the vertical (z) axis, measured
// from +x towards +y, and tilt phi down from straight above. z is up, the frame is right-handed. A narrow
// perspective, and a scale fitted to the cylinder the box sweeps as it turns about z (the spin), so turning never
// makes the scene breathe while the stage's width is used.
#pragma once
#include <cmath>
#include <string>

namespace viz {

struct V3 {
  double x = 0, y = 0, z = 0;
};
inline V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline V3 operator*(V3 a, double k) { return {a.x * k, a.y * k, a.z * k}; }
inline double dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline double norm(V3 a) { return std::sqrt(dot(a, a)); }
inline V3 unit(V3 a) { double n = norm(a); return n > 0 ? a * (1 / n) : a; }

constexpr double kDeg = 3.14159265358979323846 / 180;

// where the camera is: a named view, or one reached by turning
struct Orbit {
  double theta = 30 * kDeg; // x comes towards the viewer on the left, y goes right
  double phi = 70 * kDeg;   // 3Blue1Brown's tilt (mean 72.6 degrees over his 3D scenes)
  double zoom = 1;          // 1: the whole box fits
  bool aimed = false;       // looking at target (data units); else at the box's centre
  V3 target;
};
// "default", "top", "front", "side": false for any other name
bool namedView(const std::string &name, Orbit &o);
// the tilt the camera keeps to: never under the floor, never so flat the floor is a line, never straight down
// (looking straight down, "up" on screen is undefined; a hair off it, y is up)
double clampTilt(double phi);

struct Projected {
  double x = 0, y = 0;  // screen
  double depth = 0;     // distance along the view direction: larger is further
  double scale = 1;     // screen pixels per world unit at that depth (perspective)
};

struct Space {
  // the data box and how its units map to the world: equal units (a sphere is round), or each axis
  // scaled to the same length (a box, for z ranges far from x and y: a loss surface)
  double lo[3] = {-1, -1, -1}, hi[3] = {1, 1, 1};
  bool equal = true;
  // the screen area it is drawn in
  double ax = 0, ay = 0, aw = 800, ah = 600;
  double fov = 30 * kDeg;   // narrow: little distortion, still a sense of depth
  Orbit view;

  V3 centre() const { return {(lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2}; }
  V3 aim() const { return view.aimed ? view.target : centre(); } // what the camera looks at, data units
  V3 world(V3 data) const;        // data units -> world units (the box centred on the origin)
  double radius() const;          // the box's bounding sphere, in world units
  Projected project(V3 data) const;
  V3 eye() const;                 // where the camera is, in world units
  double focal() const;           // screen pixels per world unit at unit depth, fitted to the area
  V3 forward() const;             // the view direction, world units
  // how a world direction looks on screen (unit length when it lies flat in the view): which way an axis points
  double screenDir(V3 dataDir, double &dx, double &dy) const;

private:
  mutable double fitKey_[12] = {}, fit_ = 0; // focal() for the view, area and box it was worked out for
};

} // namespace viz
