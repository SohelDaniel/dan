// place: named regions of the stage. A 3x3 vocabulary (top-left … bottom-right, center, top, left …)
// and a finer 6x6 grid of slots a1 … f6 (columns a–f, rows 1–6), as in Code2Video's anchor grid.
#pragma once
#include "geom.hpp"
#include <optional>
#include <string>

namespace place {
std::optional<Rect> regionRect(const std::string &name, const Rect &stage);
}
