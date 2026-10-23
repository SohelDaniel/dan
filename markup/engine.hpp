// The engine owns the dictionary: model words in, visual channels out.
//
//   thing        -> a mark with a stable identity (never deleted, only morphed)
//   magnitude    -> size (bar length / dot area from a shared zero)
//   category     -> color (same category, same color; unrelated things never share one)
//   order        -> position (declaration order left-to-right, or the schema's axis)
//   part-of      -> enclosure (group box)
//   connection   -> line / arrow
//   equals/similar -> dashed link with = / ≈
//   name, value  -> short text label
//   change       -> motion over time (morph)
//   voice subject-> focus (everything else dimmed)
#pragma once
#include "../rm/raymanim.hpp"
#include "script.hpp"
#include "tex.hpp"
#include "place/check.hpp"
#include "place/flow.hpp"
#include "place/labels.hpp"
#include "place/spring.hpp"
#include "viz/viz.hpp"
#include "viz/space.hpp"
#include <functional>
#include "tts.hpp"
#include <memory>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace mk {

inline bool isAxes(const std::string &schema) { return schema == "axes" || schema == "plane"; }
// a 3D world, z up (space.cpp): projected through one mapping, drawn as 2D strokes and triangles by depth
inline bool isSpace(const std::string &schema) { return schema == "space"; }
// kinds only a space has (the others there are the 2D ones given a z: dot, vector, line, arrow, path, polygon, grid)
inline bool isSpaceKind(const std::string &k) { return k == "surface" || k == "sphere" || k == "plane"; }
// placement words: never colours, magnitudes or box rows. Only on= / in= / over= still place (touching on
// purpose); the rest are retired (position comes from structure) and only linted.
inline bool isPlacementKey(const std::string &k) {
  return k == "region" || k == "below" || k == "above" || k == "left-of" || k == "right-of" || k == "near" ||
         k == "align" || k == "on" || k == "in" || k == "over";
}

// kinds drawn in world units on axes / plane
inline bool isWorldKind(const std::string &k) {
  return k == "circle" || k == "rect" || k == "polygon" || k == "angle" || k == "vector" || k == "grid";
}
// fields over the plane: drawn under everything else on axes/plane, no body, no place to find
inline bool isFieldKind(const std::string &k) { return k == "field" || k == "slope" || k == "heat" || k == "contour"; }
// curves: strokes across the axes (a function's graph, or an ODE solution through a point)
inline bool isCurveKind(const std::string &k) { return k == "path" || k == "solution"; }
// text-like blocks that stack in the top band unless placed
inline bool isBlockKind(const std::string &k) {
  return k == "tex" || k == "card" || k == "code" || k == "array" || k == "table" || k == "area";
}
// annotations: marks about other things (of=…): annotate.cpp
inline bool isAnnotationKind(const std::string &k) {
  return k == "brace" || k == "enclose" || k == "underline" || k == "cross" || k == "check";
}
// "a[3]" -> "a" (and k = 3); "a" -> "a" (k = -1)
inline std::string baseId(const std::string &ref, int *cell = nullptr) {
  size_t b = ref.find('[');
  if (cell) *cell = b == std::string::npos ? -1 : std::atoi(ref.c_str() + b + 1);
  return b == std::string::npos ? ref : ref.substr(0, b);
}

struct Obj {
  std::string id, kind, category;
  Props props;
  int order = 0;
  float appear = 0;   // 0..1 entrance (Create-style)
  float enterIn = 0;  // seconds until its entrance starts (things arriving together enter one after another)
  float lit = 1;      // focus brightness, eased
  float pulse = 0;    // indicate: 0..1..0
  std::string pulseRef; // what the pulse points at (the thing, or one cell: a[3])
  // a category (string) property mid-morph: prop -> (old text, progress 0..1)
  std::map<std::string, std::pair<std::string, float>> strBlend;
};

// text over a busy 3D picture: a dark outline, then the text (space.cpp)
void textOutlined(const std::string &s, Vector2 at, float size, Color c, float ax, float ay);

struct Mark : Obj {
  std::string view;          // the named view it belongs to ("" with a single schema)
  std::string texPrev;       // formulas: what it was before its last change, and when that change settled
  float texSettledAt = -1e9f; //   (what the change brought in stays lit for a moment)
  Vector2 pos{0, 0}, tpos{0, 0};
  bool placed = false;    // has had a first layout (so it doesn't fly in from 0,0)
  bool anchorBottom = false; // pos is bottom-centre (free layout) instead of centre
  bool positioned = false;   // the schema gave it a place this frame
  float barScale = 0;        // px per unit for bars (free layout)
  bool direct = false;       // position is data (axes, number line): no layout easing
  float labelLift = 0;       // raises a name label that would collide with a neighbour's
  int labelQuad = 0;         // axes: which corner of its dot the name sits at (0 NE, 1 NW, 2 SE, 3 SW)
  bool culled = false;       // outside the camera's view this frame: not drawn, no labels
  Rectangle spaceBox{0, 0, 0, 0}; // in a space: where it is on screen this frame (projected)
};

