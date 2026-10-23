#include "engine.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <functional>
#include <regex>
#include <sstream>
#include <sys/stat.h>
#include <tuple>

namespace mk {

using namespace rm;

static const std::set<std::string> kPositional = {"x",  "y",     "t",     "at",     "x1",  "y1",  "x2",
                                                  "y2", "slope", "from",  "to",     "w",   "width", "extend",
                                                  "r",  "radius", "min",  "max",    "step", "domain", "year",
                                                  "xt", "yt",     "bend", "pos",    "range", "size",
                                                  "opacity", "dx", "dy", "h", "height", "pts", "cols", "line",
                                                  "grid", "axes", "equal", "index", "cells", "items", "n", "m", "xlabel", "ylabel",
                                                  "vx", "vy", "levels", "labels", "trail", "colors",
                                                  "parts", "by", "bylabels", "bycolors"};
static const std::set<std::string> kTextKeys = {"label", "name", "text", "set", "fn",   "of",   "color",
                                                "shape", "ease", "unit", "note", "title", "tex",
                                                "at", "to", "side", "style", "cmap", "levels", "trail"};

Engine::Engine(EngineOptions opt) : opt_(std::move(opt)) {}
Engine::~Engine() {
  stopVoice();
  bool audio = tts_ != nullptr;
  tts_.reset();
  if (audio && IsAudioDeviceReady()) CloseAudioDevice();
}

// ================================================================ loading
bool Engine::load(const std::string &path) {
  path_ = path;
  std::ifstream f(path);
  if (!f) {
    script_.errors = {"cannot open " + path};
    return false;
  }
  std::stringstream ss;
  ss << f.rdbuf();
  return loadText(ss.str());
}

static void eachLine(const Script &s, const std::function<void(const Line &)> &fn) {
  for (auto &st : s.steps) {
    fn(st.main);
    for (auto &w : st.withs) fn(w);
  }
}

bool Engine::loadText(const std::string &text) {
  script_.parse(text);
  for (auto &e : script_.errors) std::fprintf(stderr, "parse: %s\n", e.c_str());
  analyze(true);
  prefetched_.clear();
  startShadow(text);
  applyPointers();
  prepareVoice();
  reset();
  return script_.errors.empty();
}

bool Engine::appendText(const std::string &text) {
  size_t from = script_.steps.size(), errs = script_.errors.size();
  size_t lastWiths = from ? script_.steps[from - 1].withs.size() : 0;
  bool ok = script_.append(text);
  // a leading `with` joined a step that already ran: run it now, alongside as closely as we can
  if (from && from - 1 < cursor_) {
    const Step &last = script_.steps[from - 1];
    bool ran = last.hook.empty() || hookFired_[from - 1];
    if (ran)
      for (size_t w = lastWiths; w < last.withs.size(); w++) {
        Line ln = last.withs[w]; // a copy: a later append may grow this vector
        execLine(ln);
      }
  }
  if (!opt_.shadow)
    for (size_t i = errs; i < script_.errors.size(); i++) std::fprintf(stderr, "parse: %s\n", script_.errors[i].c_str());
  analyze(false);
  for (size_t i = from; i < script_.steps.size(); i++) // its Says have arrived: no longer only prefetched
    if (script_.steps[i].main.verb == Verb::SAY && !prefetched_.empty() && prefetched_.front() == spokenText(script_.steps[i].main))
      prefetched_.pop_front();
  if (shadow_) { // read this part's spoken values (and what to point at) before its voice is asked for
    shadow_->appendText(text);
    shadow_->runToEnd();
    applyPointers();
  }
  hookFired_.resize(script_.steps.size(), false);
  readbackPrinted_ = false; // report the scene again once this part has played
  idleSent_ = false;
  queueVoice(from, false);
  // a hooked line that arrives after its sentence already reached the hook runs now, late but not lost
  for (size_t i = from; i < script_.steps.size(); i++) {
    const std::string &h = script_.steps[i].hook;
    if (h.empty()) continue;
    size_t say = i;
    while (say > 0 && script_.steps[say - 1].main.verb != Verb::SAY) say--;
    if (say == 0) continue;
    say--; // the Say this line belongs to
    auto f = firedBySay_.find(say);
    bool ended = say < cursor_ && !(say_.active && sayStep_ == say);
    if (ended || (say < cursor_ && f != firedBySay_.end() && f->second.count(h))) {
      hookFired_[i] = true;
      execStep(i);
    }
  }
  return ok;
}

void Engine::analyze(bool fresh) {
  // ---- prescan: every literal a property ever takes, so scales never jump (axiom 2)
  scenePrescan_.clear();
  Prescan cur;
  auto see = [&](const std::string &k, const Value &v) {
    if (v.type != Value::NUM) return;
    cur.maxAbs[k] = std::max(cur.maxAbs[k], std::fabs(v.num));
    if (!cur.minVal.count(k)) cur.minVal[k] = cur.maxVal[k] = v.num;
    cur.minVal[k] = std::min(cur.minVal[k], v.num);
    cur.maxVal[k] = std::max(cur.maxVal[k], v.num);
    bool isInt = std::fabs(v.num - std::round(v.num)) < 1e-9;
    cur.isInt[k] = cur.isInt.count(k) ? (cur.isInt[k] && isInt) : isInt;
  };
  bool hasAxes = false, hasT = false, hasLinks = false, hasMag = false, hasWorld = false, hasFn = false, hasBox = false;
  autoSchemas_.clear();
  int blockLines = 0; // blocks this scene will hold (to choose the band's arrangement before they arrive)
  std::set<std::string> sceneBlockIds;
  sceneBlocks_.clear();
  auto closeScene = [&] {
    sceneBlocks_.push_back(blockLines);
    blockLines = 0;
    sceneBlockIds.clear();
    autoSchemas_.push_back(hasWorld && !hasFn ? "plane"
                           : hasAxes          ? "axes"
                           : hasT             ? "timeline"
                           : (hasBox || (hasLinks && !hasMag)) ? "graph" // boxes are graph things from the first one,
                                                                 : "free");  // so links or numbers arriving later don't re-lay the scene
    hasAxes = hasT = hasLinks = hasMag = hasWorld = hasFn = hasBox = false;
    scenePrescan_.push_back(cur);
    cur = Prescan{};
  };
  std::set<std::string> defined, sayHooks, deferredHooks, parts; // parts: formula parts named by annotations (f[id])
  std::vector<std::pair<int, std::string>> pointed; // [id] in Say lines
  eachLine(script_, [&](const Line &ln) {
    if (!ln.deferred.empty()) deferredHooks.insert(ln.deferred);
    switch (ln.verb) {
    case Verb::MARK:
      defined.insert(ln.id);
      for (auto &p : ln.props) {
        see(p.key, p.val);
        if (p.key == "x" || p.key == "x1" || p.key == "fn" || p.key == "xt") hasAxes = true;
        if (p.key == "fn" || p.key == "xt") hasFn = true;
        if ((p.key == "t" || p.key == "at" || p.key == "year") && p.val.isNumeric()) hasT = true;
        if ((!kPositional.count(p.key) || (p.key == "m" && ln.kind != "grid")) && p.val.isNumeric() && ln.kind != "label" && ln.kind != "param") hasMag = true;
      }
      if (ln.kind == "path" || ln.kind == "region") hasAxes = hasFn = true;
      if (isWorldKind(ln.kind)) hasWorld = true;
      if (ln.kind == "box") hasBox = true;
      for (auto &p : ln.props) // [1,000, 2,500]: a thousands comma splits a number in two
        if (p.val.type == Value::LIST)
          for (size_t i = 1; i < p.val.list.size(); i++) {
            const std::string &a = p.val.list[i - 1], &b = p.val.list[i];
            // only when written glued, "1,000": "[41, 124]" (computed data) is two numbers
            if (b.size() == 3 && std::all_of(b.begin(), b.end(), ::isdigit) && !a.empty() && std::all_of(a.begin(), a.end(), ::isdigit) && a.size() <= 3 &&
                ln.raw.find(a + "," + b) != std::string::npos) {
              warn("list: line " + std::to_string(ln.lineNo) + ": " + ln.id + "." + p.key + " has '" + a + "' then '" + b +
                   "': a comma splits a number; write " + a + b + " or quote it (\"" + a + "," + b + "\")");
              break;
            }
          }
      if (const Value *of = findProp(ln.props, "of"); of && isAnnotationKind(ln.kind))
        for (auto &ref : of->type == Value::LIST ? of->list : std::vector<std::string>{of->str})
          if (size_t b = ref.find('['); b != std::string::npos) parts.insert(ref.substr(b + 1, ref.find(']') - b - 1));
      if (isBlockKind(ln.kind) && sceneBlockIds.insert(ln.id).second) blockLines++;
      if (ln.kind == "slope") hasAxes = hasFn = true;                                   // y against x
      else if (isFieldKind(ln.kind) || ln.kind == "solution") { hasWorld = true; hasFn = false; } // the plane
      break;
    case Verb::LINK: defined.insert(ln.id); hasLinks = true; break;
    case Verb::SEND: // a formula part named by send is a part, like one an annotation names
      for (auto *ref : {&ln.a, &ln.b})
        if (size_t b = ref->find('['); b != std::string::npos) parts.insert(ref->substr(b + 1, ref->find(']') - b - 1));
      break;
    case Verb::GROUP: defined.insert(ln.id); break;
    case Verb::MORPH:
      if (ln.hasFrom) see(ln.prop, ln.from);
      see(ln.prop, ln.to);
      break;
    case Verb::SAY:
      for (auto &w : ln.words) for (auto &h : w.hooks) if (h.rfind("@pt:", 0) != 0) sayHooks.insert(h);
      for (auto &w : ln.words)
        for (auto &h : w.hooks)
          if (h.rfind("@pt:", 0) == 0) pointed.push_back({ln.lineNo, h.substr(4)});
      for (auto &h : ln.trailingHooks) sayHooks.insert(h);
      break;
    case Verb::SCENE: closeScene(); break;
    default: break;
    }
  });
  closeScene();
  autoSchema_ = autoSchemas_[std::min(sceneIdx_, autoSchemas_.size() - 1)];
  usePrescan();

  // ---- static lint
  if (fresh) lint_.clear();
  else // whole-script lints are re-derived: a chunk can end between a Say and its @hook lines
    for (auto it = lint_.begin(); it != lint_.end();)
      it = (it->rfind("hook:", 0) == 0 || it->find("which is never marked") != std::string::npos || it->find("\\textcolor{") != std::string::npos) ? lint_.erase(it)
                                                                                                    : std::next(it);
  for (auto &h : deferredHooks)
    if (!sayHooks.count(h)) warn("hook: lines tagged @" + h + " have no |" + h + "| spoken, so they only run when their sentence ends");
  for (auto &h : sayHooks)
    if (!deferredHooks.count(h)) warn("hook: |" + h + "| is spoken but no line is tagged @" + h);
  for (auto &[line, id] : pointed)
    if (!defined.count(baseId(id))) warn("say: line " + std::to_string(line) + " points at [" + id + "], which is never marked");
  eachLine(script_, [&](const Line &ln) {
    auto need = [&](const std::string &id, const char *what) {
      if (!id.empty() && id != "none" && !defined.count(baseId(id)))
        warn(std::string(what) + ": line " + std::to_string(ln.lineNo) + " refers to '" + id + "', which is never marked");
    };
    if (ln.verb == Verb::MORPH) need(ln.id, "morph");
    if (ln.verb == Verb::FOCUS) need(ln.id, "focus");
    if (ln.verb == Verb::MARK)
      for (const char *k : {"region", "below", "above", "left-of", "right-of", "near", "align", "pos"})
        if (findProp(ln.props, k))
          warn("layout: '" + ln.id + "' " + k + "= is ignored: position comes from structure (links make a flow, "
               "groups make lanes, order is order)");
    if (ln.verb == Verb::INDICATE) need(ln.id, "indicate");
    if (ln.verb == Verb::SEND) { need(ln.a, "send"); need(ln.b, "send"); if (!ln.id.empty()) need(ln.id, "send via"); }
    if (ln.verb == Verb::LINK) { need(ln.a, "link"); need(ln.b, "link"); }
    if (ln.verb == Verb::GROUP) for (auto &m : ln.members) need(m, "group");
  });

  // ---- formulas: typeset every literal one now (warms the cache; headless check reports TeX errors)
  eachLine(script_, [&](const Line &ln) {
    const Value *v = nullptr;
    if (ln.verb == Verb::MARK) v = findProp(ln.props, "tex");
    if (ln.verb == Verb::MORPH && ln.prop == "tex") v = &ln.to;
    if (!v || v->type != Value::STR) return;
    std::string src = v->str;
    for (size_t p; (p = src.find("\\val{")) != std::string::npos;) {
      size_t q = src.find('}', p);
      src.replace(p, q == std::string::npos ? std::string::npos : q - p + 1, "0");
    }
    if (opt_.shadow) return;
    if (!opt_.instant) { tex::request(src); return; }
    if (!fresh) return; // appended live: the window shows TeX errors itself
    TexPtr L = tex::typeset(src);
    std::string where = "tex: line " + std::to_string(ln.lineNo) + ": ";
    if (!L->ok) warn(where + L->error);
  });
  // \textcolor{name}: a thing, a part an annotation points at, or a colour. Read from the source, so it
  // is re-derived as the script streams (the annotation naming a part may come after its formula)
  eachLine(script_, [&](const Line &ln) {
    const Value *v = ln.verb == Verb::MARK ? findProp(ln.props, "tex") : ln.verb == Verb::MORPH && ln.prop == "tex" ? &ln.to : nullptr;
    if (!v || v->type != Value::STR) return;
    for (size_t p = 0; (p = v->str.find("\\textcolor{", p)) != std::string::npos;) {
      size_t a = p + 11, b = v->str.find('}', a);
      if (b == std::string::npos) break;
      std::string name = v->str.substr(a, b - a);
      Color c;
      if (!defined.count(name) && !parts.count(name) && !colorByName(name, c))
        warn("tex: line " + std::to_string(ln.lineNo) + ": \\textcolor{" + name + "} is neither a thing on stage, a part an annotation points at, nor a color name");
      p = b;
    }
  });
}

void Engine::usePrescan() {
  if (scenePrescan_.empty()) return;
  const Prescan &p = scenePrescan_[std::min(sceneIdx_, scenePrescan_.size() - 1)];
  maxAbs_ = p.maxAbs; minVal_ = p.minVal; maxVal_ = p.maxVal; propIsInt_ = p.isInt;
}

void Engine::reset() {
  stopVoice();
  cursor_ = 0;
  hookFired_.assign(script_.steps.size(), false);
  sayStep_ = 0;
  firedBySay_.clear();
  marks_.clear(); links_.clear(); groups_.clear(); order_.clear();
  actives_.clear();
  say_ = SayRun{};
  focus_.clear();
  schema_ = "auto";
  schemaProps_.clear();
  views_.clear();
  curView_.clear();
  dataCtx_.clear();
  curData_.clear();
  title_.clear();
  sceneIdx_ = 0;
  if (!autoSchemas_.empty()) autoSchema_ = autoSchemas_[0];
  usePrescan();
  stageFade_ = 1;
  since_.clear();
  caption_.clear();
  colorSlot_.clear();
  nextSlot_ = 0;
  rangesInit_ = false;
  barScale_.clear();
  graphSig_.clear();
  graphPos_.clear();
  flowMemo_.clear();
  bandH_ = -1;
  bandNeed_ = 0;
  zoom_ = zoomT_ = 1;
  zoomFresh_ = true;
  zoomSettle_ = 0.6f;
  linkRoute_.clear();
  cam_ = Camera{};
  hist_.clear();
  bornAt_.clear();
  clock_ = 0;
  lastTex_.clear();
  labelSlots_.clear();
  linkSlots_.clear();
  audioLog_.clear();
  paused_ = false;
  waitingPause_ = false;
  readbackPrinted_ = false;
  idleSent_ = false;
  emit("reset");
  // runtime lint is regenerated, static lint kept
  for (auto it = lint_.begin(); it != lint_.end();)
    it = (it->rfind("identity", 0) == 0 || it->rfind("zero", 0) == 0) ? lint_.erase(it) : std::next(it);
}

static std::string jsonStr(const std::string &s) {
  std::string o = "\"";
  for (unsigned char c : s) {
    if (c == '"' || c == '\\') { o += '\\'; o += (char)c; }
    else if (c == '\n') o += "\\n";
    else if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); o += b; }
    else o += (char)c;
  }
  return o + "\"";
}

