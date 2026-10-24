// Unit tests for the engine, headless (font metrics only, no window): identity, colour, views, layout,
// annotations, the learner's sliders. Each case is a bug that once shipped, or a rule the dictionary makes.
//   make test
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
#include "../markup/engine.hpp"
#include "../rm/raymanim.hpp"

using namespace mk;

static void initFonts() {
  static bool fonts = (SetTraceLogLevel(LOG_ERROR), rm::initFontMetrics(nullptr), true);
  (void)fonts;
}

// a scene played to its end, instantly
static Engine &played(Engine &e, const std::string &text) {
  initFonts();
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

// ---------------------------------------------------------------- space (3D)

TEST_CASE("space: a point is where the projection puts it, and it moves when the camera turns") {
  Engine e(instant());
  played(e, "schema space x=[-2,2] y=[-2,2] z=[-2,2]\nmark p dot x=1 y=1 z=1 label=\"p\"\nmark q dot x=-1 y=-1 z=-1\n");
  place::Picture before = e.picture();
  const place::Ink *p = body(before, "p"), *q = body(before, "q");
  REQUIRE((p && q));
  CHECK(p->box.y < q->box.y);                      // higher in space, higher on screen
  float px = p->box.x;
  played(e, "schema space x=[-2,2] y=[-2,2] z=[-2,2]\nmark p dot x=1 y=1 z=1 label=\"p\"\nmark q dot x=-1 y=-1 z=-1\ncamera orbit=90\n");
  CHECK(body(e.picture(), "p")->box.x != doctest::Approx(px)); // turned: it moved on screen
}

TEST_CASE("space: points lined up in this view are not 'equal'; only a meeting in 3D says so") {
  Engine e(instant());
  // from the front, (0, -1, 0) and (0, 1, 0) are one behind the other
  played(e, "schema space x=[-2,2] y=[-2,2] z=[-2,2]\nmark a dot x=0 y=-1 z=0\nmark b dot x=0 y=1 z=0\ncamera view=front\n"
            "mark c dot x=1 y=1 z=1\nmark d dot x=1 y=1 z=1\n");
  std::string rb = e.readback(true);
  CHECK(rb.find("# overlapping: a b") == std::string::npos);
  CHECK(rb.find("# overlapping: c d") != std::string::npos);
}

TEST_CASE("space: the camera lines that turned it come back in the readback, in order, and spin stays last") {
  Engine e(instant());
  played(e, "schema space\nmark p dot x=1 y=1 z=1\ncamera view=top\ncamera spin\ncamera orbit=45\n");
  std::string rb = e.readback(false);
  size_t top = rb.find("camera view=top"), orbit = rb.find("camera orbit=45"), spin = rb.find("camera spin");
  REQUIRE(top != std::string::npos);
  REQUIRE(orbit != std::string::npos);
  REQUIRE(spin != std::string::npos);
  CHECK(top < orbit);
  CHECK(orbit < spin);
}

TEST_CASE("space: every name in a scene of every kind is clear of the others, of the lines and of the numbers") {
  Engine e(instant());
  played(e, R"scene(schema space x=[-3,3] y=[-3,3] z=[-2,3]
mark B sphere x=1.5 y=-1.5 z=1 r=0.8 label="sphere"
mark P plane normal=[0,0,1] d=2.2 label="z = 2.2"
mark h path xt={2*cos(t)} yt={2*sin(t)} zt={t/5} range=[0,12] label="helix"
mark a vector dx=2 dy=0 dz=0 label="a"
mark b vector dx=0 dy=2 dz=0 label="b"
mark c vector dx=0 dy=0 dz=2 label="a × b"
mark F polygon pts=[0,0,0, 2,0,0, 2,2,0, 0,2,0] label="area 4"
mark q dot x=-2 y=2 z=1.5 drop=true label="q"
camera spin
)scene");
  std::vector<std::string> msgs;
  e.layoutCheck(&msgs);
  for (auto &m : msgs) CHECK_MESSAGE(m.find("overlaps") == std::string::npos, m);
}

// the rest after a formula step: the second sentence starts this long after the first ended and its formula settled
static float restBetween(const std::string &text) {
  EngineOptions o; // live timing: the rest is time
  o.quiet = true;
  o.voice = false;
  Engine e(o);
  REQUIRE(e.loadText(text));
  for (int i = 0; i < 60 * 60 && !e.finished(); i++) e.update(1.f / 60);
  float end1 = -1, settled = -1, start2 = -1;
  int says = 0;
  for (auto &ev : e.timeline) {
    float t = std::stof(ev);
    std::string kind = ev.substr(ev.find(' ') + 1, 3);
    if (kind == "say" && ++says == 2) start2 = t;
    else if (kind == "end" && says == 1) end1 = t;
    else if (kind == "tex" && says == 1) settled = t;
  }
  REQUIRE(start2 >= 0);
  return start2 - std::max(end1, settled);
}

TEST_CASE("a sentence that changed a formula rests a second before the next one; no change, no rest; a wait counts") {
  const std::string f = "mark f tex tex=\"a+b\"\n";
  CHECK(restBetween(f + "Say: \"One.\"\nmorph f.tex -> \"a+b=c\" 1.2s\nSay: \"Two.\"\n") >= 0.95f);            // a step after the sentence
  CHECK(restBetween(f + "Say: \"One |go|two.\"\n@go morph f.tex -> \"a+b=c\" 1.2s\nSay: \"Two.\"\n") >= 0.95f); // a step on a word
  CHECK(restBetween(f + "Say: \"One.\"\nSay: \"Two.\"\n") < 0.1f);                                                // nothing changed
  float w = restBetween(f + "Say: \"One.\"\nmorph f.tex -> \"a+b=c\" 1.2s\nwait 2s\nSay: \"Two.\"\n");           // Claude's own beat
  CHECK(w >= 1.95f);
  CHECK(w < 2.3f);
}

TEST_CASE("a formula part pointed at with indicate f[part] is a named part: no tex lint, as with an annotation") {
  Engine e(instant());
  played(e, "mark f tex tex=\"\\textcolor{sq}{(x+3)^2} = 7\"\nSay: \"One thing |sq|squared.\"\n@sq indicate f[sq]\n");
  bool texLint = false;
  for (auto &w : e.lintMessages()) if (w.find("textcolor{sq}") != std::string::npos) texLint = true;
  CHECK(!texLint);
  Engine bare(instant());
  played(bare, "mark f tex tex=\"\\textcolor{sq}{(x+3)^2} = 7\"\nSay: \"One thing squared.\"\n");
  bool bareLint = false;
  for (auto &w : bare.lintMessages()) if (w.find("textcolor{sq}") != std::string::npos) bareLint = true;
  CHECK(bareLint);
}

// ---- axiom 2, Structure: scales fixed by the prescan, also for values the script computes, also live

// a live engine without a window or a voice (sentences on estimated timing), stepped at 60 fps
static EngineOptions live() {
  EngineOptions o;
  o.quiet = true;
  return o;
}
static void run(Engine &e, float seconds) { for (int i = 0; i < (int)std::lround(seconds * 60); i++) e.update(1.f / 60); }
static void runOut(Engine &e) {
  for (int g = 0; g < 60 * 300 && !e.finished(); g++) e.update(1.f / 60);
  run(e, 1); // and settle
}
static Rectangle inkBox(Engine &e, const std::string &id) {
  place::Picture p = e.picture();
  const place::Ink *b = body(p, id);
  REQUIRE(b);
  return {b->box.x, b->box.y, b->box.w, b->box.h};
}
static bool offScreen(Engine &e) {
  std::vector<std::string> msgs;
  e.layoutCheck(&msgs);
  for (auto &m : msgs) if (m.find("off the screen") != std::string::npos) return true;
  return false;
}

static const char *kExprBar = "title \"A bar driven by an expression\"\nmark clock param value=0\n"
                              "mark a bar score value=3 label=\"a\"\nmark b bar score value={clock*clock} label=\"b\"\n"
                              "Say: \"a is three, b starts at zero.\"\nSay: \"Now b |go|grows to thirty-six.\"\n"
                              "@go morph clock.value 0 -> 6 4s ease=linear\nSay: \"And a never moved.\"\n";

TEST_CASE("structure: a bar whose value is an expression is in the prescan too, so the shared scale never moves") {
  initFonts();
  Engine e(live());
  REQUIRE(e.loadText(kExprBar));
  run(e, 2); // a has entered, b is still zero
  float before = inkBox(e, "a").height;
  runOut(e); // b grew to 36
  float after = inkBox(e, "a").height;
  CHECK(after > 10);
  CHECK(std::fabs(before - after) < 1); // a's three never changed, nor did its bar
  for (auto &l : e.lintMessages()) CHECK(l.rfind("structure:", 0) != 0); // a whole script: nothing moved
}

TEST_CASE("structure: a dot driven by a param keeps to its inferred axes; the same for a point in a space") {
  initFonts();
  Engine e(live());
  REQUIRE(e.loadText("title \"A dot driven by a param, no ranges\"\nmark k param value=2\nmark p dot x=1 y=1 label=\"p\"\n"
                     "mark q dot x={k} y=3 label=\"q\"\nSay: \"p and q.\"\nSay: \"q |go|slides to twelve.\"\n"
                     "@go morph k.value 2 -> 12 2s\nSay: \"Where did it go?\"\n"));
  runOut(e);
  CHECK(!offScreen(e));
  Engine s(live());
  REQUIRE(s.loadText("title \"A point leaves the box\"\nschema space\nmark k param value=1\nmark p dot x={k} y=0 z=0 label=\"p\"\n"
                     "Say: \"p |go|moves to ten.\"\n@go morph k.value 1 -> 10 2s\nSay: \"Gone.\"\n"));
  runOut(s);
  CHECK(!offScreen(s));
  int spaceLints = 0;
  for (auto &l : s.lintMessages()) if (l.rfind("space:", 0) == 0) spaceLints++;
  CHECK(spaceLints == 0); // the box was sized for the whole path
}

TEST_CASE("structure, live: a later beat's bigger value joins the scales at the next sentence start, eased, with one lint") {
  initFonts();
  Engine e(live());
  REQUIRE(e.loadText("title \"Streamed dots with a magnitude\"\nmark a dot size v=4 label=\"a\"\nmark b dot size v=9 label=\"b\"\n"
                     "Say: \"a is four and b is nine.\"\n"));
  runOut(e);
  float r0 = inkBox(e, "a").width / 2;
  REQUIRE(e.appendText("mark c dot size v=100 label=\"c\"\nSay: \"And c is a hundred.\"\n"));
  e.update(1.f / 60); // c enters and its sentence starts: the scale begins to move now
  float r1 = inkBox(e, "a").width / 2;
  runOut(e);
  float r2 = inkBox(e, "a").width / 2;
  CHECK(r2 < r0 * 0.5f);                         // the scale did move: a is smaller next to a hundred
  CHECK(std::fabs(r1 - r0) < 0.25f * (r0 - r2)); // but not in one frame
  int structure = 0;
  for (auto &l : e.lintMessages()) if (l.rfind("structure:", 0) == 0) { structure++; CHECK(l.find("'v'") != std::string::npos); }
  CHECK(structure == 1);
}

TEST_CASE("structure, live: a morph-only beat that pushes a thing past the inferred axes widens them; it never leaves the plot") {
  initFonts();
  Engine e(live());
  REQUIRE(e.loadText("title \"Streamed dots on inferred axes\"\nmark p dot x=1 y=1 label=\"p\"\nmark q dot x=2 y=3 label=\"q\"\n"
                     "Say: \"p and q sit near the origin.\"\n"));
  runOut(e);
  REQUIRE(e.appendText("Say: \"q |go|slides to twelve.\"\n@go morph q.x 2 -> 12 1s\n"));
  runOut(e);
  CHECK(!offScreen(e));
}

// ---------------------------------------------------------------- riders: a thing on= a curve, a line or a surface

// the centre of a thing's body (a dot) as the checker sees it
static Vector2 bodyCentre(Engine &e, const std::string &id) {
  place::Picture p = e.picture();
  const place::Ink *b = body(p, id);
  REQUIRE(b);
  return {b->box.x + b->box.w / 2, b->box.y + b->box.h / 2};
}
static bool samePlace(Vector2 a, Vector2 b) { return std::fabs(a.x - b.x) < 1 && std::fabs(a.y - b.y) < 1; }

static const char *kCurve = "schema axes x=[0,6] y=[0,9]\nmark f path fn={x^2/4} label=\"f\"\n";

TEST_CASE("a dot on=f x=2 sits at (2, f(2)); morphing its x walks it along the curve; {q.y} speaks the height; nothing derived is stored") {
  Engine rider(instant()), typed(instant());
  played(rider, std::string(kCurve) + "mark q dot on=f x=2 label=\"Q\"\nSay: \"Q is at height {q.y}.\"\nSay: \"Q |walks|walks.\"\n"
                                      "@walks morph q.x -> 3 1s\nSay: \"Now {q.y}.\"\n");
  played(typed, std::string(kCurve) + "mark q dot x=3 y=2.25 label=\"Q\"\n"); // the same point, typed
  CHECK(samePlace(bodyCentre(rider, "q"), bodyCentre(typed, "q")));
  CHECK(rider.spokenLines() == std::vector<std::string>{"Q is at height 1.", "Q walks.", "Now 2.25."});
  CHECK(rider.readback(false).find("mark q dot on=f x=3 label=Q\n") != std::string::npos); // the relation and the free coordinate only
}

TEST_CASE("riders: a dot on a line at x=, a dot on a parametric path at t=, a vector whose tail rides the curve") {
  Engine rider(instant()), typed(instant());
  const char *world = "schema axes x=[0,6] y=[0,9]\nmark L line x1=0 y1=1 x2=6 y2=7\nmark o path xt={2*cos(t)+3} yt={sin(t)+4}\nmark f path fn={x^2/4}\n";
  played(rider, std::string(world) + "mark p dot on=L x=3\nmark r dot on=o t=1\nmark v vector on=f x=4 dx=1 dy=0.5\n");
  played(typed, std::string(world) + "mark p dot x=3 y=4\nmark r dot x={2*cos(1)+3} y={sin(1)+4}\nmark v vector x=4 y=4 dx=1 dy=0.5\n");
  CHECK(samePlace(bodyCentre(rider, "p"), bodyCentre(typed, "p")));
  CHECK(samePlace(bodyCentre(rider, "r"), bodyCentre(typed, "r")));
  place::Picture a = rider.picture(), b = typed.picture();
  const place::Ink *va = anyInk(a, "v"), *vb = anyInk(b, "v");
  REQUIRE((va && vb));
  CHECK(std::fabs(va->box.x - vb->box.x) < 1);
  CHECK(std::fabs(va->box.y - vb->box.y) < 1);
}

TEST_CASE("space: a dot on=S x=1 y=1 takes the surface's height; {p.z} says it, in the instant run and in the live shadow") {
  const char *bowl = "schema space x=[-3,3] y=[-3,3] z=[0,4]\nmark S surface fn={0.4*(x^2+y^2)} label=\"S\"\ncamera spin\n";
  Engine rider(instant()), typed(instant());
  played(rider, std::string(bowl) + "mark p dot on=S x=1 y=1 drop=true label=\"p\"\nSay: \"Height {p.z}.\"\n");
  played(typed, std::string(bowl) + "mark p dot x=1 y=1 z=0.8 drop=true label=\"p\"\n");
  CHECK(samePlace(bodyCentre(rider, "p"), bodyCentre(typed, "p")));
  CHECK(rider.spokenLines() == std::vector<std::string>{"Height 0.8."});
  EngineOptions o; // live: the shadow reads the value before the beat plays, with no layout
  o.quiet = true;
  Engine live(o);
  REQUIRE(live.loadText(std::string(bowl) + "mark p dot on=S x=1 y=1 label=\"p\"\n"));
  REQUIRE(live.appendText("Say: \"Height {p.z}.\"\n"));
  CHECK(live.spokenLines() == std::vector<std::string>{"Height 0.8."});
}

TEST_CASE("riders: what can't ride, a missing free coordinate and a morph of the derived one are said; a clean rider scene round-trips with no zero: or say: lint") {
  Engine e(instant());
  played(e, std::string(kCurve) + "mark k array items=[1,2] on=f x=1\nmark q dot on=f\nmark r dot on=f x=1 y=3\nmorph r.y -> 4 1s\n");
  CHECK(hasLint(e, "'k' on=f: only a dot, a label or a vector can ride"));
  CHECK(hasLint(e, "'q' on=f needs x="));
  CHECK(hasLint(e, "'r' on=f: y= is ignored"));
  CHECK(hasLint(e, "morph r.y"));
  Engine d(instant());
  played(d, std::string(kCurve) + "mark p dot on=f x=2 label=\"P\"\nmark q dot on=f x=4 label=\"Q\"\nSay: \"Q is at height {q.y}.\"\nmorph q.x -> 2.1 1s\n");
  CHECK(!hasLint(d, "zero:"));
  CHECK(!hasLint(d, "say:"));
  std::string rb = d.readback(false);
  Engine again(instant());
  played(again, rb);
  CHECK(again.readback(false) == rb);
}

TEST_CASE("the derivative example written with on=f puts P, Q and the secant where the version with the function typed twice did") {
  Engine rider(instant()), typed(instant());
  const char *head = "schema axes x=[0,6] y=[0,9]\nmark f path fn={x^2/4}\n";
  played(typed, std::string(head) + "mark h param value=2\nmark p dot point x=2 y={f(2)} label=\"P\"\nmark q dot point x={2+h} y={f(2+h)} label=\"Q\"\n"
                                    "mark sec line x1=2 y1={f(2)} x2={2+h} y2={f(2+h)} extend=1\nmorph h.value 2 -> 0.5 1s\n");
  played(rider, std::string(head) + "mark p dot point on=f x=2 label=\"P\"\nmark q dot point on=f x=4 label=\"Q\"\n"
                                    "mark sec line x1={p.x} y1={p.y} x2={q.x} y2={q.y} extend=1\nmorph q.x -> 2.5 1s\n");
  CHECK(samePlace(bodyCentre(rider, "p"), bodyCentre(typed, "p")));
  CHECK(samePlace(bodyCentre(rider, "q"), bodyCentre(typed, "q")));
  place::Picture a = rider.picture(), b = typed.picture();
  const place::Ink *sa = anyInk(a, "sec"), *sb = anyInk(b, "sec");
  REQUIRE((sa && sb));
  CHECK(std::fabs(sa->box.y - sb->box.y) < 1);
  CHECK(std::fabs(sa->box.h - sb->box.h) < 1);
}

TEST_CASE("a rider's trail is the curve it walked: the same stroke as the dot written with the function typed twice") {
  auto trailOf = [](const std::string &text) {
    EngineOptions o; // live: a trail is history over time
    o.quiet = true;
    Engine e(o);
    REQUIRE(e.loadText(text));
    for (int i = 0; i < 150; i++) e.update(1.f / 60); // 2.5 s: the 2 s walk and a little after
    place::Picture p = e.picture();
    const place::Ink *t = anyInk(p, "q~trail");
    REQUIRE(t);
    return t->line;
  };
  auto a = trailOf(std::string(kCurve) + "mark q dot on=f x=1 trail=all\nmorph q.x -> 5 2s\n");
  auto b = trailOf(std::string(kCurve) + "mark h param value=1\nmark q dot x={h} y={f(h)} trail=all\nmorph h.value -> 5 2s\n");
  REQUIRE(a.size() > 10);
  REQUIRE(a.size() == b.size());
  for (size_t i = 0; i < a.size(); i++) {
    CHECK(std::fabs(a[i].x - b[i].x) < 1);
    CHECK(std::fabs(a[i].y - b[i].y) < 1);
  }
}

// ---------------------------------------------------------------- cues belong to the sentence that says them

TEST_CASE("a cued line written one beat early waits for its word in the next sentence, and says so") {
  initFonts();
  const std::string text = "mark a bar score v=3\nSay: \"First, a long sentence that takes a while to say out loud.\"\n"
                           "@go morph a.v -> 6 1s\nSay: \"Second, after a good many words, it finally |go|doubles.\"\n";
  Engine e(live());
  REQUIRE(e.loadText(text));
  float firstEnd = -1;
  for (int i = 0; i < 60 * 30 && firstEnd < 0; i++) {
    e.update(1.f / 60);
    for (auto &ev : e.timeline) if (ev.find(" end ") != std::string::npos) firstEnd = std::stof(ev);
  }
  REQUIRE(firstEnd > 0);
  for (int i = 0; i < 90; i++) e.update(1.f / 60); // 1.5 s on: the morph would have landed had it run at the end
  CHECK(e.readback(false).find("v=3") != std::string::npos);
  runOut(e);
  CHECK(e.readback(false).find("v=6") != std::string::npos);
  Engine check(instant());
  played(check, text);
  bool linted = false;
  for (auto &w : check.lintMessages()) if (w.find("hook: @go") != std::string::npos) linted = true;
  CHECK(linted);
}

TEST_CASE("a cue spoken without its own line never fires a later sentence's line early") {
  initFonts();
  Engine e(live());
  REQUIRE(e.loadText("mark a bar score v=3\nmark b bar score v=5\nSay: \"This |go|sentence has no line for its cue at all.\"\n"
                     "Say: \"And this one, after a good many words, finally |go|changes b.\"\n@go morph b.v -> 1 1s\n"));
  float firstEnd = -1;
  for (int i = 0; i < 60 * 30 && firstEnd < 0; i++) {
    e.update(1.f / 60);
    for (auto &ev : e.timeline) if (ev.find(" end ") != std::string::npos) firstEnd = std::stof(ev);
  }
  CHECK(e.readback(false).find("mark b bar score v=5") != std::string::npos);
  runOut(e);
  CHECK(e.readback(false).find("v=1") != std::string::npos);
}

TEST_CASE("a curve morphed to another function blends between them; it never vanishes on the way") {
  initFonts();
  Engine e(live());
  REQUIRE(e.loadText("schema axes x=[0,3] y=[0,10]\nmark f path fn={x^2} label=\"f\"\nwait 0.1s\nmorph f.fn -> {x^3} 2s\n"));
  run(e, 1.1); // halfway through the morph
  double mid = e.curveAt("f", 2);
  CHECK(std::isfinite(mid));
  CHECK(mid > 4.2);
  CHECK(mid < 7.8);
  runOut(e);
  CHECK(e.curveAt("f", 2) == doctest::Approx(8));
  CHECK(e.readback(false).find("fn={x^3}") != std::string::npos);
}
