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
#include "place/spring.hpp"
#include "viz/viz.hpp"
#include <functional>
#include "tts.hpp"
#include <memory>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace mk {

inline bool isAxes(const std::string &schema) { return schema == "axes" || schema == "plane"; }
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
  return k == "tex" || k == "card" || k == "code" || k == "array" || k == "table";
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
  float lit = 1;      // focus brightness, eased
  float pulse = 0;    // indicate: 0..1..0
  std::string pulseRef; // what the pulse points at (the thing, or one cell: a[3])
  // a category (string) property mid-morph: prop -> (old text, progress 0..1)
  std::map<std::string, std::pair<std::string, float>> strBlend;
};

struct Mark : Obj {
  Vector2 pos{0, 0}, tpos{0, 0};
  bool placed = false;    // has had a first layout (so it doesn't fly in from 0,0)
  bool anchorBottom = false; // pos is bottom-centre (free layout) instead of centre
  bool positioned = false;   // the schema gave it a place this frame
  float barScale = 0;        // px per unit for bars (free layout)
  bool direct = false;       // position is data (axes, number line): no layout easing
  float labelLift = 0;       // raises a name label that would collide with a neighbour's
  int labelQuad = 0;         // axes: which corner of its dot the name sits at (0 NE, 1 NW, 2 SE, 3 SW)
  bool culled = false;       // outside the camera's view this frame: not drawn, no labels
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
  enum Kind { NUM, STR, WAIT, PAUSE, CLEAR, PULSE, CAM } kind = NUM;
  std::string id, prop;
  double from = 0, to = 0;
  Value finalVal;           // what the prop becomes when done (keeps expressions)
  float t = 0, dur = 1;
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
  bool movableLabel(const Mark &m, const TextLabel &l);
  std::map<std::string, std::vector<LabelSlot>> labelSlots_;
  std::map<std::string, LabelSlot> linkSlots_;
  int labelFlips = 0;                   // labels that changed side between frames (stability metric)
  std::vector<std::string> flipLog;     // which, when (the first 40), for the layout report
  // curve and line geometry shared by drawing and labels
  std::vector<Vector2> curvePoints(const Mark &m);
  bool lineEnds(const Mark &m, Vector2 &a, Vector2 &b, bool &hasSlope);
  bool vectorEnds(const Mark &m, Vector2 &from, Vector2 &to);
  place::Report layoutCheck(std::vector<std::string> *messages = nullptr);
  const std::vector<std::string> &parseErrors() const { return script_.errors; }
  std::vector<std::string> lintMessages() const { return {lint_.begin(), lint_.end()}; }
  const std::string &path() const { return path_; }
  float time() const { return clock_; }
  // (start time, wav file) for every narrated line, so a recording can be given its soundtrack
  std::vector<std::pair<float, std::string>> audioLog() const;

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
  void layoutFree(Rectangle area, const std::vector<std::string> &ids);
  void layoutGraph(Rectangle area, const std::vector<std::string> &ids);
  void layoutNumberline(Rectangle area);
  void layoutAxes(Rectangle area);
  void layoutTimeline(Rectangle area);
  void zeroCheck();
  float footprint(const Mark &m);
  float dotRadius(const Mark &m);
  Vector2 boxSize(const Mark &m);
  Rectangle bodyBounds(const Mark &m);
  Rectangle fullBounds(const Mark &m);   // incl. name labels
  Rectangle groupBounds(const Group &g, int depth = 0);
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
  float nlMin_ = 0, nlMax_ = 10, axX0_ = -1, axX1_ = 5, axY0_ = -1, axY1_ = 10, tlMin_ = 0, tlMax_ = 10;
  bool rangesInit_ = false;
  float nlTarget_[2] = {0, 10}, axTarget_[4] = {-1, 5, -1, 10}, tlTarget_[2] = {0, 10}; // eased towards
  std::string nlProp_;
  rm::Axes axes_;
  rm::NumberLine nl_;
  std::map<std::string, float> barScale_; // eased px/unit per property
  std::string graphSig_;
  std::map<std::string, Vector2> graphPos_;
  place::FlowMemo flowMemo_;              // each thing's column and height last time: newcomers are inserted
  struct LinkRoute {
    std::vector<Vector2> via, viaT;       // waypoints through the gaps: drawn (eased) and target
    float portA = 0, portB = 0;           // where it leaves and enters, along the sides
    bool back = false;                    // against the flow: an arc
  };
  std::map<std::string, LinkRoute> linkRoute_; // links laid out by the flow (graph schema)

  // voice
  std::unique_ptr<Tts> tts_;
  std::map<int, UttPtr> lineUtt_;          // by Line::uid
  void queueVoice(size_t fromStep, bool report);
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
  float lastDt_ = 0;
  std::map<std::string, int> pointerSide_;
  std::vector<int> sceneBlocks_;   // per scene: how many blocks its script declares
  bool blockRows_ = false;         // this scene's blocks flow in rows (once chosen, kept: no mid-scene reshuffle)
  std::map<std::string, float> crossedSince_; // label key -> when a line started crossing it
  float stickyFor(const std::string &key); // pointer label id -> side it took (0 below, 1 right, 2 above, 3 left)
  bool camPoint(const std::string &id, const Props &req, double &x, double &y);
  bool camBox(const std::vector<std::string> &ids, double &x0, double &x1, double &y0, double &y1);
  void camApply(Rectangle area);  // called by layoutAxes once the schema's view is known

  // ---- fields (fields.cpp)
  std::function<double(double, double)> fn2(const Mark &m, const char *key);
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
