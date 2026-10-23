// tutor: reads an explanation written in the markup and plays it.
//
//   ./tutor scripts/bank.txt                 play it (hot-reloads when the file changes)
//   ./tutor scripts/bank.txt                 narrated by Inworld TTS (streamed) when an API key is found
//   ./tutor scripts/bank.txt --mute          no voice: estimated word timing
//   ./tutor scripts/bank.txt --voice-id Ashley --speed 1.1
//   ./tutor scripts/bank.txt --record out.mp4
//   ./tutor scripts/bank.txt --readback      no window: run to the end, print the scene as markup
//   ./tutor scripts/bank.txt --check         no window: decodability test (readback -> parse -> same scene)
//   ./tutor s.txt --snapshot out --at 2,5.5,end   frames at those times -> out_2.png ... (hidden window, muted)
//   ./tutor s.txt --layout-report out.json  no window: play it, check the layout every 0.5 s, write scores
//   ./tutor s.txt --placer old               labels at their old fixed offsets (also TUTOR_PLACER=old); default: placed
//   ./tutor live.txt --stdin                 live: lines arriving on stdin continue the running script
//                                            (no restart); a line "#!load <file>" reloads from scratch
#include "markup/engine.hpp"
#include "rm/raymanim.hpp"
#ifdef __APPLE__
#include <objc/message.h>
#include <objc/runtime.h>
#endif
#include <cstdio>
#include <fstream>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <set>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

// live: the stage keeps talking and drawing even when it is behind other windows. macOS would
// otherwise throttle an occluded app (App Nap), and the conversation would stall.
static void keepAwake() {
#ifdef __APPLE__
  using Cls = id (*)(Class, SEL);
  id info = ((Cls)objc_msgSend)(objc_getClass("NSProcessInfo"), sel_registerName("processInfo"));
  id reason = ((id (*)(Class, SEL, const char *))objc_msgSend)(objc_getClass("NSString"),
                                                              sel_registerName("stringWithUTF8String:"), "live tutor stage");
  // NSActivityUserInitiated | NSActivityLatencyCritical
  id act = ((id (*)(id, SEL, unsigned long long, id))objc_msgSend)(info, sel_registerName("beginActivityWithOptions:reason:"),
                                                                   0x00FFFFFFULL | 0xFF00000000ULL, reason);
  ((id (*)(id, SEL))objc_msgSend)(act, sel_registerName("retain")); // held for the life of the process
#endif
}

static int usage() {
  std::fprintf(stderr,
               "usage: tutor <script.txt> [--mute] [--voice-id Brian] [--speed 1.0] [--model inworld-tts-2]\n"
               "             [--inworld-key KEY] [--record out.mp4] [--readback] [--check] [--font file.ttf]\n"
               "voice: Inworld TTS; key from --inworld-key, $INWORLD_API_KEY or ./.inworld_key\n");
  return 2;
}

#ifdef __APPLE__
#include <IOKit/pwr_mgt/IOPMLib.h>
// what `caffeinate -u` does: declare user activity, which wakes the display for a few seconds
static void wakeDisplay() {
  IOPMAssertionID id;
  if (IOPMAssertionDeclareUserActivity(CFSTR("tutor snapshot"), kIOPMUserActiveLocal, &id) == kIOReturnSuccess)
    IOPMAssertionRelease(id);
}
#else
static void wakeDisplay() {}
#endif

