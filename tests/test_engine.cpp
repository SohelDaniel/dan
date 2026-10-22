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

static const char *kRefs = "mark a bar value=3 label=\"a\"\nmark b bar value=4 label=\"b\"\n"
                           "Say: \"A is {a} and b is {b}, so together {a + b}.\"\n"
                           "Say: \"Now a |grow|grows to {a}.\"\n@grow morph a.value 3 -> 10 1s\n"
                           "Say: \"Half of it is {a / 2}.\"\n";

TEST_CASE("{expr} in a Say speaks the value on stage when the word is reached, a morph at its end value") {
  Engine e(instant());
  played(e, kRefs);
  auto said = e.spokenLines();
  REQUIRE(said.size() == 3);
  CHECK(said[0] == "A is 3 and b is 4, so together 7.");
  CHECK(said[1] == "Now a grows to 10.");
  CHECK(said[2] == "Half of it is 5.");
}

TEST_CASE("live, the shadow reads the values before the beat plays, streamed or whole, as the instant run does") {
  EngineOptions o; // a live engine without a window or a voice
  o.quiet = true;
  std::string text = kRefs;
  size_t cut = text.find("Say: \"Now");
  Engine whole(o), streamed(o);
  REQUIRE(whole.loadText(text));
  REQUIRE(streamed.loadText(text.substr(0, cut)));
  REQUIRE(streamed.appendText(text.substr(cut)));
  std::vector<std::string> want = {"A is 3 and b is 4, so together 7.", "Now a grows to 10.", "Half of it is 5."};
  CHECK(whole.spokenLines() == want);
  CHECK(streamed.spokenLines() == want);
}

TEST_CASE("{expr} changes only what is said: the scene and its readback are the same as with typed numbers") {
  Engine a(instant()), b(instant());
  played(a, kRefs);
  std::string typed = kRefs;
  for (auto [from, to] : std::vector<std::pair<std::string, std::string>>{{"{a + b}", "7"}, {"{a / 2}", "5"}, {"{a}", "3"}, {"{b}", "4"}})
    for (size_t p; (p = typed.find(from)) != std::string::npos;) typed.replace(p, from.size(), to);
  played(b, typed);
  CHECK(a.readback(false) == b.readback(false));
}

TEST_CASE("{a[k]} speaks an array cell's item, {M[r][c]} a grid's, as written") {
  Engine e(instant());
  played(e, "mark a array items=[2,5,cat,12]\nmark M array cols=2 items=[1,2,3,4]\nSay: \"Slot {a[2]} holds {a[3]}; row one ends in {M[1][1]}.\"\n");
  auto said = e.spokenLines();
  REQUIRE(said.size() == 1);
  CHECK(said[0] == "Slot cat holds 12; row one ends in 4.");
}

TEST_CASE("live, an array cell is read off the stage too (binary search: the middle is {a[4]})") {
  EngineOptions o;
  o.quiet = true;
  Engine e(o);
  REQUIRE(e.loadText("mark a array items=[2,5,8,12,16,23]\nmark mid label text=\"mid\" at=a[0]\n"
                     "Say: \"Check the |m|middle: {a[4]}.\"\n@m mark mid at=a[4]\nSay: \"{a[4]} is too small.\"\n"));
  std::vector<std::string> want = {"Check the middle: 16.", "16 is too small."};
  CHECK(e.spokenLines() == want);
}

TEST_CASE("after the learner drags a slider, {k} says the value they set") {
  EngineOptions o; // live: the shadow reads {k}, so it has to hear about the drag
  o.quiet = true;
  Engine e(o);
  played(e, "mark k param value=1 range=[0,2] label=\"k\" set=learner\n");
  for (float x = 0; x < 1280; x += 4) e.pointer({x, 572}, true);
  e.pointer({1279, 572}, false);
  REQUIRE(e.appendText("Say: \"Now k is {k}.\"\n"));
  CHECK(e.spokenLines() == std::vector<std::string>{"Now k is 2."});
}

static bool hasLint(Engine &e, const std::string &part) {
  for (auto &l : e.lintMessages()) if (l.find(part) != std::string::npos) return true;
  return false;
}

TEST_CASE("binding: a symbol said as a name must be on stage (f over an unlabelled heat map), contractions and C++ aren't symbols") {
  const char *field = "schema plane x=[-1,3] y=[-1,3]\nmark H heat fn={x^2 + y^2}%s\n"
                      "Say: \"Picture f as a height. That's it, and it doesn't move.\"\nSay: \"In C++ it is one line.\"\nSay: \"Done.\"\n";
  char bare[512], named[512];
  std::snprintf(bare, sizeof bare, field, "");
  std::snprintf(named, sizeof named, field, " label=\"f(x, y) = x² + y²\"");
  Engine a(instant()), b(instant());
  played(a, bare);
  played(b, named);
  CHECK(hasLint(a, "says \"f\" as a name"));
  CHECK(!hasLint(b, "as a name"));
  CHECK(!hasLint(a, "says \"s\"")); // that's
  CHECK(!hasLint(a, "says \"t\"")); // doesn't
  CHECK(!hasLint(a, "says \"c\"")); // C++
}

