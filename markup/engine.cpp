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
                                                  "parts", "by", "bylabels", "bycolors",
                                                  "z", "z1", "z2", "dz", "zt", "normal", "units", "zlabel", "drop"};
static const std::set<std::string> kTextKeys = {"label", "name", "text", "set", "fn",   "of",   "color",
                                                "shape", "ease", "unit", "note", "title", "tex",
                                                "at", "to", "side", "style", "cmap", "levels", "trail", "drop", "units"};
// riders: what can be ridden (on=), and what can ride it
static bool rideable(const std::string &kind) { return isCurveKind(kind) || kind == "line" || kind == "arrow" || kind == "surface"; }
static bool canRide(const std::string &kind) { return kind == "dot" || kind == "label" || kind == "vector"; }

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
  evalScan_.clear();
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
  // its values join the scales at the next sentence start, not now (mid-sentence, for no spoken reason); a stage
  // nothing has been said on yet is still being set up: at once
  if (sceneSpoken_) scanPending_ = true; else usePrescan();
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

// does this sentence say |name|?
static bool saysHook(const Line &say, const std::string &name) {
  if (say.verb != Verb::SAY) return false;
  for (auto &w : say.words) for (auto &h : w.hooks) if (h == name) return true;
  for (auto &h : say.trailingHooks) if (h == name) return true;
  return false;
}