// a piece of text placed outside (or beside) the thing it belongs to: a name, a value, a category.
// Layout decides where it goes; drawing only renders it; the checker sees exactly this box.
struct TextLabel {
  enum Ink : uint8_t { Fg, Muted, Mark } ink = Fg;  // foreground, quiet, or the thing's own color
  std::string text, role;                            // role: "name", "value", "category", "label"
  Vector2 at{0, 0};
  float size = 18, ax = 0.5f, ay = 0.5f;            // anchor: 0 = left/top, 1 = right/bottom
  std::string blendKey;                              // a category that cross-fades while it morphs
  bool leader = false;                               // a thin line back to the thing
  Vector2 leaderFrom{0, 0}, leaderTo{0, 0};
  float leaderWidth = 1, leaderAlpha = 0.5f;
};

// how a link is drawn: an arc over baseline bars, or a straight / bent line between edges
struct LinkGeom {
  bool ok = false, overBars = false;
  Vector2 pa{0, 0}, pb{0, 0};
  float bend = 0;                                    // bulge for arcs (0: straight)
  float labelT0 = 0, labelT1 = 1;                    // the stretch of line its label may sit on
  std::string label;
  Vector2 mid{0, 0};                                 // where the label goes
  std::vector<Vector2> line;                         // the drawn path, sampled (for checks)
};

struct Link : Obj {
  std::string a, b;
  bool directed = true;
};

struct Group : Obj {
  std::vector<std::string> members;
};

struct Active {
  enum Kind { NUM, STR, WAIT, PAUSE, CLEAR, PULSE, CAM, FLIGHT, ORBIT } kind = NUM; // ORBIT: a space view's camera (id: the view)
  std::string id, prop;
  std::string dest, via;    // FLIGHT: where the copy goes, and the link it travels along
  double from = 0, to = 0;
  Value finalVal;           // what the prop becomes when done (keeps expressions)
  float t = 0, dur = 1;
  float delay = 0;          // waits this long before it starts (one of a staggered set), neither moving nor drawn
  rm::RateFunc ease = rm::rate::smooth;
  bool blocking = true;
  std::string log;          // for readback "since:"
};

struct SayRun {
  bool active = false;
  int uid = 0;              // the Line's uid (events name the line by it)
  std::vector<SayWord> words;
  std::vector<std::string> trailing;
  std::vector<float> start, end;
  float t = 0, total = 0;
  size_t nextWord = 0;
  bool trailingFired = false;
  UttPtr utt;               // Inworld audio for this line (null: estimated timing)
  bool audioStarted = false;
  float waited = 0;         // seconds spent waiting for the first audio chunk
  float lingering = 0;      // subtitle stays a while after speaking
};

struct EngineOptions {
  bool instant = false;     // headless: everything completes immediately
  bool voice = false;       // Inworld realtime TTS
  bool recording = false;   // video time drives the clock (audio is muxed afterwards)
  bool verbose = false;     // print each line, hook and voice start with times
  bool events = false;      // live: print "@ev ..." lines on stdout (say / word / end / idle / reset)
  bool quiet = false;       // don't print or save the readback when the script finishes (snapshots)
  bool newPlacer = true;    // labels placed by the placement module (--placer old: the fixed offsets)
  bool shadow = false;      // the instant copy a live engine keeps to read {expr} in Say ahead of play: no layout, no TeX, silent
  float wpm = 170;          // estimated timing when there is no voice
  TtsConfig tts;
};

class Engine : public EvalCtx {
public:
  explicit Engine(EngineOptions opt = {});
  ~Engine() override;

  bool load(const std::string &path);       // reads + parses + prescans
  bool loadText(const std::string &text);
  // live use: continue the running script without restarting the stage (streamed turns)
  bool appendText(const std::string &text);
  // start synthesising a Say line before its beat arrives (the server sends it as soon as it is written)
  // returns the context it asked with (the sentences before), which the line's beat will ask with too
  std::vector<std::string> prefetchVoice(const std::string &sayLine);
  std::vector<std::string> spokenLines();   // every Say as it will be spoken, {expr} read off the stage
  std::string pulsing(const std::string &id); // what of this thing pulses right now ("a[2]", "a"), "" if nothing
  // numbers and names said aloud: how many, how many on stage at their word, pointed by the script, pointed ahead
  std::string pointingReport() const;
  // numbers said aloud with the words they span ("seven hundred eighty-four", "zero point one four"); pure, for tests too
  struct SpokenNum { size_t first = 0, last = 0; double v = 0; bool percent = false; std::string spoken; };
  static std::vector<SpokenNum> readNumbers(const std::vector<std::string> &toks);
  static void sayTokens(const std::vector<std::string> &words, std::vector<std::string> &toks, std::vector<size_t> &owner);
  bool speaking() const { return say_.active && say_.utt != nullptr; }
  // stop what is playing: the rest of the script so far runs instantly (state stays consistent), voice stops
  void interrupt();
  void emit(const std::string &ev);         // "@ev <ev>" on stdout when events are on
  void reset();                             // back to t=0
  void update(float dt);
  void draw();
  void skip();                              // finish what is running now
  void togglePause();
  bool paused() const { return paused_; }
  bool waiting() const { return waitingPause_; }
  bool finished() const;
  void runToEnd();                          // headless simulation
  void setShowDictionary(bool b) { showDict_ = b; }
  bool showDictionary() const { return showDict_; }
  void resume();                            // releases a `pause` line

