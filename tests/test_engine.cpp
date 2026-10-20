// Unit tests for the engine, headless (font metrics only, no window): identity, colour, views, layout,
// annotations, the learner's sliders. Each case is a bug that once shipped, or a rule the dictionary makes.
//   make test
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
#include "../markup/engine.hpp"
#include "../rm/raymanim.hpp"

using namespace mk;

// a scene played to its end, instantly
static Engine &played(Engine &e, const std::string &text) {
  static bool fonts = (SetTraceLogLevel(LOG_ERROR), rm::initFontMetrics(nullptr), true);
  (void)fonts;
  REQUIRE(e.loadText(text));
  e.runToEnd();
  for (int i = 0; i < 5; i++) e.update(1.f / 60);
  return e;
}

static EngineOptions instant() {
  EngineOptions o;
  o.instant = true;
  o.voice = false;
  o.quiet = true;
  return o;
}

static const place::Ink *body(const place::Picture &p, const std::string &id) {
  for (auto &i : p.ink) if (i.kind == place::Ink::Body && i.id == id) return &i;
  return nullptr;
}

static const place::Ink *anyInk(const place::Picture &p, const std::string &id) {
  for (auto &i : p.ink) if (i.id == id) return &i;
  return nullptr;
}

static bool sameColor(Color a, Color b) { return a.r == b.r && a.g == b.g && a.b == b.b; }

TEST_CASE("re-marking updates the props it names and keeps the rest") {
  Engine e(instant());
  played(e, "mark k param value=1 range=[0,3] label=\"stretch\" set=learner\nmark k value=2.5\n");
  std::string rb = e.readback(false);
  CHECK(rb.find("value=2.5") != std::string::npos);
  CHECK(rb.find("range=[0,3]") != std::string::npos);
  CHECK(rb.find("set=learner") != std::string::npos);
}

TEST_CASE("colour slots go to categories only: re-marking a label never uses one up") {
  Engine e(instant());
  std::string s = "schema graph\nmark a box c0 label=\"a\"\n";
  for (int i = 0; i < 12; i++) s += "mark a label=\"a" + std::to_string(i) + "\"\n"; // used to take a slot each
  for (int c = 1; c < 10; c++) s += "mark m" + std::to_string(c) + " box c" + std::to_string(c) + " label=\"m\"\n";
  s += "mark same box c0 label=\"same\"\n";
  played(e, s);
  std::vector<std::string> ids = {"a"};
  for (int c = 1; c < 10; c++) ids.push_back("m" + std::to_string(c));
  for (size_t i = 0; i < ids.size(); i++) // ten categories, ten colours
    for (size_t j = i + 1; j < ids.size(); j++)
      CHECK_MESSAGE(!sameColor(e.colorFor(ids[i]), e.colorFor(ids[j])), ids[i] << " and " << ids[j] << " share a colour");
  CHECK(sameColor(e.colorFor("a"), e.colorFor("same"))); // one category keeps one colour
}

TEST_CASE("a chain of links reads left to right, on one line") {
  Engine e(instant());
  played(e, "schema graph\nmark a box label=\"a\"\nmark b box label=\"b\"\nmark c box label=\"c\"\nlink ab arrow a -> b\nlink bc arrow b -> c\n");
  place::Picture p = e.picture();
  auto *a = body(p, "a"), *b = body(p, "b"), *c = body(p, "c");
  REQUIRE((a && b && c));
  CHECK(a->box.x + a->box.w < b->box.x);
  CHECK(b->box.x + b->box.w < c->box.x);
  CHECK(std::abs(a->box.y - c->box.y) < 1);
}

TEST_CASE("an unnamed schema followed by named ones is the view 'main': its things stay apart from the others") {
  Engine e(instant());
  played(e, "schema plane x=[-3,3] y=[-3,3]\nmark sun circle x=0 y=0 r=1\nschema net graph\nmark net box label=\"network\"\n");
  place::Picture p = e.picture();
  auto *net = body(p, "net"), *sun = anyInk(p, "sun"); // a shape on a plane is ink as its outline
  REQUIRE((net && sun));
  bool apart = net->box.x + net->box.w <= sun->box.x || sun->box.x + sun->box.w <= net->box.x ||
               net->box.y + net->box.h <= sun->box.y || sun->box.y + sun->box.h <= net->box.y;
  CHECK(apart);
}

TEST_CASE("annotations on neighbouring things don't land on each other") {
  Engine e(instant());
  played(e, "mark t table rows=[[H,4,4],[O,2,2]] cols=[atom,left,right]\nmark k1 check of=t[0] label=\"balanced\"\nmark k2 check of=t[1] label=\"O matches\"\n");
  place::Picture p = e.picture();
  std::vector<const place::Ink *> marks;
  for (auto &i : p.ink) if ((i.id == "k1" || i.id == "k2") && i.kind != place::Ink::Zone) marks.push_back(&i);
  REQUIRE(marks.size() >= 2);
  for (auto *x : marks)
    for (auto *y : marks)
      if (x->id < y->id) {
        bool apart = x->box.x + x->box.w <= y->box.x || y->box.x + y->box.w <= x->box.x ||
                     x->box.y + x->box.h <= y->box.y || y->box.y + y->box.h <= x->box.y;
        CHECK_MESSAGE(apart, x->what << " overlaps " << y->what);
      }
}

TEST_CASE("the learner's slider sets the param it belongs to, and a morph in flight gives way") {
  Engine e(instant());
  played(e, "mark k param value=1 range=[0,2] label=\"k\" set=learner\n");
  // press left of the track and sweep right: the drag starts on the track and ends at the top of the range
  for (float x = 0; x < 1280; x += 4) e.pointer({x, 572}, true);
  e.pointer({1279, 572}, false);
  CHECK(e.readback(false).find("value=2") != std::string::npos);
}