int main(int argc, char **argv) {
  std::string path, record, font, key, snapshot, at = "end", layoutReport;
  int windowX = -1, windowY = -1; // --window-pos X,Y: where the window opens (a side explanation beside the lesson)
  bool mute = false;
  bool readbackOnly = false, check = false, live = false;
  mk::EngineOptions opt;
  if (const char *v = std::getenv("TUTOR_VOICE"); v && *v) opt.tts.voiceId = v; // --voice-id wins
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "--voice") mute = false;
    else if (a == "--verbose" || a == "-v") opt.verbose = true;
    else if (a == "--mute" || a == "--no-voice") mute = true;
    else if (a == "--voice-id" && i + 1 < argc) opt.tts.voiceId = argv[++i];
    else if (a == "--speed" && i + 1 < argc) opt.tts.speakingRate = std::clamp((float)std::atof(argv[++i]), 0.5f, 1.5f);
    else if (a == "--model" && i + 1 < argc) opt.tts.modelId = argv[++i];
    else if (a == "--inworld-key" && i + 1 < argc) key = argv[++i];
    else if (a == "--wpm" && i + 1 < argc) opt.wpm = (float)std::atof(argv[++i]);
    else if (a == "--record" && i + 1 < argc) record = argv[++i];
    else if (a == "--readback") readbackOnly = true;
    else if (a == "--check") check = true;
    else if (a == "--stdin") live = true;
    else if (a == "--snapshot" && i + 1 < argc) snapshot = argv[++i];
    else if (a == "--at" && i + 1 < argc) at = argv[++i];
    else if (a == "--layout-report" && i + 1 < argc) layoutReport = argv[++i];
    else if (a == "--placer" && i + 1 < argc) opt.newPlacer = std::string(argv[++i]) != "old";
    else if (a == "--font" && i + 1 < argc) font = argv[++i];
    else if (a == "--window-pos" && i + 1 < argc) std::sscanf(argv[++i], "%d,%d", &windowX, &windowY);
    else if (a == "-h" || a == "--help") return usage();
    else if (path.empty() && a[0] != '-') path = a;
    else return usage();
  }
  if (path.empty()) return usage();
  if (const char *pl = std::getenv("TUTOR_PLACER"); pl && std::string(pl) == "old") opt.newPlacer = false;
  opt.tts.apiKey = mk::Tts::loadKey(key);
  opt.voice = !mute && !opt.tts.apiKey.empty();
  opt.recording = !record.empty();

  // ---------------------------------------------------------- layout report: play headless, sample the checker
  if (!layoutReport.empty()) {
    SetTraceLogLevel(LOG_ERROR); // stdout carries the report
    rm::initFontMetrics(font.empty() ? nullptr : font.c_str());
    opt.voice = false;
    opt.quiet = true;
    mk::Engine eng(opt);
    eng.load(path);
    auto esc = [](const std::string &s) {
      std::string o;
      for (char c : s) { if (c == '"' || c == '\\') o += '\\'; if ((unsigned char)c >= 0x20) o += c; }
      return o;
    };
    const float dt = 1.f / 60;
    float t = 0, worst = 100, sum = 0;
    int samples = 0, maxOverlaps = 0, settle = 0;
    std::string series;
    std::vector<std::string> finalMsgs, worstMsgs;
    float worstAt = 0;
    place::Report last;
    for (int frame = 0; frame < 60 * 900; frame++) {
      if (eng.waiting()) eng.resume();
      eng.update(dt);
      t += dt;
      bool done = eng.finished();
      if (done) settle++;
      if (frame % 30 == 29 || settle == 60) {
        std::vector<std::string> msgs;
        last = eng.layoutCheck(&msgs);
        if (last.score < worst) { worst = last.score; worstMsgs = msgs; worstAt = t; }
        sum += last.score;
        samples++;
        maxOverlaps = std::max(maxOverlaps, last.overlaps);
        char b[64];
        std::snprintf(b, sizeof b, "%s[%.1f,%.1f,%d]", series.empty() ? "" : ",", t, last.score, last.overlaps);
        series += b;
        if (settle == 60) { finalMsgs = msgs; break; }
      }
    }
    FILE *f = layoutReport == "-" ? stdout : std::fopen(layoutReport.c_str(), "w");
    if (!f) { std::fprintf(stderr, "can't write %s\n", layoutReport.c_str()); return 1; }
    std::fprintf(f, "{\"script\":\"%s\",\"final\":%.1f,\"worst\":%.1f,\"mean\":%.1f,\"maxOverlaps\":%d,\"labelFlips\":%d,\"samples\":[%s],\"issues\":[",
                 esc(path).c_str(), last.score, worst, samples ? sum / samples : 100.f, maxOverlaps, eng.labelFlips, series.c_str());
    for (size_t i = 0; i < finalMsgs.size(); i++) std::fprintf(f, "%s\"%s\"", i ? "," : "", esc(finalMsgs[i]).c_str());
    std::fprintf(f, "],\"worstAt\":%.1f,\"worstIssues\":[", worstAt);
    for (size_t i = 0; i < worstMsgs.size(); i++) std::fprintf(f, "%s\"%s\"", i ? "," : "", esc(worstMsgs[i]).c_str());
    std::fprintf(f, "],\"flips\":[");
    for (size_t i = 0; i < eng.flipLog.size(); i++) std::fprintf(f, "%s\"%s\"", i ? "," : "", esc(eng.flipLog[i]).c_str());
    std::fprintf(f, "],\"composition\":{\"fill\":%.2f,\"offset\":%.2f}}\n", last.fill, last.offset);
    if (f != stdout) std::fclose(f);
    return 0;
  }

  // ---------------------------------------------------------- headless modes
  if (readbackOnly || check) {
    SetTraceLogLevel(LOG_ERROR); // stdout carries the readback
    rm::initFontMetrics(font.empty() ? nullptr : font.c_str()); // real text sizes without a window
    opt.instant = true;
    opt.voice = false;
    mk::Engine eng(opt);
    bool ok = eng.load(path);
    eng.runToEnd();
    if (live) { // headless live: chunks separated by "#!chunk" lines, each appended to the finished stage
      std::string line, chunk;
      auto flush = [&] { if (!chunk.empty()) { ok = eng.appendText(chunk) && ok; eng.runToEnd(); chunk.clear(); } };
      while (std::getline(std::cin, line)) {
        if (line.rfind("#!chunk", 0) == 0) flush();
        else chunk += line + "\n";
      }
      flush();
    }
    std::string rb = eng.readback(true);
    if (readbackOnly) std::printf("%s", rb.c_str());
    if (check) {
      // Decodability: the final scene, written back in the vocabulary and re-read,
      // must give the same scene.
      std::string model = eng.readback(false);
      mk::Engine again(opt);
      again.loadText(model);
      again.runToEnd();
      std::string model2 = again.readback(false);
      bool same = model == model2;
      auto lints = eng.lintMessages();
      int zero = 0;
      for (auto &l : lints) if (l.rfind("zero", 0) == 0 || l.rfind("identity", 0) == 0 || l.rfind("tex", 0) == 0) zero++;
      std::printf("decodable: %s\n", same ? "yes (readback re-parses to the same scene)" : "NO");
      if (!same) std::printf("--- first\n%s--- re-read\n%s", model.c_str(), model2.c_str());
      std::printf("axiom warnings: %d\n", zero);
      std::printf("%s\n", eng.pointingReport().c_str());
      for (auto &l : lints) std::printf("  %s\n", l.c_str());
      // the layout checker on the final scene: what overlaps by accident
      std::vector<std::string> layoutMsgs;
      place::Report lr = eng.layoutCheck(&layoutMsgs);
      std::set<std::string> seen;
      int shown = 0;
      for (auto &m : layoutMsgs)
        if (seen.insert(m).second && shown++ < 6) std::printf("  layout: %s\n", m.c_str());
      std::printf("layout score: %.0f/100\n", lr.score);
      return (same && ok && zero == 0) ? 0 : 1;
    }
    return ok ? 0 : 1;
  }

  // ---------------------------------------------------------- snapshot: frames at given times, offscreen
  if (!snapshot.empty()) {
    wakeDisplay(); // a sleeping display gives no GL context, and the hidden window would crash
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(rm::W, rm::H, "tutor snapshot");
    if (!IsWindowReady()) {
      std::fprintf(stderr, "snapshot: no window (is the display asleep?)\n");
      return 2;
    }
    rm::initGraphics();
    rm::initFonts(font.empty() ? nullptr : font.c_str());
    opt.voice = false;
    opt.quiet = true;
    mk::Engine eng(opt);
    eng.load(path);
    RenderTexture2D rt = LoadRenderTexture(rm::W, rm::H);
    std::vector<std::string> times;
    for (size_t s = 0, e; s <= at.size(); s = e + 1) {
      e = at.find(',', s);
      if (e == std::string::npos) e = at.size();
      if (e > s) times.push_back(at.substr(s, e - s));
    }
    const float dt = 1.f / 60;
    float t = 0;
    auto shoot = [&](const std::string &label) {
      // let pending formulas typeset (the helper answers in milliseconds once warm)
      for (int i = 0; i < 40; i++) { eng.update(0); BeginTextureMode(rt); eng.draw(); EndTextureMode(); }
      BeginTextureMode(rt);
      eng.draw();
      EndTextureMode();
      Image img = LoadImageFromTexture(rt.texture);
      ImageFlipVertical(&img);
      std::string out = snapshot + "_" + label + ".png";
      ExportImage(img, out.c_str());
      UnloadImage(img);
      std::printf("%s\n", out.c_str());
    };
    for (auto &w : times) {
      if (w == "end") {
        for (int guard = 0; guard < 60 * 600 && !eng.finished(); guard++) {
          if (eng.waiting()) eng.resume();
          eng.update(dt);
          t += dt;
        }
        for (int i = 0; i < 90; i++) eng.update(dt); // settle entrances and easing
      } else {
        float target = (float)std::atof(w.c_str());
        while (t < target) {
          if (eng.waiting()) eng.resume();
          eng.update(dt);
          t += dt;
        }
      }
      shoot(w);
    }
    UnloadRenderTexture(rt);
    CloseWindow();
    return 0;
  }

  // ---------------------------------------------------------- window
  // live: no vsync (a hidden window's buffer swap can block on macOS); SetTargetFPS paces frames
  SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI | (live ? 0 : FLAG_VSYNC_HINT));
  if (live) keepAwake();
  SetTraceLogLevel(LOG_WARNING);
  InitWindow(rm::W, rm::H, ("tutor — " + path).c_str());
  if (windowX >= 0 && windowY >= 0) SetWindowPosition(windowX, windowY);
  SetTargetFPS(60);
  SetExitKey(live ? KEY_NULL : KEY_ESCAPE); // live: Esc cancels typing, it doesn't close the stage
  opt.events = live;
  rm::initGraphics();
  rm::initFonts(font.empty() ? nullptr : font.c_str());

  mk::Engine eng(opt);
  eng.load(path);
  long modTime = GetFileModTime(path.c_str());
  float reloadCheck = 0, endTimer = 0;
  rm::Recorder rec;
  if (!record.empty()) rec.start(record.c_str(), 60);
  int shot = 0;

  if (live) setvbuf(stdout, nullptr, _IOLBF, 0); // events reach the server line by line
  // live: a reader thread hands stdin lines to the frame loop
  std::mutex inMu;
  std::deque<std::string> inLines;
  if (live) {
    std::thread([&inMu, &inLines] {
      std::string line;
      while (std::getline(std::cin, line)) {
        std::lock_guard<std::mutex> lk(inMu);
        inLines.push_back(line);
      }
    }).detach();
  }

  // live: the learner can type on the stage itself; the server shows thinking / listening states
  bool typing = false, pausedForTyping = false;
  std::string typed, uiState = "idle", echo;
  std::string captureDir; // #!capture <dir>: this frame and the stage as of now, written after it is drawn
  float uiT = 0;
  auto jsonStr = [](const std::string &s) {
    std::string o = "\"";
    for (unsigned char c : s) {
      if (c == '"' || c == '\\') { o += '\\'; o += (char)c; }
      else if (c >= 0x20) o += (char)c;
    }
    return o + "\"";
  };
  auto popUtf8 = [](std::string &s) {
    while (!s.empty() && ((unsigned char)s.back() & 0xC0) == 0x80) s.pop_back();
    if (!s.empty()) s.pop_back();
  };

  // TUTOR_SHOT=file.png:seconds saves the real window's own frame (high-DPI and all) and quits: the one
  // check offscreen snapshots can't make
  std::string shotFile;
  float shotAt = 0, shotClock = 0;
  // TUTOR_BURST=prefix:seconds:frames saves that many consecutive frames of the real window from then on (for
  // flicker: what changes from one frame to the next, which sampled snapshots can't see)
  std::string burstPrefix;
  float burstAt = -1;
  int burstFrames = 0, burstDone = 0;
  if (const char *e = std::getenv("TUTOR_BURST"); e) {
    char pre[512] = {0};
    if (std::sscanf(e, "%511[^:]:%f:%d", pre, &burstAt, &burstFrames) == 3) burstPrefix = pre;
  }
  if (const char *e = std::getenv("TUTOR_SHOT"); e && std::strchr(e, ':')) {
    shotFile.assign(e, std::strchr(e, ':'));
    shotAt = (float)std::atof(std::strchr(e, ':') + 1);
  }

  while (!WindowShouldClose()) {
    // a hitch is clamped so motion doesn't jump, but not while the voice speaks: then
    // animations must keep pace with the audio clock
    float dt = record.empty() ? std::min(GetFrameTime(), eng.speaking() ? 0.25f : 0.05f) : 1.f / 60.f;

    if (live) {
      std::string chunk;
      std::deque<std::string> got;
      {
        std::lock_guard<std::mutex> lk(inMu);
        got.swap(inLines);
      }
      for (auto &l : got) {
        if (l.rfind("#!", 0) == 0 && !chunk.empty()) { eng.appendText(chunk); chunk.clear(); }
        if (l.rfind("#!load ", 0) == 0) {
          path = l.substr(7);
          eng.load(path);
          SetWindowTitle(("tutor — " + path).c_str());
        } else if (l.rfind("#!voice ", 0) == 0) { // a Say line ahead of its beat: start its audio now
          eng.prefetchVoice(l.substr(8));
        } else if (l == "#!interrupt") {
          eng.interrupt();
        } else if (l == "#!pause") { // the learner asked about this moment: hold it
          if (!eng.paused()) eng.togglePause();
        } else if (l.rfind("#!capture ", 0) == 0) {
          captureDir = l.substr(10);
        } else if (l.rfind("#!state ", 0) == 0) { // "#!state thinking <question>" | listening | idle
          std::string rest = l.substr(8);
          size_t sp = rest.find(' ');
          uiState = rest.substr(0, sp);
          if (sp != std::string::npos) echo = rest.substr(sp + 1);
          uiT = 0;
        } else {
          chunk += l + "\n";
        }
      }
      if (!chunk.empty()) eng.appendText(chunk);
    }

    uiT += dt;
    if (live && typing) {
      for (int c; (c = GetCharPressed()) != 0;) {
        int n;
        const char *u = CodepointToUTF8(c, &n);
        typed.append(u, n);
      }
      if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) popUtf8(typed);
      if (IsKeyPressed(KEY_ESCAPE)) {
        typing = false;
        if (pausedForTyping && eng.paused()) eng.togglePause();
      } else if (IsKeyPressed(KEY_ENTER) && !typed.empty()) {
        typing = false;
        if (typed[0] == '?') { // "?…": a question about this moment, answered beside the lesson, which stays paused
          std::string q = typed.substr(1);
          while (!q.empty() && q[0] == ' ') q.erase(0, 1);
          if (!eng.paused()) eng.togglePause();
          std::printf("@ev explain %s\n", jsonStr(q).c_str());
          echo = q.empty() ? "explain this" : q;
        } else {
          if (pausedForTyping && eng.paused()) eng.togglePause();
          std::printf("@ev input %s\n", jsonStr(typed).c_str());
          echo = typed;
        }
        uiState = "thinking";
        uiT = 0;
      }
    } else if (live) {
      // start typing: any key that makes a character (space pauses, as always)
      for (int c; (c = GetCharPressed()) != 0;) {
        if (c == ' ') continue;
        int n;
        const char *u = CodepointToUTF8(c, &n);
        typed.assign(u, n);
        typing = true;
        pausedForTyping = !eng.paused();
        if (pausedForTyping) eng.togglePause(); // the voice waits while you type
        break;
      }
    }

    eng.pointer(GetMousePosition(), IsMouseButtonDown(MOUSE_BUTTON_LEFT));
    if (!typing && IsKeyPressed(KEY_SPACE)) {
      if (eng.waiting()) eng.resume();
      else eng.togglePause();
    }
    if (!typing && (IsKeyPressed(KEY_RIGHT) || (!live && IsKeyPressed(KEY_ENTER)))) eng.skip();
    if (!live && (IsKeyPressed(KEY_R) || IsKeyPressed(KEY_LEFT))) eng.reset();
    if (!live && IsKeyPressed(KEY_D)) eng.setShowDictionary(!eng.showDictionary());
    if (!live && IsKeyPressed(KEY_S)) {
      std::string f = "tutor_shot_" + std::to_string(shot++) + ".png";
      TakeScreenshot(f.c_str());
      std::printf("saved %s\n", f.c_str());
    }
    shotClock += dt;
    if (shotAt > 0 && shotClock >= shotAt) { TakeScreenshot(shotFile.c_str()); break; }

    // hot reload: edit the .txt, the stage restarts
    reloadCheck += dt;
    if (!live && reloadCheck > 0.5f) {
      reloadCheck = 0;
      long t = GetFileModTime(path.c_str());
      if (t != modTime) {
        modTime = t;
        std::printf("reloading %s\n", path.c_str());
        eng.load(path);
      }
    }

    eng.update(dt);
    BeginDrawing();
    eng.draw();
    if (live) {
      using namespace rm;
      if (typing) { // the prompt bar
        Rectangle bar{170, 630, 940, 50};
        roundedBox(bar, 14, 2, col::blue, fade(col::bg, 0.96f));
        std::string shown = typed;
        while (measure(shown, 24).x > bar.width - 60 && !shown.empty()) shown.erase(0, 1);
        text(shown, {bar.x + 20, bar.y + bar.height / 2}, 24, col::fg, 0, 0.5f);
        float cx = bar.x + 22 + measure(shown, 24).x;
        if (std::fmod(uiT, 1.f) < 0.6f) DrawRectangle((int)cx, (int)bar.y + 13, 2, 24, col::fg);
        text("Enter to ask  ·  start with ? to ask about this moment  ·  Esc to cancel", {bar.x + bar.width, bar.y + bar.height + 8}, 14,
             col::muted, 1, 0);
      } else if (uiState == "thinking") { // your question, and a breathing dot while the answer is on its way
        if (!echo.empty()) text("\u201c" + echo + "\u201d", {W / 2.f, 600}, 22, fade(col::muted, 0.9f), 0.5f, 0.5f);
        for (int i = 0; i < 3; i++) {
          float p = 0.5f + 0.5f * std::sin(uiT * 5 - i * 0.7f);
          DrawCircleV({W / 2.f - 18 + i * 18.f, 640}, 4 + 2 * p, fade(col::blue, 0.35f + 0.65f * p));
        }
      } else if (uiState == "listening") { // a check question was asked: your turn
        float p = 0.5f + 0.5f * std::sin(uiT * 2.5f);
        textBg("your turn  ·  just start typing", {W / 2.f, 652}, 17, fade(col::fg, 0.7f + 0.3f * p),
               fade(col::blue, 0.25f), 0.5f, 0.5f, 10);
      }
    }
    EndDrawing();
    rec.frame();
    if (!burstPrefix.empty() && burstDone < burstFrames && shotClock >= burstAt) {
      Image img = LoadImageFromScreen();
      ExportImage(img, (burstPrefix + "_" + std::to_string(burstDone++) + ".png").c_str());
      UnloadImage(img);
    }
    if (!captureDir.empty()) { // what the learner sees, and the stage as of now (not the end of the turn)
      Image img = LoadImageFromScreen();
      ExportImage(img, (captureDir + "/frame.png").c_str());
      UnloadImage(img);
      std::ofstream(captureDir + "/stage.txt") << eng.readback(false);
      std::printf("@ev captured %s\n", captureDir.c_str());
      captureDir.clear();
    }

    if (!record.empty()) {
      if (eng.finished()) endTimer += dt;
      if (endTimer > 2.0f) break;
    }
  }
  rec.stop();
  CloseWindow();

  // give the recording its narration: each clip delayed to the moment it was spoken
  const auto &clips = eng.audioLog();
  if (!record.empty() && !clips.empty()) {
    std::string tmp = record + ".voiced.mp4";
    std::string cmd = "ffmpeg -y -loglevel error -i \"" + record + "\"";
    for (auto &c : clips) cmd += " -i \"" + c.second + "\"";
    cmd += " -filter_complex \"";
    std::string mix;
    for (size_t i = 0; i < clips.size(); i++) {
      int ms = (int)(clips[i].first * 1000);
      cmd += "[" + std::to_string(i + 1) + "]adelay=" + std::to_string(ms) + "|" + std::to_string(ms) + "[a" + std::to_string(i) + "];";
      mix += "[a" + std::to_string(i) + "]";
    }
    cmd += mix + "amix=inputs=" + std::to_string(clips.size()) + ":normalize=0[a]\" -map 0:v -map \"[a]\" -c:v copy -c:a aac \"" + tmp + "\"";
    if (std::system(cmd.c_str()) == 0 && std::rename(tmp.c_str(), record.c_str()) == 0)
      std::printf("added narration to %s\n", record.c_str());
    else
      std::fprintf(stderr, "could not add narration (ffmpeg failed)\n");
  }
  return 0;
}
