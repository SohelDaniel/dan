// Unit tests for the markup parser (markup/script.cpp).
//   make test
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
#include "../markup/script.hpp"
#include <algorithm>

using namespace mk;

static const SayWord *wordWithHook(const Line &say, const std::string &h) {
  for (auto &w : say.words)
    if (std::find(w.hooks.begin(), w.hooks.end(), h) != w.hooks.end()) return &w;
  return nullptr;
}

TEST_CASE("a cue spoken with |bars| stays on its word") {
  Script s;
  REQUIRE(s.parse("Say: \"it |grow|grows fast\"\n@grow mark a box"));
  const SayWord *w = wordWithHook(s.steps[0].main, "grow");
  REQUIRE(w);
  CHECK(w->text == "grows");
}

TEST_CASE("a tagged line whose cue is never spoken glues to the word it names") {
  Script s;
  REQUIRE(s.parse("Say: \"The model scores each answer, then we go on.\"\n@score indicate a\n@go mark b box\n@zebra mark c box"));
  const Line &say = s.steps[0].main;
  REQUIRE(wordWithHook(say, "score"));
  CHECK(wordWithHook(say, "score")->text == "scores");
  REQUIRE(wordWithHook(say, "go"));
  CHECK(wordWithHook(say, "go")->text == "go");
  CHECK(!wordWithHook(say, "zebra")); // nothing like it: it runs when the sentence ends
}

TEST_CASE("cue gluing: two-letter cues match whole words only; the sentence is the last Say") {
  Script s;
  REQUIRE(s.parse("Say: \"Gone now.\"\nSay: \"Higher still.\"\n@go mark a box\n@high mark b box"));
  CHECK(!wordWithHook(s.steps[0].main, "go"));
  CHECK(!wordWithHook(s.steps[1].main, "go"));
  CHECK(wordWithHook(s.steps[1].main, "high"));
}

TEST_CASE("cue gluing across appends: the tag can arrive in the next chunk") {
  Script whole, parts;
  const char *a = "Say: \"Watch it double.\"\n", *b = "@double mark x box\n";
  REQUIRE(whole.parse(std::string(a) + b));
  REQUIRE(parts.parse(a));
  REQUIRE(parts.append(b));
  CHECK(wordWithHook(whole.steps[0].main, "double"));
  CHECK(wordWithHook(parts.steps[0].main, "double"));
}

TEST_CASE("lists: quotes protect commas inside an item") {
  Script s;
  REQUIRE(s.parse("mark t table rows=[[\"1,000\", b]]"));
  const Value *v = findProp(s.steps[0].main.props, "rows");
  REQUIRE(v);
  REQUIRE(v->type == Value::LIST);
  Value row = Value::parse(v->list[0]);
  REQUIRE(row.type == Value::LIST);
  CHECK(row.list.size() == 2);
  CHECK(row.list[0] == "1,000");
}

TEST_CASE("{expr} in a Say: one word however it is spaced; a plain {id} points at its thing") {
  Script s;
  REQUIRE(s.parse("Say: \"Together |sum|{a + b}, and a is {a.value}.\"\n"));
  const auto &w = s.steps[0].main.words;
  REQUIRE(w.size() == 6);
  CHECK(w[1].text == "{a + b},");
  CHECK(w[1].ref);
  CHECK(std::find(w[1].hooks.begin(), w[1].hooks.end(), "sum") != w[1].hooks.end());
  CHECK(std::none_of(w[1].hooks.begin(), w[1].hooks.end(), [](const std::string &h) { return h.rfind("@pt:", 0) == 0; }));
  CHECK(w[5].text == "{a.value}.");
  CHECK(std::find(w[5].hooks.begin(), w[5].hooks.end(), "@pt:a") != w[5].hooks.end());
  CHECK(!w[0].ref);
}

TEST_CASE("stagger= on indicate and send: a duration, anything else is an error") {
  Script s;
  REQUIRE(s.parse("indicate a[0:4] stagger=0.45s\nsend a[0:2] -> t stagger=300ms 0.8s\n"));
  CHECK(s.steps[0].main.stagger == doctest::Approx(0.45));
  CHECK(s.steps[1].main.stagger == doctest::Approx(0.3));
  CHECK(s.steps[1].main.dur == doctest::Approx(0.8));
  Script bad;
  CHECK(!bad.parse("indicate a[0:4] stagger=fast\n"));
}

TEST_CASE("a list reads back whole: blank items and items with commas are quoted") {
  Script s;
  REQUIRE(s.parse("mark t array items=[,\" \",\"cat, dog\",x]\n"));
  const Value *v = findProp(s.steps[0].main.props, "items");
  REQUIRE(v);
  Script again;
  REQUIRE(again.parse("mark t array items=" + v->markup() + "\n"));
  CHECK(findProp(again.steps[0].main.props, "items")->list == v->list);
}

TEST_CASE("a path, line or arrow from= one thing to= another is a link between them") {
  Script s;
  REQUIRE(s.parse("mark p path from=a to=b label=\"route\"\nmark w line from=a to=b\nmark r arrow from=a to=b\n"));
  const Line &p = s.steps[0].main, &w = s.steps[1].main, &r = s.steps[2].main;
  CHECK(p.verb == Verb::LINK);
  CHECK(p.id == "p");
  CHECK(p.kind == "arrow"); // a path has a start and an end
  CHECK((p.a == "a" && p.b == "b" && p.directed));
  CHECK(findProp(p.props, "label"));
  CHECK(!findProp(p.props, "from")); // the ends are the link's own, not properties
  CHECK(!findProp(p.props, "to"));
  CHECK((w.verb == Verb::LINK && w.kind == "line" && !w.directed)); // a line has no direction
  CHECK((r.verb == Verb::LINK && r.kind == "arrow" && r.directed));
}

TEST_CASE("path as a link's kind word is the plain link for its arrow") {
  Script s;
  REQUIRE(s.parse("mark p path a -> b label=\"route\"\nlink q path a -- b\n"));
  CHECK((s.steps[0].main.verb == Verb::LINK && s.steps[0].main.id == "p" && s.steps[0].main.kind == "arrow"));
  CHECK((s.steps[1].main.id == "q" && s.steps[1].main.kind == "line"));
}

TEST_CASE("a path of its own is still a curve: fn=, xt= yt=, numbers, one end, its own geometry") {
  Script s;
  REQUIRE(s.parse("mark f path fn={x^2} label=\"f\"\nmark o path xt={cos(t)} yt={sin(t)} range=[0,6.3]\n"
                  "mark g path fn={x} from=0 to=2\nmark h path from=a\nmark L line x1=0 y1=0 x2=1 y2=1 from=a to=b\n"
                  "link path a -> b\n"));
  for (int i = 0; i < 5; i++) CHECK_MESSAGE(s.steps[i].main.verb == Verb::MARK, "line " << i + 1);
  CHECK(s.steps[5].main.id == "path"); // a lone word before the ends is the link's id, as before
}