  std::string readback(bool withComments = true);
  Color colorFor(const std::string &id) { Mark *m = mark(id); return m ? colorOf(*m) : BLANK; } // for tests
  // layout: everything drawn, as the placement checker sees it; and its verdict (placement.cpp)
  place::Picture picture();
  // a mark's outside text, positioned (placement.cpp). legacy: the fixed offsets, ignoring the placer
  std::vector<TextLabel> markLabels(const Mark &m, bool legacy = false);
  Rectangle labelBox(const TextLabel &t);
  std::vector<TextLabel> markLabelsAt(const Mark &m); // the fixed offsets (what the old layout did)
  LinkGeom linkGeometry(const Link &l, bool legacy = false);
  LinkGeom linkGeometryAt(const Link &l);
  std::string linkLabel(const Link &l);   // the text on a link: its label (with its value), else its properties
  std::vector<Rectangle> furniture();   // tick numbers and axis titles (placement.cpp)
  std::vector<Rectangle> furnitureHere(); // the current data view's
  int regionOf(const Mark &m, int depth = 0); // the band (1) or its view (2, 3, ...): the part of the stage it belongs to
  void addFrames(place::Picture &p);      // each view's frame: its plot, ticks, title and the line between views
  const Value *attachment(const Mark &m) const; // on= / in= / over=: touching a thing on purpose
  // the new placer (--placer new): every movable label placed together, each frame, with hysteresis
  // where the placer put a label (box, side) and where it is drawn now. The springs act on the label's
  // offset from what it names: when the thing moves the label moves with it at once; when the label
  // changes side, it glides there.
  struct LabelSlot {
    Rectangle box{0, 0, 0, 0};
    int side = -1;
    bool shown = true;
    Vector2 anchor{0, 0};
    place::Spring x, y;   // offset from anchor
    Vector2 at() const { return {anchor.x + x.x, anchor.y + y.x}; }
  };
  void placeAllLabels(float dt);
  struct LabelPass; // one frame's placement (placement.cpp)
  void labelObstacles(LabelPass &lp);
  void markLabelRequests(LabelPass &lp);
  void labelSpots(const Mark &m, const std::string &k, const std::vector<Vector2> &stroke, Rectangle box, place::LabelReq &r);
  void linkLabelRequests(LabelPass &lp);
  void keepLabels(const LabelPass &lp, const std::vector<place::LabelOut> &out, float dt);
  bool movableLabel(const Mark &m, const TextLabel &l);
  std::map<std::string, std::vector<LabelSlot>> labelSlots_;
  std::map<std::string, LabelSlot> linkSlots_;
  int labelFlips = 0;                   // labels that changed side between frames (stability metric)
  std::vector<std::string> flipLog;     // which, when (the first 40), for the layout report
  // curve and line geometry shared by drawing and labels
  std::vector<Vector2> curvePoints(const Mark &m);
  bool curveEnd(const Mark &m, Vector2 &p);  // where a curve visibly ends: its last point inside its plot
  size_t visibleEnd(const std::vector<Vector2> &pts); // index of that point in pts (pts.size() if none is inside)
  bool lineEnds(const Mark &m, Vector2 &a, Vector2 &b, bool &hasSlope);
  bool vectorEnds(const Mark &m, Vector2 &from, Vector2 &to);
  place::Report layoutCheck(std::vector<std::string> *messages = nullptr);
  const std::vector<std::string> &parseErrors() const { return script_.errors; }
  std::vector<std::string> lintMessages() const { return {lint_.begin(), lint_.end()}; }
  const std::string &path() const { return path_; }
  float time() const { return clock_; }
  // (start time, wav file) for every narrated line, so a recording can be given its soundtrack
  std::vector<std::pair<float, std::string>> audioLog() const;

  // the learner's hand: the mouse at screen size, each frame (drags a slider of a `set=learner` param)
  void pointer(Vector2 p, bool down);

