// place: a seeded, platform-stable random source (splitmix64). Never std:: distributions,
// whose output differs between standard libraries: the same seed must give the same layout everywhere.
#pragma once
#include <cstdint>
#include <string>

namespace place {

struct Rng {
  uint64_t s;
  explicit Rng(uint64_t seed) : s(seed) {}
  uint64_t next() {
    uint64_t z = (s += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
  }
  float unit() { return (float)((next() >> 40) * (1.0 / 16777216.0)); } // [0,1)
  int below(int n) { return n > 0 ? (int)(next() % (uint64_t)n) : 0; }
};

inline uint64_t fnv1a(const std::string &s, uint64_t h = 1469598103934665603ull) {
  for (unsigned char c : s) { h ^= c; h *= 1099511628211ull; }
  return h;
}

} // namespace place