void Engine::emit(const std::string &ev) {
  if (!opt_.events) return;
  std::printf("@ev %s\n", ev.c_str());
  std::fflush(stdout);
}

void Engine::interrupt() {
  if (say_.active) emit("end " + std::to_string(say_.uid));
  if (tts_) tts_->stop();
  prefetched_.clear(); // beats asked for ahead won't come now
  bool was = opt_.instant;
  opt_.instant = true;
  say_.active = false;
  say_.lingering = 0;
  waitingPause_ = false;
  paused_ = false;
  for (auto &a : actives_) {
    if (a.kind == Active::NUM && a.t < a.dur) { // it jumps to its end now: history says so too
      Obj *o = find(a.id);
      const Value *cur = o ? findProp(o->props, a.prop) : nullptr;
      recordSet(a.id, a.prop, cur ? *cur : Value::number(a.from), a.finalVal);
    }
    a.t = a.dur + 1;
  }
  tickActives(0);
  while (!finished()) update(0);
  opt_.instant = was;
  for (auto &id : order_) if (Obj *o = find(id)) o->appear = 1;
  readbackPrinted_ = false;
}

void Engine::warn(const std::string &msg) {
  if (lint_.insert(msg).second && !opt_.shadow) std::fprintf(stderr, "lint: %s\n", msg.c_str());
}

void Engine::caption(const std::string &rule) {
  caption_ = rule;
  captionT_ = 0;
}

// ================================================================ model access
Obj *Engine::find(const std::string &id) {
  if (auto it = marks_.find(id); it != marks_.end()) return &it->second;
  if (auto it = links_.find(id); it != links_.end()) return &it->second;
  if (auto it = groups_.find(id); it != groups_.end()) return &it->second;
  return nullptr;
}
Mark *Engine::mark(const std::string &id) {
  auto it = marks_.find(id);
  return it == marks_.end() ? nullptr : &it->second;
}

static int gEvalDepth = 0;

double Engine::num(const Value &v, int depth) {
  if (v.type == Value::NUM) return v.num;
  if (v.type == Value::EXPR && v.expr) {
    if (gEvalDepth > 48) return NAN;
    gEvalDepth++;
    bool hx = hasX;
    hasX = false;
    double r = eval(*v.expr, *this, depth);
    hasX = hx;
    gEvalDepth--;
    return r;
  }
  if (v.type == Value::STR) {
    char *end;
    double d = std::strtod(v.str.c_str(), &end);
    if (end != v.str.c_str() && *end == 0) return d;
  }
  return NAN;
}

double Engine::num(const Obj &o, const std::string &key, bool *ok, int depth) {
  if (std::isfinite(histT_)) { // as it was at histT_ (trails)
    double h;
    if (histValue(o.id, key, histT_, h)) { if (ok) *ok = true; return h; }
  }
  const Value *v = findProp(o.props, key);
  if (!v || !v->isNumeric()) {
    if (ok) *ok = false;
    return 0;
  }
  if (ok) *ok = true;
  return num(*v, depth);
}

std::string Engine::magProp(const Mark &m) const {
  if (m.kind == "path" || m.kind == "region" || isFieldKind(m.kind) || m.kind == "solution") return "";
  for (auto &p : m.props) {
    bool positional = kPositional.count(p.key) && !(p.key == "m" && m.kind != "grid"); // m= is a matrix only on a grid
    if (p.val.isNumeric() && !positional && !kTextKeys.count(p.key)) return p.key;
  }
  return "";
}
std::string Engine::catProp(const Mark &m) const {
  if (!m.category.empty()) return "@category";
  for (auto &p : m.props)
    if (p.val.type == Value::STR && !kTextKeys.count(p.key) && !isPlacementKey(p.key)) return p.key;
  return "";
}
std::string Engine::catValue(const Mark &m) const {
  std::string k = catProp(m);
  if (k.empty()) return "";
  if (k == "@category") return m.category;
  return findProp(m.props, k)->str;
}

