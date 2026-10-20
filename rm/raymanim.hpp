// raymanim — a small 2D Manim-style animation library on top of raylib.
//
//   * colors, rate functions (easings), text, thick strokes, arrows, polygon fill
//   * coordinate systems: NumberLine, Axes (manim-style ticks and labels)
//   * VMobject: everything is a set of point paths, so any shape can Transform into any other
//   * Animations: Create, Uncreate, FadeIn, FadeOut, Transform, MoveTo, Shift, ScaleBy,
//                 Rotate, Indicate, Write, ChangeValue, Wait, AnimationGroup, Succession
//   * ValueTracker + updaters (manim's always_redraw)
//   * Scene with blocking play()/wait() like manim's construct(), optional mp4 recording
//
// Coordinates are logical pixels on a 1280x720 canvas, y down. U(x, y) converts manim units
// (frame 14.22 x 8, origin at centre, y up) into pixels, if you prefer manim coordinates.
#pragma once
#include "raylib.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace rm {

constexpr int W = 1280, H = 720;

// ---------------------------------------------------------------- math helpers
inline Vector2 v2(float x, float y) { return {x, y}; }
inline Vector2 operator+(Vector2 a, Vector2 b) { return {a.x + b.x, a.y + b.y}; }
inline Vector2 operator-(Vector2 a, Vector2 b) { return {a.x - b.x, a.y - b.y}; }
inline Vector2 operator*(Vector2 a, float s) { return {a.x * s, a.y * s}; }
inline Vector2 &operator+=(Vector2 &a, Vector2 b) { a.x += b.x; a.y += b.y; return a; }
float len(Vector2 a);
Vector2 norm(Vector2 a);
Vector2 lerp(Vector2 a, Vector2 b, float t);
float lerpf(float a, float b, float t);
Vector2 U(float x, float y);             // manim units -> pixels
float smoothApproach(float cur, float target, float dt, float speed = 10.f);
Vector2 smoothApproach(Vector2 cur, Vector2 target, float dt, float speed = 10.f);

// ---------------------------------------------------------------- colors
namespace col {
extern const Color bg, fg, muted, grid, blue, orange, green, red, yellow, purple, teal, pink, brown, lime;
}
Color categorical(int i);                // 10 well separated hues
int categoricalCount();
Color fade(Color c, float alpha);        // multiply alpha
Color lerpColor(Color a, Color b, float t);
bool colorByName(const std::string &name, Color &out);

// ---------------------------------------------------------------- rate functions
using RateFunc = std::function<float(float)>;
namespace rate {
float linear(float t);
float smooth(float t);                   // manim default
float rush_into(float t);
float rush_from(float t);
float slow_into(float t);
float double_smooth(float t);
float there_and_back(float t);
float there_and_back_with_pause(float t);
float ease_in_out_cubic(float t);
float ease_out_back(float t);
float wiggle(float t);
}
RateFunc rateByName(const std::string &name); // unknown -> smooth

// ---------------------------------------------------------------- text
void initFonts(const char *preferred = nullptr); // call after InitWindow
void initFontMetrics(const char *preferred = nullptr); // no window needed: exact text sizes for headless runs
Vector2 measureMetrics(const std::string &s, float size);  // the headless measure (to compare with the GPU font)
bool fontsReady();
Vector2 measure(const std::string &s, float size);
// anchor: (0,0) top-left, (0.5,0.5) centre, (1,1) bottom-right
void text(const std::string &s, Vector2 pos, float size, Color c, float ax = 0, float ay = 0);
void textBg(const std::string &s, Vector2 pos, float size, Color c, Color bg, float ax = 0.5f, float ay = 0.5f, float pad = 4);
std::string fmtNum(double v);            // 70, 2.5, 0.01
std::vector<std::string> wrap(const std::string &s, float size, float maxW); // greedy word wrap
Vector2 measureMono(const std::string &s, float size);                     // monospace (code)
void textMono(const std::string &s, Vector2 pos, float size, Color c);     // top-left anchored

