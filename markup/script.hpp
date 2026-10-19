// Parser for the explanation markup.
//
//   mark  <id> [kind] [category] key=value ...        thing + properties
//   link  <id> [kind] <a> -> <b> key=value ...        relation (connected, cause, maps-to)
//   group <id> part-of=[a,b,...] key=value ...         relation (part-of)
//   morph <id>.<prop> [from] -> <to> [1.5s] [ease=…]   change
//   focus <id> | none                                  the path (bounded focus)
//   Say: "text with |hook| markers"                    the voice
//   with <line>     runs alongside the previous line
//   @name <line>    deferred until the voice reaches |name|
//
// Setup lines (not part of the model): title "…", schema <free|numberline|axes|timeline|graph> …,
// wait 1s, pause.  Comments start with # or //.
#pragma once
#include "expr.hpp"
#include <deque>
#include <map>
#include <string>
#include <vector>

namespace mk {

struct Value {
  enum Type { NUM, STR, EXPR, LIST } type = NUM;
  double num = 0;
  std::string str;              // STR text, or EXPR source
  ExprPtr expr;
  std::vector<std::string> list;

  static Value number(double v) { Value x; x.type = NUM; x.num = v; return x; }
  static Value string(const std::string &s) { Value x; x.type = STR; x.str = s; return x; }
  static Value parse(const std::string &tok, std::string *err = nullptr);
  bool isNumeric() const { return type == NUM || type == EXPR; }
  std::string markup() const;   // how it is written back in readback
};

struct Prop {
  std::string key;
  Value val;
};
using Props = std::vector<Prop>;
const Value *findProp(const Props &p, const std::string &k);
Value *findProp(Props &p, const std::string &k);
void setProp(Props &p, const std::string &k, const Value &v);

enum class Verb { MARK, LINK, GROUP, MORPH, FOCUS, SAY, WAIT, PAUSE, SCHEMA, TITLE, SCENE, INDICATE };

struct SayWord {
  std::string text;             // as displayed
  std::vector<std::string> hooks; // fire when this word is reached
};

struct Line {
  Verb verb = Verb::MARK;
  int lineNo = 0;
  int uid = 0;                  // unique within a Script, stable across appends
  std::string raw;
  // mark / link / group / schema / focus
  std::string id, kind, category;
  Props props;
  // link
  std::string a, b;
  bool directed = true;
  // group
  std::vector<std::string> members;
  // morph
  std::string prop;
  Value from, to;
  bool hasFrom = false;
  float dur = 1.0f;
  std::string ease = "smooth";
  // say
  std::vector<SayWord> words;
  std::vector<std::string> trailingHooks; // markers after the last word
  // modifiers
  bool with = false;
  std::string deferred;         // @name
};

struct Step {
  Line main;
  std::vector<Line> withs;      // run alongside main
  std::string hook;             // non-empty: only fires when the voice reaches |hook|
};

struct Script {
  std::deque<Step> steps;       // a deque: appending never moves existing steps
  std::vector<std::string> errors;
  bool parse(const std::string &text);
  // continues the script (live/streamed use): line numbers carry on, a leading `with`
  // joins the last existing step. Returns false if this chunk had errors.
  bool append(const std::string &text);
private:
  int lines_ = 0, nextUid_ = 0;
};

bool isMarkKind(const std::string &k);
bool isLinkKind(const std::string &k);
std::vector<std::string> tokenize(const std::string &line, std::string &err);

} // namespace mk
