#include "tex.hpp"
#include "../third_party/json.hpp"
#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <spawn.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

extern char **environ;

namespace mk::tex {

using json = nlohmann::json;
static const char *kCacheDir = ".tutor_cache/tex";

// ================================================================ helper process
static std::string exeDir() {
  char buf[4096] = {0};
#ifdef __APPLE__
  uint32_t n = sizeof buf;
  if (_NSGetExecutablePath(buf, &n) != 0) return ".";
#else
  ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 1);
  if (n <= 0) return ".";
#endif
  std::string p = buf;
  size_t s = p.rfind('/');
  return s == std::string::npos ? "." : p.substr(0, s);
}

static bool exists(const std::string &p) { struct stat st; return stat(p.c_str(), &st) == 0; }

static std::string helperPath() {
  if (const char *e = std::getenv("TUTOR_TEX_HELPER"); e && exists(e)) return e;
  for (auto base : {exeDir(), std::string(".")}) {
    std::string p = base + "/tools/tex/tex2paths.mjs";
    if (exists(p)) return p;
  }
  return "";
}

static uint64_t fnv(const std::string &s) {
  uint64_t h = 1469598103934665603ull;
  for (unsigned char c : s) { h ^= c; h *= 1099511628211ull; }
  return h;
}
static std::string cachePath(const std::string &src) {
  char b[32];
  std::snprintf(b, sizeof b, "%016llx", (unsigned long long)fnv("v1|" + src));
  return std::string(kCacheDir) + "/" + b + ".json";
}

static TexPtr parseReply(const std::string &line) {
  auto L = std::make_shared<TexLayout>();
  json j = json::parse(line, nullptr, false);
  if (j.is_discarded()) { L->error = "bad reply from the TeX helper"; return L; }
  L->ok = j.value("ok", false);
  if (!L->ok) { L->error = j.value("error", std::string("TeX error")); return L; }
  L->w = j.value("w", 0.f); L->h = j.value("h", 0.f); L->base = j.value("base", 0.f);
  for (auto &s : j["shapes"]) {
    TexShape sh;
    sh.id = s.value("id", std::string());
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    for (auto &c : s["pts"]) {
      std::vector<Vector2> pts;
      for (size_t i = 0; i + 1 < c.size(); i += 2) {
        Vector2 p{c[i].get<float>(), c[i + 1].get<float>()};
        pts.push_back(p);
        x0 = std::min(x0, p.x); y0 = std::min(y0, p.y); x1 = std::max(x1, p.x); y1 = std::max(y1, p.y);
      }
      if (pts.size() >= 3) sh.contours.push_back(std::move(pts));
    }
    sh.box = x0 <= x1 ? Rectangle{x0, y0, x1 - x0, y1 - y0} : Rectangle{0, 0, 0, 0};
    L->shapes.push_back(std::move(sh));
  }
  for (auto &g : j["glyphs"]) {
    TexGlyph tg;
    tg.key = g.value("k", std::string());
    tg.shape = g.value("s", 0);
    tg.at = {g.value("x", 0.f), g.value("y", 0.f)};
    if (g.contains("c") && g["c"].is_string()) tg.color = g["c"].get<std::string>();
    if (tg.shape >= 0 && tg.shape < (int)L->shapes.size()) L->glyphs.push_back(tg);
  }
  return L;
}

class Service {
public:
  static Service &get() {
    static Service *s = new Service(); // never destroyed: the helper exits on stdin EOF with us
    return *s;
  }

  TexPtr lookup(const std::string &src) {
    std::lock_guard<std::mutex> lk(mu_);
    if (auto it = done_.find(src); it != done_.end()) return it->second;
    if (auto L = fromDisk(src)) { done_[src] = L; return L; }
    return nullptr;
  }

  TexPtr request(const std::string &src) {
    if (auto L = lookup(src)) return L;
    std::lock_guard<std::mutex> lk(mu_);
    if (!start()) return failed("TeX renderer unavailable: needs node and tools/tex (npm install there)");
    if (!queued_.count(src)) {
      queue_.push_back(src);
      queued_.insert(src);
      // live values ask for a new string every frame: keep only the freshest few
      while (queue_.size() > 6) { queued_.erase(queue_.front()); queue_.pop_front(); }
      cv_.notify_all();
    }
    return nullptr;
  }