  // EvalCtx
  bool var(const std::string &name, double &out) override;
  bool member(const std::string &id, const std::string &prop, double &out) override;
  bool call(const std::string &name, const std::vector<ExprPtr> &args, double &out, int depth) override;

private:
  // ---- model
  Obj *find(const std::string &id);
  Mark *mark(const std::string &id);
  double num(const Obj &o, const std::string &key, bool *ok = nullptr, int depth = 0);
  double num(const Value &v, int depth = 0);
  std::string magProp(const Mark &m) const;
  std::string catProp(const Mark &m) const;
  std::string catValue(const Mark &m) const;
  std::string effectiveKind(const Mark &m) const;
  bool related(const std::string &a, const std::string &b);
  std::string groupOf(const std::string &id) const;
  std::string topGroup(const std::string &id) const; // the outermost group around it ("" if none)

  // ---- runtime
  void analyze(bool fresh);                 // prescan + static lint over the whole script
  void execStep(size_t idx);
  void execLine(const Line &ln);
  void doMark(const Line &ln);
  void doLink(const Line &ln);
  void doGroup(const Line &ln);
  void doMorph(const Line &ln);
  void doCamera(const Line &ln);
  void doSpaceCamera(const Line &ln);         // view=, orbit=, tilt=, spin, zoom= on= in a space (space.cpp)
  void settleActive(const std::string &id, const std::string &prop);
  void startSay(const Line &ln);
  void fireHook(const std::string &name);
  void closeSentence();
  void checkBinding(size_t sayIdx, size_t nextIdx); // binding.cpp: words vs picture, per sentence (--check)
  struct StageFacts {
    std::string text;                       // all readable text on stage
    std::map<std::string, int> kindCount;   // visible marks per kind and per category
    int links = 0, labelledLinks = 0;
    std::map<std::string, int> linkColors;
  };
  StageFacts stageFacts();
  // pointing ahead (binding.cpp): a number or a name said aloud that exactly one thing on stage shows gets a pointer
  // on its word, worked out by the instant run (the shadow, live) as each word is reached
  static bool pointable(const SpokenNum &n, const std::vector<std::string> &toks);
  std::string holderOfValue(double v, bool percent);
  std::string holderOfName(const std::string &letter);
  std::map<int, std::vector<std::pair<size_t, std::string>>> autoPoint_; // by Say uid: (word, target)
  struct PointStats { int spoken = 0, onStage = 0, pointed = 0, added = 0; } pointStats_;
  void applyPointers();                     // live: the shadow's pointers onto Says that haven't started
  StageFacts sayFacts_;                     // at the start of the current sentence                    // run this sentence's cued lines that never got their cue
  bool blocking() const;
  void tickActives(float dt);
  void tickSay(float dt);
  void caption(const std::string &rule);
  void warn(const std::string &msg);

  // ---- colors (Zero: unrelated things never share a color)
  Color colorOf(const Mark &m);
  Color colorForKey(const std::string &key);
  void claimColor(const Mark &m);

