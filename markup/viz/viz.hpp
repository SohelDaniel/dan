// viz: the pure maths behind the camera, fields and trails (no raylib, no Engine, unit-tested).
//
//   ZoomPath     smooth zoom+pan (van Wijk & Nuij 2003, as in d3 interpolateZoom, rho = sqrt 2)
//   tickLevels   nice tick steps that cross-fade with zoom instead of popping
//   contours     marching squares with linear edge interpolation, saddles by centre value (d3-contour)
//   niceLevels   contour levels on a nice step (matplotlib MaxNLocator / d3 ticks)
//   rk4          one fixed step of classic Runge-Kutta
//   streamlines  evenly spaced streamlines (Jobard & Lefer 1997, d_test = 0.5 d_sep)
//   colormap     perceptually uniform maps: viridis (sequential), a dark-centred diverging map
//   arrowLength  L tanh(|v|/L) (manimgl): zero vectors vanish, so critical points stay visible
#pragma once
#include <cmath>
#include <functional>
#include <vector>

namespace viz {

struct V2 {
  double x = 0, y = 0;
};
inline V2 operator+(V2 a, V2 b) { return {a.x + b.x, a.y + b.y}; }
inline V2 operator-(V2 a, V2 b) { return {a.x - b.x, a.y - b.y}; }
inline V2 operator*(V2 a, double k) { return {a.x * k, a.y * k}; }
inline double norm(V2 a) { return std::sqrt(a.x * a.x + a.y * a.y); }
inline double smoothstep(double a, double b, double x) {
  double t = std::fmin(1, std::fmax(0, (x - a) / (b - a)));
  return t * t * (3 - 2 * t);
}

// ---------------------------------------------------------------- camera
// A view is a centre and a width (both in a space where x and y have the same unit).
// Three kinds of move:
//   anchored  zooming in on something already in view (or out from where we are): that point glides
//             to its new place on screen while the zoom is log-linear, so it never leaves the view
//   linear    a short move: ease centre and log(width)
//   van Wijk  anything else (long pans): zoom out, travel, zoom in
struct ZoomPath {
  V2 c0, c1;
  double w0 = 1, w1 = 1;
  double S = 0;          // path length in the paper's units (|S| / 0.9 is the preferred duration)
  bool linear = false;
  bool anchored = false;
  V2 anchor;             // the point that stays in view
  void set(V2 from, double wFrom, V2 to, double wTo, double aspect = 0.5625); // aspect = height / width
  void at(double t, V2 &c, double &w) const; // t in 0..1 (already eased by the caller)
  double seconds() const;                     // clamp(|S| / 0.9, 0.6, 2.0)
private:
  double r0 = 0, d = 0;
};

// ---------------------------------------------------------------- ticks
// The finest nice step (1, 2, 5 x 10^k) whose spacing is at least minPx is drawn fully; the next finer
// step fades in as its spacing grows from minPx/2 to minPx, so zooming never pops a tick into place.
struct TickLevel {
  double step = 1;
  float alpha = 1;       // lines/ticks
  float labelAlpha = 1;  // numbers (they need more room than ticks)
};
void tickLevels(double range, double px, double minPx, TickLevel &major, TickLevel &minor);
double niceStepFloor(double raw); // largest nice step <= raw
double niceStepAbove(double s);   // the next nice step after s

// ---------------------------------------------------------------- contours
// values: nx * ny samples, row-major, j = 0 at the bottom. Returns polylines in grid coordinates
// (x = i, y = j, fractional). NaN counts as below every level.
std::vector<std::vector<V2>> contours(const std::vector<float> &values, int nx, int ny, double level);
std::vector<double> niceLevels(double lo, double hi, int n);
std::vector<double> logLevels(double lo, double hi, int n); // geometric (loss bowls): resolves the valley

// ---------------------------------------------------------------- fields
using Field = std::function<V2(V2)>;
V2 rk4(const Field &f, V2 p, double h);

struct StreamOpts {
  double x0 = 0, y0 = 0, x1 = 1, y1 = 1; // domain
  double dsep = 0.06;                      // separation between lines (same units as the domain)
  double dtest = 0.5;                      // stop when closer than dtest * dsep to another line
  double h = 0.01;                         // step length (lines advance by arc length)
  int maxSteps = 600;
  double minLength = 0;                    // drop lines shorter than this (default 2 * dsep)
};
// f gives the field in domain units; lines follow its direction (forward and backward from each seed).
// Each returned line runs in the direction of the field.
std::vector<std::vector<V2>> streamlines(const Field &f, const StreamOpts &o);

// ---------------------------------------------------------------- colour
struct RGB {
  float r = 0, g = 0, b = 0;
};
RGB viridis(double t);   // t in 0..1
RGB diverging(double t); // t in 0..1, dark at 0.5 (for signed values on a dark background)

inline double arrowLength(double n, double L) { return L * std::tanh(n / L); }

// ---------------------------------------------------------------- trails
// The sample times of a trail over [t0, t1]: an absolute grid (k * h, so samples don't slide with the
// frame time and the line doesn't shimmer), the exact ends, and every breakpoint inside (where a
// motion starts or stops: corners stay exact).
std::vector<double> trailTimes(double t0, double t1, double h, const std::vector<double> &breaks);

} // namespace viz
