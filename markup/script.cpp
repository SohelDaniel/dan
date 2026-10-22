#include "script.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <regex>
#include <sstream>

namespace mk {

static std::string trim(const std::string &s) {
  size_t a = 0, b = s.size();
  while (a < b && std::isspace((unsigned char)s[a])) a++;
  while (b > a && std::isspace((unsigned char)s[b - 1])) b--;
  return s.substr(a, b - a);
}
static std::string lower(std::string s) {
  for (auto &c : s) c = (char)std::tolower((unsigned char)c);
  return s;
}
static bool startsWith(const std::string &s, const std::string &p) { return s.rfind(p, 0) == 0; }

static std::string stripQuotes(const std::string &s) {
  if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\'')))
    return s.substr(1, s.size() - 2);
  // smart quotes (3 bytes each in UTF-8)
  if (startsWith(s, "\xE2\x80\x9C") && s.size() >= 6 && s.compare(s.size() - 3, 3, "\xE2\x80\x9D") == 0)
    return s.substr(3, s.size() - 6);
  return s;
}

static bool parseNumber(const std::string &s, double &out) {
  if (s.empty()) return false;
  char *end = nullptr;
  out = std::strtod(s.c_str(), &end);
  return end && *end == 0 && std::isfinite(out);
}

bool isMarkKind(const std::string &k) {
  static const char *kinds[] = {"dot",   "bar",  "box",  "line",    "arrow", "label", "path",  "region", "tex",
                                "circle", "rect", "polygon", "angle", "vector", "card", "code", "array", "table", "param", "grid",
                                "field", "slope", "heat", "contour", "solution", "brace", "enclose", "underline", "cross", "check", "area"};
  for (auto c : kinds) if (k == c) return true;
  return false;
}
bool isLinkKind(const std::string &k) {
  static const char *kinds[] = {"line", "arrow", "similar", "equals", "cause", "maps-to", "connected"};
  for (auto c : kinds) if (k == c) return true;
  return false;
}

// ------------------------------------------------------------------ Value
Value Value::parse(const std::string &tokIn, std::string *err) {
  std::string tok = trim(tokIn);
  Value v;
  if (tok.size() >= 2 && tok.front() == '{' && tok.back() == '}') {
    v.type = EXPR;
    v.str = trim(tok.substr(1, tok.size() - 2));
    std::string e;
    v.expr = parseExpr(v.str, e);
    if (!e.empty() && err) *err = "expression {" + v.str + "}: " + e;
    return v;
  }
  if (tok.size() >= 2 && tok.front() == '[' && tok.back() == ']') {
    v.type = LIST;
    std::string inner = tok.substr(1, tok.size() - 2), cur;
    int depth = 0;
    char quote = 0; // a comma inside quotes is text: "1,000"
    for (char c : inner) {
      if (quote) { if (c == quote) quote = 0; cur += c; continue; }
      if (c == '"' || c == '\'') quote = c;
      if (c == '{' || c == '(' || c == '[') depth++; // nested lists stay whole: by=[[1,0],[10,989]]
      if (c == '}' || c == ')' || c == ']') depth--;
      if (c == ',' && depth == 0) { if (!trim(cur).empty()) v.list.push_back(stripQuotes(trim(cur))); cur.clear(); }
      else cur += c;
    }
    if (!trim(cur).empty()) v.list.push_back(stripQuotes(trim(cur)));
    return v;
  }
  if (tok.size() >= 2 && (tok.front() == '"' || tok.front() == '\'')) return string(stripQuotes(tok));
  double d;
  if (parseNumber(tok, d)) return number(d);
  if (tok == "true" || tok == "yes") return number(1);
  if (tok == "false" || tok == "no") return number(0);
  return string(tok);
}