  // ---- layout (layout.cpp)
  std::string schemaNow() const;
  void layout(float dt);
  Rectangle layoutBand(const std::string &schema, bool alone, float dt); // blocks at the top; the rest of the stage
  bool inBand(const Mark &m) const;                               // a block the band places
  bool plotOnStage() const;                                       // the scene or a view is a plot
  float bandFit() const;                                          // the scale at which the band leaves a plot its room
  void layoutPointers();                                          // at= / on= / in= / over=
  void layoutFree(Rectangle area, const std::vector<std::string> &ids);
  void layoutGraph(Rectangle area, const std::vector<std::string> &ids, const std::string &view = "");
  void layoutNumberline(Rectangle area);
  void layoutAxes(Rectangle area);
  // ---- space (space.cpp)
  // a thing in space, in data units: what it draws, and what its name, the placer and the checker see of it
  struct SpaceShape {
    struct Stroke {
      std::vector<viz::V3> pts;
      Color c{};
      float width = 3, alpha = 1;
      float headSize = 14;  // pixels, at most 40% of the arrow as drawn (a short arrow is not all head)
      bool head = false, dashed = false, ink = true; // ink: a line the checker and the placer see (not mesh, not shadow)
    };
    struct Tri {
      viz::V3 a, b, c;
      Color col{};
      float alpha = 1;
    };
    std::vector<Stroke> strokes;
    std::vector<Tri> tris;
    std::vector<viz::V3> dots;
    bool ok = false;
  };
  SpaceShape spaceShape(const Mark &m);
  bool spacePoint(const Mark &m, viz::V3 &p); // where a thing is: a point, a tip, a centre
  void spaceRanges();
  void layoutSpace(Rectangle area, float dt);
  void drawSpace();                           // the current space view: furniture and things, back to front
  std::vector<Vector2> spaceStroke(const Mark &m); // its line on screen (vectors, lines, curves), for labels and the checker
  bool spaceName(const Mark &m, Vector2 &at, float &ax, float &ay); // where its name goes
  std::vector<std::pair<Rectangle, std::string>> spaceTicks(); // tick numbers and axis names, on screen
  int spaceCoincide(const Mark &a, const Mark &b); // -1: not both in a space; 1: they are at one point in 3D; 0: only in projection
  void layoutTimeline(Rectangle area);
  void zeroCheck();
  float footprint(const Mark &m);
  float dotRadius(const Mark &m);
  Vector2 boxSize(const Mark &m);
  // arrays: cols= makes a grid (rows of that many cells, row-major). A reference names cells:
  // a[3], a[1:3] (a span), and on a grid M[1][2], M[1][:] (a row), M[:][2] (a column)
  int arrayCols(const Mark &m);
  bool showsIndexRow(const Mark &m);        // a row of cells numbers them underneath (index=0 turns it off; grids don't)
  // a row of cells seated on= another's cells (a kernel on the input it slides along): the gap it keeps above them,
  // and the room a block needs above it for what is seated on it (its height, that gap, its own name)
  float seatGap(const Mark &seated, const Mark &target);
  float roomAbove(const Mark &target);
  // the targets of a staggered line: each cell of a span, in order ("a[0:2]" -> a[0] a[1] a[2]); one target otherwise
  std::vector<std::string> staggerTargets(const std::string &ref, float stagger);
  std::vector<int> cellsOf(const Mark &m, const std::string &ref);
  // what a box shows, used by both its size and its drawing: the name (wrapped), then one row per
  // property that is content (not position, style or structure)
  std::vector<std::string> boxNameLines(const Mark &m);
  std::vector<std::pair<std::string, std::string>> boxRows(const Mark &m);
  Rectangle bodyBounds(const Mark &m);
  Rectangle fullBounds(const Mark &m);   // incl. name labels
  Rectangle groupBounds(const Group &g, int depth = 0, bool arcs = true); // arcs: around its return links too
  double pathAt(const Mark &path, double x, int depth = 0);
  bool paramAt(const Mark &path, double t, double &x, double &y);
  bool isParametric(const Mark &m) const;
  void clearStage();
  void computeRanges(float dt);

  // ---- drawing (render.cpp)
  void drawMark(const Mark &m);
  void drawLink(const Link &l);
  void drawGroup(const Group &g);
  void drawTex(const Mark &m);
  void drawLabels(const Mark &m);
  void drawWorld(const Mark &m);            // circle rect polygon angle vector
  void drawBlock(const Mark &m);            // card code array table
  void drawPulses();
  Vector2 blockSize(const Mark &m);
  Rectangle cellRect(const Mark &m, int k); // array cell k on screen
  bool anchorBox(const std::string &ref, Rectangle &out); // "id" or "id[k]" -> its box on screen
  bool worldExtent(const Mark &m, float &x0, float &x1, float &y0, float &y1);
  std::vector<double> listNums(const Mark &m, const std::string &key);
  float opacityOf(const Obj &o);
  void drawSubtitle();
  // ---- what the learner sets (controls.cpp)
  struct Control { std::string id, name; double value = 0, lo = 0, hi = 1; float width = 0; Rectangle track{}; };
  bool learnerSets(const Mark &m);
  std::vector<Control> controls();
  void drawControls();
  std::string dragging_;
  void drawHud();
  void drawInfo();
  float alphaOf(const Obj &o);
  bool ghost(const Obj &o);  // hidden (faded to a ghost): still drawn faintly, but it takes no room
  std::string valueText(const Mark &m, const std::string &key);
  std::string displayName(const Obj &o) const;
  std::string inkName(const Mark &m);
  std::string labelText(const Mark &m);

  // ---- formulas (tex marks)
  std::string texSource(const std::string &raw);   // \val{expr} -> the value now
  std::string valueText(const std::string &expr);  // a value as shown and spoken: integers stay integers
  TexPtr texNow(const Mark &m);                     // current layout (last good one while typesetting)
  float texSize(const Mark &m);                     // font px
  std::map<std::string, TexPtr> lastTex_;
  std::map<std::string, Value> texExpr_;

  // ---- voice
  void prepareVoice();
  void stopVoice();

