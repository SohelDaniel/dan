// place: the graph schema's layout, a layered drawing in the manner of Sugiyama and Graphviz dot.
// Position comes from structure only: links make columns (cause left, effect right), declaration order
// is the order inside a column ("model order", as ELK keeps it), and part-of groups are lanes whose
// members stay together, with columns shared across lanes so stages line up for comparison.
// A link that skips columns gets waypoints through the gaps, so it never crosses a box.
// Streaming: a memo keeps each thing's column and height, so new things are inserted and the
// things already on stage keep their order (Misue et al.'s mental map).
#pragma once
#include "geom.hpp"
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace place {

struct FlowEdge { int a, b; float labelW = 0; }; // a -> b, in declaration order; labelW: room its label needs
struct FlowInput {
  Rect area;
  std::vector<std::string> id;  // things in declaration order
  std::vector<Vec> size;
  std::vector<int> lane;        // top-level group (lanes numbered in declaration order), -1: none
  std::vector<int> category;    // first-seen index: things without links form columns by category
  std::vector<FlowEdge> edges;
};
struct FlowMemo { std::map<std::string, std::pair<int, float>> slot; }; // id -> (column, centre y) last time
struct FlowRoute {
  std::vector<Vec> via;         // waypoints through the gaps (empty: the next column)
  float portA = 0, portB = 0;   // offsets along the source's right side and the target's left side
  bool back = false;            // runs against the flow (a cycle): drawn as an arc
};
struct FlowOut {
  std::vector<Vec> centre;
  std::vector<int> rank;        // column of each thing
  std::vector<FlowRoute> route; // per edge
  bool overflow = false;        // wider or taller than the area even with the gaps at their minimum
};

FlowOut layoutFlow(const FlowInput &in, FlowMemo &memo);

// a polyline through the points, each piece a cubic leaving and arriving horizontally
std::vector<Vec> smoothRoute(const std::vector<Vec> &pts, int perSeg = 12);

} // namespace place
