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

// a sentence's words as lower-case tokens, hyphenated numbers split ("eighty-four"), each with the word it came from;
// {expr} words are left out (they point at their thing already)
void Engine::sayTokens(const std::vector<std::string> &words, std::vector<std::string> &toks, std::vector<size_t> &owner) {
  for (size_t i = 0; i < words.size(); i++) {
    if (words[i].find('{') != std::string::npos) continue;
    std::string w = lowerAscii(words[i]);
    for (auto &c : w) if (c == '-' || c == '/') c = ' ';
    std::istringstream in(w);
    for (std::string t; in >> t;) {
      while (!t.empty() && !std::isalnum((unsigned char)t.back())) t.pop_back();
      while (!t.empty() && !std::isalnum((unsigned char)t.front())) t.erase(t.begin());
      if (!t.empty()) { toks.push_back(t); owner.push_back(i); }
    }
  }
}

// numbers said aloud, with the tokens they span: "fifty nine", "seven hundred eighty four", "zero point one four",
// "seven and a half", "40 percent". "one" alone is too common to mean a number; "four times, because…" is a ratio
std::vector<Engine::SpokenNum> Engine::readNumbers(const std::vector<std::string> &w) {
  auto val = [&](size_t i, double &v) {
    if (i >= w.size()) return false;
    auto it = numberWords().find(w[i]);
    if (it != numberWords().end()) { v = it->second; return true; }
    char *end;
    v = std::strtod(w[i].c_str(), &end);
    return end != w[i].c_str() && *end == 0 && std::isdigit((unsigned char)w[i][0]);
  };
  // a single digit; after "point" (and in "one hundred") "one" is a digit too
  auto digit = [&](size_t i, double &v) {
    if (i < w.size() && w[i] == "one") { v = 1; return true; }
    return val(i, v) && v < 10 && v == std::floor(v);
  };
  std::vector<SpokenNum> out;
  for (size_t i = 0; i < w.size(); i++) {
    SpokenNum n;
    n.first = i;
    double v = 0, u;
    bool one = w[i] == "one" && i + 1 < w.size() && (w[i + 1] == "hundred" || w[i + 1] == "thousand" || w[i + 1] == "point");
    if (w[i] == "point" && digit(i + 1, u) && (i == 0 || !numberWords().count(w[i - 1]))) v = 0, i--; // "point six"
    else if (one) v = 1;
    else if (!val(i, v)) continue;
    // tens and units, hundreds, thousands
    auto below100 = [&](double &x) {
      if (x >= 20 && x < 100 && std::fmod(x, 10) == 0 && digit(i + 1, u) && u > 0) { x += u; i++; }
    };
    below100(v);
    if (i + 1 < w.size() && w[i + 1] == "hundred") {
      v *= 100; i++;
      if (i + 1 < w.size() && w[i + 1] == "and") i++;
      double r;
      if (val(i + 1, r) && r < 100) { i++; below100(r); v += r; }
    }
    if (i + 1 < w.size() && w[i + 1] == "thousand") { // "ninety nine thousand nine hundred (and) five"
      v *= 1000; i++;
      if (i + 1 < w.size() && w[i + 1] == "and") i++;
      double r;
      if (val(i + 1, r) && r < 100) {
        i++;
        below100(r);
        if (i + 1 < w.size() && w[i + 1] == "hundred") {
          r *= 100; i++;
          if (i + 1 < w.size() && w[i + 1] == "and") i++;
          double t;
          if (val(i + 1, t) && t < 100) { i++; below100(t); r += t; }
        }
        v += r;
      }
    }
    // decimals: every single digit after "point" ("zero point one four" -> 0.14)
    if (i + 1 < w.size() && w[i + 1] == "point" && digit(i + 2, u)) {
      i++;
      std::string frac;
      while (digit(i + 1, u)) { frac += char('0' + (int)u); i++; }
      v += std::stod("0." + frac);
    }
    if (i + 3 < w.size() && w[i + 1] == "and" && w[i + 2] == "a" && w[i + 3] == "half") { v += 0.5; i += 3; }
    if (i + 1 < w.size() && w[i + 1] == "times" && !(val(i + 2, u))) continue; // a ratio, not a value to show
    if (i + 1 < w.size() && w[i + 1] == "percent") { n.percent = true; i++; }
    n.last = i;
    n.v = v;
    for (size_t k = n.first; k <= n.last; k++) n.spoken += (k > n.first ? " " : "") + w[k];
    out.push_back(n);
  }
  return out;
}