std::string Value::markup() const {
  switch (type) {
  case NUM: {
    std::ostringstream o;
    if (std::fabs(num - std::round(num)) < 1e-9) o << (long long)std::llround(num);
    else { o.precision(6); o << num; }
    return o.str();
  }
  case STR: {
    bool plain = !str.empty();
    for (char c : str)
      if (std::isspace((unsigned char)c) || c == '"' || c == '=' || c == '#' || c == '{' || c == '}' || c == '[' ||
          c == ']' || c == '\\')
        plain = false;
    double d;
    if (parseNumber(str, d)) plain = false;
    return plain ? str : "\"" + str + "\"";
  }
  case EXPR: return "{" + str + "}";
  case LIST: {
    std::string s = "[";
    for (size_t i = 0; i < list.size(); i++) { // quoted when it wouldn't read back whole: a comma in it, or a blank
      const std::string &it = list[i];
      bool nested = !it.empty() && it.front() == '[';
      bool blank = it.empty() || std::isspace((unsigned char)it.front()) || std::isspace((unsigned char)it.back());
      s += (i ? ", " : "") + (!nested && (blank || it.find(',') != std::string::npos) ? "\"" + it + "\"" : it);
    }
    return s + "]";
  }
  }
  return "";
}

const Value *findProp(const Props &p, const std::string &k) {
  for (auto &x : p) if (x.key == k) return &x.val;
  return nullptr;
}
Value *findProp(Props &p, const std::string &k) {
  for (auto &x : p) if (x.key == k) return &x.val;
  return nullptr;
}
void setProp(Props &p, const std::string &k, const Value &v) {
  if (auto x = findProp(p, k)) *x = v;
  else p.push_back({k, v});
}

// ------------------------------------------------------------------ tokenizer
std::vector<std::string> tokenize(const std::string &line, std::string &err) {
  std::vector<std::string> out;
  std::string cur;
  int depth = 0;
  bool inQ = false;
  for (size_t i = 0; i < line.size(); i++) {
    char c = line[i];
    if (inQ) { cur += c; if (c == '"') inQ = false; continue; }
    if (c == '"') { inQ = true; cur += c; continue; }
    if (c == '{' || c == '[' || c == '(') depth++;
    if (c == '}' || c == ']' || c == ')') depth--;
    if (depth == 0 && cur.empty() && c == '#') break; // trailing comment
    if (depth == 0 && std::isspace((unsigned char)c)) {
      if (!cur.empty()) out.push_back(cur), cur.clear();
      continue;
    }
    cur += c;
  }
  if (inQ) err = "unterminated quote";
  else if (depth != 0) err = "unbalanced brackets";
  if (!cur.empty()) out.push_back(cur);
  return out;
}

static bool isDuration(const std::string &t, float &out) {
  if (t.size() < 2) return false;
  double d;
  if (t.size() > 2 && t.compare(t.size() - 2, 2, "ms") == 0 && parseNumber(t.substr(0, t.size() - 2), d)) {
    out = (float)(d / 1000.0);
    return true;
  }
  if (t.back() == 's' && parseNumber(t.substr(0, t.size() - 1), d)) { out = (float)d; return true; }
  return false;
}
static bool isArrowTok(const std::string &t) { return t == "->" || t == "→" || t == "--" || t == "<->" || t == "—"; }
static bool splitKV(const std::string &t, std::string &k, std::string &v) {
  size_t eq = t.find('=');
  if (eq == std::string::npos || eq == 0) return false;
  // a '=' inside braces/quotes before any key chars doesn't count
  for (size_t i = 0; i < eq; i++)
    if (!(std::isalnum((unsigned char)t[i]) || t[i] == '_' || t[i] == '-' || t[i] == '.')) return false;
  k = t.substr(0, eq);
  v = t.substr(eq + 1);
  return true;
}

// cue names match case-insensitively: a sentence may start with |Level| for @level
static std::string cueName(std::string s) {
  for (auto &c : s) c = (char)std::tolower((unsigned char)c);
  return s;
}

