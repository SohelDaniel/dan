#include "regions.hpp"

namespace place {

std::optional<Rect> regionRect(const std::string &n, const Rect &s) {
  auto third = [&](int c, int r, int cw = 1, int rh = 1) {
    return Rect{s.x + c * s.w / 3, s.y + r * s.h / 3, cw * s.w / 3, rh * s.h / 3};
  };
  if (n == "top-left") return third(0, 0);
  if (n == "top-right") return third(2, 0);
  if (n == "bottom-left") return third(0, 2);
  if (n == "bottom-right") return third(2, 2);
  if (n == "center" || n == "centre") return third(1, 1);
  if (n == "top") return third(0, 0, 3, 1);
  if (n == "bottom") return third(0, 2, 3, 1);
  if (n == "left") return third(0, 0, 1, 3);
  if (n == "right") return third(2, 0, 1, 3);
  // a 6x6 slot: a1 (top-left) … f6 (bottom-right)
  if (n.size() == 2 && n[0] >= 'a' && n[0] <= 'f' && n[1] >= '1' && n[1] <= '6')
    return Rect{s.x + (n[0] - 'a') * s.w / 6, s.y + (n[1] - '1') * s.h / 6, s.w / 6, s.h / 6};
  return std::nullopt;
}

} // namespace place