std::string Engine::effectiveKind(const Mark &m) const {
  if (!m.kind.empty()) return m.kind;
  if (findProp(m.props, "tex")) return "tex";
  std::string s = schemaOf(m);
  bool hasMag = !magProp(m).empty();
  if (findProp(m.props, "shape")) return "dot"; // shape is a channel only dots carry
  if (s == "graph" || s == "sequence") return "box";
  if (s == "numberline" || s == "timeline") return "dot";
  if (s == "axes") {
    if (findProp(m.props, "x") && findProp(m.props, "y")) return "dot";
    if (findProp(m.props, "fn") || findProp(m.props, "xt")) return "path";
    return hasMag ? "label" : "dot";
  }
  return hasMag ? "bar" : "dot";
}

std::string Engine::topGroup(const std::string &id) const {
  std::string g = groupOf(id), last;
  for (int guard = 0; !g.empty() && guard < 16; guard++) { last = g; g = groupOf(g); }
  return last;
}

std::string Engine::groupOf(const std::string &id) const {
  for (auto &g : order_) {
    auto it = groups_.find(g);
    if (it == groups_.end()) continue;
    for (auto &m : it->second.members)
      if (m == id) return g;
  }
  return "";
}

bool Engine::related(const std::string &a, const std::string &b) {
  for (auto &[id, l] : links_)
    if ((l.a == a && l.b == b) || (l.a == b && l.b == a)) return true;
  std::string ga = groupOf(a), gb = groupOf(b);
  if (!ga.empty() && ga == gb) return true;
  Mark *ma = mark(a), *mb = mark(b);
  if (ma && mb) {
    std::string ca = catValue(*ma), cb = catValue(*mb);
    if (!ca.empty() && ca == cb) return true;
    // color= is the author saying "these are the same" (a cluster, a shared weight)
    auto cv = findProp(ma->props, "color"), cw = findProp(mb->props, "color");
    if (cv && cw && cv->type == Value::STR && cw->type == Value::STR && cv->str == cw->str) return true;
    auto of = [](Mark *m) { auto v = findProp(m->props, "of"); return v ? v->str : std::string(); };
    if (of(ma) == b || of(mb) == a) return true;
  }
  return false;
}

std::vector<std::string> Engine::boxNameLines(const Mark &m) { return wrap(displayName(m), 22, 260); }

std::vector<std::pair<std::string, std::string>> Engine::boxRows(const Mark &m) {
  static const std::set<std::string> notContent = {"label", "name", "color", "unit", "shape", "ease", "style", "cmap",
                                                   "tex", "text", "fn", "of", "note", "title", "side"};
  std::vector<std::pair<std::string, std::string>> rows;
  for (auto &p : m.props) {
    if (notContent.count(p.key) || isPlacementKey(p.key) || (kPositional.count(p.key) && p.key != "m")) continue;
    if (p.key == "m" && p.val.type == Value::LIST) continue; // a matrix (on a grid), not a magnitude
    rows.push_back({p.key, p.val.isNumeric() ? valueText(m, p.key) : p.val.type == Value::STR ? p.val.str : p.val.markup()});
  }
  return rows;
}

std::string Engine::displayName(const Obj &o) const {
  if (auto v = findProp(o.props, "label"); v && v->type == Value::STR) return v->str;
  if (auto v = findProp(o.props, "name"); v && v->type == Value::STR) return v->str;
  std::string s = o.id;
  for (auto &c : s) if (c == '_') c = ' ';
  if (!s.empty()) s[0] = (char)std::toupper((unsigned char)s[0]);
  return s;
}

// ================================================================ EvalCtx
bool Engine::var(const std::string &name, double &out) {
  Mark *m = mark(name);
  if (!m) return false;
  std::string k = magProp(*m);
  if (k.empty()) return false;
  out = num(*m, k);
  return true;
}
bool Engine::member(const std::string &id, const std::string &prop, double &out) {
  Obj *o = find(id);
  if (!o) return false;
  bool ok;
  out = num(*o, prop, &ok);
  return ok;
}
bool Engine::call(const std::string &name, const std::vector<ExprPtr> &args, double &out, int depth) {
  static const std::set<std::string> agg = {"mean", "avg", "average", "total", "sum", "maxof", "minof", "count", "median"};
  if (agg.count(name) && args.size() == 1 && args[0]->kind == Expr::VAR) {
    const std::string &prop = args[0]->name;
    std::vector<double> vals;
    for (auto &id : order_) {
      Mark *m = mark(id);
      if (!m) continue;
      const Value *v = findProp(m->props, prop);
      if (v && v->type == Value::NUM) vals.push_back(v->num); // literal values only: no self reference
    }
    if (vals.empty()) { out = NAN; return true; }
    double s = 0;
    for (double v : vals) s += v;
    if (name == "total" || name == "sum") out = s;
    else if (name == "count") out = (double)vals.size();
    else if (name == "maxof") out = *std::max_element(vals.begin(), vals.end());
    else if (name == "minof") out = *std::min_element(vals.begin(), vals.end());
    else if (name == "median") {
      std::sort(vals.begin(), vals.end());
      size_t n = vals.size();
      out = n % 2 ? vals[n / 2] : (vals[n / 2 - 1] + vals[n / 2]) / 2;
    } else out = s / vals.size();
    return true;
  }
  // calculus helpers over a path mark: integral(f, a, b), deriv(f, x)
  if ((name == "integral" || name == "deriv") && !args.empty() && args[0]->kind == Expr::VAR) {
    Mark *p = mark(args[0]->name);
    if (!p || !findProp(p->props, "fn")) return false;
    if (name == "deriv" && args.size() == 2) {
      double x = eval(*args[1], *this, depth), h = 1e-4;
      out = (pathAt(*p, x + h, depth) - pathAt(*p, x - h, depth)) / (2 * h);
      return true;
    }
    if (name == "integral" && args.size() == 3) {
      double a = eval(*args[1], *this, depth), b = eval(*args[2], *this, depth);
      const int n = 200; // Simpson
      double h = (b - a) / n, s = pathAt(*p, a, depth) + pathAt(*p, b, depth);
      for (int i = 1; i < n; i++) s += pathAt(*p, a + i * h, depth) * (i % 2 ? 4 : 2);
      out = s * h / 3;
      return true;
    }
    return false;
  }
  Mark *m = mark(name);
  if (m && findProp(m->props, "fn") && args.size() == 1) {
    double x = eval(*args[0], *this, depth);
    out = pathAt(*m, x, depth);
    return true;
  }
  return false;
}

bool Engine::isParametric(const Mark &m) const { return findProp(m.props, "xt") && findProp(m.props, "yt"); }

// a parametric curve: xt={...} yt={...} in terms of t, sampled over range=[a,b]
bool Engine::paramAt(const Mark &p, double t, double &x, double &y) {
  const Value *xt = findProp(p.props, "xt"), *yt = findProp(p.props, "yt");
  if (!xt || !yt || xt->type != Value::EXPR || yt->type != Value::EXPR) return false;
  if (gEvalDepth > 48) return false;
  gEvalDepth++;
  double ox = this->x;
  bool ohx = hasX;
  const char *on = xName;
  this->x = t;
  hasX = true;
  xName = "t";
  x = eval(*xt->expr, *this);
  y = eval(*yt->expr, *this);
  this->x = ox;
  hasX = ohx;
  xName = on;
  gEvalDepth--;
  return std::isfinite(x) && std::isfinite(y);
}

void Engine::clearStage() {
  views_.clear();
  curView_.clear();
  dataCtx_.clear();
  curData_.clear();
  marks_.clear();
  links_.clear();
  groups_.clear();
  order_.clear();
  focus_.clear();
  actives_.erase(std::remove_if(actives_.begin(), actives_.end(),
                                [](const Active &a) { return a.kind == Active::NUM || a.kind == Active::STR; }),
                 actives_.end());
  colorSlot_.clear();
  nextSlot_ = 0;
  labelSlots_.clear();
  linkSlots_.clear();
  barScale_.clear();
  graphSig_.clear();
  graphPos_.clear();
  flowMemo_.clear();
  bandH_ = -1;
  bandNeed_ = 0;
  zoom_ = zoomT_ = 1;
  zoomFresh_ = true;
  zoomSettle_ = 0.6f;
  linkRoute_.clear();
  rangeSig_.clear();
  rangesInit_ = false;
  cam_ = Camera{};
  blockRows_ = false;
  pointerSide_.clear();
  annVariant_.clear();
  crossedSince_.clear();
  clearFieldCaches();
  hist_.clear();
  bornAt_.clear();
  schema_ = "auto";
  schemaProps_.clear();
  if (sceneIdx_ + 1 < autoSchemas_.size()) autoSchema_ = autoSchemas_[++sceneIdx_];
  usePrescan();
  since_.push_back("scene change");
}

double Engine::pathAt(const Mark &p, double x, int depth) {
  const Value *fn = findProp(p.props, "fn");
  if (!fn || fn->type != Value::EXPR || !fn->expr) return NAN;
  if (gEvalDepth > 48) return NAN;
  gEvalDepth++;
  double ox = this->x;
  bool ohx = hasX;
  const char *on = xName;
  this->x = x;
  hasX = true;
  xName = "x";
  double r = eval(*fn->expr, *this, depth);
  this->x = ox;
  hasX = ohx;
  xName = on;
  gEvalDepth--;
  return r;
}

