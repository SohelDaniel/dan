// place: the exact layout checker. Everything drawn is "ink": bodies, labels, link lines, group
// boxes, furniture (axes, ticks) and reserved screen zones (title, subtitles). The checker finds the
// overlaps nobody asked for, things off the canvas or in a reserved zone, labels far from what they
// name and links that cut through things, and turns them into short messages a scene author (Claude)
// can act on, plus a 0..100 score.
#pragma once
#include "geom.hpp"
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace place {

struct Ink {
  enum Kind : uint8_t { Body, Label, Link, Group, Furniture, Zone } kind = Body;
  std::string id;            // the thing's id (owner's id for labels)
  std::string what;          // in words: "bar 'bob'", "name of 'alice'", "link a->b"
  Rect box;                  // for links: the bounding box of the line
  std::vector<Vec> line;     // links and curves: the polyline actually drawn
  int owner = -1;            // labels: the ink they name; links: endpoint a
  int other = -1;            // links: endpoint b
  std::vector<int> members;  // groups: the inks they enclose
  int priority = 1;          // weighs overlaps in the score (focus/emphasis higher)
  float maxDist = 60;        // labels: how far from their owner still reads as "theirs"
};

struct Picture {
  Rect canvas{0, 0, 1280, 720};
  Rect stage{40, 66, 1200, 494};
  std::vector<Ink> ink;
  std::set<std::pair<int, int>> allowed; // intended overlaps (ascending pairs): related, on/in/over …
  int add(Ink i) { ink.push_back(std::move(i)); return (int)ink.size() - 1; }
  void allow(int a, int b) { if (a >= 0 && b >= 0 && a != b) allowed.insert({std::min(a, b), std::max(a, b)}); }
};

struct Diag {
  enum Code : uint8_t { Overlap, OffCanvas, InZone, LabelFar, LinkThrough, LinksAlong } code = Overlap;
  int a = -1, b = -1;
  float amount = 0;          // overlap area (px²), or distance / overhang in px
};

struct Report {
  std::vector<Diag> diags;
  float score = 100;                     // 100 = clean
  // composition, kept apart from the score: how much of the stage the drawing's extent covers (0..1)
  // and how far its centre is off the stage's centre (fraction of the stage size)
  float fill = 0, offset = 0;
  int overlaps = 0;
};

Report check(const Picture &p);
std::string describe(const Picture &p, const Report &r, const Diag &d); // "name of 'alice' overlaps bar 'bob'"

} // namespace place
