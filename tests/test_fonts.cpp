// The headless text measure must agree with the font the stage draws with (needs a hidden window):
//   make test-fonts
#include "../rm/raymanim.hpp"
#include <cmath>
#include <cstdio>

int main() {
  SetConfigFlags(FLAG_WINDOW_HIDDEN);
  SetTraceLogLevel(LOG_ERROR);
  InitWindow(320, 200, "fonts");
  rm::initFonts();
  rm::initFontMetrics();
  const char *samples[] = {"Alice", "market price", "Heavy kid's turning push", "E(v)", "sorted, looking for 23",
                           "θ = 45°", "1 kg", "x² + y² = r²", "Planet number 12", "W", "iii"};
  int bad = 0;
  for (float size : {14.f, 17.f, 19.f, 20.f, 24.f}) {
    for (const char *s : samples) {
      Vector2 gpu = rm::measure(s, size), cpu = rm::measureMetrics(s, size);
      float err = std::fabs(gpu.x - cpu.x) / std::max(1.f, gpu.x);
      if (err > 0.02f || std::fabs(gpu.y - cpu.y) > 0.5f) {
        std::printf("MISMATCH %-28s size %4.0f  gpu %.1fx%.1f  cpu %.1fx%.1f\n", s, size, gpu.x, gpu.y, cpu.x, cpu.y);
        bad++;
      }
    }
  }
  CloseWindow();
  std::printf("%s: headless text measure %s the GPU font\n", bad ? "FAIL" : "ok", bad ? "differs from" : "matches");
  return bad ? 1 : 0;
}