  EngineOptions opt_;
  std::string path_;
  Script script_;
  size_t cursor_ = 0;
  std::vector<bool> hookFired_;
  std::map<std::string, Mark> marks_;
  std::map<std::string, Link> links_;
  std::map<std::string, Group> groups_;
  std::vector<std::string> order_; // all ids in declaration order
  std::vector<Active> actives_;
  SayRun say_;
  std::string focus_;
  std::string schema_ = "auto", autoSchema_ = "free", title_;
  float headline_ = 1; // 0: a new scene's name, large in the centre of an empty stage; 1: in the corner
  bool stageEmpty();
  std::vector<std::string> autoSchemas_; // one per scene
  size_t sceneIdx_ = 0;
  float stageFade_ = 1;                   // scene transitions
  Props schemaProps_;
  std::vector<std::string> since_;
  std::set<std::string> lint_;
  std::string caption_;
  float captionT_ = 0;
  bool paused_ = false, showDict_ = true, waitingPause_ = false;
  float clock_ = 0;
  bool readbackPrinted_ = false;
  bool idleSent_ = false;

  // prescan: stable scales across a scene (Structure preservation); one per scene, the current one active
  std::map<std::string, double> maxAbs_;
  std::map<std::string, double> minVal_, maxVal_;
  std::map<std::string, bool> propIsInt_;
  struct Prescan {
    std::map<std::string, double> maxAbs, minVal, maxVal;
    std::map<std::string, bool> isInt;
  };
  std::vector<Prescan> scenePrescan_;
  void usePrescan(); // make the current scene's prescan the active one

  // colors
  std::map<std::string, int> colorSlot_;
  int nextSlot_ = 0;

  // ranges for schemas, eased
  float binR_ = 10;         // number line with bin=: the dot size that lets its tallest column fit
  float nlMin_ = 0, nlMax_ = 10, axX0_ = -1, axX1_ = 5, axY0_ = -1, axY1_ = 10, tlMin_ = 0, tlMax_ = 10;
  bool rangesInit_ = false;
  float nlTarget_[2] = {0, 10}, axTarget_[4] = {-1, 5, -1, 10}, tlTarget_[2] = {0, 10}; // eased towards
  std::string nlProp_;
  rm::Axes axes_;
  rm::NumberLine nl_;
  std::map<std::string, float> barScale_; // eased px/unit per property
  std::set<std::string> barEased_;        // the scales eased this frame (each once)
  std::map<std::string, std::string> graphSig_; // per view ("" for the scene's one graph)
  std::map<std::string, Vector2> graphPos_;
  // views: named schemas sharing the stage (schema net graph / schema loss axes …), in declaration order.
  // Each view that holds data (axes, plane, number line, timeline) has its own ranges and axes (DataCtx).
  struct View { std::string id, schema; Props props; Rectangle area{0, 0, 0, 0}; };
  std::vector<View> views_;
  std::string curView_;                    // new marks join it
  std::string schemaOf(const Mark &m) const; // the schema that lays this mark out
  bool inDataView(const Mark &m) const;      // laid out by the scene's data schema (always, with no views)
  void layoutViews(Rectangle area, const std::vector<std::string> &body);
  // sequence (a conversation): parties across the top, each link a message on its own row, in order
  void layoutSequence(Rectangle area, const std::vector<std::string> &ids);
  std::map<std::string, float> seqRow_;   // message (link id) -> its row's y
  float seqBottom_ = 0;                   // where the lifelines end
  std::map<std::string, place::FlowMemo> flowMemo_; // per view: each thing's column and place last time (newcomers are inserted)
  struct LinkRoute {
    std::vector<Vector2> via, viaT;       // waypoints through the gaps: drawn (eased) and target
    float portA = 0, portB = 0;           // where it leaves and enters, along the sides
    bool back = false;                    // against the flow: an arc
    bool down = false;                    // a tree: leaves the bottom, enters the top
  };
  std::map<std::string, LinkRoute> linkRoute_; // links laid out by the flow (graph schema)

  // voice
  std::unique_ptr<Tts> tts_;
  std::map<int, UttPtr> lineUtt_;          // by Line::uid
  // {expr} in Say: each word as spoken, read off the stage when the word is reached (after the lines cued on it,
  // a morph at its end value). Worked out by an instant run: this engine's own, or the shadow's when live
  std::map<int, std::vector<std::string>> spoken_; // by Line::uid
  std::unique_ptr<Engine> shadow_;
  void startShadow(const std::string &text);
  std::string cellText(const std::string &inner);
  std::string spokenProp(const std::string &inner);
  std::string spokenWord(const std::string &w);
  std::vector<std::string> spokenWords(const Line &ln);
  std::string spokenText(const Line &ln);
  void queueVoice(size_t fromStep, bool report);
  // the voice's context: the sentences spoken just before a line. A Say prefetched by the server (#!voice) arrives
  // before the beat that comes ahead of it, so lines prefetched but not yet in the script count too, in order
public:
  std::vector<std::string> saysBefore(size_t step, size_t n); // test-visible: the context a scripted Say asks with
private:
  std::deque<std::string> prefetched_;     // Say texts asked for ahead of their beat, oldest first
  // hooks belong to the sentence they are spoken in
  size_t sayStep_ = 0;                     // step index of the current/last Say
  std::map<size_t, std::set<std::string>> firedBySay_;
  std::vector<std::pair<float, UttPtr>> audioLog_;
  void alignWords();
  std::string rangeSig_;

