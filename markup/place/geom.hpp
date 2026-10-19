// place: geometry. Plain value types, no raylib (the placer is tested without a window).
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace place {

struct Vec {
  float x = 0, y = 0;
  Vec operator+(Vec o) const { return {x + o.x, y + o.y}; }
  Vec operator-(Vec o) const { return {x - o.x, y - o.y}; }
  Vec operator*(float s) const { return {x * s, y * s}; }
  bool operator==(const Vec &o) const { return x == o.x && y == o.y; }
};
inline float length(Vec v) { return std::sqrt(v.x * v.x + v.y * v.y); }

// axis-aligned box: x,y is the top-left corner (screen coordinates, y down)
struct Rect {
  float x = 0, y = 0, w = 0, h = 0;
  float right() const { return x + w; }
  float bottom() const { return y + h; }
  Vec center() const { return {x + w / 2, y + h / 2}; }
  bool empty() const { return w <= 0 || h <= 0; }
  static Rect around(Vec c, Vec size) { return {c.x - size.x / 2, c.y - size.y / 2, size.x, size.y}; }
  bool operator==(const Rect &o) const { return x == o.x && y == o.y && w == o.w && h == o.h; }
};

inline Rect inflate(Rect r, float d) { return {r.x - d, r.y - d, r.w + 2 * d, r.h + 2 * d}; }

// the shared area of two boxes (0 if they only touch)
inline float overlapArea(const Rect &a, const Rect &b) {
  float w = std::min(a.right(), b.right()) - std::max(a.x, b.x);
  float h = std::min(a.bottom(), b.bottom()) - std::max(a.y, b.y);
  return w > 0 && h > 0 ? w * h : 0;
}
inline bool overlaps(const Rect &a, const Rect &b, float tol = 0.5f) {
  float w = std::min(a.right(), b.right()) - std::max(a.x, b.x);
  float h = std::min(a.bottom(), b.bottom()) - std::max(a.y, b.y);
  return w > tol && h > tol;
}
inline bool contains(const Rect &outer, const Rect &inner, float tol = 0.5f) {
  return inner.x >= outer.x - tol && inner.y >= outer.y - tol && inner.right() <= outer.right() + tol &&
         inner.bottom() <= outer.bottom() + tol;
}

struct Seg {
  Vec a, b;
};

// does the segment pass through the box's interior? (Liang–Barsky clipping)
inline bool hits(const Seg &s, const Rect &r) {
  float t0 = 0, t1 = 1, dx = s.b.x - s.a.x, dy = s.b.y - s.a.y;
  auto clip = [&](float p, float q) {
    if (std::fabs(p) < 1e-9f) return q >= 0;
    float t = q / p;
    if (p < 0) { if (t > t1) return false; t0 = std::max(t0, t); }
    else { if (t < t0) return false; t1 = std::min(t1, t); }
    return true;
  };
  return clip(-dx, s.a.x - r.x) && clip(dx, r.right() - s.a.x) && clip(-dy, s.a.y - r.y) &&
         clip(dy, r.bottom() - s.a.y) && t1 - t0 > 1e-4f;
}

// the 8 compass sides (and centre) a label can take around its anchor
enum class Side : uint8_t { N, NE, E, SE, S, SW, W, NW, C };
constexpr int kSides = 9;
inline Vec sideDir(Side s) {
  static const Vec d[kSides] = {{0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, 0}};
  return d[(int)s];
}

} // namespace place