// ------------------------------------------------------------------ Say
static void parseSay(const std::string &body, Line &ln) {
  std::string text = stripQuotes(trim(body));
  // {expr} speaks a value off the stage: its spaces don't split words ({a + b} is one word)
  for (size_t i = 0, depth = 0; i < text.size(); i++) {
    if (text[i] == '{') depth++;
    else if (text[i] == '}' && depth) depth--;
    else if (text[i] == ' ' && depth) text[i] = '\x01';
  }
  std::istringstream in(text);
  std::string w;
  std::vector<std::string> pending; // silent markers waiting for the next word
  while (in >> w) {
    std::string clean;
    std::vector<std::string> hooks;
    size_t i = 0;
    while (i < w.size()) {
      if (w[i] == '[') { // [id] or [a[3]]: point at that thing on this word
        size_t j = i + 1;
        while (j < w.size() && (std::isalnum((unsigned char)w[j]) || w[j] == '_')) j++;
        if (j < w.size() && w[j] == '[') { // a cell: a[3]
          size_t k = w.find(']', j);
          if (k != std::string::npos) j = k + 1;
        }
        if (j > i + 1 && j < w.size() && w[j] == ']' && std::isalpha((unsigned char)w[i + 1])) {
          hooks.push_back("@pt:" + w.substr(i + 1, j - i - 1));
          i = j + 1;
          continue;
        }
      }
      if (w[i] == '|') {
        size_t j = w.find('|', i + 1);
        if (j == std::string::npos) { clean += w.substr(i); break; }
        hooks.push_back(cueName(w.substr(i + 1, j - i - 1)));
        i = j + 1;
      } else {
        clean += w[i++];
      }
    }
    bool hasWord = std::any_of(clean.begin(), clean.end(), [](char c) { return std::isalnum((unsigned char)c) || (c & 0x80); });
    if (!hasWord) {
      // a bare marker like |go| or |go|. : silent, fires where it sits
      if (!clean.empty() && !ln.words.empty()) ln.words.back().text += clean;
      for (auto &h : hooks) pending.push_back(h);
      continue;
    }
    std::replace(clean.begin(), clean.end(), '\x01', ' ');
    SayWord sw;
    sw.text = clean;
    sw.hooks = pending;
    pending.clear();
    // thirty|go| : marker glued to a word fires when that word is spoken
    for (auto &h : hooks) sw.hooks.push_back(h);
    // {id} or {id.prop}: the value is spoken and its thing is pointed at on that word, as [id] does
    for (size_t a = clean.find('{'); a != std::string::npos; a = clean.find('{', a + 1)) {
      sw.ref = true;
      size_t b = clean.find('}', a);
      if (b == std::string::npos) break;
      std::string inner = trim(clean.substr(a + 1, b - a - 1));
      std::string id = inner.substr(0, inner.find('.'));
      bool plain = !id.empty() && std::isalpha((unsigned char)id[0]) &&
                   std::all_of(inner.begin(), inner.end(), [](char c) { return std::isalnum((unsigned char)c) || c == '_' || c == '.'; });
      static const std::regex cell(R"(^[A-Za-z_]\w*(\[\d+\])+$)"); // a cell points at that cell
      std::string target = plain ? id : std::regex_match(inner, cell) ? inner : "";
      if (!target.empty() && std::find(sw.hooks.begin(), sw.hooks.end(), "@pt:" + target) == sw.hooks.end())
        sw.hooks.push_back("@pt:" + target);
    }
    ln.words.push_back(sw);
  }
  ln.trailingHooks = pending;
}

// ------------------------------------------------------------------ lines
// ---- one reader per verb: each gets the line's tokens (the verb first) and fills the Line
using Toks = std::vector<std::string>;
struct Reading {
  const Toks &toks;
  const std::string &verb, &raw; // the verb (lower case, aliases resolved) and the whole line
  Line &ln;
  std::string &err;
  bool fail(const std::string &why) { err = why; return false; }
  // key=value tokens from `from` on, into props
  bool props(size_t from, Props &into) {
    for (size_t i = from; i < toks.size(); i++) {
      std::string k, v, verr;
      if (!splitKV(toks[i], k, v)) return fail("expected key=value, got '" + toks[i] + "'");
      setProp(into, k, Value::parse(v, &verr));
      if (!verr.empty()) return fail(verr);
    }
    return true;
  }
};

