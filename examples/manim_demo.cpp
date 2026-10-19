// The raymanim library used directly, the way you would write a manim Scene.
//   make manim_demo && ./manim_demo            (add --record demo.mp4 to export)
#include "../rm/raymanim.hpp"
#include <cmath>
#include <cstring>

using namespace rm;

struct Demo : Scene {
  using Scene::Scene;

  void construct() override {
    // --- shapes, Create, Transform (circle -> square -> triangle)
    auto title = TextMob("raymanim", {W / 2.f, 90}, 54);
    play(Write(title, 1.2f));

    auto shape = Circle({W / 2.f, 380}, 110, col::blue);
    shape->setFill(col::blue, 0.25f);
    play(Create(shape));
    auto square = Square({W / 2.f, 380}, 200, col::orange);
    square->setFill(col::orange, 0.25f);
    play(rm::Transform(shape, square));
    auto triangle = RegularPolygon({W / 2.f, 380}, 130, 3, col::green);
    triangle->setFill(col::green, 0.25f);
    play(rm::Transform(shape, triangle));
    play(Rotate(shape, (float)M_PI, 1.2f));
    play(Indicate(shape));

    // --- groups and arrangement
    std::vector<MobPtr> dots;
    for (int i = 0; i < 6; i++) dots.push_back(Dot({0, 0}, 16, categorical(i)));
    auto row = VGroup(dots);
    arrange(*row, {1, 0}, 40);
    row->moveTo({W / 2.f, 600});
    play(Group({FadeOut(shape, 0.8f), FadeIn(row, 0.8f, {0, 30})}));
    play(Shift(row, {0, -220}, 1));
    std::vector<AnimPtr> pops;
    for (auto &d : row->submobjects) pops.push_back(Indicate(d, col::fg, 0.6f));
    play(Group(pops, 0.25f)); // lag_ratio
    play(FadeOut(row, 0.6f));

    // --- axes, a function graph, and a ValueTracker driving updaters (always_redraw)
    Axes ax;
    ax.area = {140, 150, 1000, 470};
    ax.xmin = -4; ax.xmax = 4; ax.ymin = -1.5f; ax.ymax = 1.5f;
    struct AxesView : Mobject {
      Axes a;
      void draw() override { a.draw(col::muted, opacity); }
    };
    auto av = std::make_shared<AxesView>();
    av->a = ax;
    add(av);

    auto k = Tracker(1);
    auto graph = FunctionGraph(ax, [](double x) { return std::sin(x); }, ax.xmin, ax.xmax, col::blue);
    graph->addUpdater([ax, k](Mobject &m, float) {
      float kk = k->value;
      m.paths = {ax.plot([kk](double x) { return std::sin(kk * x); }, ax.xmin, ax.xmax)};
    });
    play(Create(graph, 2));

    auto dot = Dot(ax.toScreen(1, std::sin(1.f)), 10, col::yellow);
    dot->addUpdater([ax, k](Mobject &m, float) { m.moveTo(ax.toScreen(1, std::sin(k->value * 1))); });
    auto label = TextMob("k = 1.00", {W / 2.f, 140}, 26, col::yellow);
    label->addUpdater([k](Mobject &m, float) {
      char buf[32];
      std::snprintf(buf, sizeof buf, "sin(k·x),  k = %.2f", k->value);
      static_cast<Text &>(m).str = buf;
    });
    play(Group({FadeOut(title, 0.5f), FadeIn(dot), FadeIn(label)}));
    play(ChangeValue(k, 3, 3));
    play(ChangeValue(k, 0.5f, 2));
    wait(1);
  }
};

int main(int argc, char **argv) {
  SceneConfig cfg;
  cfg.title = "raymanim demo";
  for (int i = 1; i + 1 < argc; i++)
    if (!std::strcmp(argv[i], "--record")) cfg.recordPath = argv[i + 1];
  Demo d(cfg);
  d.render();
}
