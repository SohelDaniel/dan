#include "skyline.hpp"
#include <algorithm>

namespace place {

float Skyline::place(float x0, float x1, float h) {
  float level = 0;
  for (auto &s : spans_)
    if (s.x1 > x0 + 0.5f && s.x0 < x1 - 0.5f) level = std::max(level, s.h);
  spans_.push_back({x0 - gap_, x1 + gap_, level + h + gap_});
  return level;
}

float Skyline::height() const {
  float h = 0;
  for (auto &s : spans_) h = std::max(h, s.h);
  return h;
}

} // namespace place
