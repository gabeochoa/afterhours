#pragma once

#include <cstdint>
#include <list>
#include <string_view>
#include <unordered_map>

#include "developer.h"

namespace afterhours {

// Measuring a string walks it glyph by glyph, and the render path does that
// for every label every frame even though the text has not changed. Measured
// on wm: 995 measurements a frame on one screen resolving to 101 distinct
// questions, so about nine in ten were answered already, and across frames
// every one of them was.
//
// This memo sits at the backend's measure_text, so every caller benefits and
// none of them changes. It is keyed on the text, the size, the spacing and the
// font's own identity, so a different face or a reloaded atlas is a different
// entry rather than a stale hit.
//
// TextMeasureCache is the richer, app-facing version of this and keys on a
// font NAME. The backend does not have a name here, only a Font, which is why
// this is separate rather than a call into that.
namespace measure_memo {

constexpr std::size_t kMaxEntries = 4096;

struct Entry {
  std::uint64_t key;
  Vector2Type size;
};

inline std::list<Entry> &lru() {
  static std::list<Entry> l;
  return l;
}

inline std::unordered_map<std::uint64_t, std::list<Entry>::iterator> &index() {
  static std::unordered_map<std::uint64_t, std::list<Entry>::iterator> m;
  return m;
}

inline std::uint64_t &hits() {
  static std::uint64_t n = 0;
  return n;
}

inline std::uint64_t &misses() {
  static std::uint64_t n = 0;
  return n;
}

inline std::uint64_t hash(std::string_view text, float size, float spacing,
                          std::uint64_t font_id) {
  std::uint64_t h = 1469598103934665603ull;
  const auto mix = [&h](std::uint64_t v) {
    h ^= v;
    h *= 1099511628211ull;
  };
  for (unsigned char c : text)
    mix(c);
  // Quantised so a float that differs in its last bit is not a separate entry.
  mix(static_cast<std::uint64_t>(size * 64.f));
  mix(static_cast<std::uint64_t>(spacing * 64.f));
  mix(font_id);
  return h;
}

// Returns true and fills `out` on a hit.
inline bool lookup(std::uint64_t key, Vector2Type &out) {
  auto &idx = index();
  const auto it = idx.find(key);
  if (it == idx.end()) {
    misses()++;
    return false;
  }
  lru().splice(lru().begin(), lru(), it->second);
  out = it->second->size;
  hits()++;
  return true;
}

inline void store(std::uint64_t key, const Vector2Type &size) {
  auto &idx = index();
  while (idx.size() >= kMaxEntries) {
    idx.erase(lru().back().key);
    lru().pop_back();
  }
  lru().push_front({key, size});
  idx[key] = lru().begin();
}

// A font reloaded under the same handle would otherwise keep serving the old
// metrics. Callers that swap an atlas at runtime should say so.
inline void clear() {
  lru().clear();
  index().clear();
}

} // namespace measure_memo

} // namespace afterhours
