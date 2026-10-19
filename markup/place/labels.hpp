// place: label placement, the way map renderers place city names. Each label gets candidate spots
// around the thing it names (8 sides at a few distances, or points along a curve); labels are placed
// most-important first, each at its cheapest spot. Cost = side preference + distance + overlap with
// things, other labels, lines and reserved zones. A label keeps its previous side unless another is
// clearly better (hysteresis), so labels don't flip back and forth as things move.
#pragma once
#include "geom.hpp"
#include <vector>

namespace place {

struct LabelReq {
  Rect anchor;                    // the thing it names
  Vec size;                       // the label's box
  int priority = 1;               // higher: placed first; 2+: never hidden
  std::vector<Side> sides;        // allowed sides, best first (empty: S, E, N, W, then diagonals)
  std::vector<Vec> along;         // extra candidate centres (e.g. points near a curve's end)
  int prevSide = -1;              // where it was last time (index into sides, or -2 for an `along` spot)
  int owner = -1;                 // obstacle index of the thing it names (it may sit on it)
};

struct LabelOut {
  Rect box;
  int side = -1;                  // index into the request's sides (-2: along)
  bool shown = true;
  bool clean = true;              // no overlap at all
};

struct LabelWorld {
  Rect bounds{0, 0, 1280, 720};
  std::vector<Rect> obstacles;               // things (bodies, blocks)
  std::vector<std::vector<Vec>> strokes;     // curves, links, axes: labels shouldn't sit on them
  std::vector<Rect> zones;                   // reserved screen areas (title, subtitles)
};

std::vector<LabelOut> placeLabels(const std::vector<LabelReq> &reqs, const LabelWorld &world);

} // namespace place