// a number worth pointing at: not a small count of things ("two groups", "four ways", "five of them") nor an
// ordinal ("step two")
bool Engine::pointable(const SpokenNum &n, const std::vector<std::string> &toks) {
  static const std::set<std::string> ordinal = {"step", "part", "stage", "phase", "round", "case", "line", "chapter", "level", "option"};
  static const std::set<std::string> notPlural = {"is", "was", "has", "as", "us", "its", "this", "thus", "yes", "plus", "minus", "times", "less"};
  if (n.first > 0 && ordinal.count(toks[n.first - 1])) return false;
  if (n.v < 10 && n.v == std::floor(n.v) && n.last + 1 < toks.size()) {
    // the next word, or the one after an adjective ("two smaller questions")
    if (toks[n.last + 1] == "of") return false; // "five of them", "two of the three"
    for (size_t k = n.last + 1; k < std::min(toks.size(), n.last + 3); k++) {
      const std::string &t = toks[k];
      if (t.size() > 2 && t.back() == 's' && !notPlural.count(t)) return false;
    }
  }
  return true;
}

// the one visible thing that shows this value: a cell, a label, a value, a link ("" none, "?" several)
std::string Engine::holderOfValue(double v, bool percent) {
  static const std::regex numRe(R"(-?\d+(\.\d+)?)");
  auto has = [&](const std::string &t) {
    std::string d = plainDigits(t);
    for (std::sregex_iterator r(d.begin(), d.end(), numRe), e; r != e; ++r) {
      double x = std::fabs(std::stod(r->str()));
      if (std::fabs(x - v) < 1e-9 || (percent && std::fabs(x - v / 100) < 1e-9)) return true;
    }
    return false;
  };
  std::set<std::string> plain, formulas;
  for (auto &id : order_) {
    if (Mark *m = mark(id); m && opacityOf(*m) > 0.3f) {
      std::string k = effectiveKind(*m);
      if (k == "param" || k == "code") continue; // numbers in code aren't values to point at
      for (auto &p : m->props) {
        if (p.key == "items" && p.val.type == Value::LIST) {
          for (size_t c = 0; c < p.val.list.size(); c++) if (has(p.val.list[c]) && plainDigits(p.val.list[c]).find_first_not_of("0123456789.-") == std::string::npos)
            plain.insert(id + "[" + std::to_string(c) + "]");
        } else if (p.key == "tex" && p.val.type == Value::STR) {
          if (has(texSource(p.val.str))) formulas.insert(id);
        } else if ((p.key == "label" || p.key == "text" || p.key == "name") && p.val.type == Value::STR) {
          if (has(p.val.str)) plain.insert(id);
        } else if (p.key == magProp(*m) && p.val.isNumeric()) {
          if (has(fmtNum(num(p.val)))) plain.insert(id);
        }
      }
    } else if (auto lt = links_.find(id); lt != links_.end() && opacityOf(lt->second) > 0.3f) {
      for (auto &p : lt->second.props)
        if ((p.key == "label" && p.val.type == Value::STR && has(p.val.str)) ||
            (p.key != "opacity" && p.key != "bend" && p.val.isNumeric() && has(fmtNum(num(p.val)))))
          plain.insert(id);
    }
  }
  for (auto it = plain.begin(); it != plain.end();) // a cell beats its own array's label ("looking for 23" over the 23)
    it = (it->find('[') == std::string::npos && std::any_of(plain.begin(), plain.end(), [&](const std::string &c) {
           return c.rfind(*it + "[", 0) == 0; })) ? plain.erase(it) : std::next(it);
  if (plain.size() == 1) return *plain.begin();
  if (plain.empty() && formulas.size() == 1) return *formulas.begin();
  return plain.empty() && formulas.empty() ? "" : "?";
}