  // ---- camera (axes and plane): a view override on the schema's ranges, so everything that maps
  // data to pixels (marks, labels, the placer, the checker) sees the same view. Text and strokes stay
  // screen-size; curves are re-sampled for the visible range.
  struct CamView { double cx = 0, cy = 0, z = 1; }; // data centre, zoom relative to the schema's view
  struct Camera {
    bool on = false;          // a view override is in force
    CamView now, from, to;
    bool moving = false;      // a move is under way (or waiting to be resolved against the layout)
    bool resolved = false;    // 'to' has been worked out from the request
    float k = 1;              // eased progress of the move, 0..1
    bool release = false;     // the move goes back to the schema's view, then the override ends
    Line req;                 // the camera line being carried out
    std::string follow;       // keep this thing in view
    float vx = 0, vy = 0;     // follow spring velocities
    std::string spec;         // the camera line as written back by readback
  } cam_;
  // A view that holds data (axes, plane, number line, timeline) owns all the state above that maps data
  // to the screen. The current one lives in the members (so the code that reads axes_, nl_, cam_ … is
  // unchanged); the others wait in dataCtx_. DataScope makes a view's state current for a while.
  // a space view's camera: the projection (its ranges, area and orbit), a move in progress, the slow spin
  struct SpaceView {
    viz::Space proj;
    viz::Orbit from, to;
    float k = 1;                        // eased progress of the move, 0..1
    bool spin = false;
    float spinRate = 0;                 // radians per second, easing towards the spin's speed or 0
    bool rangesInit = false;
    std::string rangeSig;               // the ranges are worked out again when this changes (things added)
    double lo[3] = {-3, -3, -3}, hi[3] = {3, 3, 3}; // where the ranges are easing to
    std::vector<std::string> spec;      // the camera lines that brought it here, as readback writes them
  } spc_;
  struct DataCtx {
    std::string schema = "auto", nlProp, rangeSig;
    Props props;
    float binR = 10, nlMin = 0, nlMax = 10, axX0 = -1, axX1 = 5, axY0 = -1, axY1 = 10, tlMin = 0, tlMax = 10;
    float nlTarget[2] = {0, 10}, axTarget[4] = {-1, 5, -1, 10}, tlTarget[2] = {0, 10};
    bool rangesInit = false;
    rm::Axes axes;
    rm::NumberLine nl;
    Camera cam;
    SpaceView spc;
  };
  std::map<std::string, DataCtx> dataCtx_; // by view id: every data view but the current one (its slot is idle)
  std::string curData_;                    // the data view whose state is in the members ("" without views)
  void swapData(DataCtx &c);               // the members <-> c
  void useData(const std::string &view);   // make that data view's state current ("" or not a data view: no change)
  std::string dataViewOf(const Mark &m) const; // the data view it belongs to ("" if none)
  struct DataScope {
    Engine &e;
    std::string was;
    DataScope(Engine &en, const std::string &view) : e(en), was(en.curData_) { e.useData(view); }
    ~DataScope() { e.useData(was); }
  };
  float lastDt_ = 0;
  float entranceDelay();          // for a thing created now: after the others created at this moment
  float staggerAt_ = -1;          // when the last thing was created
  int staggerN_ = 0;              // how many were created at that moment
  std::map<std::string, int> pointerSide_;
  std::vector<int> sceneBlocks_;   // per scene: how many blocks its script declares
  // fit: a scene that outgrows the stage is drawn smaller (never below 0.6, so text stays legible).
  // Layout, labels and the checker work in the enlarged layout stage; drawing scales it back.
  float zoom_ = 1, zoomT_ = 1;          // drawn and target scale; set by the scene's first content, then only shrinks
  bool zoomFresh_ = true;               // nothing measured yet in this scene
  float zoomSettle_ = 0.6f;             // seconds left in which the scale may still grow
  Rectangle stageRect() const;           // the stage in layout coordinates (the screen's at zoom 1)
  Rectangle toStage(Rectangle screen) const; // a screen rectangle (a reserved zone) in layout coordinates
  void fitStage(float dt);              // after layout: shrink if what's laid out outgrew the stage
  // annotations (annotate.cpp): where one goes is derived from what it is about
  struct Annotation {
    bool ok = false;
    Rectangle target{0, 0, 0, 0}, box{0, 0, 0, 0}, bounds{0, 0, 0, 0};
    std::vector<Vector2> path;          // brace / underline stroke
    std::string label;
    Vector2 labelAt{0, 0}, labelAnchor{0.5f, 0};
    Rectangle labelBox{0, 0, 0, 0};
    int side = 0;                        // 0 below, 1 right, 2 above, 3 left
  };
  Annotation annotation(const Mark &m, int variant = -1);
  std::map<std::string, int> annVariant_; // each annotation's chosen placement (kept while it stays clear)
  bool annotationTarget(const Mark &m, Rectangle &out);
  bool annotationHost(const Mark &m, Rectangle &out); // the formulas it names a part of (its name stays off the rest)
  int partLabelSide(const Mark &m, const Mark &block); // -1 above, 1 below: where its name goes off a formula it names a row of
  bool texPartBox(const Mark &m, const std::string &part, Rectangle &out);
  void layoutAnnotations();
  bool refBox(const std::string &ref, Rectangle &out); // a thing, cell, span or formula part, as drawn now
  void drawFlights();
  void drawFurniture();
  std::vector<std::pair<std::string, Rectangle>> screenZones();
  // the area diagram (area.cpp): parts of a whole as proportional strips, each optionally split again
  Vector2 areaSize(const Mark &m);
  Rectangle areaCell(const Mark &m, int i, int j);
  bool areaRef(const Mark &m, const std::string &ref, Rectangle &out);
  void drawArea(const Mark &m, float alpha);
  std::vector<Vector2> shapeOutline(const Mark &m); // a rect, circle, polygon or angle as its outline
  float annotationRoom(const Mark &block);
  void drawAnnotation(const Mark &m);
  Vector2 infoOrigin();                  // where labels with no place of their own stack: below the band, or in the plot's corner
  float bandNeed_ = 0;              // the band's full height (bandH_ is capped above a plot, and eased)
  double z_ = 0;                    // a 3D field's layer: what z means in its expressions while it is sampled
  bool hasZ_ = false;
  float bandH_ = -1;                // the block band's height, eased (-1: not laid out yet)
  bool blockRows_ = false;         // this scene's blocks flow in rows (once chosen, kept: no mid-scene reshuffle)
  std::map<std::string, float> crossedSince_; // label key -> when a line started crossing it
  float stickyFor(const std::string &key); // pointer label id -> side it took (0 below, 1 right, 2 above, 3 left)
  bool camPoint(const std::string &id, const Props &req, double &x, double &y);
  bool camBox(const std::vector<std::string> &ids, double &x0, double &x1, double &y0, double &y1);
  void camApply(Rectangle area);  // called by layoutAxes once the schema's view is known

