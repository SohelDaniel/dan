// Binding: the words and the picture tell one story. At the end of each sentence (in --check), a few
// claims that must be visible are checked against what is on stage at that moment. Narrow on purpose:
// an "explain everything you say" critic flags metaphors and clutters the stage (tested: 9-16 complaints
// per scene, all scenes ~4/10), so only claims with an unambiguous visual form are checked:
//   numbers   a number said aloud appears on stage (a label, a value, a formula) or counts what is visible
//   sameness  "same / share" while nothing this sentence touches shares a colour
//   each-own  "each … its own" over several arrows of which none carries a name or a value
//   symbols   a formula symbol that names a thing on stage (w_{11} for a link labelled w₁₁) but isn't
//             \textcolor'ed like it
// Problems come back to Claude as `binding:` lines in <last_scene_issues>.
#include "engine.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <regex>
#include <sstream>

namespace mk {
using rm::fmtNum;

namespace {
const std::map<std::string, double> &numberWords() {
  static const std::map<std::string, double> m = {
      {"zero", 0},     {"two", 2},        {"three", 3},     {"four", 4},     {"five", 5},      {"six", 6},
      {"seven", 7},    {"eight", 8},      {"nine", 9},      {"ten", 10},     {"eleven", 11},   {"twelve", 12},
      {"thirteen", 13}, {"fourteen", 14}, {"fifteen", 15},  {"sixteen", 16}, {"seventeen", 17}, {"eighteen", 18},
      {"nineteen", 19}, {"twenty", 20},   {"thirty", 30},   {"forty", 40},   {"fifty", 50},    {"sixty", 60},
      {"seventy", 70}, {"eighty", 80},    {"ninety", 90},   {"hundred", 100}, {"thousand", 1000}};
  return m;
}
std::string lowerAscii(std::string s) {
  for (auto &c : s) c = (char)std::tolower((unsigned char)c);
  return s;
}
// unicode subscripts/superscripts to plain digits: w₁₁ -> w11
std::string plainDigits(const std::string &s) {
  static const char *sub[] = {"₀", "₁", "₂", "₃", "₄", "₅", "₆", "₇", "₈", "₉"};
  std::string o = s;
  for (int d = 0; d < 10; d++)
    for (size_t p; (p = o.find(sub[d])) != std::string::npos;) o.replace(p, std::strlen(sub[d]), std::string(1, char('0' + d)));
  return o;
}
// a formula without braces, \textcolor{..}{..} groups removed, so "w_{11}" reads "w_11"
std::string flatTex(std::string t) {
  std::string o;
  for (char c : t) if (c != '{' && c != '}' && c != ' ') o += c;
  return o;
}
} // namespace

Engine::StageFacts Engine::stageFacts() {
  StageFacts f;
  auto visible = [&](const Obj &o) { return opacityOf(o) > 0.3f; };
  std::vector<std::string> texts; // every piece of text the learner can read
  for (auto &id : order_) {
    if (Mark *m = mark(id); m && visible(*m)) {
      std::string k = effectiveKind(*m);
      if (k == "param") continue;
      f.kindCount[k]++;
      if (!m->category.empty()) f.kindCount["cat:" + m->category]++;
      for (auto &p : m->props) {
        if (p.val.type == Value::STR) texts.push_back(p.val.str);
        else if (p.val.type == Value::LIST) for (auto &it : p.val.list) texts.push_back(it);
        else if (p.key == magProp(*m)) texts.push_back(fmtNum(num(p.val)));
      }
      for (auto &a : actives_) // a value on its way (instant runs land it on the next tick)
        if (a.kind == Active::NUM && a.id == id) texts.push_back(fmtNum(a.to));
      if (isAxes(schemaNow())) // on axes and a plane, coordinates can be read off the grid
        for (const char *key : {"x", "y", "dx", "dy", "x1", "y1", "x2", "y2", "r", "w", "h", "to", "from"}) {
          bool ok;
          double v = num(*m, key, &ok);
          if (ok && std::isfinite(v)) texts.push_back(fmtNum(v));
        }
      if (k == "tex") // live values in formulas: the source as typeset, \val{} filled in
        if (const Value *t = findProp(m->props, "tex"); t && t->type == Value::STR) texts.push_back(texSource(t->str));
    } else if (auto lt = links_.find(id); lt != links_.end() && visible(lt->second)) {
      f.links++;
      bool named = false;
      for (auto &p : lt->second.props) {
        if (p.key == "opacity" || p.key == "bend") continue;
        if (p.key == "label" || p.val.isNumeric()) named = true;
        if (p.key == "color" && p.val.type == Value::STR) f.linkColors[p.val.str]++;
        if (p.val.type == Value::STR) texts.push_back(p.val.str);
        else if (p.val.isNumeric()) texts.push_back(fmtNum(num(p.val)));
      }
      if (named) f.labelledLinks++;
    }
  }
  if (!title_.empty()) texts.push_back(title_);
  for (auto &t : texts) f.text += " " + plainDigits(t) + " ";
  if (std::getenv("TUTOR_DUMP_BINDING")) std::fprintf(stderr, "facts: %s\n", f.text.c_str());
  return f;
}

void Engine::checkBinding(size_t sayIdx, size_t nextIdx) {
  const Line &say = script_.steps[sayIdx].main;
  std::string text;
  for (auto &w : say.words) text += (text.empty() ? "" : " ") + w.text;
  std::string low = lowerAscii(text);
  bool question = !text.empty() && text.find('?') != std::string::npos;
  bool last = true; // the closing sentence generalises past the picture ("a thousand items take ten looks")
  for (size_t i = sayIdx + 1; i < script_.steps.size(); i++) if (script_.steps[i].main.verb == Verb::SAY) { last = false; break; }
  std::string where = "binding: line " + std::to_string(say.lineNo) + " ";
  int issued = 0;
  auto issue = [&](const std::string &m) { if (issued++ < 2) warn(where + m); };

  // what is on stage, visibly: at the start of the sentence and now (a number shown and then changed counts)
  StageFacts now = stageFacts();
  const StageFacts &before = sayFacts_;
  std::string allText = before.text + now.text;
  auto counted = [&](int v) {
    for (const StageFacts *f : {&before, (const StageFacts *)&now}) {
      for (auto &[k, n] : f->kindCount) if (n == v) return true;
      if (f->links == v) return true;
    }
    return false;
  };
  int links = now.links, labelledLinks = now.labelledLinks;
  const auto &linkColors = now.linkColors;

  // what this sentence touched: its own lines, its cued lines, what it points at
  std::set<std::string> touched;
  auto touch = [&](const Line &ln) { if (!ln.id.empty()) touched.insert(baseId(ln.id)); };
  for (size_t i = sayIdx; i < nextIdx && i < script_.steps.size(); i++) {
    if (i > sayIdx && script_.steps[i].hook.empty()) break; // set-up lines for the next sentence
    if (i > sayIdx) touch(script_.steps[i].main);
    for (auto &w : script_.steps[i].withs) touch(w);
  }

  // 1. numbers said aloud (questions are hypotheticals; "one" is too common to mean a number)
  if (!question && !last) {
    std::vector<std::pair<std::string, double>> said;
    std::vector<std::string> words;
    std::istringstream in(low);
    for (std::string w; in >> w;) {
      while (!w.empty() && !std::isalnum((unsigned char)w.back())) w.pop_back();
      while (!w.empty() && !std::isalnum((unsigned char)w.front())) w.erase(w.begin());
      if (!w.empty()) words.push_back(w);
    }
    auto wordVal = [&](const std::string &w, double &v) {
      auto it = numberWords().find(w);
      if (it != numberWords().end()) { v = it->second; return true; }
      char *end;
      v = std::strtod(w.c_str(), &end);
      return end != w.c_str() && *end == 0 && std::isdigit((unsigned char)w[0]);
    };
    for (size_t i = 0; i < words.size(); i++) {
      double v;
      if (words[i] == "point" && i + 1 < words.size() && wordVal(words[i + 1], v) && v < 10) { // "point six"
        if (i == 0 || !numberWords().count(words[i - 1])) said.push_back({"point " + words[i + 1], v / 10});
        i++;
        continue;
      }
      if (!wordVal(words[i], v)) continue;
      std::string spoken = words[i];
      // compound numbers: "fifty nine", "two hundred", "seven and a half"
      double u;
      if (v >= 20 && v < 100 && std::fmod(v, 10) == 0 && i + 1 < words.size() && wordVal(words[i + 1], u) && u < 10) {
        v += u; spoken += " " + words[++i];
      }
      if (i + 1 < words.size() && (words[i + 1] == "hundred" || words[i + 1] == "thousand")) {
        v *= words[i + 1] == "hundred" ? 100 : 1000; spoken += " " + words[++i];
      }
      // decimals: "one point six" -> 1.6
      if (i + 2 < words.size() && words[i + 1] == "point" && wordVal(words[i + 2], u) && u < 10) {
        v += u / 10; spoken += " point " + words[i + 2]; i += 2;
      }
      if (i + 3 < words.size() && words[i + 1] == "and" && words[i + 2] == "a" && words[i + 3] == "half") {
        v += 0.5; spoken += " and a half"; i += 3;
      }
      // "four times, because…" is a ratio, not a value to show ("two times three" is a product: keep it)
      double nx;
      if (i + 1 < words.size() && words[i + 1] == "times" && !(i + 2 < words.size() && wordVal(words[i + 2], nx))) continue;
      said.push_back({spoken, v});
    }
    for (auto &[word, v] : said) {
      bool shown = false;
      // as text on stage: a number token equal to it (signs and trailing zeros aside)
      static const std::regex num(R"(-?\d+(\.\d+)?)");
      for (std::sregex_iterator r(allText.begin(), allText.end(), num), e; r != e && !shown; ++r)
        if (std::fabs(std::fabs(std::stod(r->str())) - v) < 1e-9) shown = true;
      // or as a count of what is visible
      if (counted((int)v)) shown = true;
      if (!shown) issue("says \"" + word + "\", but no " + fmtNum(v) + " is on stage and nothing visible counts " +
                        fmtNum(v) + " (put the number where it happens: a label, a value on a link, a \\val{} in a formula)");
    }
  }

  // 2. sameness: something this sentence touches should share a colour
  // "share", or "the same <n> <things>" (plural): "the same rule" is not a claim about things on stage
  // the verb "share" (not "a share of"), "shared", or "the same <n> <things>" (plural, and things, not steps)
  static const std::regex same(R"(\b(share|shares)\s+(the|a|one|its|their|this|these|those|same|every)\b|\bshared\b|\b(same|identical)\s+(\w+\s+)?(\w+s)\b)");
  static const std::set<std::string> abstract = {"steps", "rules", "ideas", "ways", "reasons", "times", "results", "patterns",
                                                 "words", "questions", "answers", "kinds", "moves", "tricks", "things"};
  std::smatch sm;
  bool sameClaim = std::regex_search(low, sm, same) && !(sm[5].matched && abstract.count(sm[5].str()));
  if (!question && sameClaim) {
    bool group = false;
    std::map<std::string, int> cats, cols;
    for (auto &id : touched) {
      if (Mark *m = mark(id)) {
        if (!m->category.empty()) cats[m->category]++;
        if (auto c = findProp(m->props, "color"); c && c->type == Value::STR) cols[c->str]++;
      } else if (auto lt = links_.find(id); lt != links_.end()) {
        if (auto c = findProp(lt->second.props, "color"); c && c->type == Value::STR) cols[c->str]++;
      }
    }
    for (auto &[k, n] : cats) if (n >= 2) group = true;
    for (auto &[k, n] : cols) if (n >= 2) group = true;
    for (auto &[k, n] : linkColors) if (n >= 2 && touched.size() <= 1) group = true; // an earlier colour group, pointed at
    if (!group)
      issue("says things are the same / shared, but nothing it shows shares a colour (give exactly those things one "
            "colour: color=yellow on links, a shared category on marks)");
  }

  // 3. "each … its own" over unnamed arrows
  static const std::regex own(R"(\b(each|every)\b.*\b(own|different|separate)\b|\bits own\b)");
  if (std::regex_search(low, own) && links >= 3 && labelledLinks == 0)
    issue("says each has its own, but none of the " + std::to_string(links) + " arrows carries a name or value (label a "
          "few: label=\"w₁₁\", or w=0.5 to show the value)");

  // 4. formula symbols that name a thing on stage, not coloured like it
  for (auto &tid : touched) {
    Mark *t = mark(tid);
    if (!t || effectiveKind(*t) != "tex") continue;
    const Value *src = findProp(t->props, "tex");
    if (!src || src->type != Value::STR) continue;
    std::string flat = flatTex(src->str);
    auto named = [&](const std::string &id, const std::string &label) {
      std::string l = plainDigits(label);
      if (l.size() < 2 || l.size() > 6) return;
      std::string sym = l.substr(0, 1) + "_" + l.substr(1); // w11 -> w_11
      if (!std::isalpha((unsigned char)l[0]) || !std::isdigit((unsigned char)l[1])) return;
      if (flat.find(sym) == std::string::npos) return;
      if (src->str.find("\\textcolor{" + id + "}") != std::string::npos) return;
      if (src->str.find("\\textcolor") != std::string::npos && flat.find("\\textcolor") != std::string::npos &&
          flat.find(sym) > flat.find("\\textcolor"))
        return; // coloured some way (by a colour name): fine
      issue("formula '" + tid + "' writes " + sym + " but doesn't colour it like '" + id + "' (\\textcolor{" + id + "}{…})");
    };
    for (auto &[lid, l] : links_)
      if (auto lb = findProp(l.props, "label"); lb && lb->type == Value::STR) named(lid, lb->str);
  }
}

} // namespace mk
