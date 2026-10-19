// place: a skyline (as music engravers stack articulations above a staff). Annotations along a
// row, taken left to right, each settle at the lowest level that is free across their width, so
// labels under an array or above a number line stack only where they would otherwise collide.
#pragma once
#include <vector>

namespace place {

class Skyline {
public:
  explicit Skyline(float gap = 4) : gap_(gap) {}
  // an annotation spanning [x0, x1] that is h tall: returns its offset from the row (0 = first level)
  float place(float x0, float x1, float h);
  float height() const;   // the deepest level used so far
private:
  struct Span { float x0, x1, h; };
  std::vector<Span> spans_;
  float gap_;
};

} // namespace place
