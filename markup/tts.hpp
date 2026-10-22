// Realtime narration with Inworld TTS.
//
// Every Say line becomes an Utterance. Utterances are requested in script order by a small
// worker pool (so the next line is usually ready before the voice gets to it), streamed as raw
// 16-bit PCM over NDJSON, and played through a raylib AudioStream as the chunks arrive.
// Inworld returns word timestamps alongside the audio; the engine uses them so |hooks| fire
// on the exact word. Finished utterances are cached on disk, so a re-run costs nothing.
#pragma once
#include "raylib.h"
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace mk {

struct TtsConfig {
  std::string apiKey;                 // Inworld Basic credential (base64), as given by the portal
  std::string voiceId = "Brian";     // friendly, encouraging tutor voice ($TUTOR_VOICE or --voice-id to change)
  std::string modelId = "inworld-tts-2";
  float speakingRate = 1.0f;          // 0.5 .. 1.5
  // quality (Inworld TTS docs, 2026): numbers, symbols and units read as words; the model's denoising; and the
  // sentences just before, so each one continues the explanation instead of starting cold (not billed)
  bool normalizeText = true;          // applyTextNormalization ON
  bool enhance = true;                // enhanceGeneration
  int contextLines = 2;               // synthesisContext: this many earlier sentences (0: none)
  std::string cacheDir = ".tutor_cache";
  int workers = 3;
};

struct Utterance {
  std::string text, key;
  std::vector<std::string> context;   // earlier sentences, sent as synthesisContext
  std::mutex mu;
  std::vector<int16_t> pcm;           // mono, Tts::kRate
  std::vector<std::string> words;     // spoken words only (no spaces / punctuation tokens)
  std::vector<float> wordStart, wordEnd;
  bool done = false, failed = false, cached = false;
  std::string error;
  std::string wavPath;                // written when done (for muxing into recordings)

  float seconds();                    // audio received so far
};
using UttPtr = std::shared_ptr<Utterance>;

class Tts {
public:
  static constexpr int kRate = 48000;  // inworld-tts-2's native rate: real detail above 12 kHz (measured), not upsampled

  explicit Tts(TtsConfig cfg);
  ~Tts();
  bool ok() const { return !cfg_.apiKey.empty(); }

  // cached, in flight, or queued (FIFO); context: the sentences spoken just before (the last contextLines are used)
  UttPtr request(const std::string &text, const std::vector<std::string> &context = {});

  // playback: one utterance at a time on the audio thread
  void play(UttPtr u);
  void stop();
  void setPaused(bool p) { paused_ = p; }
  size_t played() const { return readPos_.load(); } // samples of the current utterance handed to the device
  // samples actually heard: what was handed over, minus what is still queued in the stream's
  // buffers and the device (raylib double-buffers 1024 frames). Hooks key on this.
  static constexpr size_t kOutputLatency = 3072; // 64 ms, as before the rate doubled
  size_t heard() const { size_t p = readPos_.load(); return p > kOutputLatency ? p - kOutputLatency : 0; }
  bool hasAudio() const { return audio_; }
  UttPtr current();

  static std::string loadKey(const std::string &cliKey); // --inworld-key, $INWORLD_API_KEY, .inworld_key

private:
  void worker();
  void fetch(const UttPtr &u);
  void saveCache(const UttPtr &u);
  bool loadCache(const UttPtr &u);
  static void audioCallback(void *buffer, unsigned int frames);

  TtsConfig cfg_;
  std::mutex qmu_;
  std::condition_variable qcv_;
  std::deque<UttPtr> queue_;
  std::map<std::string, UttPtr> all_;
  std::vector<std::thread> threads_;
  std::atomic<bool> quit_{false};

  AudioStream stream_{};
  bool audio_ = false;
  std::mutex pmu_;
  UttPtr cur_;
  std::atomic<size_t> readPos_{0};
  std::atomic<bool> paused_{false};
  static Tts *instance_;
};

} // namespace mk
