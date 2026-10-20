// What the learner sets: a param written `set=learner` gets a slider under the stage. Dragging it
// changes the value, so whatever the param drives (a field, a curve, a position) follows at once;
// on release the stage reports it ("@ev set k 2.5"), and Claude hears what the learner tried.
//   mark k param value=1 range=[0,3] label="spring constant" set=learner
#include "engine.hpp"
#include <cmath>

namespace mk {
using namespace rm;

static const float kTrackW = 150, kRowY = 572;

bool Engine::learnerSets(const Mark &m) {
  const Value *s = findProp(m.props, "set");
  return effectiveKind(m) == "param" && s && s->type == Value::STR && s->str == "learner";
}

// its range=[a,b]; else 0 to twice its value
static void rangeOf(const Mark &m, double value, double &lo, double &hi) {
  lo = 0, hi = value > 0 ? 2 * value : 1;
  if (const Value *r = findProp(m.props, "range"); r && r->type == Value::LIST && r->list.size() == 2) {
    lo = std::atof(r->list[0].c_str());
    hi = std::atof(r->list[1].c_str());
  }
  if (hi <= lo) hi = lo + 1;
}

// one row, centred between the stage and the subtitles: name, track, value
std::vector<Engine::Control> Engine::controls() {
  std::vector<Control> out;
  float total = 0;
  for (auto &id : order_)
    if (Mark *m = mark(id); m && learnerSets(*m) && alphaOf(*m) > 0.01f) {
      Control c;
      c.id = id;
      c.name = displayName(*m);
      c.value = num(*m, "value");
      rangeOf(*m, c.value, c.lo, c.hi);
      c.width = measure(c.name, 16).x + 10 + kTrackW + 10 + 48;
      total += c.width + (out.empty() ? 0 : 32);
      out.push_back(c);
    }
  float x = (W - total) / 2;
  for (auto &c : out) {
    c.track = {x + measure(c.name, 16).x + 10, kRowY - 2, kTrackW, 4};
    x += c.width + 32;
  }
  return out;
}

void Engine::pointer(Vector2 p, bool down) {
  if (!down) {
    if (!dragging_.empty())
      if (Mark *m = mark(dragging_)) emit("set " + dragging_ + " " + fmtNum(num(*m, "value")));
    dragging_.clear();
    return;
  }
  auto cs = controls();
  if (dragging_.empty())
    for (auto &c : cs) {
      Rectangle hit{c.track.x - 10, c.track.y - 12, c.track.width + 20, c.track.height + 24};
      if (CheckCollisionPointRec(p, hit)) dragging_ = c.id;
    }
  for (auto &c : cs) {
    Mark *m = mark(c.id);
    if (c.id != dragging_ || !m) continue;
    double t = std::clamp((p.x - c.track.x) / c.track.width, 0.f, 1.f);
    double step = std::pow(10, std::floor(std::log10((c.hi - c.lo) / 100))); // about a hundred stops
    double v = std::round((c.lo + t * (c.hi - c.lo)) / step) * step;
    for (size_t i = 0; i < actives_.size();) // the learner's hand wins over a morph in flight
      if (actives_[i].id == c.id) actives_.erase(actives_.begin() + (long)i);
      else i++;
    setProp(m->props, "value", Value::number(v));
  }
}

void Engine::drawControls() {
  for (auto &c : controls()) {
    Mark *m = mark(c.id);
    float a = alphaOf(*m);
    Color k = colorOf(*m);
    float t = (float)std::clamp((c.value - c.lo) / (c.hi - c.lo), 0.0, 1.0);
    Vector2 knob{c.track.x + t * c.track.width, c.track.y + c.track.height / 2};
    text(c.name, {c.track.x - 10, knob.y}, 16, fade(col::muted, a), 1, 0.5f);
    DrawRectangleRounded(c.track, 1, 4, fade(col::muted, 0.35f * a));
    DrawRectangleRounded({c.track.x, c.track.y, knob.x - c.track.x, c.track.height}, 1, 4, fade(k, 0.8f * a));
    DrawCircleV(knob, dragging_ == c.id ? 9 : 7, fade(k, a));
    text(fmtNum(c.value), {c.track.x + c.track.width + 10, knob.y}, 16, fade(col::fg, a), 0, 0.5f);
  }
}

} // namespace mk
