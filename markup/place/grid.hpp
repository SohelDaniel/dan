// place: a uniform collision grid (as map renderers do for labels). Boxes go into every cell
// they touch; a query only looks at those cells, so collision tests stay cheap as scenes grow.
#pragma once
#include "geom.hpp"
#include <vector>

namespace place {

class Grid {
public:
  Grid(Rect bounds, float cell = 25);
  void insert(int id, const Rect &r);
  void insert(int id, const Seg &s);           // a segment: every cell its bounding box touches
  std::vector<int> query(const Rect &r) const; // ids that may touch r (each once, ascending)
  void clear();

private:
  int cx(float x) const;
  int cy(float y) const;
  Rect bounds_;
  float cell_;
  int nx_, ny_;
  std::vector<std::vector<int>> cells_;
};

} // namespace place