// ---------------------------------------------------------------- primitives
void initGraphics();                     // call after InitWindow (disables backface culling)
void stroke(const std::vector<Vector2> &pts, float thick, Color c, bool closed = false);
void strokePartial(const std::vector<Vector2> &pts, float frac, float thick, Color c, bool closed = false);
// A polyline whose width and colour (alpha) vary per point: trails that taper and fade, streamlines.
// Mitred joins (limited to 2x, as Godot's sharp_limit), and a 1 px feather on both edges so it is
// smooth without MSAA (snapshots have none). Repeated points are skipped.
struct StripPt {
  Vector2 p;
  float width;
  Color c;
};
void strip(const std::vector<StripPt> &pts);
void fillPolygon(const std::vector<Vector2> &pts, Color c); // concave ok (ear clipping)
void tri(Vector2 a, Vector2 b, Vector2 c, Color col);
void arrow(Vector2 a, Vector2 b, float thick, Color c, float head = 14);
void arcArrow(Vector2 a, Vector2 b, float bulge, float thick, Color c, float frac = 1, bool head = true);
void pathArrow(std::vector<Vector2> pts, float thick, Color c, float frac = 1, bool head = true); // a polyline, drawn up to frac
void dashedPath(const std::vector<Vector2> &pts, float thick, Color c, float frac = 1, float dash = 9, float gap = 7);
Vector2 arcPoint(Vector2 a, Vector2 b, float bulge, float t);
void dashed(Vector2 a, Vector2 b, float thick, Color c, float dash = 9, float gap = 7);
void roundedBox(Rectangle r, float radius, float thick, Color strokeC, Color fillC);
Vector2 rectEdgePoint(Rectangle r, Vector2 toward); // where a ray from the centre exits r
float niceStep(float range, int targetTicks);
void niceRange(float &lo, float &hi, bool includeZero);

// ---------------------------------------------------------------- coordinate systems
struct NumberLine {
  float min = 0, max = 10, step = 1;
  Vector2 a{140, 360}, b{1140, 360};
  Vector2 toScreen(float v) const;
  float fromScreen(Vector2 p) const;
  void draw(Color c, float alpha = 1, bool labels = true) const;
};

struct Axes {
  Rectangle area{140, 80, 1000, 520};
  float xmin = -1, xmax = 5, ymin = -1, ymax = 10;
  Vector2 toScreen(float x, float y) const;
  Vector2 toWorld(Vector2 p) const;
  void draw(Color c, float alpha = 1, bool labels = true, bool grid = true) const;
  std::vector<Vector2> plot(const std::function<double(double)> &f, float x0, float x1, int n = 400) const;
  void fillUnder(const std::function<double(double)> &f, float x0, float x1, Color c, int n = 200) const;
};

// ---------------------------------------------------------------- mobjects
struct Mobject;
using MobPtr = std::shared_ptr<Mobject>;
using Updater = std::function<void(Mobject &, float dt)>;

struct Mobject : std::enable_shared_from_this<Mobject> {
  // every visible thing is a set of paths; text overrides draw()
  std::vector<std::vector<Vector2>> paths;
  bool closed = true;
  Color strokeColor = col::fg, fillColor = BLANK;
  float strokeWidth = 4, opacity = 1, fillOpacity = 0;
  float drawFrac = 1;                     // 0..1, used by Create
  std::vector<MobPtr> submobjects;
  std::vector<Updater> updaters;
  float zIndex = 0;

  virtual ~Mobject() = default;
  virtual void draw();
  virtual MobPtr copy() const;
  void update(float dt);

  Vector2 center() const;
  Rectangle bounds() const;
  float width() const { return bounds().width; }
  float height() const { return bounds().height; }
  Mobject &moveTo(Vector2 p);
  Mobject &shift(Vector2 d);
  Mobject &scale(float s, Vector2 about);
  Mobject &scale(float s) { return scale(s, center()); }
  Mobject &rotate(float radians, Vector2 about);
  Mobject &rotate(float radians) { return rotate(radians, center()); }
  Mobject &setColor(Color c);
  Mobject &setFill(Color c, float op = 1);
  Mobject &setStroke(Color c, float w);
  Mobject &nextTo(const Mobject &other, Vector2 dir, float buff = 20);
  Mobject &addUpdater(Updater u) { updaters.push_back(std::move(u)); return *this; }
  Mobject &add(MobPtr m) { submobjects.push_back(std::move(m)); return *this; }
  void allPaths(std::vector<std::vector<Vector2>> &out) const; // flattened incl. children
  // apply fn to this and every descendant
  void family(const std::function<void(Mobject &)> &fn);
};