  // ---- fields (fields.cpp)
  std::function<double(double, double)> fn2(const Mark &m, const char *key, const char *a = "x", const char *b = "y");
  std::function<viz::V2(viz::V2)> vecField(const Mark &m);
  void drawFieldMark(Mark &m);
  void drawVecField(Mark &m, float a, Color c);
  void drawSlope(Mark &m, float a, Color c);
  void drawHeat(Mark &m, float a);
  void drawContour(Mark &m, float a, Color c);
  bool sampleGrid(const Mark &m, int step, std::vector<float> &vals, int &nx, int &ny);
  std::vector<Vector2> solutionPoints(const Mark &m);
  void clearFieldCaches();
  std::map<std::string, double> fieldScale_;                 // magnitude scale, locked per mark + source
  std::map<std::string, std::vector<double>> fieldLevels_;   // heat range / contour levels, locked likewise
  struct StreamCache { std::string sig; std::vector<std::vector<viz::V2>> lines; std::vector<std::vector<float>> mags; };
  std::map<std::string, StreamCache> streamCache_;
  std::map<std::string, Texture2D> heatTex_;

  // ---- history and trails (trails.cpp). Every numeric change is recorded as a segment (when, how long,
  // from, to, ease), so any mark's position at any past time can be evaluated exactly: trails are
  // recomputed from the motion, not accumulated per frame, so they are the same at any frame rate, in
  // snapshots, after a skip or after appended lines.
  struct HistSeg {
    double t0 = 0;
    float dur = 0;
    double from = 0, to = 0;
    rm::RateFunc ease = nullptr;
    Value final, before;      // the value it ends at; the value it had before (for times earlier than this)
    bool set = false;         // an instant change
  };
  std::map<std::string, std::vector<HistSeg>> hist_; // by "id.prop"
  std::map<std::string, double> bornAt_;
  double histT_ = NAN;        // when finite, num() reads properties as they were at this time
  float frameDt_ = 0;
  void recordActive(const Active &a, const Value &before);
  void recordSet(const std::string &id, const std::string &prop, const Value &before, const Value &after);
  bool histValue(const std::string &id, const std::string &prop, double t, double &out);
  std::vector<double> histBreaks(double t0, double t1);
  bool trailSpec(const Mark &m, double &window, bool &all);
  void drawTrail(Mark &m);
  std::vector<std::vector<rm::StripPt>> trailRuns(Mark &m);
  std::vector<Vector2> trailLine(Mark &m); // thinned, for the placer and the checker
};

} // namespace mk
