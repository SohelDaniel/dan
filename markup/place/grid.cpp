#include "grid.hpp"
#include <algorithm>

namespace place {

Grid::Grid(Rect bounds, float cell) : bounds_(bounds), cell_(cell) {
  nx_ = std::max(1, (int)std::ceil(bounds.w / cell));
  ny_ = std::max(1, (int)std::ceil(bounds.h / cell));
  cells_.assign((size_t)nx_ * ny_, {});
}
int Grid::cx(float x) const { return std::clamp((int)std::floor((x - bounds_.x) / cell_), 0, nx_ - 1); }
int Grid::cy(float y) const { return std::clamp((int)std::floor((y - bounds_.y) / cell_), 0, ny_ - 1); }

void Grid::insert(int id, const Rect &r) {
  for (int j = cy(r.y); j <= cy(r.bottom()); j++)
    for (int i = cx(r.x); i <= cx(r.right()); i++) cells_[(size_t)j * nx_ + i].push_back(id);
}
void Grid::insert(int id, const Seg &s) {
  Rect r{std::min(s.a.x, s.b.x), std::min(s.a.y, s.b.y), std::fabs(s.b.x - s.a.x), std::fabs(s.b.y - s.a.y)};
  insert(id, r);
}
std::vector<int> Grid::query(const Rect &r) const {
  std::vector<int> out;
  for (int j = cy(r.y); j <= cy(r.bottom()); j++)
    for (int i = cx(r.x); i <= cx(r.right()); i++) {
      const auto &c = cells_[(size_t)j * nx_ + i];
      out.insert(out.end(), c.begin(), c.end());
    }
  std::sort(out.begin(), out.end());
  out.erase(std::unique(out.begin(), out.end()), out.end());
  return out;
}
void Grid::clear() { for (auto &c : cells_) c.clear(); }

} // namespace place