void Engine::analyze(bool fresh) {
  // ---- prescan: every literal a property ever takes, so scales never jump (axiom 2); the values the script
  // computes come from running it (sampleValues), and both become the scales in usePrescan
  scenePrescan_.clear();
  Prescan cur;
  auto see = [&](const std::string &k, const Value &v) {
    if (v.type != Value::NUM) return;
    cur.extent(k, v.num);
    bool isInt = std::fabs(v.num - std::round(v.num)) < 1e-9;
    cur.isInt[k] = cur.isInt.count(k) ? (cur.isInt[k] && isInt) : isInt;
  };
  bool hasAxes = false, hasT = false, hasLinks = false, hasMag = false, hasWorld = false, hasFn = false, hasBox = false, hasSpace = false;
  bool hasSurface = false, camMoves = false; // a space's surface, and whether the camera ever turns while it is shown
  autoSchemas_.clear();
  int blockLines = 0; // blocks this scene will hold (to choose the band's arrangement before they arrive)
  std::set<std::string> sceneBlockIds;
  sceneBlocks_.clear();
  // riders (on= a curve, a line or a surface): what each thing of the scene is, the keys it was ever given, what it
  // sits on, the solutions' of= and the morphs, so the lint judges the whole scene
  std::map<std::string, std::string> kindOf, ofOf;
  std::map<std::string, std::set<std::string>> keysOf;
  struct Seat { std::string target, key; int line = 0; };
  std::map<std::string, Seat> seatOf;
  std::vector<const Line *> morphs;
  std::vector<std::string> riderLints; // raised after the static lint reset below, so a fresh load keeps them
  auto lintRiders = [&] {
    auto note = [&](const std::string &s) { riderLints.push_back(s); };
    auto kindNow = [&](const std::string &id) { // a kindless fn= is a curve, or a surface in a space
      auto it = kindOf.find(id);
      std::string k = it == kindOf.end() ? "" : it->second;
      return k == "fn" ? (hasSpace ? "surface" : "path") : k;
    };
    auto coords = [&](const std::string &target, const std::string &tk, std::vector<std::string> &need, std::vector<std::string> &gives) {
      need = {"x"}; gives = {"y"}; // the free coordinate the author gives, and the ones the ride gives
      if (keysOf[target].count("xt")) { need = {"t"}; gives = {"x", "y"}; if (keysOf[target].count("zt")) gives.push_back("z"); }
      else if (tk == "surface") { need = {"x", "y"}; gives = {"z"}; }
    };
    for (auto &[id, on] : seatOf) {
      std::string tk = kindNow(on.target), rk = kindNow(id);
      if (!rideable(tk)) continue; // seated on a box, a cell, an array: the attachment
      std::string where = "relation: line " + std::to_string(on.line) + ": '" + id + "' " + on.key + "=" + on.target;
      std::vector<std::string> need, gives;
      coords(on.target, tk, need, gives);
      if (!rk.empty() && !canRide(rk)) { note(where + ": only a dot, a label or a vector can ride a " + tk + " (an array seats on an array)"); continue; }
      if (on.key != "on") { note(where + ": a " + tk + " is ridden with on=, giving " + need[0] + "="); continue; }
      if (hasSpace && (tk == "line" || tk == "arrow")) { note(where + ": a segment in space can't be ridden; ride a path (t=) or a surface (x= y=)"); continue; }
      if (tk == "solution" && kindNow(ofOf[on.target]) != "slope") { note(where + ": a particle's path has no y for an x; a dot rides the solution of a slope field"); continue; }
      for (auto &k : need)
        if (!keysOf[id].count(k)) note(where + " needs " + k + "= (the " + tk + " gives " + gives[0] + (gives.size() > 1 ? " and " + gives[1] : "") + ")");
      for (auto &k : gives)
        if (keysOf[id].count(k)) note(where + ": " + k + "= is ignored, the " + tk + " gives it");
    }
    for (const Line *m : morphs) { // a morph of the coordinate the ride gives moves nothing: say what to morph instead
      auto s = seatOf.find(m->id);
      if (s == seatOf.end() || s->second.key != "on") continue;
      std::string tk = kindNow(s->second.target), rk = kindNow(m->id);
      if (!rideable(tk) || (!rk.empty() && !canRide(rk))) continue;
      std::vector<std::string> need, gives;
      coords(s->second.target, tk, need, gives);
      if (std::find(gives.begin(), gives.end(), m->prop) != gives.end())
        note("relation: line " + std::to_string(m->lineNo) + ": morph " + m->id + "." + m->prop + ": " + m->id + " rides " + s->second.target +
             ", so " + m->prop + " comes from it; morph " + m->id + "." + need[0] + " instead");
    }
    kindOf.clear(); ofOf.clear(); keysOf.clear(); seatOf.clear(); morphs.clear();
  };
  auto closeScene = [&] {
    lintRiders();
    // a still picture of a surface is ambiguous (a bowl or a dome?): shape reads from motion (the kinetic depth effect)
    if (hasSurface && !camMoves)
      warn("space: a surface is shown but the camera never moves; its shape reads from motion: add `camera spin` (or an "
           "`orbit=`) while the surface is the subject");
    hasSurface = camMoves = false;
    sceneBlocks_.push_back(blockLines);
    blockLines = 0;
    sceneBlockIds.clear();
    autoSchemas_.push_back(hasSpace           ? "space"
                           : hasWorld && !hasFn ? "plane"
                           : hasAxes          ? "axes"
                           : hasT             ? "timeline"
                           : (hasBox || (hasLinks && !hasMag)) ? "graph" // boxes are graph things from the first one,
                                                                 : "free");  // so links or numbers arriving later don't re-lay the scene
    hasAxes = hasT = hasLinks = hasMag = hasWorld = hasFn = hasBox = hasSpace = false;
    scenePrescan_.push_back(cur);
    cur = Prescan{};
  };
  std::set<std::string> defined, sayHooks, deferredHooks, parts; // parts: formula parts named by annotations (f[id])
  std::vector<std::pair<int, std::string>> pointed; // [id] in Say lines
  // a curve, a field or a world shape needs a plot (axes, plane, space) to be drawn on; on any other schema it would
  // be in the readback and not on stage (axioms 3 and 6)
  std::map<std::string, std::string> viewSchema; // view -> its schema, in this scene
  std::string schemaNow;                         // the schema the next marks are laid out by ("" = not said: inferred)
  std::vector<std::string> undrawable;
  eachLine(script_, [&](const Line &ln) {
    if (!ln.deferred.empty()) deferredHooks.insert(ln.deferred);
    if (ln.verb == Verb::SCHEMA) {
      if (!ln.kind.empty()) schemaNow = ln.kind;
      else if (auto v = viewSchema.find(ln.id); v != viewSchema.end()) schemaNow = v->second;
      if (!ln.id.empty() && !ln.kind.empty()) viewSchema[ln.id] = ln.kind;
    }
    if (ln.verb == Verb::SCENE) { viewSchema.clear(); schemaNow.clear(); }
    if (ln.verb == Verb::MARK && !ln.kind.empty() && !schemaNow.empty() && schemaNow != "auto" && !isAxes(schemaNow) && !isSpace(schemaNow) &&
        (isWorldKind(ln.kind) || isFieldKind(ln.kind) || isCurveKind(ln.kind) || ln.kind == "region" || ln.kind == "line" || ln.kind == "arrow"))
      undrawable.push_back("draw: line " + std::to_string(ln.lineNo) + ": " + ln.kind + " '" + ln.id + "' can't be drawn on schema " + schemaNow +
                           " (only on axes, plane or space), so it would be in the scene but not on stage");
    switch (ln.verb) {
    case Verb::MARK:
      defined.insert(ln.id);
      if (!ln.kind.empty()) kindOf[ln.id] = ln.kind;
      else if (!kindOf.count(ln.id)) kindOf[ln.id] = findProp(ln.props, "fn") ? "fn" : findProp(ln.props, "xt") ? "path" : "";
      for (const char *k : {"on", "in", "over"})
        if (const Value *v = findProp(ln.props, k); v && v->type == Value::STR) seatOf[ln.id] = {baseId(v->str), k, ln.lineNo};
      if (const Value *v = findProp(ln.props, "of"); v && v->type == Value::STR) ofOf[ln.id] = v->str;
      for (auto &p : ln.props) {
        keysOf[ln.id].insert(p.key);
        see(p.key, p.val);
        if (p.key == "x" || p.key == "x1" || p.key == "fn" || p.key == "xt") hasAxes = true;
        if (p.key == "fn" || p.key == "xt") hasFn = true;
        if ((p.key == "t" || p.key == "at" || p.key == "year") && p.val.isNumeric()) hasT = true;
        if (p.key == "z" || p.key == "dz" || p.key == "zt" || p.key == "z1") hasSpace = true;
        bool planeOffset = p.key == "d" && ln.kind == "plane"; // a x + b y + c z = d: where the plane is, not how much
        if ((!kPositional.count(p.key) || (p.key == "m" && ln.kind != "grid")) && !planeOffset && p.val.isNumeric() && ln.kind != "label" && ln.kind != "param") hasMag = true;
      }
      if (ln.kind == "path" || ln.kind == "region") hasAxes = hasFn = true;
      if (isWorldKind(ln.kind)) hasWorld = true;
      if (ln.kind == "box") hasBox = true;
      if (isSpaceKind(ln.kind)) hasSpace = true;
      if (ln.kind == "surface" || (findProp(ln.props, "fn") && hasSpace)) hasSurface = true;
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
    case Verb::CAMERA:
      if (ln.kind == "spin" || findProp(ln.props, "orbit") || findProp(ln.props, "view") || findProp(ln.props, "tilt")) camMoves = true;
      break;
    case Verb::LINK: defined.insert(ln.id); hasLinks = true; break;
    case Verb::SEND: // a formula part named by send is a part, like one an annotation names
      for (auto *ref : {&ln.a, &ln.b})
        if (size_t b = ref->find('['); b != std::string::npos) parts.insert(ref->substr(b + 1, ref->find(']') - b - 1));
      break;
    case Verb::INDICATE: // pointing at a part (indicate f[sq]) names it too
      if (size_t b = ln.id.find('['); b != std::string::npos && ln.id.find(':') == std::string::npos)
        parts.insert(ln.id.substr(b + 1, ln.id.find(']') - b - 1));
      break;
    case Verb::GROUP: defined.insert(ln.id); break;
    case Verb::MORPH:
      morphs.push_back(&ln);
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

  // ---- static lint
  if (fresh) lint_.clear();
  else // whole-script lints are re-derived: a chunk can end between a Say and its @hook lines
    for (auto it = lint_.begin(); it != lint_.end();)
      it = (it->rfind("hook:", 0) == 0 || it->rfind("relation:", 0) == 0 || it->find("which is never marked") != std::string::npos || it->find("\\textcolor{") != std::string::npos) ? lint_.erase(it)
                                                                                                    : std::next(it);
  for (auto &w : riderLints) warn(w);
  for (auto &w : undrawable) warn(w);
  for (auto &h : deferredHooks)
    if (!sayHooks.count(h)) warn("hook: lines tagged @" + h + " have no |" + h + "| spoken, so they only run when their sentence ends");
  for (auto &h : sayHooks)
    if (!deferredHooks.count(h)) warn("hook: |" + h + "| is spoken but no line is tagged @" + h);
  // a cue belongs to its sentence: a line written after a sentence that doesn't say its cue, before one that does
  for (size_t i = 0, say = SIZE_MAX; i < script_.steps.size(); i++) {
    const Step &st = script_.steps[i];
    if (st.main.verb == Verb::SAY) { say = i; continue; }
    if (st.hook.empty() || say == SIZE_MAX || saysHook(script_.steps[say].main, st.hook)) continue;
    size_t next = i + 1;
    while (next < script_.steps.size() && script_.steps[next].main.verb != Verb::SAY) next++;
    if (next < script_.steps.size() && saysHook(script_.steps[next].main, st.hook))
      warn("hook: @" + st.hook + " on line " + std::to_string(st.main.lineNo) + " sits before the sentence that says |" +
           st.hook + "| (line " + std::to_string(script_.steps[next].main.lineNo) + "): it fires there; put it after that line");
  }
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
    if (ln.verb == Verb::LINK && !ln.writtenAs.empty()) // read as a link: say the one way to write it
      warn("link: line " + std::to_string(ln.lineNo) + ": '" + ln.id + "' joins two things, so it is a link: write `link " + ln.id +
           " " + ln.kind + " " + ln.a + (ln.directed ? " -> " : " -- ") + ln.b + "`" +
           (ln.writtenAs == "path" ? " (a path is a curve: fn= or xt= yt=)" : "") +
           (ln.category.empty() ? "" : "; a link has no category, so '" + ln.category + "' is dropped"));
    if (ln.verb == Verb::GROUP) for (auto &m : ln.members) need(m, "group");
    for (auto &n : ln.notes) warn("mark: line " + std::to_string(ln.lineNo) + ": " + n);
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

void Engine::Prescan::extent(const std::string &k, double v) {
  maxAbs[k] = std::max(maxAbs[k], std::fabs(v));
  if (!minVal.count(k)) minVal[k] = maxVal[k] = v;
  minVal[k] = std::min(minVal[k], v);
  maxVal[k] = std::max(maxVal[k], v);
}

void Engine::Prescan::widen(const Prescan &o) {
  for (auto &[k, lo] : o.minVal) { extent(k, lo); extent(k, o.maxVal.at(k)); }
}

// The prescan reads literals; the values a script computes ({expr}, and whatever a morph drives through one) are
// read off an instant run of it: a live engine's shadow, or this engine itself when it is the instant one. Every
// numeric value every mark holds, per scene. True if an extent grew.
bool Engine::sampleValues() {
  if (!opt_.instant || shadow_) return false; // a live engine's values come from its shadow (also while it interrupts)
  if (evalScan_.size() <= sceneIdx_) evalScan_.resize(sceneIdx_ + 1);
  Prescan &s = evalScan_[sceneIdx_];
  bool grew = false;
  for (auto &[id, m] : marks_)
    for (auto &p : m.props) {
      if (!p.val.isNumeric()) continue;
      double v = num(p.val);
      if (!std::isfinite(v)) continue;
      auto lo = s.minVal.find(p.key);
      if (lo != s.minVal.end() && v >= lo->second && v <= s.maxVal[p.key]) continue;
      s.extent(p.key, v);
      grew = true;
    }
  return grew;
}

// the current scene's extents, literal and run, become the active ones; a change re-derives the inferred ranges
void Engine::usePrescan() {
  scanPending_ = false;
  if (scenePrescan_.empty()) return;
  size_t i = std::min(sceneIdx_, scenePrescan_.size() - 1);
  Prescan p = scenePrescan_[i];
  const std::vector<Prescan> &ran = shadow_ ? shadow_->evalScan_ : evalScan_;
  if (i < ran.size()) p.widen(ran[i]);
  if (p.maxAbs != maxAbs_ || p.minVal != minVal_ || p.maxVal != maxVal_) scanGen_++;
  maxAbs_ = p.maxAbs; minVal_ = p.minVal; maxVal_ = p.maxVal; propIsInt_ = p.isInt;
}

// does a thing on stage take its size or place from this property's extent? A bar's length or a dot's area from
// its magnitude; a place on axes, in a space box, on a timeline or a number line whose range the author did not give
bool Engine::scaledBy(const std::string &k) {
  static const std::set<std::string> xs = {"x", "x1", "x2", "from", "to"}, ys = {"y", "y1", "y2"}, zs = {"z", "z1", "z2"}, ts = {"t", "at", "year"};
  for (auto &[id, m] : marks_) {
    std::string s = schemaOf(m), kind = effectiveKind(m);
    const Props *sp = &schemaProps_;
    for (auto &v : views_) if (v.id == m.view || (m.view.empty() && &v == &views_[0])) sp = &v.props;
    auto given = [&](const char *key) { return findProp(*sp, key) != nullptr; };
    if (isAxes(s)) { if ((xs.count(k) && !given("x")) || (ys.count(k) && !given("y"))) return true; }
    else if (isSpace(s)) { if ((xs.count(k) && !given("x")) || (ys.count(k) && !given("y")) || (zs.count(k) && !given("z"))) return true; }
    else if (s == "timeline") { if (ts.count(k) && !given("range")) return true; }
    else if (s == "numberline") { if (kind == "dot" && magProp(m) == k && !given("range")) return true; }
    else if ((kind == "bar" || kind == "dot") && magProp(m) == k) return true;
  }
  return false;
}

// Live, a later part of the script is in the prescan now. Its extents join the scales at the start of a sentence,
// not the moment it arrived; a property that things on stage already show, whose extent moved, is said once: the
// scale had to move under the learner, and the author could have named the range up front.
void Engine::applyPending() {
  if (!scanPending_) return;
  std::map<std::string, double> lo = minVal_, hi = maxVal_;
  usePrescan();
  for (auto &[k, top] : maxVal_) {
    auto was = lo.find(k);
    if (was == lo.end() || (minVal_[k] >= was->second && top <= hi[k]) || grewLinted_.count(k) || !scaledBy(k)) continue;
    grewLinted_.insert(k);
    warn("structure: the scale of '" + k + "' moved from " + fmtNum(was->second) + "…" + fmtNum(hi[k]) + " to " +
         fmtNum(minVal_[k]) + "…" + fmtNum(top) + " while things showing it were on stage (a later beat brought a value "
         "outside it): give the range up front, max= on a bar or x=[…] y=[…] on the schema, or start a scene");
  }
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
  dotScale_.clear();
  grewLinted_.clear();
  spaceOut_.clear();
  sceneSpoken_ = false;
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
  spc_ = SpaceView{};
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
  quiet_ = 0;
  restDue_ = false;
  anySay_ = false;
  timeline.clear();
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

void Engine::logTime(const std::string &ev) {
  if (opt_.instant) return;
  char b[32];
  std::snprintf(b, sizeof b, "%.2f ", clock_);
  timeline.push_back(b + ev);
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
  if (findProp(o.props, "on")) // a rider: the coordinate its curve gives it comes from the relation, whatever else was written
    if (auto it = marks_.find(o.id); it != marks_.end())
      if (double r; rideCoord(it->second, key, r, depth)) { if (ok) *ok = true; return r; }
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
  if (m.kind == "path" || m.kind == "region" || isFieldKind(m.kind) || m.kind == "solution" || isSpaceKind(m.kind)) return "";
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
  if (isSpace(s)) { // fn: a surface over x and y; xt yt zt: a curve (in u and v: a surface); else a point
    if (findProp(m.props, "fn")) return "surface";
    if (findProp(m.props, "xt")) return findProp(m.props, "u") || findProp(m.props, "v") ? "surface" : "path";
    return "dot";
  }
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
  if (hasZ_ && name == "z") { out = z_; return true; } // a 3D field's layer (space.cpp)
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
bool Engine::paramAt(const Mark &p, double t, double &x, double &y, double *z) {
  const Value *xt = findProp(p.props, "xt"), *yt = findProp(p.props, "yt"), *zt = findProp(p.props, "zt");
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
  if (z) *z = zt && zt->type == Value::EXPR && zt->expr ? eval(*zt->expr, *this) : 0;
  this->x = ox;
  hasX = ohx;
  xName = on;
  gEvalDepth--;
  return std::isfinite(x) && std::isfinite(y) && (!z || std::isfinite(*z));
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
  dotScale_.clear();
  grewLinted_.clear();
  spaceOut_.clear();
  sceneSpoken_ = false;
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
  spc_ = SpaceView{};
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
  if (auto fb = p.fnBlend.find("fn"); fb != p.fnBlend.end() && fb->second.second < 1 && fb->second.first.expr)
    r += (eval(*fb->second.first.expr, *this, depth) - r) * (1 - fb->second.second);
  this->x = ox;
  hasX = ohx;
  xName = on;
  gEvalDepth--;
  return r;
}

// ---- riders: a dot, a label or a vector `on=` a curve, a line or a surface. The author gives the free coordinate
// (x; t on a parametric path; x and y on a surface); the coordinate the ride gives is worked out here from the
// relation each time it is read (num), so layout, the camera, trails, {q.y} and the shadow all agree, and nothing
// derived is stored: the readback keeps `on=f x=2`, and `morph q.x` walks q along f.
const Mark *Engine::ridden(const Mark &m) {
  const Value *on = findProp(m.props, "on");
  if (!on || on->type != Value::STR || !canRide(effectiveKind(m))) return nullptr;
  const Mark *t = mark(baseId(on->str));
  if (!t || t == &m || !rideable(effectiveKind(*t))) return nullptr;
  if ((t->kind == "line" || t->kind == "arrow") && isSpace(schemaOf(*t))) return nullptr; // a segment in space has no y for an x
  return t;
}

bool Engine::rideCoord(const Mark &m, const std::string &key, double &out, int depth) {
  const Mark *t = ridden(m);
  if (!t) return false;
  std::string k = effectiveKind(*t);
  bool ok;
  if (isParametric(*t)) { // xt yt (zt) in t: the rider gives t
    if (key != "x" && key != "y" && !(key == "z" && findProp(t->props, "zt"))) return false;
    double tt = num(m, "t", &ok, depth), px, py, pz;
    if (!ok || !paramAt(*t, tt, px, py, &pz)) return false;
    out = key == "x" ? px : key == "y" ? py : pz;
    return true;
  }
  if (k == "surface") { // z = f(x, y): the rider gives x and y
    if (key != "z") return false;
    bool hy;
    double px = num(m, "x", &ok, depth), py = num(m, "y", &hy, depth);
    if (!ok || !hy) return false;
    out = surfaceAt(*t, px, py, depth);
    return true;
  }
  if (key != "y") return false; // y at x on a curve or a line
  double px = num(m, "x", &ok, depth);
  if (!ok) return false;
  if (k == "line" || k == "arrow") return lineAt(*t, px, out);
  if (k == "solution") return solutionAt(*t, px, out);
  out = pathAt(*t, px, depth);
  return true;
}

// y at x on a line: through x y with slope=, or from x1 y1 to x2 y2 (false when vertical)
bool Engine::lineAt(const Mark &l, double X, double &Y) {
  bool hs;
  double slope = num(l, "slope", &hs);
  if (hs) { Y = num(l, "y") + slope * (X - num(l, "x")); return std::isfinite(Y); }
  bool h1, h2;
  double x1 = num(l, "x1", &h1), x2 = num(l, "x2", &h2);
  if (!h1 || !h2 || std::fabs(x2 - x1) < 1e-12) return false;
  Y = num(l, "y1") + (num(l, "y2") - num(l, "y1")) * (X - x1) / (x2 - x1);
  return std::isfinite(Y);
}

// y at x on the solution curve of a slope field: integrated from its start as the drawn curve is (viz::rk4), in data
// units, so the shadow (no layout) agrees with the stage. A particle's path through a field has no y for an x.
bool Engine::solutionAt(const Mark &s, double X, double &Y) {
  const Value *of = findProp(s.props, "of");
  Mark *src = of && of->type == Value::STR ? mark(of->str) : nullptr;
  if (!src || effectiveKind(*src) != "slope") return false;
  auto f = fn2(*src, "fn");
  double x0, y0;
  if (auto from = listNums(s, "from"); from.size() >= 2) { x0 = from[0]; y0 = from[1]; }
  else {
    bool hx, hy;
    x0 = num(s, "x", &hx);
    y0 = num(s, "y", &hy);
    if (!hx || !hy) return false;
  }
  if (!f || !std::isfinite(x0 + y0 + X)) return false;
  viz::Field g = [&f](viz::V2 p) { return viz::V2{1, f(p.x, p.y)}; }; // (dx, dy) per unit x
  const int n = 400;
  viz::V2 p{x0, y0};
  for (int i = 0; i < n; i++) p = viz::rk4(g, p, (X - x0) / n);
  Y = p.y;
  return std::isfinite(Y);
}

// z = f(x, y) of a surface: the tree walk with both free variables set, as pathAt does with one
double Engine::surfaceAt(const Mark &s, double X, double Y, int depth) {
  const Value *fn = findProp(s.props, "fn");
  if (!fn || fn->type != Value::EXPR || !fn->expr) return NAN;
  if (gEvalDepth > 48) return NAN;
  gEvalDepth++;
  double ox = this->x, oy = this->y;
  bool ohx = hasX, ohy = hasY;
  const char *on = xName, *om = yName;
  this->x = X; this->y = Y; hasX = hasY = true; xName = "x"; yName = "y";
  double r = eval(*fn->expr, *this, depth);
  if (auto fb = s.fnBlend.find("fn"); fb != s.fnBlend.end() && fb->second.second < 1 && fb->second.first.expr)
    r += (eval(*fb->second.first.expr, *this, depth) - r) * (1 - fb->second.second);
  this->x = ox; this->y = oy; hasX = ohx; hasY = ohy; xName = on; yName = om;
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
// Uncategorised data placed on one plot is one series and shares one ink: samples, events, points (axiom 3 names
// "data placed on one plot" as a relation). Off a plot an uncategorised thing is a kind of its own. Decided when
// the thing is made and kept on the mark, so a view named later cannot move its colour.
std::string Engine::seriesKey(const Mark &m) const {
  if (auto at = findProp(m.props, "at"); at && at->type == Value::STR) return ""; // a pointer names a thing
  std::string k = effectiveKind(m), s = schemaOf(m);
  if (!((k == "dot" && isDataSchema(s)) || (k == "bar" && isAxes(s)))) return "";
  std::string v = dataViewOf(m);
  return "plot:" + (v.empty() ? "main" : v); // the unnamed schema becomes the "main" view once another is named
}
// the palette key: its category, else its plot's series, else itself
std::string Engine::colorKey(const Mark &m, const std::string &category) const {
  if (!category.empty()) return "cat:" + category;
  return m.series.empty() ? "id:" + m.id : m.series;
}
void Engine::claimColor(const Mark &m) {
  if (findProp(m.props, "of") || findProp(m.props, "color")) return;
  std::string c = catValue(m);
  if (c.empty() && isBlockKind(effectiveKind(m))) return; // formulas, cards, code: ink, not a color slot
  if (effectiveKind(m) == "param") return;                  // an invisible driver (a clock, a constant)
  colorForKey(colorKey(m, c));
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
  Color now = colorForKey(colorKey(m, catValue(m)));
  if (!cp.empty()) { // a category that is changing cross-fades from the colour it really had
    auto it = m.strBlend.find(cp);
    if (it != m.strBlend.end()) return lerpColor(colorForKey(colorKey(m, it->second.first)), now, it->second.second);
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
    say_.lingering = std::max(say_.lingering, a.dur + 0.6f); // the words stay readable while the stage rests
    break;
  }
  case Verb::PAUSE:
    if (!opt_.instant) {
      waitingPause_ = true;
      emit("waiting " + std::to_string(currentPart())); // a lecture's check: the learner answers before it goes on
    }
    break;
  case Verb::SCHEMA:
    // an unnamed schema and named ones in one scene: the unnamed one is a view too ("main"), so each keeps
    // its own part of the stage (a plane with its field beside a network, not under it)
    if (!ln.id.empty() && views_.empty() && schema_ != "auto") {
      views_.push_back({"main", schema_, schemaProps_});
      for (auto &[id, m] : marks_) if (m.view.empty()) m.view = "main";
      if (isAxes(schema_) || isSpace(schema_) || schema_ == "numberline" || schema_ == "timeline") curData_ = "main";
    }
    if (!ln.id.empty() || !views_.empty()) { // a named view: create it, or select it for the marks that follow
      Line named = ln;
      if (named.id.empty()) named.id = "main";
      const Line &ln = named;
      auto v = std::find_if(views_.begin(), views_.end(), [&](const View &w) { return w.id == ln.id; });
      if (v == views_.end()) { views_.push_back({ln.id, ln.kind.empty() ? "free" : ln.kind, ln.props}); v = views_.end() - 1; }
      else if (!ln.kind.empty()) { v->schema = ln.kind; v->props = ln.props; }
      curView_ = ln.id;
      bool data = isAxes(v->schema) || isSpace(v->schema) || v->schema == "numberline" || v->schema == "timeline";
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
    if (!opt_.instant) emit("part " + std::to_string(currentPart()));
    break;
  }
  }
}

// A re-mark of a mark, a link or a group: each property it names changes in place, eased like a morph (axioms 1, 4)
void Engine::updateProps(Obj &o, const Props &props) {
  for (auto &p : props) {
    settleActive(o.id, p.key);
    Value *cur = findProp(o.props, p.key);
    double was = cur && cur->isNumeric() ? num(*cur) : NAN, to = p.val.isNumeric() ? num(p.val) : NAN;
    if (std::isfinite(was) && std::isfinite(to) && (was != to || cur->markup() != p.val.markup())) {
      // a number, or an expression ({k}): eased from where it is, landing on what was written
      Active a;
      a.kind = Active::NUM; a.id = o.id; a.prop = p.key;
      a.from = was; a.to = to; a.finalVal = p.val;
      a.dur = opt_.instant ? 0 : 0.7f;
      a.blocking = false;
      a.log = "mark " + o.id + "." + p.key + " " + fmtNum(a.from) + " -> " + fmtNum(a.to);
      recordActive(a, *cur);
      actives_.push_back(a);
    } else if (cur && cur->type == Value::LIST && p.val.type == Value::LIST && cur->markup() != p.val.markup()) {
      // a list re-marked: points glide, array items travel to their new cells
      o.strBlend[p.key] = {cur->markup(), opt_.instant ? 1.f : 0.f};
      *cur = p.val;
      Active a;
      a.kind = Active::STR; a.id = o.id; a.prop = p.key; a.dur = opt_.instant ? 0 : 0.9f; a.blocking = false;
      actives_.push_back(a);
    } else if (cur && cur->type == Value::STR && p.val.type == Value::STR && cur->str != p.val.str) {
      o.strBlend[p.key] = {cur->str, opt_.instant ? 1.f : 0.f};
      *cur = p.val;
      if (Mark *mk = mark(o.id); mk && p.key == catProp(*mk)) colorForKey("cat:" + p.val.str); // only a category's new value is a colour (not a label, a pointer's at=)
      Active a;
      a.kind = Active::STR; a.id = o.id; a.prop = p.key; a.dur = opt_.instant ? 0 : 0.8f; a.blocking = false;
      actives_.push_back(a);
    } else {
      if (p.val.isNumeric() || (cur && cur->isNumeric())) recordSet(o.id, p.key, cur ? *cur : Value::number(NAN), p.val);
      setProp(o.props, p.key, p.val);
    }
  }
}

void Engine::doMark(const Line &ln) {
  if (auto lk = links_.find(ln.id); lk != links_.end()) { // "mark w11 color=yellow" on a link: update it in place
    updateProps(lk->second, ln.props);
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
    if (findProp(m.props, "tex")) { logTime("texnew " + ln.id); restDue_ = anySay_; } // a formula appearing is a formula changing (settled 1 s later); the opening formula needs no rest
    order_.push_back(ln.id);
    Mark &mm = marks_.emplace(ln.id, m).first->second;
    bornAt_[ln.id] = clock_;
    mm.series = seriesKey(mm);
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
    else if (!mm.series.empty()) rule += " · data on one plot -> one color";
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
  updateProps(m, ln.props);
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
    updateProps(l, ln.props);
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
    updateProps(it->second, ln.props);
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

  if (ln.to.type == Value::EXPR && cur && cur->type == Value::EXPR && !std::isfinite(num(ln.to))) {
    // a function of x (a curve, a surface) becomes another: the two blend over the morph (pathAt, surfaceAt)
    o->fnBlend[ln.prop] = {*cur, dur > 0 ? 0.f : 1.f};
    setProp(o->props, ln.prop, ln.to);
    Active a;
    a.kind = Active::STR; a.id = ln.id; a.prop = ln.prop; a.dur = dur;
    a.ease = rateByName(ln.ease);
    a.log = "morph " + target + " " + cur->markup() + " -> " + ln.to.markup();
    actives_.push_back(a);
    caption("change of function -> the curve bends from one to the other (" + target + ")");
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
  if (opt_.instant) { // the values on the way: something driven by this one may peak between the ends (x(10-x))
    Value *v = findProp(o->props, ln.prop);
    Value keep = *v;
    for (int i = 1; i < 8; i++) {
      float k = i / 8.f;
      *v = Value::number(a.from + (a.to - a.from) * (a.ease ? a.ease(k) : k));
      sampleValues();
    }
    *v = keep;
  }

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
  applyPending(); // a sentence start is where a scale may move (live): before this one's words
  sceneSpoken_ = true;
  if (opt_.instant) sayFacts_ = stageFacts(); // for the binding check at the end of the sentence
  say_ = SayRun{};
  say_.active = true;
  anySay_ = true;
  say_.uid = ln.uid;
  if (opt_.events && !opt_.instant) emit("say " + std::to_string(ln.uid) + " " + jsonStr(spokenText(ln)));
  if (!opt_.instant) logTime("say " + std::to_string(ln.uid));
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
  // so the same marker name can be reused sentence after sentence. A line armed earlier is found too, but a later
  // sentence's line never fires early.
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
  fireIn(0, sayStep_); // lines armed earlier (set-up before the first sentence, or a beat early); never a later sentence's
}

// A cue line whose |cue| was never spoken (a typo, a capital, a forgotten marker) still runs, at the
// end of its sentence: a slip in the script must not hide the point of the explanation.
void Engine::closeSentence() {
  size_t next = sayStep_ + 1;
  while (next < script_.steps.size() && script_.steps[next].main.verb != Verb::SAY) next++;
  for (size_t i = sayStep_ + 1; i < next; i++)
    if (!script_.steps[i].hook.empty() && !hookFired_[i]) {
      if (next < script_.steps.size() && saysHook(script_.steps[next].main, script_.steps[i].hook))
        continue; // written one beat early: it stays armed and fires on its word in the next sentence (linted at load)
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
    logTime("end " + std::to_string(say_.uid));
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
      if (auto fb = o->fnBlend.find(a.prop); fb != o->fnBlend.end()) fb->second.second = e;
    } else if (a.kind == Active::CAM) {
      cam_.k = e;
    } else if (a.kind == Active::ORBIT) {
      DataScope ds(*this, a.id);
      spc_.k = e;
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
          if (Mark *mk = mark(o->id)) { mk->texPrev = it->second.first; mk->texSettledAt = clock_; logTime("tex " + o->id); restDue_ = true; }
        o->strBlend.erase(a.prop);
        o->fnBlend.erase(a.prop);
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
    if (say_.active) { emit("end " + std::to_string(say_.uid)); logTime("end " + std::to_string(say_.uid)); }
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

// ================================================================ seeking (a prepared lecture: parts and sentences)
// The stage as it stands just before `step`: everything before it runs again, instantly (as interrupt() finishes a
// beat), so the picture is the one the script built, with its history (axioms 1 and 6). Then it plays on.
void Engine::seekStep(size_t step) {
  step = std::min(step, script_.steps.size());
  reset();
  bool was = opt_.instant;
  opt_.instant = true;
  stopAt_ = step;
  for (int guard = 0; guard < 100000 && (cursor_ < step || blocking()); guard++) {
    waitingPause_ = false;
    update(0);
  }
  stopAt_ = SIZE_MAX;
  opt_.instant = was;
  say_.lingering = 0;
  for (auto &id : order_) if (Obj *o = find(id)) o->appear = 1;
  readbackPrinted_ = false;
}

size_t Engine::stepAtLine(int lineNo) const {
  for (size_t i = 0; i < script_.steps.size(); i++)
    if (script_.steps[i].main.lineNo >= lineNo) return i;
  return script_.steps.size();
}

std::vector<size_t> Engine::partSteps() const {
  std::vector<size_t> out;
  for (size_t i = 0; i < script_.steps.size(); i++)
    if (script_.steps[i].main.verb == Verb::SCENE) out.push_back(i);
  return out;
}

int Engine::currentPart() const {
  int n = 0;
  for (size_t s : partSteps()) if (s < cursor_) n++;
  return n;
}

std::string Engine::partTitle(int n) const {
  auto parts = partSteps();
  return n >= 1 && n <= (int)parts.size() ? script_.steps[parts[n - 1]].main.id : "";
}

void Engine::seekPart(int n) {
  auto parts = partSteps();
  if (parts.empty()) { seekStep(0); return; }
  seekStep(parts[std::clamp(n, 1, (int)parts.size()) - 1]);
}

void Engine::backSentence() {
  auto isSay = [&](size_t i) { return i < script_.steps.size() && script_.steps[i].main.verb == Verb::SAY; };
  size_t to = sayStep_; // the sentence playing, or the last one played: from its start
  if (!isSay(to) || (say_.active && say_.t <= 2)) { // it has just begun: the one before
    size_t prev = 0;
    for (size_t i = 0; i < sayStep_; i++) if (isSay(i)) prev = i;
    to = prev;
  }
  seekStep(to);
}

std::string Engine::sayingNow() {
  return say_.active && sayStep_ < script_.steps.size() ? spokenText(script_.steps[sayStep_].main) : "";
}

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
  while (!blocking() && cursor_ < script_.steps.size() && cursor_ < stopAt_) {
    const Step &next = script_.steps[cursor_];
    if (next.hook.empty() && next.main.verb == Verb::SAY && restDue_ && !opt_.instant) {
      // a formula changed in the last sentence: it rests, still, before the next sentence starts
      restDue_ = false;
      if (float need = kRest - quiet_; need > 0.01f) {
        Active a;
        a.kind = Active::WAIT;
        a.dur = need;
        actives_.push_back(a);
        say_.lingering = std::max(say_.lingering, need + 0.6f);
        break;
      }
    }
    size_t idx = cursor_++;
    if (!script_.steps[idx].hook.empty()) continue; // armed: runs when the voice reaches |hook|
    execStep(idx);
  }
  tickSay(dt);
  tickActives(dt);
  if (opt_.instant && sampleValues() && !scanPending_) usePrescan(); // the run reads what the prescan can't: as it goes
  bool moving = say_.active;
  for (auto &a : actives_) if (a.kind != Active::WAIT) moving = true;
  quiet_ = moving ? 0 : quiet_ + dt;
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
        if (p.val.type == Value::EXPR && p.key != "fn" && p.key != "xt" && p.key != "yt" && p.key != "zt" && p.key != "vx" && p.key != "vy" && p.key != "vz") {
          double v = num(p.val);
          evals.push_back(m.id + "." + p.key + " = " + fmtNum(v));
          if (!std::isfinite(v))
            warn("value: " + m.id + "." + p.key + " = {" + p.val.str + "} is not a number (division by zero, a cyclic reference, or an unknown name)");
        }
      }
      if (const Mark *r = ridden(m)) // what the ride gives it, for the reader: nothing is stored
        for (const char *key : {"x", "y", "z"})
          if (double v; rideCoord(m, key, v)) {
            evals.push_back(m.id + "." + key + " = " + fmtNum(v) + " (on " + r->id + ")");
            if (!std::isfinite(v)) warn("value: " + m.id + "." + key + " on " + r->id + " is not a number there (outside its domain?)");
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
  for (auto &l : spc_.spec) o << l << "\n"; // a space's camera: the lines that turned it, in order
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
      if (int c = spaceCoincide(a, b); c >= 0) { // in space: at one point in 3D, not lined up in this view
        if (c == 1) o << "# overlapping: " << a.id << " " << b.id << "\n";
        continue;
      }
      if (CheckCollisionRecs(bodyBounds(a), bodyBounds(b))) o << "# overlapping: " << a.id << " " << b.id << "\n";
    }
  if (!since_.empty()) {
    o << "# since:";
    for (size_t i = 0; i < since_.size(); i++) o << (i ? ";" : "") << " " << since_[i];
    o << "\n";
  }
  // binding is judged per sentence against the whole script (--check), not mid-stream; structure is about the
  // live path (a scale that moved under the learner), not the scene: the server reads it off the stage's stderr
  for (auto &l : lint_)
    if (l.rfind("binding:", 0) != 0 && l.rfind("structure:", 0) != 0) o << "# lint: " << l << "\n";
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

// A line is ready when its voice is in the disk cache (written after it is done), so a lecture warmed with
// --prepare-voice plays at once, and offline.
void Engine::voiceProgress(int &total, int &ready, int &failed) {
  total = ready = failed = 0;
  for (auto &[uid, u] : lineUtt_) {
    std::lock_guard<std::mutex> l(u->mu);
    total++;
    if (u->failed) failed++;
    else if (u->cached || !u->wavPath.empty()) ready++;
  }
}

void Engine::stopVoice() {
  if (tts_) tts_->stop();
}

} // namespace mk