static bool readMark(Reading &r) {
  auto &[toks, verb, raw, ln, err] = r;
  ln.verb = Verb::MARK;
  if (toks.size() < 2) return r.fail("mark needs an id");
  ln.id = toks[1];
  for (size_t i = 2; i < toks.size(); i++) { // key=value, a kind and a category, in any order
    std::string k, v;
    if (splitKV(toks[i], k, v)) {
      std::string verr;
      setProp(ln.props, k, Value::parse(v, &verr));
      if (!verr.empty()) return r.fail(verr);
    } else if (isMarkKind(lower(toks[i]))) {
      if (!ln.kind.empty()) return r.fail("kind given twice ('" + ln.kind + "' and '" + toks[i] + "')");
      ln.kind = lower(toks[i]);
    } else if (ln.category.empty()) {
      ln.category = stripQuotes(toks[i]);
    } else {
      return r.fail("unexpected '" + toks[i] + "' (one kind and one category per mark)");
    }
  }
  return true;
}

static bool readLink(Reading &r) {
  auto &[toks, verb, raw, ln, err] = r;
  ln.verb = Verb::LINK;
  size_t ar = 0;
  for (size_t i = 1; i < toks.size(); i++) if (isArrowTok(toks[i])) { ar = i; break; }
  if (ar < 2 || ar + 1 >= toks.size()) return r.fail("link needs 'a -> b' (or 'a -- b')");
  ln.a = toks[ar - 1];
  ln.b = toks[ar + 1];
  ln.directed = toks[ar] == "->" || toks[ar] == "→";
  std::vector<std::string> head(toks.begin() + 1, toks.begin() + (long)ar - 1);
  for (auto &h : head) {
    if (ln.id.empty() && !(head.size() == 1 && isLinkKind(lower(h)))) ln.id = h;
    else if (ln.kind.empty() && isLinkKind(lower(h))) ln.kind = lower(h);
    else return r.fail("unexpected '" + h + "' in link");
  }
  if (ln.kind == "cause" || ln.kind == "maps-to") ln.kind = "arrow";
  if (ln.kind == "connected") ln.kind = "line";
  if (ln.kind.empty()) ln.kind = ln.directed ? "arrow" : "line";
  if (ln.id.empty()) ln.id = ln.a + (ln.directed ? "->" : "--") + ln.b;
  return r.props(ar + 2, ln.props);
}

static bool readGroup(Reading &r) {
  auto &[toks, verb, raw, ln, err] = r;
  ln.verb = Verb::GROUP;
  if (toks.size() < 2) return r.fail("group needs an id");
  ln.id = toks[1];
  for (size_t i = 2; i < toks.size(); i++) {
    std::string k, v;
    if (splitKV(toks[i], k, v)) {
      Value val = Value::parse(v);
      if (k == "part-of" || k == "members" || k == "of" || k == "parts") ln.members = val.type == Value::LIST ? val.list : std::vector<std::string>{val.str};
      else setProp(ln.props, k, val);
    } else if (toks[i].front() == '[') {
      ln.members = Value::parse(toks[i]).list;
    } else {
      ln.members.push_back(toks[i]);
    }
  }
  if (ln.members.empty()) return r.fail("group needs part-of=[a,b,...]");
  return true;
}

static bool readMorph(Reading &r) {
  auto &[toks, verb, raw, ln, err] = r;
  ln.verb = Verb::MORPH;
  if (toks.size() < 3) return r.fail("morph needs id.prop and a target value");
  size_t dot = toks[1].find('.');
  if (dot == std::string::npos) return r.fail("morph target must be id.property");
  ln.id = toks[1].substr(0, dot);
  ln.prop = toks[1].substr(dot + 1);
  std::vector<std::string> vals;
  for (size_t i = 2; i < toks.size(); i++) {
    std::string k, v;
    float d;
    if (isArrowTok(toks[i])) continue;
    if (isDuration(toks[i], d)) { ln.dur = d; continue; }
    if (splitKV(toks[i], k, v) && toks[i].front() != '{' && toks[i].front() != '"') {
      if (k == "ease") ln.ease = v;
      else if (k == "dur" || k == "duration") { double x; if (parseNumber(v, x)) ln.dur = (float)x; }
      else return r.fail("unknown morph option '" + k + "'");
      continue;
    }
    vals.push_back(toks[i]);
  }
  std::string verr;
  if (vals.size() == 2) {
    ln.from = Value::parse(vals[0], &verr);
    ln.to = Value::parse(vals[1], &verr);
    ln.hasFrom = true;
  } else if (vals.size() == 1) {
    ln.to = Value::parse(vals[0], &verr);
  } else {
    return r.fail("morph wants '[from] -> to [duration]'");
  }
  return verr.empty() || r.fail(verr);
}

