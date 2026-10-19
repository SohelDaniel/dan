// place: a critically damped spring (closed form; exact for any dt, so it never overshoots or blows up
// on a slow frame). Things glide to where the placer puts them instead of snapping.
//   Daniel Holden, "Spring-It-On" (2021), https://theorangeduck.com/page/spring-roll-call
#pragma once
#include <cmath>

namespace place {

struct Spring {
  float x = 0, v = 0;
  // halflife: seconds to cover half the remaining distance
  void step(float goal, float halflife, float dt) {
    float y = (4.f * 0.69314718f / (halflife + 1e-5f)) / 2.f;
    float j0 = x - goal, j1 = v + j0 * y, e = std::exp(-y * dt);
    x = e * (j0 + j1 * dt) + goal;
    v = e * (v - j1 * y * dt);
  }
};

} // namespace place
