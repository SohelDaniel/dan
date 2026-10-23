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
  CHECK(rb.find("range=[0, 3]") != std::string::npos);
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

TEST_CASE("a row of cells seated on= another's cells sits over them, aligned, clear of their name, on the stage") {
  Engine e(instant());
  played(e, "mark x array items=[1,2,3,4,5] label=\"input\"\nmark k array items=[1,1,1] label=\"kernel\" on=x[1:3]\n");
  place::Picture p = e.picture();
  const place::Ink *x = body(p, "x"), *k = body(p, "k");
  REQUIRE(x);
  REQUIRE(k);
  float cell = x->box.w / 5;
  CHECK(k->box.x == doctest::Approx(x->box.x + cell).epsilon(0.02)); // over cells 1..3
  CHECK(k->box.y + k->box.h <= x->box.y);  // above: its index row clear of x's name (each body includes both)
  CHECK(k->box.y >= 0);                                               // not pushed off the top
}

TEST_CASE("the voice never reads code aloud: a word property is spoken, a formula or nothing is left out") {
  Engine e(instant());
  played(e, "mark s box label=\"switch\" state=on\nmark f tex tex=\"y = 2x\"\nSay: \"It is {s.state}, see {f.tex} and {nope.v}.\"\n");
  CHECK(e.spokenLines() == std::vector<std::string>{"It is on, see  and ."});
  bool told = false;
  for (auto &l : e.lintMessages()) if (l.find("{f.tex} is a formula") != std::string::npos) told = true;
  CHECK(told);
}

// the follow-up answer that overlapped (pause and ask, 2026-10-22): a code box, two formulas and a graph, then a
// plane view beside the graph. Overlap needs a relation; nothing here relates the band to the plane
static const char *kBandOverPlot = R"scene(schema graph
mark src code text="a;\nb;\nc;\nd;\ne;\nf;\ng;\nh;" line=4 label=layer::backward
mark x1 box input label="x₁ = 2"
mark x2 box input label="x₂ = 1"
mark x3 box input label="x₃ = 3"
mark u1 box unit label="unit 1"
mark u2 box unit label="unit 2"
link a arrow x1 -> u1
link b arrow x2 -> u1
link c arrow x3 -> u2
mark bx brace of=[x1, x2, x3] label="from the layer before"
mark eq tex tex="dz = da \odot \text{drelu}([1.5,\ 0.7]) = [0.5,\ -0.25]" size=28
mark mm tex tex="\begin{pmatrix}1 & 0.5 & 1.5\\ \textcolor{bot}{-0.5} & \textcolor{bot}{-0.25} & \textcolor{bot}{-0.75}\end{pmatrix}" size=26
mark nope cross of=mm
hide nope
mark rb enclose of=mm[bot] label="dz₂ · x = −0.25 · (2, 1, 3)"
schema geo plane x=[-1.2,3] y=[-2.2,1.6] grid=false label="unit 1's weights"
mark xv vector input x=0 y=0 dx=2 dy=1 label="x = (2, 1)"
)scene";

TEST_CASE("a band too tall for the room above a plot shrinks the stage, it never spills onto the plot") {
  Engine e(instant());
  played(e, kBandOverPlot);
  place::Picture p = e.picture();
  const place::Ink *plot = nullptr;
  for (auto &i : p.ink) if (i.kind == place::Ink::Frame && i.what == "the plot 'geo'") plot = &i;
  REQUIRE(plot); // the checker sees the plot
  for (const char *id : {"src", "eq", "mm"}) {
    const place::Ink *b = body(p, id);
    REQUIRE(b);
    CHECK_MESSAGE(b->box.y + b->box.h <= plot->box.y, id << " reaches into the plot");
  }
}

TEST_CASE("a hidden note leaves no ghost: a faint cross would still say wrong; the readback keeps it") {
  Engine e(instant());
  played(e, kBandOverPlot);
  CHECK(anyInk(e.picture(), "nope") == nullptr);
  std::string rb = e.readback(false);
  CHECK(rb.find("mark nope cross of=mm opacity=0.12") != std::string::npos);
}

TEST_CASE("a named enclose of a formula's row is named off the formula, never on another row") {
  Engine e(instant()); // a code box right above: the corners above the top row are taken
  played(e, R"scene(mark src code text="matrix da = blame;\nmatrix dz = da;\ndw = dz * x.transpose();" line=3
mark mm tex tex="\begin{pmatrix}\textcolor{top}{1} & \textcolor{top}{0.5}\\ \textcolor{bot}{-0.5} & \textcolor{bot}{-0.25}\end{pmatrix}" size=26
mark ra enclose of=mm[top] label="dz₁ · x = 0.5 · (2, 1)"
mark rb enclose of=mm[bot] label="dz₂ · x = −0.25 · (2, 1)"
mark eq tex tex="dw = dz\, x^T" size=28
)scene");
  place::Picture p = e.picture();
  const place::Ink *mm = body(p, "mm"), *top = nullptr, *bot = nullptr;
  for (auto &i : p.ink) {
    if (i.kind == place::Ink::Label && i.id == "ra") top = &i;
    if (i.kind == place::Ink::Label && i.id == "rb") bot = &i;
  }
  REQUIRE((mm && top && bot));
  CHECK(top->box.y + top->box.h <= mm->box.y + 1); // the upper row's name: above the formula
  CHECK(bot->box.y >= mm->box.y + mm->box.h - 1);  // the lower row's name: below it
}

TEST_CASE("a named brace beside a column of inputs gets room beside the graph: on the screen, off the links") {
  Engine e(instant());
  played(e, kBandOverPlot);
  std::vector<std::string> msgs;
  e.layoutCheck(&msgs);
  for (auto &m : msgs) CHECK_MESSAGE(m.find("brace 'bx'") == std::string::npos, m);
}