  TexPtr typeset(const std::string &src) {
    if (auto L = request(src)) return L;
    std::unique_lock<std::mutex> lk(mu_);
    // re-queue if it was dropped as stale
    if (!queued_.count(src) && !done_.count(src)) { queue_.push_back(src); queued_.insert(src); cv_.notify_all(); }
    cv_.wait_for(lk, std::chrono::seconds(20), [&] { return done_.count(src) > 0 || dead_; });
    if (auto it = done_.find(src); it != done_.end()) return it->second;
    return failed("TeX helper did not answer");
  }

  bool available() {
    std::lock_guard<std::mutex> lk(mu_);
    return start();
  }

private:
  TexPtr failed(const std::string &why) {
    auto L = std::make_shared<TexLayout>();
    L->error = why;
    return L;
  }

  TexPtr fromDisk(const std::string &src) {
    std::ifstream f(cachePath(src));
    if (!f) return nullptr;
    std::string line;
    std::getline(f, line);
    auto L = parseReply(line);
    return L->ok ? L : nullptr;
  }

  // caller holds mu_
  bool start() {
    if (started_) return !dead_;
    started_ = true;
    std::string helper = helperPath();
    if (helper.empty()) { dead_ = true; return false; }
    int in[2], out[2];
    if (pipe(in) != 0 || pipe(out) != 0) { dead_ = true; return false; }
    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_adddup2(&fa, in[0], 0);
    posix_spawn_file_actions_adddup2(&fa, out[1], 1);
    posix_spawn_file_actions_addclose(&fa, in[1]);
    posix_spawn_file_actions_addclose(&fa, out[0]);
    const char *argv[] = {"node", helper.c_str(), nullptr};
    pid_t pid;
    int rc = posix_spawnp(&pid, "node", &fa, nullptr, const_cast<char **>(argv), environ);
    posix_spawn_file_actions_destroy(&fa);
    close(in[0]);
    close(out[1]);
    if (rc != 0) { close(in[1]); close(out[0]); dead_ = true; return false; }
    toHelper_ = fdopen(in[1], "w");
    fromHelper_ = fdopen(out[0], "r");
    mkdir(".tutor_cache", 0755);
    mkdir(kCacheDir, 0755);
    std::thread([this] { loop(); }).detach();
    return true;
  }

  void loop() {
    std::string line;
    for (;;) {
      std::string src;
      {
        std::unique_lock<std::mutex> lk(mu_);
        cv_.wait(lk, [&] { return !queue_.empty(); });
        src = queue_.front();
        queue_.pop_front();
      }
      json req = {{"tex", src}, {"display", true}};
      std::string msg = req.dump() + "\n";
      bool ok = std::fputs(msg.c_str(), toHelper_) >= 0 && std::fflush(toHelper_) == 0;
      line.clear();
      if (ok) {
        char buf[65536];
        while (std::fgets(buf, sizeof buf, fromHelper_)) {
          line += buf;
          if (!line.empty() && line.back() == '\n') break;
        }
      }
      std::lock_guard<std::mutex> lk(mu_);
      queued_.erase(src);
      if (line.empty()) { dead_ = true; done_[src] = failed("TeX helper exited"); cv_.notify_all(); return; }
      auto L = parseReply(line);
      done_[src] = L;
      if (L->ok) { std::ofstream f(cachePath(src)); f << line; }
      cv_.notify_all();
    }
  }

  std::mutex mu_;
  std::condition_variable cv_;
  std::deque<std::string> queue_;
  std::set<std::string> queued_;
  std::map<std::string, TexPtr> done_;
  bool started_ = false, dead_ = false;
  FILE *toHelper_ = nullptr, *fromHelper_ = nullptr;
};

TexPtr request(const std::string &src) { return Service::get().request(src); }
TexPtr typeset(const std::string &src) { return Service::get().typeset(src); }
bool available() { return Service::get().available(); }