static bool readFocus(Reading &r) {
  Line &ln = r.ln;
  ln.verb = Verb::FOCUS;
  ln.id = r.toks.size() > 1 ? r.toks[1] : "none";
  size_t dot = ln.id.find('.');
  if (dot != std::string::npos) { ln.prop = ln.id.substr(dot + 1); ln.id = ln.id.substr(0, dot); }
  if (ln.id == "all" || ln.id == "nothing") ln.id = "none";
  return true;
}

// pointing: a pulse and a ring on a thing (or one cell: a[3]); transient, not part of the model
// indicate a[2] | indicate a[0:4] stagger=0.3s: the cells pulse one after another (counting along them)
static bool readIndicate(Reading &r) {
  r.ln.verb = Verb::INDICATE;
  if (r.toks.size() < 2) return r.fail(r.verb + " needs a thing");
  r.ln.id = r.toks[1];
  for (size_t i = 2; i < r.toks.size(); i++)
    if (r.toks[i].rfind("stagger=", 0) == 0 && !isDuration(r.toks[i].substr(8), r.ln.stagger))
      return r.fail("indicate: stagger takes a duration (stagger=0.3s)");
  return true;
}

// send a[2] -> f[x] [1s] [via=l1] [stagger=0.3s]: a copy of a thing (a cell, a formula part) travels to another and
// lands there (3Blue1Brown's TransformFromCopy); via= takes it along a link. With stagger=, each cell of a span
// goes in turn (to the same target, or cell by cell to a span as long). Like indicate, it isn't part of the model.
static bool readSend(Reading &r) {
  auto &[toks, verb, raw, ln, err] = r;
  ln.verb = Verb::SEND;
  if (toks.size() < 4 || toks[2] != "->") return r.fail("send needs a source and a target: send a[2] -> f[x]");
  ln.a = toks[1];
  ln.b = toks[3];
  ln.dur = 1.0f;
  for (size_t i = 4; i < toks.size(); i++) {
    float d;
    if (isDuration(toks[i], d)) ln.dur = d;
    else if (toks[i].rfind("via=", 0) == 0) ln.id = toks[i].substr(4);
    else if (toks[i].rfind("stagger=", 0) == 0 && isDuration(toks[i].substr(8), ln.stagger)) continue;
    else return r.fail("send: unexpected '" + toks[i] + "' (a duration like 1s, via=<link>, or stagger=0.3s)");
  }
  return true;
}

// hide / show: fade a thing to a ghost and back. It stays on stage (identity); this is a morph of opacity.
static bool readHideShow(Reading &r) {
  auto &[toks, verb, raw, ln, err] = r;
  ln.verb = Verb::MORPH;
  if (toks.size() < 2) return r.fail(verb + " needs a thing");
  ln.id = toks[1];
  ln.prop = "opacity";
  ln.to = Value::number(verb == "hide" ? 0.12 : 1);
  ln.dur = 0.6f;
  for (size_t i = 2; i < toks.size(); i++) {
    float d;
    if (isDuration(toks[i], d)) ln.dur = d;
  }
  return true;
}

static bool readWait(Reading &r) {
  r.ln.verb = Verb::WAIT;
  r.ln.dur = 1;
  if (r.toks.size() > 1) {
    float d;
    double x;
    if (isDuration(r.toks[1], d)) r.ln.dur = d;
    else if (parseNumber(r.toks[1], x)) r.ln.dur = (float)x;
  }
  return true;
}

static bool readPause(Reading &r) { r.ln.verb = Verb::PAUSE; return true; }

