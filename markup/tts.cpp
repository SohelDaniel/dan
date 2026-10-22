#include "tts.hpp"
#include "../third_party/json.hpp"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <curl/curl.h>
#include <fstream>
#include <sstream>
#include <sys/stat.h>

namespace mk {

using json = nlohmann::json;

Tts *Tts::instance_ = nullptr;

float Utterance::seconds() {
  std::lock_guard<std::mutex> l(mu);
  return (float)pcm.size() / Tts::kRate;
}

// ------------------------------------------------------------------ helpers
static std::string fnv(const std::string &s) {
  uint64_t h = 1469598103934665603ull;
  for (unsigned char c : s) { h ^= c; h *= 1099511628211ull; }
  char buf[20];
  std::snprintf(buf, sizeof buf, "%016llx", (unsigned long long)h);
  return buf;
}

static std::string b64decode(const std::string &in) {
  static int T[256];
  static bool init = false;
  if (!init) {
    for (int &t : T) t = -1;
    const char *a = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (int i = 0; i < 64; i++) T[(unsigned char)a[i]] = i;
    init = true;
  }
  std::string out;
  out.reserve(in.size() * 3 / 4);
  int val = 0, bits = -8;
  for (unsigned char c : in) {
    if (T[c] < 0) continue;
    val = (val << 6) + T[c];
    bits += 6;
    if (bits >= 0) { out.push_back(char((val >> bits) & 0xFF)); bits -= 8; }
  }
  return out;
}

static bool spoken(const std::string &w) {
  for (unsigned char c : w) if (std::isalnum(c) || (c & 0x80)) return true;
  return false;
}

std::string Tts::loadKey(const std::string &cli) {
  auto trim = [](std::string s) {
    while (!s.empty() && std::isspace((unsigned char)s.back())) s.pop_back();
    while (!s.empty() && std::isspace((unsigned char)s.front())) s.erase(s.begin());
    return s;
  };
  if (!cli.empty()) return trim(cli);
  if (const char *e = std::getenv("INWORLD_API_KEY"); e && *e) return trim(e);
  for (const char *p : {".inworld_key", "../.inworld_key"}) {
    std::ifstream f(p);
    if (f) { std::stringstream ss; ss << f.rdbuf(); std::string k = trim(ss.str()); if (!k.empty()) return k; }
  }
  return "";
}

// ------------------------------------------------------------------ lifecycle
Tts::Tts(TtsConfig cfg) : cfg_(std::move(cfg)) {
  instance_ = this;
  curl_global_init(CURL_GLOBAL_DEFAULT);
  mkdir(cfg_.cacheDir.c_str(), 0755);
  if (!IsAudioDeviceReady()) InitAudioDevice();
  if (IsAudioDeviceReady()) {
    SetAudioStreamBufferSizeDefault(2048); // ~43 ms at 48 kHz: hooks stay tight to the words
    stream_ = LoadAudioStream(kRate, 16, 1);
    SetAudioStreamCallback(stream_, &Tts::audioCallback);
    PlayAudioStream(stream_);
    audio_ = true;
  } else {
    std::fprintf(stderr, "voice: no audio device\n");
  }
  for (int i = 0; i < cfg_.workers; i++) threads_.emplace_back(&Tts::worker, this);
}

Tts::~Tts() {
  quit_ = true;
  qcv_.notify_all();
  for (auto &t : threads_) t.join();
  if (audio_) {
    StopAudioStream(stream_);
    UnloadAudioStream(stream_);
  }
  instance_ = nullptr;
  curl_global_cleanup();
}

UttPtr Tts::request(const std::string &text, const std::vector<std::string> &context) {
  std::vector<std::string> ctx(context.end() - std::min(context.size(), (size_t)std::max(0, cfg_.contextLines)), context.end());
  // everything that changes the audio is in the key, so a cached line is never played with other settings
  std::string key = cfg_.voiceId + "|" + cfg_.modelId + "|" + std::to_string(cfg_.speakingRate) + "|" + std::to_string(kRate) +
                    "|" + (cfg_.normalizeText ? "norm" : "") + "|" + (cfg_.enhance ? "enh" : "") + "|";
  for (auto &c : ctx) key += c + "\x1f";
  key = fnv(key + "|" + text);
  std::lock_guard<std::mutex> l(qmu_);
  auto it = all_.find(key);
  if (it != all_.end() && !it->second->failed) return it->second;
  auto u = std::make_shared<Utterance>();
  u->text = text;
  u->context = ctx;
  u->key = key;
  all_[key] = u;
  if (loadCache(u)) return u;
  queue_.push_back(u);
  qcv_.notify_one();
  return u;
}

void Tts::worker() {
  for (;;) {
    UttPtr u;
    {
      std::unique_lock<std::mutex> l(qmu_);
      qcv_.wait(l, [&] { return quit_ || !queue_.empty(); });
      if (quit_) return;
      u = queue_.front();
      queue_.pop_front();
    }
    fetch(u);
  }
}

// ------------------------------------------------------------------ streaming request
namespace {
struct StreamCtx {
  Tts *self;
  UttPtr u;
  std::string line;      // partial NDJSON line
  std::string oddByte;   // PCM bytes can split across chunks
  float timeOffset = 0;
  std::atomic<bool> *quit;
};

void handleLine(StreamCtx &c, const std::string &line) {
  if (line.find_first_not_of(" \r\t") == std::string::npos) return;
  json j = json::parse(line, nullptr, false);
  if (j.is_discarded()) return;
  if (j.contains("error")) {
    std::lock_guard<std::mutex> l(c.u->mu);
    c.u->failed = true;
    c.u->error = j["error"].value("message", j["error"].dump());
    return;
  }
  const json &r = j.contains("result") ? j["result"] : j;
  std::string pcm;
  if (r.contains("audioContent") && r["audioContent"].is_string()) pcm = c.oddByte + b64decode(r["audioContent"].get<std::string>());
  std::lock_guard<std::mutex> l(c.u->mu);
  if (!pcm.empty()) {
    size_t n = pcm.size() / 2;
    size_t old = c.u->pcm.size();
    c.u->pcm.resize(old + n);
    std::memcpy(c.u->pcm.data() + old, pcm.data(), n * 2);
    c.oddByte = pcm.size() % 2 ? pcm.substr(pcm.size() - 1) : "";
  }
  if (r.contains("timestampInfo") && r["timestampInfo"].contains("wordAlignment")) {
    const json &wa = r["timestampInfo"]["wordAlignment"];
    const json &ws = wa["words"], &st = wa["wordStartTimeSeconds"], &en = wa["wordEndTimeSeconds"];
    for (size_t i = 0; i < ws.size() && i < st.size() && i < en.size(); i++) {
      std::string w = ws[i].get<std::string>();
      if (!spoken(w)) continue;
      c.u->words.push_back(w);
      c.u->wordStart.push_back(st[i].get<float>());
      c.u->wordEnd.push_back(en[i].get<float>());
    }
  }
}

size_t onData(char *ptr, size_t size, size_t nmemb, void *ud) {
  auto &c = *static_cast<StreamCtx *>(ud);
  if (c.quit->load()) return 0; // abort transfer
  size_t n = size * nmemb;
  c.line.append(ptr, n);
  size_t nl;
  while ((nl = c.line.find('\n')) != std::string::npos) {
    std::string one = c.line.substr(0, nl);
    c.line.erase(0, nl + 1);
    handleLine(c, one);
  }
  return n;
}
} // namespace

void Tts::fetch(const UttPtr &u) {
  json body = {{"text", u->text},
               {"voiceId", cfg_.voiceId},
               {"modelId", cfg_.modelId},
               {"audioConfig", {{"audioEncoding", "PCM"}, {"sampleRateHertz", kRate}, {"speakingRate", cfg_.speakingRate}}},
               {"timestampType", "WORD"},
               {"timestampTransportStrategy", "SYNC"}};
  if (cfg_.normalizeText) body["applyTextNormalization"] = "ON";
  if (cfg_.enhance) body["enhanceGeneration"] = true;
  if (!u->context.empty()) {
    json prev = json::array();
    for (auto &c : u->context) prev.push_back({{"text", c}});
    body["synthesisContext"] = {{"previousRequests", prev}};
  }
  std::string payload = body.dump();

  for (int attempt = 0; attempt < 3 && !quit_; attempt++) {
    {
      std::lock_guard<std::mutex> l(u->mu);
      u->pcm.clear(); u->words.clear(); u->wordStart.clear(); u->wordEnd.clear();
      u->failed = false; u->error.clear();
    }
    CURL *h = curl_easy_init();
    if (!h) break;
    StreamCtx ctx{this, u, "", "", 0, &quit_};
    struct curl_slist *hdr = nullptr;
    hdr = curl_slist_append(hdr, ("Authorization: Basic " + cfg_.apiKey).c_str());
    hdr = curl_slist_append(hdr, "Content-Type: application/json");
    curl_easy_setopt(h, CURLOPT_URL, "https://api.inworld.ai/tts/v1/voice:stream");
    curl_easy_setopt(h, CURLOPT_HTTPHEADER, hdr);
    curl_easy_setopt(h, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, onData);
    curl_easy_setopt(h, CURLOPT_WRITEDATA, &ctx);
    curl_easy_setopt(h, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(h, CURLOPT_TIMEOUT, 120L);
    CURLcode rc = curl_easy_perform(h);
    long status = 0;
    curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &status);
    curl_slist_free_all(hdr);
    curl_easy_cleanup(h);
    if (!ctx.line.empty()) handleLine(ctx, ctx.line);

    std::lock_guard<std::mutex> l(u->mu);
    if (rc == CURLE_OK && status == 200 && !u->failed && !u->pcm.empty()) {
      u->done = true;
      break;
    }
    if (u->error.empty()) u->error = rc != CURLE_OK ? curl_easy_strerror(rc) : "HTTP " + std::to_string(status);
    std::fprintf(stderr, "voice: inworld request failed (%s)%s\n", u->error.c_str(), attempt < 2 ? ", retrying" : "");
    if (status == 401 || status == 403 || status == 400) break; // retrying won't help
  }
  {
    std::lock_guard<std::mutex> l(u->mu);
    if (!u->done) { u->failed = true; u->done = true; return; }
  }
  saveCache(u);
}

// ------------------------------------------------------------------ cache
void Tts::saveCache(const UttPtr &u) {
  std::string base = cfg_.cacheDir + "/" + u->key;
  std::lock_guard<std::mutex> l(u->mu);
  {
    std::ofstream f(base + ".pcm", std::ios::binary);
    f.write((const char *)u->pcm.data(), (std::streamsize)(u->pcm.size() * 2));
  }
  {
    std::ofstream f(base + ".words");
    for (size_t i = 0; i < u->words.size(); i++) f << u->words[i] << "\t" << u->wordStart[i] << "\t" << u->wordEnd[i] << "\n";
  }
  {
    // a plain WAV too, so recordings can be given their soundtrack with ffmpeg
    std::ofstream f(base + ".wav", std::ios::binary);
    uint32_t data = (uint32_t)(u->pcm.size() * 2), riff = 36 + data, fmtLen = 16, rate = kRate, byteRate = kRate * 2;
    uint16_t pcmFmt = 1, ch = 1, align = 2, bits = 16;
    f.write("RIFF", 4); f.write((char *)&riff, 4); f.write("WAVEfmt ", 8);
    f.write((char *)&fmtLen, 4); f.write((char *)&pcmFmt, 2); f.write((char *)&ch, 2);
    f.write((char *)&rate, 4); f.write((char *)&byteRate, 4); f.write((char *)&align, 2); f.write((char *)&bits, 2);
    f.write("data", 4); f.write((char *)&data, 4);
    f.write((const char *)u->pcm.data(), data);
  }
  u->wavPath = base + ".wav";
}

bool Tts::loadCache(const UttPtr &u) {
  std::string base = cfg_.cacheDir + "/" + u->key;
  std::ifstream p(base + ".pcm", std::ios::binary | std::ios::ate), w(base + ".words");
  if (!p || !w) return false;
  std::streamsize n = p.tellg();
  if (n <= 0) return false;
  p.seekg(0);
  std::lock_guard<std::mutex> l(u->mu);
  u->pcm.resize((size_t)n / 2);
  p.read((char *)u->pcm.data(), n - n % 2);
  std::string line;
  while (std::getline(w, line)) {
    std::istringstream ls(line);
    std::string word;
    float s, e;
    if (std::getline(ls, word, '\t') && ls >> s >> e) {
      u->words.push_back(word);
      u->wordStart.push_back(s);
      u->wordEnd.push_back(e);
    }
  }
  u->done = true;
  u->cached = true;
  u->wavPath = base + ".wav";
  return true;
}

// ------------------------------------------------------------------ playback
void Tts::play(UttPtr u) {
  std::lock_guard<std::mutex> l(pmu_);
  cur_ = std::move(u);
  readPos_ = 0;
  paused_ = false;
}
void Tts::stop() {
  std::lock_guard<std::mutex> l(pmu_);
  cur_.reset();
  readPos_ = 0;
}
UttPtr Tts::current() {
  std::lock_guard<std::mutex> l(pmu_);
  return cur_;
}

void Tts::audioCallback(void *buffer, unsigned int frames) {
  auto *out = static_cast<int16_t *>(buffer);
  std::memset(out, 0, frames * sizeof(int16_t));
  Tts *self = instance_;
  if (!self || self->paused_) return;
  std::lock_guard<std::mutex> l(self->pmu_);
  if (!self->cur_) return;
  Utterance &u = *self->cur_;
  std::lock_guard<std::mutex> l2(u.mu);
  size_t pos = self->readPos_.load();
  size_t avail = u.pcm.size() > pos ? u.pcm.size() - pos : 0;
  size_t n = std::min<size_t>(avail, frames);
  if (n) std::memcpy(out, u.pcm.data() + pos, n * sizeof(int16_t));
  self->readPos_ = pos + n; // an underrun plays silence and the clock waits with it
}

} // namespace mk