// ================================================================ colors
Color Engine::colorForKey(const std::string &key) {
  auto it = colorSlot_.find(key);
  if (it == colorSlot_.end()) {
    if (nextSlot_ == categoricalCount())
      warn("zero: more than " + std::to_string(categoricalCount()) + " distinct kinds of thing, colors repeat");
    it = colorSlot_.emplace(key, nextSlot_++).first;
  }
  return categorical(it->second);
}
void Engine::claimColor(const Mark &m) {
  if (findProp(m.props, "of") || findProp(m.props, "color")) return;
  std::string c = catValue(m);
  if (c.empty() && isBlockKind(effectiveKind(m))) return; // formulas, cards, code: ink, not a color slot
  if (effectiveKind(m) == "param") return;                  // an invisible driver (a clock, a constant)
  colorForKey(c.empty() ? "id:" + m.id : "cat:" + c);
}
Color Engine::colorOf(const Mark &m) {
  if (auto v = findProp(m.props, "color"); v && v->type == Value::STR) {
    Color c;
    if (colorByName(v->str, c)) return c;
  }
  if (auto v = findProp(m.props, "of"); v && v->type == Value::STR) {
    if (Mark *p = mark(v->str); p && p != &m) return colorOf(*p);
  }
  std::string cp = catProp(m);
  std::string cv = catValue(m);
  Color now = colorForKey(cv.empty() ? "id:" + m.id : "cat:" + cv);
  if (!cp.empty()) {
    auto it = m.strBlend.find(cp);
    if (it != m.strBlend.end()) return lerpColor(colorForKey("cat:" + it->second.first), now, it->second.second);
  }
  return now;
}

// ================================================================ runtime
// things created at the same moment enter one after another, 0.15 s apart (a lag of about 0.15 of an
// entrance, between his 0.1 for fades and 0.25 for distinct items), at most a second behind the first
float Engine::entranceDelay() {
  if (opt_.instant) return 0;
  if (clock_ != staggerAt_) { staggerAt_ = clock_; staggerN_ = 0; }
  return std::min(1.f, 0.15f * staggerN_++);
}

void Engine::execStep(size_t idx) {
  const Step &s = script_.steps[idx];
  if (s.main.verb == Verb::SAY) sayStep_ = idx;
  execLine(s.main);
  for (auto &w : s.withs) execLine(w);
}

void Engine::execLine(const Line &ln) {
  if (opt_.verbose) std::fprintf(stderr, "[%6.2fs] line %d: %s\n", clock_, ln.lineNo, ln.raw.c_str());
  switch (ln.verb) {
  case Verb::MARK: doMark(ln); break;
  case Verb::LINK: doLink(ln); break;
  case Verb::GROUP: doGroup(ln); break;
  case Verb::MORPH: doMorph(ln); break;
  case Verb::FOCUS:
    focus_ = ln.id == "none" ? "" : ln.id;
    if (!focus_.empty() && !find(baseId(focus_))) warn("focus: '" + focus_ + "' does not exist yet");
    caption(focus_.empty() ? "focus none -> everything at full strength"
                           : "the voice is about " + focus_ + " -> focus: it stays lit, the rest dims (axiom 5)");
    break;
  case Verb::INDICATE: { // a target that isn't on stage yet gets up to 2 s to appear (lines arrive in beats)
    auto targets = staggerTargets(ln.id, ln.stagger);
    for (size_t k = 0; k < targets.size(); k++) {
      Active a;
      a.kind = Active::PULSE;
      a.id = baseId(targets[k]);
      a.prop = targets[k];
      a.dur = opt_.instant ? 0 : 1.1f;
      a.delay = opt_.instant ? 0 : (float)k * ln.stagger;
      a.blocking = false;
      actives_.push_back(a);
    }
    caption("pointing -> " + ln.id + (targets.size() > 1 ? " pulses cell by cell" : " pulses") +
            " (signaling: the eye goes where the voice is)");
    break;
  }
  case Verb::SEND: { // a copy travels from one thing (or part) to another, then the target pulses
    // the source is what is staggered; a target span as long as it receives cell by cell, anything else all of it
    auto from = staggerTargets(ln.a, ln.stagger);
    auto to = ln.b.find(':') != std::string::npos ? staggerTargets(ln.b, ln.stagger) : std::vector<std::string>{ln.b};
    bool pairwise = from.size() > 1 && to.size() == from.size(); // a span onto a span as long: cell by cell
    for (size_t k = 0; k < from.size(); k++) {
      Active a;
      a.kind = Active::FLIGHT;
      a.prop = from[k];
      a.dest = pairwise ? to[k] : ln.b;
      a.via = ln.id;
      a.dur = opt_.instant ? 0 : ln.dur;
      a.delay = opt_.instant ? 0 : (float)k * ln.stagger;
      a.blocking = false;
      actives_.push_back(a);
    }
    caption("send " + ln.a + " -> " + ln.b + ": a copy travels there (where a value comes from, where it goes)");
    break;
  }
  case Verb::SAY: startSay(ln); break;
  case Verb::WAIT: {
    Active a;
    a.kind = Active::WAIT;
    a.dur = opt_.instant ? 0 : ln.dur;
    actives_.push_back(a);
    break;
  }
  case Verb::PAUSE:
    if (!opt_.instant) waitingPause_ = true;
    break;
  case Verb::SCHEMA:
    // an unnamed schema and named ones in one scene: the unnamed one is a view too ("main"), so each keeps
    // its own part of the stage (a plane with its field beside a network, not under it)
    if (!ln.id.empty() && views_.empty() && schema_ != "auto") {
      views_.push_back({"main", schema_, schemaProps_});
      for (auto &[id, m] : marks_) if (m.view.empty()) m.view = "main";
      if (isAxes(schema_) || schema_ == "numberline" || schema_ == "timeline") curData_ = "main";
    }
    if (!ln.id.empty() || !views_.empty()) { // a named view: create it, or select it for the marks that follow
      Line named = ln;
      if (named.id.empty()) named.id = "main";
      const Line &ln = named;
      auto v = std::find_if(views_.begin(), views_.end(), [&](const View &w) { return w.id == ln.id; });
      if (v == views_.end()) { views_.push_back({ln.id, ln.kind.empty() ? "free" : ln.kind, ln.props}); v = views_.end() - 1; }
      else if (!ln.kind.empty()) { v->schema = ln.kind; v->props = ln.props; }
      curView_ = ln.id;
      bool data = isAxes(v->schema) || v->schema == "numberline" || v->schema == "timeline";
      if (data && !ln.kind.empty()) { // the first data view's state is the members'; each later one has its own
        if (curData_.empty() || curData_ == v->id) {
          curData_ = v->id;
          schema_ = v->schema;
          schemaProps_ = v->props;
          rangesInit_ = false;
        } else {
          DataCtx &c = dataCtx_[v->id];
          c.schema = v->schema;
          c.props = v->props;
          c.rangesInit = false;
        }
      }
      caption("view " + ln.id + " (" + v->schema + ") -> its own part of the stage, its own layout rules");
      break;
    }
    schema_ = ln.kind;
    schemaProps_ = ln.props;
    rangesInit_ = false;
    caption("schema " + ln.kind + " -> its own layout rules decide position");
    break;
  case Verb::TITLE: title_ = ln.id; headline_ = stageEmpty() ? 0 : 1; break;
  case Verb::CAMERA: doCamera(ln); break;
  case Verb::SCENE: {
    Active a;
    a.kind = Active::CLEAR;
    a.prop = ln.id;
    a.dur = opt_.instant ? 0 : 0.7f;
    actives_.push_back(a);
    caption("new scene -> the stage clears; ids start fresh");
    break;
  }
  }
}

void Engine::doMark(const Line &ln) {
  if (auto lk = links_.find(ln.id); lk != links_.end()) { // "mark w11 color=yellow" on a link: update it in place
    for (auto &p : ln.props) setProp(lk->second.props, p.key, p.val);
    caption("re-mark " + ln.id + " -> the link updates in place (identity)");
    return;
  }
  if (links_.count(ln.id) || groups_.count(ln.id)) {
    warn("identity: '" + ln.id + "' is already a link/group; ids must be unique");
    return;
  }
  auto it = marks_.find(ln.id);
  if (it == marks_.end()) {
    Mark m;
    m.id = ln.id;
    m.kind = ln.kind;
    m.category = ln.category;
    m.props = ln.props;
    m.order = (int)order_.size();
    m.appear = opt_.instant ? 1 : 0;
    m.enterIn = entranceDelay();
    m.view = curView_;
    order_.push_back(ln.id);
    Mark &mm = marks_.emplace(ln.id, m).first->second;
    bornAt_[ln.id] = clock_;
    claimColor(mm);
    std::string k = effectiveKind(mm), mp = magProp(mm), cv = catValue(mm);
    std::string rule = "thing -> a " + k;
    if (!mp.empty() && (k == "bar")) rule += " · magnitude " + mp + " -> length from a shared zero";
    else if (!mp.empty() && k == "dot" && schemaOf(mm) == "numberline") rule += " · " + mp + " -> position on the line";
    else if (!mp.empty() && k == "dot") rule += " · magnitude " + mp + " -> area";
    if (k == "path") rule += " · a function -> a curve";
    if (k == "region") rule += " · part of a function -> filled area";
    if (k == "tex") rule = "formula -> typeset; \\val{x} shows a live value, \\textcolor{id}{…} ties a term to a thing";
    if (!cv.empty()) rule += " · category '" + cv + "' -> color";
    else if (k != "label" && k != "region" && k != "tex") rule += " · own color (unrelated to the rest)";
    caption(rule);
    return;
  }
  // re-mark: update in place, never redraw (axiom 1)
  Mark &m = it->second;
  if (!ln.kind.empty()) m.kind = ln.kind;
  if (!ln.category.empty() && ln.category != m.category) {
    m.strBlend["@category"] = {m.category, opt_.instant ? 1.f : 0.f};
    m.category = ln.category;
    colorForKey("cat:" + ln.category);
    Active a;
    a.kind = Active::STR; a.id = m.id; a.prop = "@category"; a.dur = opt_.instant ? 0 : 0.8f; a.blocking = false;
    actives_.push_back(a);
  }
  for (auto &p : ln.props) {
    settleActive(m.id, p.key);
    Value *cur = findProp(m.props, p.key);
    if (cur && cur->type == Value::NUM && p.val.type == Value::NUM && cur->num != p.val.num) {
      Active a;
      a.kind = Active::NUM; a.id = m.id; a.prop = p.key;
      a.from = cur->num; a.to = p.val.num; a.finalVal = p.val;
      a.dur = opt_.instant ? 0 : 0.7f;
      a.blocking = false;
      a.log = "mark " + m.id + "." + p.key + " " + fmtNum(a.from) + " -> " + fmtNum(a.to);
      recordActive(a, *cur);
      actives_.push_back(a);
    } else if (cur && cur->type == Value::LIST && p.val.type == Value::LIST && cur->markup() != p.val.markup()) {
      // a list re-marked: points glide, array items travel to their new cells
      m.strBlend[p.key] = {cur->markup(), opt_.instant ? 1.f : 0.f};
      *cur = p.val;
      Active a;
      a.kind = Active::STR; a.id = m.id; a.prop = p.key; a.dur = opt_.instant ? 0 : 0.9f; a.blocking = false;
      actives_.push_back(a);
    } else if (cur && cur->type == Value::STR && p.val.type == Value::STR && cur->str != p.val.str) {
      m.strBlend[p.key] = {cur->str, opt_.instant ? 1.f : 0.f};
      *cur = p.val;
      if (p.key == catProp(m)) colorForKey("cat:" + p.val.str); // only a category's new value is a colour (not a label, a pointer's at=)
      Active a;
      a.kind = Active::STR; a.id = m.id; a.prop = p.key; a.dur = opt_.instant ? 0 : 0.8f; a.blocking = false;
      actives_.push_back(a);
    } else {
      if (p.val.isNumeric() || (cur && cur->isNumeric())) recordSet(m.id, p.key, cur ? *cur : Value::number(NAN), p.val);
      setProp(m.props, p.key, p.val);
    }
  }
  caption("re-mark " + m.id + " -> same mark, updated in place (identity)");
}

