// place: what the placer is given and what it returns. The engine fills a Scene from its marks;
// nothing in here knows about the engine.
#pragma once
#include "geom.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace place {

enum class Rel : uint8_t { Below, Above, LeftOf, RightOf, Near, Align, On, In, Over };

struct Relation {
  Rel kind;
  int a, b;          // a is placed relative to b
  float gap = 16;
  int strength = 2;  // 0 weak, 1 medium, 2 strong (3 = required is reserved for the stage bounds)
};

struct Item {
  std::string id;
  Vec size;                  // measured box
  enum Mode : uint8_t { Fixed, Preferred, Free } mode = Preferred;
  Vec target;                // centre the schema (or data) proposes
  std::optional<Vec> prev;   // centre last time (stability)
  std::optional<Rect> region;// a named region to stay inside
  int priority = 1;
};

struct Scene {
  Rect stage{40, 66, 1200, 494};
  std::vector<Item> items;
  std::vector<Relation> rels;
  std::vector<std::pair<int, int>> mayOverlap; // intended: on/in/over, related things, a group and its members
  float gap = 12;                              // breathing room kept between things
};

enum class Code : uint8_t { RelationUnsat, Crowded };
struct Note {
  Code code;
  int a = -1, b = -1;
};

struct Placement {
  std::vector<Vec> pos;      // centre per item
  std::vector<Note> notes;
};

} // namespace place