// the one visible thing whose label carries this letter as a word ("P", "f(x, y) = …")
std::string Engine::holderOfName(const std::string &letter) {
  auto has = [&](const std::string &t) {
    std::string tok;
    for (size_t i = 0; i <= t.size(); i++) {
      if (i < t.size() && std::isalpha((unsigned char)t[i])) tok += t[i];
      else { if (lowerAscii(tok) == letter) return true; tok.clear(); }
    }
    return false;
  };
  std::set<std::string> plain, formulas;
  for (auto &id : order_) {
    if (Mark *m = mark(id); m && opacityOf(*m) > 0.3f && effectiveKind(*m) != "param") {
      for (auto &p : m->props)
        if ((p.key == "label" || p.key == "text" || p.key == "name") && p.val.type == Value::STR && has(p.val.str)) plain.insert(id);
        else if (p.key == "tex" && p.val.type == Value::STR) {
          std::string t = std::regex_replace(texSource(p.val.str), std::regex(R"(\\[A-Za-z]+(\{[^}]*\})?)"), " "); // commands and their names
          if (has(t)) formulas.insert(id);
        }
    } else if (auto lt = links_.find(id); lt != links_.end() && opacityOf(lt->second) > 0.3f) {
      if (auto lb = findProp(lt->second.props, "label"); lb && lb->type == Value::STR && has(lb->str)) plain.insert(id);
    }
  }
  if (plain.size() == 1) return *plain.begin();
  if (plain.empty() && formulas.size() == 1) return *formulas.begin();
  return plain.empty() && formulas.empty() ? "" : "?";
}

void Engine::applyPointers() {
  if (!shadow_) return;
  for (size_t i = cursor_; i < script_.steps.size(); i++) {
    Line &ln = script_.steps[i].main;
    if (ln.verb != Verb::SAY) continue;
    auto it = shadow_->autoPoint_.find(ln.uid);
    if (it == shadow_->autoPoint_.end()) continue;
    for (auto &[w, target] : it->second)
      if (w < ln.words.size()) {
        auto &hooks = ln.words[w].hooks;
        if (std::find(hooks.begin(), hooks.end(), "@pt:" + target) == hooks.end()) hooks.push_back("@pt:" + target);
      }
  }
}

std::string Engine::pulsing(const std::string &id) {
  Obj *o = find(id);
  return o && o->pulse > 0 ? (o->pulseRef.empty() ? id : o->pulseRef) : "";
}