void Engine::doLink(const Line &ln) {
  if (marks_.count(ln.id) || groups_.count(ln.id)) { warn("identity: '" + ln.id + "' is already used"); return; }
  if (!find(ln.a) || !find(ln.b)) warn("link: " + ln.id + " connects '" + ln.a + "' and '" + ln.b + "' but one is not on stage yet");
  auto it = links_.find(ln.id);
  if (it == links_.end()) {
    Link l;
    l.id = ln.id; l.kind = ln.kind; l.a = ln.a; l.b = ln.b; l.directed = ln.directed;
    l.props = ln.props; l.order = (int)order_.size(); l.appear = opt_.instant ? 1 : 0; l.enterIn = entranceDelay();
    order_.push_back(ln.id);
    links_.emplace(ln.id, l);
  } else {
    Link &l = it->second;
    l.kind = ln.kind; l.a = ln.a; l.b = ln.b; l.directed = ln.directed;
    for (auto &p : ln.props) setProp(l.props, p.key, p.val);
  }
  if (ln.kind == "arrow") caption("relation (cause / maps-to) -> an arrow between the marks");
  else if (ln.kind == "similar") caption("relation (similar) -> dashed link marked ≈");
  else if (ln.kind == "equals") caption("relation (equals) -> dashed link marked =");
  else caption("relation (connected) -> a line between the marks");
}

void Engine::doGroup(const Line &ln) {
  if (marks_.count(ln.id) || links_.count(ln.id)) { warn("identity: '" + ln.id + "' is already used"); return; }
  auto it = groups_.find(ln.id);
  if (it == groups_.end()) {
    Group g;
    g.id = ln.id; g.kind = "box"; g.members = ln.members; g.props = ln.props;
    g.order = (int)order_.size(); g.appear = opt_.instant ? 1 : 0; g.enterIn = entranceDelay();
    order_.push_back(ln.id);
    groups_.emplace(ln.id, g);
  } else {
    it->second.members = ln.members;
    for (auto &p : ln.props) setProp(it->second.props, p.key, p.val);
  }
  caption("part-of -> an enclosure around " + std::to_string(ln.members.size()) + " parts");
}

// A new change to id.prop takes over from any change still running on it: one that has already reached
// its end (instant mode, or a zero duration not yet ticked) lands first, so the new one starts from there.
void Engine::settleActive(const std::string &id, const std::string &prop) {
  for (auto it = actives_.begin(); it != actives_.end();) {
    if (it->id == id && it->prop == prop && (it->kind == Active::NUM || it->kind == Active::STR)) {
      bool done = it->dur <= 0 || it->t >= it->dur;
      if (it->kind == Active::NUM && done)
        if (Obj *o = find(id)) setProp(o->props, prop, it->finalVal);
      if (done && !it->log.empty()) { // as if it had been ticked to its end
        since_.push_back(it->log + " done");
        if (since_.size() > 16) since_.erase(since_.begin());
      }
      if (it->kind == Active::STR)
        if (Obj *o = find(id)) o->strBlend.erase(prop);
      it = actives_.erase(it);
    } else {
      ++it;
    }
  }
}

void Engine::doMorph(const Line &ln) {
  Obj *o = find(ln.id);
  if (!o) { warn("morph: no thing called '" + ln.id + "' (line " + std::to_string(ln.lineNo) + ")"); return; }
  // a newer morph of the same property takes over
  settleActive(ln.id, ln.prop);
  Value *cur = findProp(o->props, ln.prop);
  std::string target = ln.id + "." + ln.prop;
  float dur = opt_.instant ? 0 : std::max(0.f, ln.dur);

  if (ln.to.type == Value::STR || ln.to.type == Value::LIST) {
    std::string old = cur ? (cur->type == Value::STR ? cur->str : cur->markup()) : "";
    bool list = ln.to.type == Value::LIST;
    if (ln.hasFrom && cur && (list ? ln.from.markup() != old : ln.from.markup() != Value::string(old).markup() && ln.from.str != old))
      warn("identity: line " + std::to_string(ln.lineNo) + " morphs " + target + " from '" + ln.from.str +
           "' but it is '" + old + "'; morphing from the real value");
    setProp(o->props, ln.prop, ln.to);
    o->strBlend[ln.prop] = {old, dur > 0 ? 0.f : 1.f};
    if (Mark *om = mark(ln.id); om && !list && ln.prop == catProp(*om)) colorForKey("cat:" + ln.to.str); // a category's new value only
    Active a;
    a.kind = Active::STR; a.id = ln.id; a.prop = ln.prop; a.dur = dur;
    a.ease = rateByName(ln.ease);
    a.log = "morph " + target + " " + Value::string(old).markup() + " -> " + ln.to.markup();
    actives_.push_back(a);
    caption(ln.prop == "tex" ? "change of formula -> shared symbols move to their new places, the rest fade (" + target + ")"
                             : "change of category -> color morphs, label cross-fades (" + target + ")");
    return;
  }

  double start = cur ? num(*cur) : (ln.hasFrom ? num(ln.from) : ln.prop == "opacity" ? 1 : 0);
  if (!std::isfinite(start)) start = ln.hasFrom ? num(ln.from) : 0;
  if (ln.hasFrom && cur) {
    double f = num(ln.from);
    if (std::fabs(f - start) > 1e-6 * std::max(1.0, std::fabs(start)))
      warn("identity: line " + std::to_string(ln.lineNo) + " morphs " + target + " from " + fmtNum(f) + " but it is " +
           fmtNum(start) + "; morphing from the real value (no jump cuts)");
  }
  Value before = cur ? *cur : Value::number(start);
  if (!cur) setProp(o->props, ln.prop, Value::number(start));
  Active a;
  a.kind = Active::NUM; a.id = ln.id; a.prop = ln.prop;
  a.from = start; a.to = num(ln.to); a.finalVal = ln.to;
  a.dur = dur;
  a.ease = rateByName(ln.ease);
  recordActive(a, before);
  a.log = "morph " + target + " " + fmtNum(a.from) + " -> " + ln.to.markup();
  actives_.push_back(a);

  std::string ch = "its value";
  if (Mark *m = mark(ln.id)) {
    std::string k = effectiveKind(*m);
    if (ln.prop == magProp(*m)) ch = k == "bar" ? "bar length" : k == "dot" ? (schemaOf(*m) == "numberline" ? "position" : "dot area") : "the number shown";
    if (ln.prop == "x" || ln.prop == "y" || ln.prop == "t") ch = "position";
    if (ln.prop == "from" || ln.prop == "to") ch = "the region's extent";
  }
  caption("change -> motion over time: " + target + " drives " + ch + " (it morphs, never redrawn)");
}

void Engine::startSay(const Line &ln) {
  if (opt_.instant) sayFacts_ = stageFacts(); // for the binding check at the end of the sentence
  say_ = SayRun{};
  say_.active = true;
  say_.uid = ln.uid;
  if (opt_.events && !opt_.instant) emit("say " + std::to_string(ln.uid) + " " + jsonStr(spokenText(ln)));
  say_.words = ln.words;
  if (!opt_.instant)
    if (auto said = spokenWords(ln); said.size() == say_.words.size())
      for (size_t i = 0; i < said.size(); i++) say_.words[i].text = said[i];
  say_.trailing = ln.trailingHooks;
  // word timing: proportional to length, with pauses at punctuation
  std::vector<float> w;
  for (auto &sw : ln.words) {
    float x = 0.11f + 0.058f * (float)GetCodepointCount(sw.text.c_str());
    char last = sw.text.empty() ? ' ' : sw.text.back();
    if (last == ',' || last == ';' || last == ':') x += 0.18f;
    if (last == '.' || last == '!' || last == '?') x += 0.32f;
    w.push_back(x);
  }
  float scale = 170.f / std::max(60.f, opt_.wpm);
  float t = 0;
  for (float x : w) {
    say_.start.push_back(t);
    t += x * scale;
    say_.end.push_back(t);
  }
  say_.total = t;
  caption("narration -> the voice; |markers| fire the lines tagged @marker");
  if (auto u = lineUtt_.find(ln.uid); u != lineUtt_.end() && tts_ && !opt_.instant) {
    say_.utt = u->second;
    // until Inworld's timestamps arrive, a word has not been reached
    std::fill(say_.start.begin(), say_.start.end(), 1e9f);
    std::fill(say_.end.begin(), say_.end.end(), 1e9f);
    say_.total = 1e9f;
    tts_->play(say_.utt);
  }
  if (opt_.instant) {
    std::vector<std::string> said; // each word as spoken: its value once the lines cued on it have run
    // pointing ahead: a number or a name that exactly one thing on stage shows, as its word is reached
    std::vector<std::string> raw, toks;
    std::vector<size_t> owner;
    for (auto &sw : say_.words) raw.push_back(sw.text);
    sayTokens(raw, toks, owner);
    auto nums = readNumbers(toks);
    for (size_t i = 0; i < say_.words.size(); i++) {
      const SayWord &sw = say_.words[i];
      for (auto &h : sw.hooks) fireHook(h);
      said.push_back(sw.ref ? spokenWord(sw.text) : sw.text);
      bool pointed = std::any_of(sw.hooks.begin(), sw.hooks.end(), [](const std::string &h) { return h.rfind("@pt:", 0) == 0; });
      auto consider = [&](const std::string &holder) {
        pointStats_.spoken++;
        if (holder.empty()) return;
        pointStats_.onStage++;
        if (pointed) pointStats_.pointed++;
        else if (holder != "?") {
          autoPoint_[ln.uid].push_back({i, holder});
          pointStats_.added++;
          pointed = true;
          if (std::getenv("TUTOR_DUMP_POINT")) std::fprintf(stderr, "point: line %d \"%s\" -> %s\n", ln.lineNo, sw.text.c_str(), holder.c_str());
        }
      };
      for (auto &n : nums) if (owner[n.first] == i && pointable(n, toks)) consider(holderOfValue(n.v, n.percent));
      for (size_t t = 0; t < toks.size(); t++)
        if (owner[t] == i && toks[t].size() == 1 && std::isalpha((unsigned char)toks[t][0]) && toks[t] != "a" && toks[t] != "i")
          consider(holderOfName(toks[t]));
    }
    spoken_[ln.uid] = said;
    for (auto &h : say_.trailing) fireHook(h);
    closeSentence();
    say_.active = false;
  }
}