// ================================================================ rasterizer
// Nonzero winding, 5 sub-scanlines per pixel, exact horizontal coverage per span.
static Image rasterize(const TexShape &sh, float s, Vector2 &offset) {
  const int pad = 2, SS = 5;
  int w = std::max(1, (int)std::ceil(sh.box.width * s) + 2 * pad);
  int h = std::max(1, (int)std::ceil(sh.box.height * s) + 2 * pad);
  offset = {sh.box.x * s - pad, sh.box.y * s - pad};
  std::vector<float> cov((size_t)w * h, 0.f);
  struct Edge { float x0, y0, x1, y1; int dir; };
  std::vector<Edge> edges;
  for (auto &c : sh.contours)
    for (size_t i = 0; i < c.size(); i++) {
      Vector2 a = c[i], b = c[(i + 1) % c.size()];
      float ax = a.x * s - offset.x, ay = a.y * s - offset.y, bx = b.x * s - offset.x, by = b.y * s - offset.y;
      if (ay == by) continue;
      edges.push_back(ay < by ? Edge{ax, ay, bx, by, 1} : Edge{bx, by, ax, ay, -1});
    }
  std::vector<std::pair<float, int>> xs;
  std::vector<float> row(w + 1);
  for (int py = 0; py < h; py++) {
    std::fill(row.begin(), row.end(), 0.f);
    for (int sub = 0; sub < SS; sub++) {
      float y = py + (sub + 0.5f) / SS;
      xs.clear();
      for (auto &e : edges)
        if (y >= e.y0 && y < e.y1) xs.push_back({e.x0 + (y - e.y0) / (e.y1 - e.y0) * (e.x1 - e.x0), e.dir});
      std::sort(xs.begin(), xs.end());
      int wind = 0;
      for (size_t i = 0; i + 1 < xs.size(); i++) {
        wind += xs[i].second;
        if (wind == 0) continue;
        float a = std::clamp(xs[i].first, 0.f, (float)w), b = std::clamp(xs[i + 1].first, 0.f, (float)w);
        if (b <= a) continue;
        int ia = (int)a, ib = (int)b;
        if (ia == ib) { row[ia] += b - a; continue; }
        row[ia] += (ia + 1) - a;
        for (int x = ia + 1; x < ib; x++) row[x] += 1;
        if (ib < w) row[ib] += b - ib;
      }
    }
    for (int x = 0; x < w; x++) cov[(size_t)py * w + x] = std::min(1.f, row[x] / SS);
  }
  Image img = GenImageColor(w, h, BLANK);
  auto *px = (Color *)img.data;
  for (size_t i = 0; i < cov.size(); i++) px[i] = {255, 255, 255, (unsigned char)std::lround(cov[i] * 255)};
  return img;
}

struct Raster { Texture2D tex{}; Vector2 offset{0, 0}; float scale = 1; };

void drawGlyph(const TexLayout &L, const TexGlyph &g, Vector2 origin, float pxPerUnit, Color c) {
  static std::map<std::string, Raster> cache;
  const TexShape &sh = L.shapes[g.shape];
  if (sh.contours.empty() || pxPerUnit <= 0) return;
  // rasterize at the backing resolution, in size buckets so a growing formula doesn't re-raster every frame
  float dpi = std::max(1.f, GetWindowScaleDPI().x);
  float want = pxPerUnit * dpi;
  float bucket = std::pow(2.f, std::round(std::log2(want) * 8) / 8); // 1/8-octave steps
  char key[96];
  std::snprintf(key, sizeof key, "%s@%.5f", sh.id.c_str(), bucket);
  auto it = cache.find(key);
  if (it == cache.end()) {
    Raster r;
    Image img = rasterize(sh, bucket, r.offset);
    r.tex = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(r.tex, TEXTURE_FILTER_BILINEAR);
    r.scale = bucket;
    it = cache.emplace(key, r).first;
  }
  const Raster &r = it->second;
  float k = pxPerUnit / r.scale; // texture px -> logical px
  Rectangle src{0, 0, (float)r.tex.width, (float)r.tex.height};
  Rectangle dst{origin.x + g.at.x * pxPerUnit + r.offset.x * k, origin.y + g.at.y * pxPerUnit + r.offset.y * k,
                r.tex.width * k, r.tex.height * k};
  DrawTexturePro(r.tex, src, dst, {0, 0}, 0, c);
}

} // namespace mk::tex