std::string Engine::pointingReport() const {
  const PointStats &p = shadow_ ? shadow_->pointStats_ : pointStats_;
  return "pointing: " + std::to_string(p.spoken) + " numbers and names said, " + std::to_string(p.onStage) +
         " on stage as they are said, " + std::to_string(p.pointed) + " pointed by the script, " + std::to_string(p.added) +
         " pointed ahead";
}

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
  std::string text = spokenText(say); // {expr} as spoken: a value read off the stage is checked like a typed one
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
  for (auto &w : say.words) // [id] pointers in the sentence
    for (auto &h : w.hooks) if (h.rfind("@pt:", 0) == 0) touched.insert(baseId(h.substr(4)));
  for (size_t i = sayIdx; i-- > 0;) { // set-up lines just before it (since the last sentence) play as it starts
    if (script_.steps[i].main.verb == Verb::SAY) break;
    if (!script_.steps[i].hook.empty()) continue;
    touch(script_.steps[i].main);
    for (auto &w : script_.steps[i].withs) touch(w);
  }
  for (size_t i = sayIdx; i < nextIdx && i < script_.steps.size(); i++) {
    if (i > sayIdx && script_.steps[i].hook.empty()) break; // set-up lines for the next sentence
    if (i > sayIdx) touch(script_.steps[i].main);
    for (auto &w : script_.steps[i].withs) touch(w);
  }

  // 1. numbers said aloud (questions are hypotheticals; "one" is too common to mean a number)
  if (!question && !last) {
    std::vector<std::pair<std::string, double>> said;
    std::vector<std::string> toks;
    std::vector<size_t> owner;
    sayTokens(spokenWords(say), toks, owner);
    for (auto &n : readNumbers(toks)) said.push_back({n.spoken, n.percent ? -n.v : n.v});
    for (auto &[word, vs] : said) {
      bool percent = vs < 0;
      double v = std::fabs(vs);
      bool shown = false;
      // as text on stage: a number token equal to it (signs and trailing zeros aside)
      static const std::regex num(R"(-?\d+(\.\d+)?)");
      for (std::sregex_iterator r(allText.begin(), allText.end(), num), e; r != e && !shown; ++r)
        if (std::fabs(std::fabs(std::stod(r->str())) - v) < 1e-9 || (percent && std::fabs(std::fabs(std::stod(r->str())) - v / 100) < 1e-9))
          shown = true;
      // or as a count of what is visible
      if (counted((int)v)) shown = true;
      if (!shown) issue("says \"" + word + "\", but no " + fmtNum(v) + " is on stage and nothing visible counts " +
                        fmtNum(v) + " (put the number where it happens: a label, a value on a link, a \\val{} in a formula; then say it as {id})");
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

  // 4. an action described while nothing on stage changes
  static const std::regex act(R"(\b(adds?|added|multipl\w*|divides?|divided|subtracts?|slides?|moves?|grows?|shrinks?|splits?|pairs? up|combines?|flows?|swaps?|squash\w*|sums? up|rotates?|stretches|shifts?)\b)");
  if (!question && touched.empty() && std::regex_search(low, sm, act))
    issue("says something " + sm.str() + ", but nothing on stage changes during this sentence (show it happening, on that word)");

  // 5. formula symbols that name a thing on stage, not coloured like it
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

  // 6. a symbol said aloud as a name (f, g, P, theta) that nothing on stage carries: "picture f as a height"
  //    over an unlabelled heat map. Letters are whole tokens of the stage's text (labels, formulas), any case;
  //    on axes and a plane, x and y are the axes themselves
  if (!question) {
    static const std::map<std::string, std::string> greek = {
        {"alpha", "α"}, {"beta", "β"}, {"gamma", "γ"}, {"delta", "δ"}, {"theta", "θ"}, {"lambda", "λ"}, {"mu", "μ"},
        {"sigma", "σ"}, {"phi", "φ"}, {"omega", "ω"}, {"epsilon", "ε"}, {"rho", "ρ"}, {"tau", "τ"}, {"pi", "π"}};
    std::set<std::string> onStage; // single letters and greek letters (as symbols or \names) on stage
    std::string tok;
    auto flush = [&] { if (!tok.empty()) { onStage.insert(lowerAscii(tok)); tok.clear(); } };
    for (size_t i = 0; i < allText.size(); i++) {
      unsigned char c = (unsigned char)allText[i];
      if (std::isalpha(c)) tok += (char)c;
      else if (c >= 0x80) { // a multi-byte character: greek letters count on their own
        flush();
        size_t n = (c >= 0xF0) ? 4 : (c >= 0xE0) ? 3 : 2;
        onStage.insert(allText.substr(i, n));
        i += n - 1;
      } else flush();
    }
    flush();
    for (auto &[name, sym] : greek)
      if (onStage.count(name)) onStage.insert(sym); // \theta in a formula
    if (isAxes(schemaNow())) { onStage.insert("x"); onStage.insert("y"); }
    std::set<std::string> said;
    std::string w;
    auto word = [&](std::string t) {
      if (t.size() == 1 && t != "a" && t != "A" && t != "I") said.insert(lowerAscii(t));
      if (auto g = greek.find(lowerAscii(t)); g != greek.end() && t != "pi" && t != "Pi") said.insert(g->second);
    };
    std::string plain = text; // contractions are words: "that's", "doesn't" (and their curly apostrophe)
    for (size_t p; (p = plain.find("\u2019")) != std::string::npos;) plain.replace(p, 3, "'");
    // letters that aren't symbols: a language ("C++", "C#") and a list item ("part b", "option c")
    static const std::regex notSymbol(R"(\b[Cc](\+\+|#)|\b(part|option|step|plan|type|vitamin|grade)\s+[A-Za-z]\b)", std::regex::icase);
    plain = std::regex_replace(plain, notSymbol, " ");
    for (char c : plain) {
      if (std::isalpha((unsigned char)c) || (c == '\'' && !w.empty())) w += c;
      else { if (!w.empty()) word(w); w.clear(); }
    }
    if (!w.empty()) word(w);
    for (auto &s : said)
      if (!onStage.count(s))
        issue("says \"" + s + "\" as a name, but no " + s + " is on stage (the thing it stands for carries it: label=\"" + s +
              " = …\" on the curve, field or point, or a formula that writes it)");
  }
}

} // namespace mk