void Engine::fireHook(const std::string &name) {
  if (opt_.verbose)
    std::fprintf(stderr, "[%6.2fs] hook |%s| at %.2fs into the line%s\n", clock_, name.c_str(), say_.t,
                 say_.utt ? " (inworld timestamps)" : "");
  if (name.rfind("@pt:", 0) == 0) { // [id] in the sentence: point at it on this word
    Line ln;
    ln.verb = Verb::INDICATE;
    ln.id = name.substr(4);
    execLine(ln);
    return;
  }
  firedBySay_[sayStep_].insert(name);
  // A hook belongs to its sentence: fire the @name lines between this Say and the next one,
  // so the same marker name can be reused sentence after sentence. Lines placed elsewhere
  // (older scripts) are still found: first anywhere after this Say, then anywhere.
  auto fireIn = [&](size_t a, size_t b) {
    bool any = false;
    for (size_t i = a; i < b && i < script_.steps.size(); i++)
      if (script_.steps[i].hook == name && !hookFired_[i]) {
        hookFired_[i] = true;
        any = true;
        execStep(i);
      }
    return any;
  };
  size_t next = sayStep_ + 1;
  while (next < script_.steps.size() && script_.steps[next].main.verb != Verb::SAY) next++;
  if (fireIn(sayStep_ + 1, next)) return;
  if (fireIn(next, script_.steps.size())) return;
  fireIn(0, sayStep_);
}

// A cue line whose |cue| was never spoken (a typo, a capital, a forgotten marker) still runs, at the
// end of its sentence: a slip in the script must not hide the point of the explanation.
void Engine::closeSentence() {
  size_t next = sayStep_ + 1;
  while (next < script_.steps.size() && script_.steps[next].main.verb != Verb::SAY) next++;
  for (size_t i = sayStep_ + 1; i < next; i++)
    if (!script_.steps[i].hook.empty() && !hookFired_[i]) {
      hookFired_[i] = true;
      if (opt_.verbose) std::fprintf(stderr, "[%6.2fs] @%s never cued: runs at the end of its sentence\n", clock_,
                                     script_.steps[i].hook.c_str());
      execStep(i);
    }
  if (opt_.instant && sayStep_ < script_.steps.size() && script_.steps[sayStep_].main.verb == Verb::SAY) checkBinding(sayStep_, next);
  focus_.clear(); // focus is what the voice is about: it ends with its sentence (axiom 5)
}

void Engine::tickSay(float dt) {
  if (!say_.active) {
    if (say_.lingering > 0) say_.lingering -= dt;
    return;
  }
  if (say_.utt) {
    float avail;
    bool done, failed;
    {
      std::lock_guard<std::mutex> l(say_.utt->mu);
      avail = (float)say_.utt->pcm.size() / Tts::kRate;
      done = say_.utt->done;
      failed = say_.utt->failed;
    }
    if (!say_.audioStarted) {
      say_.waited += dt;
      if (failed || !tts_->hasAudio() || (!done && say_.waited > 10)) {
        // the voice didn't come: fall back to estimated timing so the lesson keeps going
        warn("voice: Inworld audio unavailable for a line, using estimated timing");
        tts_->stop();
        say_.utt.reset();
        float scale = 170.f / std::max(60.f, opt_.wpm), t = 0;
        for (size_t i = 0; i < say_.words.size(); i++) {
          float x = 0.11f + 0.058f * (float)GetCodepointCount(say_.words[i].text.c_str());
          char last = say_.words[i].text.empty() ? ' ' : say_.words[i].text.back();
          if (last == ',' || last == ';' || last == ':') x += 0.18f;
          if (last == '.' || last == '!' || last == '?') x += 0.32f;
          say_.start[i] = t;
          t += x * scale;
          say_.end[i] = t;
        }
        say_.total = t;
        say_.t = 0;
        return;
      }
      if (avail > 0 && (opt_.recording ? (avail > 0.25f || done) : tts_->played() > 0)) {
        say_.audioStarted = true;
        if (opt_.verbose) std::fprintf(stderr, "[%6.2fs] voice starts after %.2fs wait\n", clock_, say_.waited);
        audioLog_.push_back({clock_, say_.utt});
      } else {
        return; // streaming: nothing to hear yet
      }
    }
    alignWords();
    if (opt_.recording) {
      if (say_.t + dt <= avail || done) say_.t += dt; // video time, but never ahead of the audio
    } else {
      // the clock is the audio itself: what has been heard, not just handed to the device
      float sent = (float)tts_->played() / Tts::kRate, heard = (float)tts_->heard() / Tts::kRate;
      if (done && sent >= avail - 1e-3f) say_.t = std::max(say_.t + dt, heard); // played out: let it close
      else say_.t = heard;
    }
    if (done) say_.total = avail;
  } else {
    say_.t += dt;
  }
  while (say_.nextWord < say_.words.size() && say_.start[say_.nextWord] <= say_.t) {
    for (auto &h : say_.words[say_.nextWord].hooks) fireHook(h);
    emit("word " + std::to_string(say_.uid) + " " + std::to_string(say_.nextWord));
    say_.nextWord++;
  }
  float lastWordEnd = say_.end.empty() ? say_.total : std::min(say_.total, say_.end.back());
  if (say_.t >= lastWordEnd && say_.nextWord >= say_.words.size() && !say_.trailingFired) {
    say_.trailingFired = true;
    for (auto &h : say_.trailing) fireHook(h);
  }
  if (say_.t >= say_.total + (say_.utt ? 0.15f : 0.25f)) {
    // nothing promised in this sentence is dropped, even if word timing came up short
    for (; say_.nextWord < say_.words.size(); say_.nextWord++)
      for (auto &h : say_.words[say_.nextWord].hooks) fireHook(h);
    if (!say_.trailingFired) {
      say_.trailingFired = true;
      for (auto &h : say_.trailing) fireHook(h);
    }
    closeSentence();
    emit("end " + std::to_string(say_.uid));
    if (say_.utt && tts_) tts_->stop();
    say_.active = false;
    say_.lingering = 2.0f;
  }
}

bool Engine::blocking() const {
  if (say_.active || waitingPause_) return true;
  for (auto &a : actives_) if (a.blocking) return true;
  return false;
}

void Engine::tickActives(float dt) {
  std::vector<Active> landed; // flights that arrived: their target pulses next
  for (size_t i = 0; i < actives_.size();) {
    Active &a = actives_[i];
    float step = dt; // this frame's time for this animation
    if (a.delay > 0) { // one of a staggered set: it starts when its delay runs out, with whatever of the frame is left
      a.delay -= dt;
      if (a.delay > 0) { i++; continue; }
      step = -a.delay;
      a.delay = 0;
    }
    a.t += step;
    float k = a.dur > 0 ? std::clamp(a.t / a.dur, 0.f, 1.f) : 1.f;
    float e = a.ease ? a.ease(k) : k;
    Obj *o = a.id.empty() ? nullptr : find(a.id);
    if (a.kind == Active::PULSE && !o) {
      a.from += step;              // waiting for the target
      if (a.from > 2 || opt_.instant) { warn("indicate: '" + a.prop + "' never appeared"); actives_.erase(actives_.begin() + (long)i); continue; }
      a.t -= step;
      i++;
      continue;
    }
    if (a.kind == Active::PULSE && o) {
      o->pulse = rate::there_and_back(k);
      o->pulseRef = a.prop;
      if (k >= 1) o->pulse = 0;
    } else if (a.kind == Active::NUM && o) {
      setProp(o->props, a.prop, Value::number(a.from + (a.to - a.from) * e));
    } else if (a.kind == Active::STR && o) {
      auto it = o->strBlend.find(a.prop);
      if (it != o->strBlend.end()) it->second.second = e;
    } else if (a.kind == Active::CAM) {
      cam_.k = e;
    } else if (a.kind == Active::CLEAR) {
      stageFade_ = 1 - e;
      if (k >= 1) {
        std::string t = a.prop;
        actives_.erase(actives_.begin() + (long)i);
        clearStage();
        if (!t.empty()) { title_ = t; headline_ = 0; }
        stageFade_ = 1;
        continue;
      }
    }
    if (k >= 1) {
      if (a.kind == Active::FLIGHT) {
        Active p;
        p.kind = Active::PULSE;
        p.id = baseId(a.dest);
        p.prop = a.dest;
        p.dur = opt_.instant ? 0 : 0.8f;
        p.blocking = false;
        landed.push_back(p);
      }
      if (a.kind == Active::NUM && o) setProp(o->props, a.prop, a.finalVal);
      if (a.kind == Active::STR && o) {
        if (auto it = o->strBlend.find(a.prop); it != o->strBlend.end() && a.prop == "tex")
          if (Mark *mk = mark(o->id)) { mk->texPrev = it->second.first; mk->texSettledAt = clock_; }
        o->strBlend.erase(a.prop);
      }
      if (!a.log.empty()) {
        since_.push_back(a.log + " done");
        if (since_.size() > 16) since_.erase(since_.begin());
      }
      actives_.erase(actives_.begin() + (long)i);
    } else {
      i++;
    }
  }
  actives_.insert(actives_.end(), landed.begin(), landed.end());
}