TEST_CASE("numbers said aloud are read whole, with the words they span") {
  auto read = [](const std::string &s) {
    std::vector<std::string> toks;
    std::vector<size_t> owner;
    std::vector<std::string> words;
    std::istringstream in(s);
    for (std::string w; in >> w;) words.push_back(w);
    Engine::sayTokens(words, toks, owner);
    std::vector<double> out;
    for (auto &n : Engine::readNumbers(toks)) out.push_back(n.v);
    return out;
  };
  CHECK(read("seven hundred eighty-four") == std::vector<double>{784});
  CHECK(read("ninety-nine thousand nine hundred people") == std::vector<double>{99900});
  CHECK(read("zero point one four and two point five") == std::vector<double>{0.14, 2.5});
  CHECK(read("one hundred and five, seven and a half") == std::vector<double>{105, 7.5});
  CHECK(read("one of them is four times bigger") == std::vector<double>{}); // "one" alone, a ratio
  CHECK(read("forty percent") == std::vector<double>{40});
}

TEST_CASE("pointing ahead: a number said aloud pulses the one cell that shows it; live, the shadow puts the pointer on the word") {
  EngineOptions o;
  o.quiet = true;
  Engine e(o);
  REQUIRE(e.loadText("mark a array items=[2,5,8,12,16,23] label=\"sorted, looking for 23\"\n"
                     "Say: \"We want twenty-three, and the middle is sixteen.\"\nSay: \"Both are here.\"\n"));
  CHECK(e.pointingReport().find("2 pointed ahead") != std::string::npos);
}

TEST_CASE("stagger: the cells of a span pulse one after another, and it changes nothing in the scene") {
  EngineOptions o; // live: staggering is timing, so it needs a clock
  o.quiet = true;
  Engine e(o);
  REQUIRE(e.loadText("mark a array items=[3,1,4]\nindicate a[0:2] stagger=0.5s\n"));
  e.update(0.1f);
  CHECK(e.pulsing("a") == "a[0]");
  e.update(0.5f); // 0.6 s: the second cell has started
  CHECK(e.pulsing("a") == "a[1]");
  e.update(0.5f); // 1.1 s: the third
  CHECK(e.pulsing("a") == "a[2]");
  Engine plain(instant()), staggered(instant());
  played(plain, "mark a array items=[3,1,4]\nmark t box label=\"total\"\nsend a[0:2] -> t\n");
  played(staggered, "mark a array items=[3,1,4]\nmark t box label=\"total\"\nsend a[0:2] -> t stagger=0.3s\n");
  CHECK(plain.readback(false) == staggered.readback(false));
}

TEST_CASE("a curve that leaves its plot is named where it leaves, not in the corner at the end of its range") {
  Engine e(instant());
  played(e, "schema axes x=[0,6] y=[0,40]\nmark g path fn={exp(x)} label=\"e^x\"\n");
  place::Picture p = e.picture();
  const place::Ink *name = nullptr;
  for (auto &i : p.ink) if (i.kind == place::Ink::Label && i.id == "g") name = &i;
  REQUIRE(name);
  float cx = name->box.x + name->box.w / 2;
  CHECK(cx < 900); // e^x leaves the top of the plot near x = 3.7 (screen x ~ 770); the range ends at x = 6 (~1180)
}

TEST_CASE("stagger: a span sent onto a span as long goes cell by cell, each copy landing on its own cell") {
  EngineOptions o;
  o.quiet = true;
  Engine e(o);
  played(e, "mark a array items=[3,1]\nmark b array items=[0,0]\n");
  REQUIRE(e.appendText("send a[0:1] -> b[0:1] stagger=0.5s 0.4s\n"));
  auto run = [&](float secs) { for (int i = 0; i < (int)(secs * 60); i++) e.update(1.f / 60); };
  run(0.6f); // the first copy landed at 0.4 s: its cell pulses
  CHECK(e.pulsing("b") == "b[0]");
  run(0.5f); // the second left at 0.5 s and landed at 0.9 s
  CHECK(e.pulsing("b") == "b[1]");
}

TEST_CASE("voice context: a Say asked for ahead of its beat gets the same earlier sentences its beat will ask with") {
  EngineOptions o; // live, no voice: only the bookkeeping
  o.quiet = true;
  Engine e(o);
  REQUIRE(e.loadText(""));
  using V = std::vector<std::string>;
  // the server's order: each Say is sent ahead (#!voice) as soon as it is written, before the beat before it
  CHECK(e.prefetchVoice("Say: \"A first.\"") == V{});
  CHECK(e.prefetchVoice("Say: \"B second.\"") == V{"A first."});
  REQUIRE(e.appendText("Say: \"A first.\"\n"));
  V c = e.prefetchVoice("Say: \"C third.\"");
  CHECK(c == V{"A first.", "B second."});
  REQUIRE(e.appendText("Say: \"B second.\"\n"));
  REQUIRE(e.appendText("Say: \"C third.\"\n"));
  CHECK(e.saysBefore(1, 2) == V{"A first."});
  CHECK(e.saysBefore(2, 2) == c); // what C's beat asks with: the request made ahead is the one it plays
}