// schema axes x=[0,5]          the scene's one schema
// schema net graph label="…"     a named view: the marks after it are shown together in their own part of
// schema net                     the stage (two views of one thing side by side); a bare name selects it again
static bool readSchema(Reading &r) {
  auto &[toks, verb, raw, ln, err] = r;
  ln.verb = Verb::SCHEMA;
  if (toks.size() < 2) return r.fail("schema needs a name");
  static const char *ok[] = {"free", "numberline", "axes", "plane", "timeline", "graph", "sequence", "auto"};
  auto schemaName = [&](std::string k) {
    k = lower(k);
    if (k == "number-line") k = "numberline";
    return std::any_of(std::begin(ok), std::end(ok), [&](const char *s) { return k == s; }) ? k : std::string();
  };
  const std::string names = " (free, numberline, axes, plane, timeline, graph, sequence)";
  size_t rest = 2;
  if (std::string k = schemaName(toks[1]); !k.empty()) ln.kind = k;
  else if (toks[1].find('=') != std::string::npos) return r.fail("unknown schema '" + toks[1] + "'" + names);
  else { // a view
    ln.id = toks[1];
    if (toks.size() > 2 && !schemaName(toks[2]).empty()) { ln.kind = schemaName(toks[2]); rest = 3; }
    else if (toks.size() > 2 && toks[2].find('=') == std::string::npos)
      return r.fail("unknown schema '" + toks[2] + "' for view '" + ln.id + "'" + names);
  }
  return r.props(rest, ln.props);
}

// the camera (axes and plane): say what to look at, never where on screen
//   camera zoom=8 on=p | camera on=f x=1 zoom=20 | camera fit=a,b | camera follow=p | camera reset
static bool readCamera(Reading &r) {
  auto &[toks, verb, raw, ln, err] = r;
  ln.verb = Verb::CAMERA;
  ln.dur = -1; // automatic: from the length of the move
  for (size_t i = 1; i < toks.size(); i++) {
    std::string k, v;
    float d;
    if (isDuration(toks[i], d)) { ln.dur = d; continue; }
    if (splitKV(toks[i], k, v)) {
      if (k == "ease") { ln.ease = v; continue; }
      if (k == "dur" || k == "duration") { double x; if (parseNumber(v, x)) ln.dur = (float)x; continue; }
      static const char *keys[] = {"zoom", "on", "x", "y", "fit", "follow"};
      if (std::none_of(std::begin(keys), std::end(keys), [&](const char *s) { return k == s; }))
        return r.fail("unknown camera option '" + k + "' (zoom= on= x= y= fit= follow=, or camera fit / camera reset)");
      Value val = Value::parse(v);
      if (k == "fit" && val.type != Value::LIST) { // fit=a,b
        val = Value{};
        val.type = Value::LIST;
        std::stringstream ss(v);
        for (std::string id; std::getline(ss, id, ',');) if (!trim(id).empty()) val.list.push_back(trim(id));
      }
      setProp(ln.props, k, val);
      continue;
    }
    std::string w = lower(toks[i]);
    if (w == "fit" || w == "reset" || w == "home") { ln.kind = w == "home" ? "reset" : w; continue; }
    return r.fail("camera: unexpected '" + toks[i] + "'");
  }
  if (ln.kind.empty() && ln.props.empty()) ln.kind = "reset";
  return true;
}

static bool readSceneTitle(Reading &r) {
  auto &[toks, verb, raw, ln, err] = r;
  ln.verb = verb == "scene" ? Verb::SCENE : Verb::TITLE;
  // the quoted text if quoted (so a trailing # comment is dropped), else the rest of the line
  if (toks.size() > 1 && toks[1].front() == '"') ln.id = stripQuotes(toks[1]);
  else {
    std::string rest;
    for (size_t i = 1; i < toks.size(); i++) rest += (i > 1 ? " " : "") + toks[i];
    ln.id = toks.size() > 1 ? rest : trim(raw.substr(5));
  }
  return true;
}