// shapes (all return shared pointers so they can live in a Scene)
MobPtr Circle(Vector2 c, float r, Color col = col::blue);
MobPtr Dot(Vector2 c, float r = 8, Color col = col::fg);
MobPtr Rect(Rectangle r, Color col = col::fg);
MobPtr Square(Vector2 c, float side, Color col = col::fg);
MobPtr RegularPolygon(Vector2 c, float r, int n, Color col = col::fg);
MobPtr Line(Vector2 a, Vector2 b, Color col = col::fg);
MobPtr ArrowMob(Vector2 a, Vector2 b, Color col = col::fg);
MobPtr FunctionGraph(const Axes &ax, std::function<double(double)> f, float x0, float x1, Color col = col::blue);
MobPtr VGroup(std::vector<MobPtr> items);
void arrange(Mobject &group, Vector2 dir, float buff = 24); // lays out submobjects

struct Text : Mobject {
  std::string str;
  float size = 32;
  Vector2 pos;                            // centre
  float revealed = 1;                     // 0..1 for Write
  Text(std::string s, Vector2 p, float sz = 32, Color c = col::fg);
  void draw() override;
  MobPtr copy() const override;
};
std::shared_ptr<Text> TextMob(const std::string &s, Vector2 p, float size = 32, Color c = col::fg);

struct ValueTracker {
  float value;
  explicit ValueTracker(float v = 0) : value(v) {}
};
using TrackerPtr = std::shared_ptr<ValueTracker>;
inline TrackerPtr Tracker(float v) { return std::make_shared<ValueTracker>(v); }

// ---------------------------------------------------------------- animations
struct Animation {
  float runTime = 1;
  RateFunc rateFunc = rate::smooth;
  float lagRatio = 0;
  virtual ~Animation() = default;
  virtual void begin() {}
  virtual void interpolate(float alpha) = 0; // alpha already passed through rateFunc
  virtual void finish() { interpolate(1); }
  virtual std::vector<MobPtr> introduces() { return {}; } // mobjects added to scene on begin
  virtual std::vector<MobPtr> removes() { return {}; }    // mobjects removed on finish
};
using AnimPtr = std::shared_ptr<Animation>;

AnimPtr Create(MobPtr m, float rt = 1.5f);
AnimPtr Uncreate(MobPtr m, float rt = 1);
AnimPtr FadeIn(MobPtr m, float rt = 1, Vector2 shift = {0, 0});
AnimPtr FadeOut(MobPtr m, float rt = 1, Vector2 shift = {0, 0});
// raylib also defines a `Transform` struct: call this one as rm::Transform
AnimPtr Transform(MobPtr from, MobPtr to, float rt = 1); // `from` morphs into `to`'s shape
AnimPtr ReplacementTransform(MobPtr from, MobPtr to, float rt = 1);
AnimPtr MoveTo(MobPtr m, Vector2 p, float rt = 1);
AnimPtr Shift(MobPtr m, Vector2 d, float rt = 1);
AnimPtr ScaleBy(MobPtr m, float s, float rt = 1);
AnimPtr Rotate(MobPtr m, float radians, float rt = 1);
AnimPtr Indicate(MobPtr m, Color c = col::yellow, float rt = 1);
AnimPtr Write(std::shared_ptr<Text> t, float rt = 1.5f);
AnimPtr ChangeValue(TrackerPtr t, float to, float rt = 1);
AnimPtr Wait(float rt);
AnimPtr Group(std::vector<AnimPtr> anims, float lag = 0);   // run together
AnimPtr Succession(std::vector<AnimPtr> anims);            // one after another

// ---------------------------------------------------------------- scene
struct SceneConfig {
  const char *title = "raymanim";
  int fps = 60;
  const char *recordPath = nullptr; // "out.mp4" -> pipes frames to ffmpeg
  Color background = col::bg;
};

class Recorder {
public:
  bool start(const char *path, int fps);
  void frame();                       // grabs current framebuffer
  void stop();
  bool active() const { return pipe_ != nullptr; }
  ~Recorder() { stop(); }
private:
  FILE *pipe_ = nullptr;
  int w_ = 0, h_ = 0, fps_ = 60;
  std::string path_;
};

class Scene {
public:
  explicit Scene(SceneConfig cfg = {});
  virtual ~Scene();
  virtual void construct() = 0;
  void render();                      // opens the window, runs construct(), closes

  void add(MobPtr m);
  void remove(MobPtr m);
  void clear();
  void play(std::vector<AnimPtr> anims);
  void play(AnimPtr a) { play(std::vector<AnimPtr>{a}); }
  void wait(float seconds = 1);
  float time() const { return time_; }

protected:
  std::vector<MobPtr> mobjects;
  virtual void drawOverlay() {}

private:
  void frame(float dt);
  SceneConfig cfg_;
  Recorder rec_;
  float time_ = 0;
};

struct WindowClosed {}; // thrown out of play()/wait() when the user closes the window

} // namespace rm
