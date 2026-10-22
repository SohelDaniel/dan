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