void Engine::skip() {
  if (waitingPause_) { resume(); return; }
  if (say_.active) {
    if (tts_) tts_->stop();
    say_.utt.reset();
    say_.t = say_.total = std::max(say_.t, 0.f);
    for (auto &st : say_.start) st = std::min(st, say_.t);
    tickSay(0);
    if (say_.active) emit("end " + std::to_string(say_.uid));
    say_.active = false;
    say_.lingering = 2.0f;
  }
  for (auto &a : actives_) {
    if (a.kind == Active::NUM && a.t < a.dur) { // it jumps to its end now: history says so too
      Obj *o = find(a.id);
      const Value *cur = o ? findProp(o->props, a.prop) : nullptr;
      recordSet(a.id, a.prop, cur ? *cur : Value::number(a.from), a.finalVal);
    }
    a.t = a.dur + 1;
  }
  tickActives(0);
  for (auto &id : order_) if (Obj *o = find(id)) o->appear = 1;
}

void Engine::resume() { waitingPause_ = false; }

void Engine::togglePause() {
  paused_ = !paused_;
  if (tts_) tts_->setPaused(paused_);
}

bool Engine::finished() const {
  return cursor_ >= script_.steps.size() && actives_.empty() && !say_.active && !waitingPause_;
}

bool Engine::stageEmpty() {
  for (auto &[id, m] : marks_) if (alphaOf(m) > 0.05f) return false;
  return true;
}

void Engine::update(float dt) {
  if (paused_) return;
  clock_ += dt;
  frameDt_ = dt;
  while (!blocking() && cursor_ < script_.steps.size()) {
    size_t idx = cursor_++;
    if (!script_.steps[idx].hook.empty()) continue; // armed: runs when the voice reaches |hook|
    execStep(idx);
  }
  tickSay(dt);
  tickActives(dt);
  captionT_ += dt;
  // the name heads the empty stage; when the first thing arrives it goes to the corner (0.6 s)
  if (headline_ < 1 && (opt_.instant || !stageEmpty())) headline_ = opt_.instant ? 1 : std::min(1.f, headline_ + dt / 0.6f);

  // entrance + focus easing
  auto lit = [&](const Obj &o) -> float {
    if (focus_.empty()) return 1;
    if (o.id == focus_ || o.id == baseId(focus_)) return 1; // a[3]: the array stays lit, it dims its other cells
    auto g = groups_.find(focus_);
    if (g != groups_.end()) {
      std::function<bool(const Group &, int)> contains = [&](const Group &gr, int d) {
        if (d > 8) return false;
        for (auto &m : gr.members) {
          if (m == o.id) return true;
          auto sub = groups_.find(m);
          if (sub != groups_.end() && contains(sub->second, d + 1)) return true;
        }
        return false;
      };
      if (contains(g->second, 0)) return 1;
    }
    if (auto ofv = findProp(o.props, "of"); ofv && ofv->str == focus_) return 0.7f;
    return 0.2f;
  };
  // an entrance takes 1 s (3Blue1Brown's fades run 1 to 2 s; docs/3b1b-habits.md)
  float appearRate = opt_.instant ? 1e9f : 1.f / 1.0f;
  for (auto &id : order_) {
    Obj *o = find(id);
    if (!o) continue;
    if (o->enterIn > 0 && !opt_.instant) { o->enterIn -= dt; continue; }
    o->appear = std::min(1.f, o->appear + dt * appearRate);
    o->lit = opt_.instant ? lit(*o) : smoothApproach(o->lit, lit(*o), dt, 7);
  }
  if (!opt_.shadow) layout(opt_.instant ? 1.f : dt);

  if (finished() && !idleSent_ && !opt_.instant) {
    idleSent_ = true;
    emit("idle");
  }
  if (finished() && !readbackPrinted_) {
    readbackPrinted_ = true;
    zeroCheck();
    if (!opt_.instant && !opt_.quiet) {
      std::string rb = readback();
      std::printf("%s\n", rb.c_str());
      std::fflush(stdout);
      if (!path_.empty()) {
        std::ofstream out(path_ + ".readback");
        out << rb << "\n";
      }
    }
  }
}

void Engine::runToEnd() {
  for (int guard = 0; guard < 100000 && !finished(); guard++) {
    if (waitingPause_) waitingPause_ = false;
    update(0);
  }
  update(0);
  zeroCheck();
}

// ================================================================ formulas
float Engine::texSize(const Mark &m) {
  bool ok;
  double s = num(m, "size", &ok);
  return ok && std::isfinite(s) && s > 4 ? (float)s : 38.f;
}

std::string Engine::texSource(const std::string &raw) {
  std::string out;
  size_t i = 0;
  for (size_t p; (p = raw.find("\\val{", i)) != std::string::npos;) {
    out += raw.substr(i, p - i);
    size_t q = p + 5;
    int depth = 1;
    while (q < raw.size() && depth > 0) { if (raw[q] == '{') depth++; if (raw[q] == '}') depth--; q++; }
    out += valueText(raw.substr(p + 5, q - p - 6));
    i = q;
  }
  return out + raw.substr(i);
}

std::string Engine::valueText(const std::string &inner) {
  auto it = texExpr_.find(inner);
  if (it == texExpr_.end()) it = texExpr_.emplace(inner, Value::parse("{" + inner + "}")).first;
  double v = num(it->second);
  // integer-valued properties show as integers (as their labels do), even mid-morph
  std::string prop = inner;
  if (size_t dot = inner.find('.'); dot != std::string::npos) prop = inner.substr(dot + 1);
  else if (Mark *mm = mark(inner)) prop = magProp(*mm);
  bool isInt = propIsInt_.count(prop) && propIsInt_[prop];
  return std::isfinite(v) ? fmtNum(isInt ? std::round(v) : v) : "?";
}

TexPtr Engine::texNow(const Mark &m) {
  const Value *v = findProp(m.props, "tex");
  if (!v || v->type != Value::STR) return nullptr;
  // headless runs wait for the typesetter (the prescan already did the literal formulas, so this
  // is a cache hit), so checks measure formulas as the stage will draw them
  TexPtr L = opt_.instant ? (tex::available() ? tex::typeset(texSource(v->str)) : nullptr) : tex::request(texSource(v->str));
  TexPtr &last = lastTex_[m.id];
  if (L && (L->ok || !last || !last->ok)) last = L;
  return last;
}

// ================================================================ readback
std::string Engine::readback(bool comments) {
  std::ostringstream o;
  if (comments) o << "# readback t=" << fmtNum(std::round(clock_ * 10) / 10) << "s\n";
  if (!title_.empty()) o << "title \"" << title_ << "\"\n";
  if (!views_.empty()) { // views are declared where their first mark is (below)
  } else if (schema_ != "auto") {
    o << "schema " << schema_;
    for (auto &p : schemaProps_) o << " " << p.key << "=" << p.val.markup();
    o << "\n";
  } else if (comments) {
    o << "# schema (auto) " << autoSchema_ << "\n";
  }
  std::vector<std::string> evals;
  std::set<std::string> declared;
  std::string inView;
  for (auto &id : order_) {
    if (auto it = marks_.find(id); it != marks_.end()) {
      const Mark &m = it->second;
      if (!m.view.empty() && m.view != inView) { // switch to its view (declaring it the first time)
        inView = m.view;
        o << "schema " << m.view;
        if (declared.insert(m.view).second)
          for (auto &v : views_)
            if (v.id == m.view) {
              o << " " << v.schema;
              for (auto &p : v.props) o << " " << p.key << "=" << p.val.markup();
            }
        o << "\n";
      }
      o << "mark " << m.id << " " << effectiveKind(m);
      if (!m.category.empty()) o << " " << Value::string(m.category).markup();
      for (auto &p : m.props) {
        o << " " << p.key << "=" << p.val.markup();
        if (p.val.type == Value::EXPR && p.key != "fn" && p.key != "xt" && p.key != "yt" && p.key != "vx" && p.key != "vy") {
          double v = num(p.val);
          evals.push_back(m.id + "." + p.key + " = " + fmtNum(v));
          if (!std::isfinite(v))
            warn("value: " + m.id + "." + p.key + " = {" + p.val.str + "} is not a number (division by zero, a cyclic reference, or an unknown name)");
        }
      }
      o << "\n";
    } else if (auto lt = links_.find(id); lt != links_.end()) {
      const Link &l = lt->second;
      o << "link " << l.id << " " << l.kind << " " << l.a << (l.directed ? " -> " : " -- ") << l.b;
      for (auto &p : l.props) o << " " << p.key << "=" << p.val.markup();
      o << "\n";
    } else if (auto gt = groups_.find(id); gt != groups_.end()) {
      const Group &g = gt->second;
      o << "group " << g.id << " part-of=[";
      for (size_t i = 0; i < g.members.size(); i++) o << (i ? "," : "") << g.members[i];
      o << "]";
      for (auto &p : g.props) o << " " << p.key << "=" << p.val.markup();
      o << "\n";
    }
  }
  if (!focus_.empty()) o << "focus " << focus_ << "\n";
  if (cam_.on && !cam_.spec.empty()) o << cam_.spec << "\n";
  if (!comments) return o.str();

  for (auto &e : evals) o << "# value: " << e << "\n";
  // derived facts per magnitude property
  std::map<std::string, std::vector<std::pair<std::string, double>>> byProp;
  for (auto &id : order_)
    if (Mark *m = mark(id)) {
      std::string k = magProp(*m), kind = effectiveKind(*m);
      double v = k.empty() ? NAN : num(*m, k);
      if (std::isfinite(v) && (kind == "bar" || kind == "dot" || kind == "box")) byProp[k].push_back({id, v});
    }
  for (auto &[k, v] : byProp) {
    if (v.size() < 2) continue;
    auto sorted = v;
    std::stable_sort(sorted.begin(), sorted.end(), [](auto &a, auto &b) { return a.second > b.second; });
    double sum = 0;
    for (auto &x : v) sum += x.second;
    o << "# derived " << k << ": max=" << sorted.front().first << " min=" << sorted.back().first
      << " mean=" << fmtNum(sum / v.size()) << " total=" << fmtNum(sum) << " order: ";
    for (size_t i = 0; i < sorted.size(); i++) {
      if (i) o << (std::fabs(sorted[i - 1].second - sorted[i].second) < 1e-9 ? " = " : " > ");
      o << sorted[i].first;
    }
    o << "\n";
  }
  // overlaps that the schema made on purpose (equal values) or by accident
  std::vector<std::string> ids;
  for (auto &id : order_) if (marks_.count(id)) ids.push_back(id);
  for (size_t i = 0; i < ids.size(); i++)
    for (size_t j = i + 1; j < ids.size(); j++) {
      Mark &a = marks_[ids[i]], &b = marks_[ids[j]];
      std::string ka = effectiveKind(a), kb = effectiveKind(b);
      if (ka == "path" || kb == "path" || ka == "region" || kb == "region" || ka == "label" || kb == "label") continue;
      if (isFieldKind(ka) || isFieldKind(kb) || ka == "solution" || kb == "solution") continue;
      if (CheckCollisionRecs(bodyBounds(a), bodyBounds(b))) o << "# overlapping: " << a.id << " " << b.id << "\n";
    }
  if (!since_.empty()) {
    o << "# since:";
    for (size_t i = 0; i < since_.size(); i++) o << (i ? ";" : "") << " " << since_[i];
    o << "\n";
  }
  for (auto &l : lint_) // binding is judged per sentence against the whole script (--check), not mid-stream
    if (l.rfind("binding:", 0) != 0) o << "# lint: " << l << "\n";
  return o.str();
}