static bool parseLine(const std::string &rawIn, int lineNo, Line &ln, std::string &err) {
  std::string raw = trim(rawIn);
  ln.lineNo = lineNo;
  ln.raw = raw;

  // modifiers: @name and with, in either order
  for (;;) {
    if (startsWith(raw, "@")) {
      size_t sp = raw.find_first_of(" \t");
      if (sp == std::string::npos) { err = "@hook needs a line after it"; return false; }
      ln.deferred = cueName(raw.substr(1, sp - 1));
      raw = trim(raw.substr(sp));
    } else if (startsWith(lower(raw), "with ")) {
      ln.with = true;
      raw = trim(raw.substr(5));
    } else {
      break;
    }
  }

  std::string lo = lower(raw);
  if (startsWith(lo, "say:") || startsWith(lo, "say ")) {
    ln.verb = Verb::SAY;
    parseSay(raw.substr(4), ln);
    if (ln.words.empty()) { err = "Say has no words"; return false; }
    return true;
  }

  auto toks = tokenize(raw, err);
  if (!err.empty()) return false;
  if (toks.empty()) { err = "empty line"; return false; }
  std::string verb = lower(toks[0]);
  // "mark w arrow a -> b ..." means a link (a relation between two things): read it as one
  if (verb == "mark" && std::any_of(toks.begin() + 1, toks.end(), [](const std::string &t) { return isArrowTok(t); })) verb = "link";
  if (verb == "re-mark" || verb == "remark") verb = "mark"; // a re-mark is just a mark of an existing id
  static const std::map<std::string, bool (*)(Reading &)> readers = {
      {"mark", readMark},   {"link", readLink},     {"group", readGroup},       {"morph", readMorph},
      {"focus", readFocus}, {"indicate", readIndicate}, {"point", readIndicate}, {"send", readSend},
      {"hide", readHideShow}, {"show", readHideShow}, {"wait", readWait},     {"pause", readPause},
      {"schema", readSchema}, {"camera", readCamera}, {"scene", readSceneTitle}, {"title", readSceneTitle}};
  auto it = readers.find(verb);
  if (it == readers.end()) { err = "unknown verb '" + toks[0] + "'"; return false; }
  Reading r{toks, verb, raw, ln, err};
  return it->second(r);
}

// A line tagged @score whose sentence never speaks |score| would wait for the sentence to end; glue the
// cue to the word it names instead ("score", "scores"; one word starting with the other, 3+ letters).
static void glueCue(Line &say, const std::string &cue) {
  auto has = [&](const std::vector<std::string> &hs) { return std::find(hs.begin(), hs.end(), cue) != hs.end(); };
  if (has(say.trailingHooks)) return;
  for (auto &w : say.words) if (has(w.hooks)) return;
  for (auto &w : say.words) {
    std::string t;
    for (char c : w.text) if (std::isalpha((unsigned char)c)) t += (char)std::tolower((unsigned char)c);
    const std::string &shorter = t.size() < cue.size() ? t : cue, &longer = t.size() < cue.size() ? cue : t;
    if (t == cue || (shorter.size() >= 3 && longer.compare(0, shorter.size(), shorter) == 0)) { w.hooks.push_back(cue); return; }
  }
}

bool Script::parse(const std::string &text) {
  steps.clear();
  errors.clear();
  lines_ = 0;
  return append(text);
}

bool Script::append(const std::string &text) {
  size_t errs = errors.size(), first = steps.size();
  std::istringstream in(text);
  std::string raw;
  int &lineNo = lines_;
  while (std::getline(in, raw)) {
    lineNo++;
    std::string t = trim(raw);
    if (t.empty() || startsWith(t, "#") || startsWith(t, "//") || startsWith(t, "```")) continue;
    Line ln;
    ln.uid = ++nextUid_;
    std::string err;
    if (!parseLine(t, lineNo, ln, err)) {
      errors.push_back("line " + std::to_string(lineNo) + ": " + err + "   | " + t);
      continue;
    }
    if (ln.with) {
      if (steps.empty()) { errors.push_back("line " + std::to_string(lineNo) + ": 'with' has nothing before it"); continue; }
      if (!ln.deferred.empty() && ln.deferred != steps.back().hook) {
        errors.push_back("line " + std::to_string(lineNo) + ": 'with' line has a different @hook than the line it joins");
      }
      steps.back().withs.push_back(ln);
      continue;
    }
    Step s;
    s.hook = ln.deferred;
    s.main = ln;
    steps.push_back(s);
  }
  Line *say = nullptr; // the sentence the new lines belong to: the last Say before each
  for (size_t i = first; i-- > 0 && !say;) if (steps[i].main.verb == Verb::SAY) say = &steps[i].main;
  for (size_t i = first; i < steps.size(); i++)
    if (steps[i].main.verb == Verb::SAY) say = &steps[i].main;
    else if (say && !steps[i].hook.empty()) glueCue(*say, steps[i].hook);
  return errors.size() == errs;
}

} // namespace mk
