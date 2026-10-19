// TeX formulas as glyph outlines.
//
// Typesetting is done by MathJax in a long-lived node helper (tools/tex/tex2paths.mjs), which
// replies with every glyph's outline and position. Layouts are cached in memory and on disk
// (.tutor_cache/tex), so a formula is typeset once. Each distinct outline is rasterized once per
// size into a small texture (nonzero winding, analytic coverage), so glyphs can move on their
// own: a formula can morph into another by moving the glyphs the two have in common.
#pragma once
#include "raylib.h"
#include <memory>
#include <string>
#include <vector>

namespace mk {

struct TexShape {
  std::string id;                              // stable hash of the outline (shared across formulas)
  std::vector<std::vector<Vector2>> contours;  // units: 1000 per em, y down, relative to the glyph origin
  Rectangle box{0, 0, 0, 0};
};

struct TexGlyph {
  std::string key;    // character code (MathJax data-c) or "rect": what matches across formulas
  int shape = 0;      // index into TexLayout::shapes
  Vector2 at{0, 0};   // glyph origin, units from the formula's top-left
  std::string color;  // from \textcolor / \color, or empty
};

struct TexLayout {
  bool ok = false;
  std::string error;
  float w = 0, h = 0, base = 0;  // units
  std::vector<TexGlyph> glyphs;
  std::vector<TexShape> shapes;
};
using TexPtr = std::shared_ptr<const TexLayout>;

namespace tex {
// Non-blocking: the layout if it is ready, else nullptr (it is being typeset; ask again next frame).
TexPtr request(const std::string &src);
// Blocking (headless check): waits for the helper. Returns an error layout if it is unavailable.
TexPtr typeset(const std::string &src);
// Whether the helper could be started (node + tools/tex present).
bool available();
// Draws one glyph with its top-left origin at `origin` (pixels), `pxPerUnit` = font px / 1000.
void drawGlyph(const TexLayout &L, const TexGlyph &g, Vector2 origin, float pxPerUnit, Color c);
} // namespace tex

} // namespace mk