// ================================================================ voice (Inworld, streamed)
void Engine::prepareVoice() {
  lineUtt_.clear();
  queueVoice(0, true);
}

std::vector<std::string> Engine::prefetchVoice(const std::string &sayLine) {
  Script s;
  if (!s.parse(sayLine) || s.steps.empty() || s.steps[0].main.verb != Verb::SAY) return {};
  std::string text; // the same text and context queueVoice asks for, so the beat finds it cached or in flight
  for (auto &w : s.steps[0].main.words) {
    if (w.ref) return {}; // its values are read once its beat arrives
    text += (text.empty() ? "" : " ") + w.text;
  }
  std::vector<std::string> context = saysBefore(script_.steps.size(), (size_t)opt_.tts.contextLines);
  context.insert(context.end(), prefetched_.begin(), prefetched_.end());
  if (context.size() > (size_t)opt_.tts.contextLines) context.erase(context.begin(), context.end() - opt_.tts.contextLines);
  prefetched_.push_back(text);
  if (opt_.voice && !opt_.instant && !opt_.tts.apiKey.empty()) {
    if (!tts_) tts_ = std::make_unique<Tts>(opt_.tts);
    tts_->request(text, context);
  }
  return context;
}

std::vector<std::string> Engine::saysBefore(size_t step, size_t n) {
  std::vector<std::string> out;
  for (size_t i = std::min(step, script_.steps.size()); i-- > 0 && out.size() < n;)
    if (script_.steps[i].main.verb == Verb::SAY) out.insert(out.begin(), spokenText(script_.steps[i].main));
  return out;
}

void Engine::queueVoice(size_t fromStep, bool report) {
  if (!opt_.voice || opt_.instant) return;
  if (opt_.tts.apiKey.empty()) {
    if (report) warn("voice: no Inworld API key (--inworld-key, $INWORLD_API_KEY or .inworld_key), using estimated timing");
    return;
  }
  if (!tts_) tts_ = std::make_unique<Tts>(opt_.tts);
  // queue every line now, in script order: the first is speaking within ~0.5 s,
  // and the rest are fetched while earlier lines play
  int fresh = 0, total = 0;
  for (size_t si = fromStep; si < script_.steps.size(); si++) {
    const Step &st = script_.steps[si];
    std::vector<const Line *> lines{&st.main};
    for (auto &w : st.withs) lines.push_back(&w);
    for (const Line *ln : lines) {
      if (ln->verb != Verb::SAY) continue;
      UttPtr u = tts_->request(spokenText(*ln), saysBefore(si, (size_t)opt_.tts.contextLines));
      lineUtt_[ln->uid] = u;
      total++;
      std::lock_guard<std::mutex> l(u->mu);
      if (!u->cached) fresh++;
    }
  }
  if (report)
    std::fprintf(stderr, "voice: inworld %s, %d lines (%d cached, %d streaming)\n", opt_.tts.voiceId.c_str(), total,
                 total - fresh, fresh);
}

// ================================================================ values spoken off the stage ({expr} in Say)
void Engine::startShadow(const std::string &text) {
  if (opt_.instant) return;
  EngineOptions o;
  o.instant = o.shadow = o.quiet = true;
  shadow_ = std::make_unique<Engine>(o);
  shadow_->loadText(text);
  shadow_->runToEnd();
}

// an array cell, {a[3]} or {M[1][2]} in a grid: its item as written ("" when inner is not a cell)
std::string Engine::cellText(const std::string &inner) {
  static const std::regex cell(R"(^\s*([A-Za-z_]\w*)\[(\d+)\](?:\[(\d+)\])?\s*$)");
  std::smatch m;
  if (!std::regex_match(inner, m, cell)) return "";
  Mark *a = mark(m[1].str());
  const Value *items = a ? findProp(a->props, "items") : nullptr;
  if (!items || items->type != Value::LIST) return "";
  int k = std::stoi(m[2].str());
  if (m[3].matched) k = k * std::max(1, (int)num(*a, "cols")) + std::stoi(m[3].str());
  return k >= 0 && k < (int)items->list.size() ? items->list[(size_t)k] : "";
}

// {id.prop} that isn't a number: a word is spoken as it is (a label, a state); a formula, or nothing on stage, is
// not read out at all (the voice never says "dy.tex"), and the checker says why
std::string Engine::spokenProp(const std::string &inner) {
  size_t dot = inner.find('.');
  Mark *m = dot == std::string::npos ? nullptr : mark(inner.substr(0, dot));
  const Value *v = m ? findProp(m->props, inner.substr(dot + 1)) : nullptr;
  if (v && v->type == Value::STR && inner.substr(dot + 1) != "tex") return v->str;
  warn(v ? "say: {" + inner + "} is a formula, which the voice can't read: say it in words"
         : "say: {" + inner + "} has no value on stage when it is spoken, so nothing is said there");
  return "";
}

std::string Engine::spokenWord(const std::string &w) {
  // a morph started on this word counts at its end value: land the values in flight while reading
  std::vector<std::tuple<Obj *, std::string, Value>> held;
  for (auto &a : actives_)
    if (Obj *o = a.kind == Active::NUM ? find(a.id) : nullptr)
      if (const Value *v = findProp(o->props, a.prop)) {
        held.emplace_back(o, a.prop, *v);
        setProp(o->props, a.prop, Value::number(a.to));
      }
  std::string out;
  size_t i = 0;
  for (size_t a; (a = w.find('{', i)) != std::string::npos;) {
    size_t b = w.find('}', a);
    if (b == std::string::npos) break;
    std::string inner = w.substr(a + 1, b - a - 1), v = cellText(inner);
    if (v.empty()) v = valueText(inner);
    if (v == "?") v = spokenProp(inner);
    out += w.substr(i, a - i) + v;
    i = b + 1;
  }
  for (auto it = held.rbegin(); it != held.rend(); ++it) setProp(std::get<0>(*it)->props, std::get<1>(*it), std::get<2>(*it));
  return out + w.substr(i);
}

std::vector<std::string> Engine::spokenWords(const Line &ln) {
  const auto &said = shadow_ ? shadow_->spoken_ : spoken_;
  if (auto it = said.find(ln.uid); it != said.end() && it->second.size() == ln.words.size()) return it->second;
  std::vector<std::string> out; // not run yet: the values now
  for (auto &w : ln.words) out.push_back(w.ref ? spokenWord(w.text) : w.text);
  return out;
}

std::vector<std::string> Engine::spokenLines() {
  std::vector<std::string> out;
  eachLine(script_, [&](const Line &ln) { if (ln.verb == Verb::SAY) out.push_back(spokenText(ln)); });
  return out;
}

std::string Engine::spokenText(const Line &ln) {
  std::string text;
  for (auto &w : spokenWords(ln)) text += (text.empty() ? "" : " ") + w;
  return text;
}

void Engine::alignWords() {
  // Match the script's words to Inworld's spoken words in order. Text normalisation can
  // split or merge words ("30" -> "thirty"), so allow a short look-ahead and otherwise step on.
  auto norm = [](const std::string &w) {
    std::string o;
    for (unsigned char c : w) if (std::isalnum(c) || (c & 0x80)) o += (char)std::tolower(c);
    return o;
  };
  std::vector<std::string> api;
  std::vector<float> st, en;
  bool done;
  {
    std::lock_guard<std::mutex> l(say_.utt->mu);
    for (auto &w : say_.utt->words) api.push_back(norm(w));
    st = say_.utt->wordStart;
    en = say_.utt->wordEnd;
    done = say_.utt->done;
  }
  size_t j = 0;
  for (size_t i = 0; i < say_.words.size(); i++) {
    std::string w = norm(say_.words[i].text);
    if (w.empty()) { say_.start[i] = i ? say_.start[i - 1] : 0; continue; }
    if (j >= api.size()) {
      say_.start[i] = done ? (en.empty() ? 0 : en.back()) : 1e9f;
      continue;
    }
    size_t k = j;
    for (size_t look = j; look < std::min(api.size(), j + 4); look++)
      if (api[look] == w || (api[look].size() > 2 && w.rfind(api[look], 0) == 0)) { k = look; break; }
    say_.start[i] = st[k];
    say_.end[i] = en[k];
    j = k + 1;
  }
}

std::vector<std::pair<float, std::string>> Engine::audioLog() const {
  std::vector<std::pair<float, std::string>> out;
  for (auto &[t, u] : audioLog_) {
    std::lock_guard<std::mutex> l(u->mu);
    if (!u->wavPath.empty()) out.push_back({t, u->wavPath});
  }
  return out;
}

void Engine::stopVoice() {
  if (tts_) tts_->stop();
}

} // namespace mk
