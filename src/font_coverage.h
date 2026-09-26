#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <utility>
#include <vector>

namespace afterhours::font_coverage {

// The codepoints a typeface actually covers, as merged ranges, parsed from
// its cmap table. Backends record this at load because neither raylib nor
// fontstash can answer it afterwards: raylib's glyph array gains a
// placeholder entry for every requested codepoint, covered or not, and
// fontstash keeps its cmap behind implementation-only structs.
using Ranges = std::vector<std::pair<uint32_t, uint32_t>>;

inline uint16_t u16(const unsigned char *p) {
  return static_cast<uint16_t>((p[0] << 8) | p[1]);
}
inline uint32_t u32(const unsigned char *p) {
  return (static_cast<uint32_t>(u16(p)) << 16) | u16(p + 2);
}

// Covered ranges out of one cmap subtable (format 4 or 12); false when the
// format is neither. Format 4 coverage is per codepoint: a segment whose
// glyph ids land on 0 does not cover, whatever its range says.
inline bool parse_subtable(const unsigned char *d, size_t n, size_t off,
                           Ranges &out) {
  if (off + 2 > n)
    return false;
  const uint16_t format = u16(d + off);
  if (format == 12) {
    if (off + 16 > n)
      return false;
    const uint32_t groups = u32(d + off + 12);
    if (off + 16 + static_cast<size_t>(groups) * 12 > n)
      return false;
    for (uint32_t g = 0; g < groups; g++) {
      const unsigned char *p = d + off + 16 + static_cast<size_t>(g) * 12;
      if (u32(p + 8) != 0)
        out.emplace_back(u32(p), u32(p + 4));
    }
    return true;
  }
  if (format == 4) {
    if (off + 14 > n)
      return false;
    const size_t seg_count = u16(d + off + 6) / 2;
    const size_t end_off = off + 14;
    const size_t start_off = end_off + 2 * seg_count + 2;
    const size_t delta_off = start_off + 2 * seg_count;
    const size_t range_off = delta_off + 2 * seg_count;
    if (range_off + 2 * seg_count > n)
      return false;
    for (size_t s = 0; s < seg_count; s++) {
      const uint32_t first = u16(d + start_off + 2 * s);
      const uint32_t last = u16(d + end_off + 2 * s);
      if (first == 0xFFFF)
        continue;
      const int16_t delta = static_cast<int16_t>(u16(d + delta_off + 2 * s));
      const uint16_t range_offset = u16(d + range_off + 2 * s);
      for (uint32_t cp = first; cp <= last; cp++) {
        uint32_t glyph = 0;
        if (range_offset == 0) {
          glyph = (cp + delta) & 0xFFFF;
        } else {
          const size_t glyph_off =
              range_off + 2 * s + range_offset + 2 * (cp - first);
          if (glyph_off + 2 <= n)
            glyph = u16(d + glyph_off);
          if (glyph != 0)
            glyph = (glyph + delta) & 0xFFFF;
        }
        if (glyph == 0)
          continue;
        if (!out.empty() && out.back().second + 1 == cp)
          out.back().second = cp;
        else
          out.emplace_back(cp, cp);
      }
    }
    return true;
  }
  return false;
}

inline bool parse_cmap(const unsigned char *d, size_t n, Ranges &out) {
  if (!d || n < 12)
    return false;
  const uint16_t tables = u16(d + 4);
  size_t cmap_off = 0;
  for (uint16_t i = 0;
       i < tables && 12 + static_cast<size_t>(i) * 16 + 16 <= n; i++) {
    const unsigned char *rec = d + 12 + static_cast<size_t>(i) * 16;
    if (std::memcmp(rec, "cmap", 4) == 0) {
      cmap_off = u32(rec + 8);
      break;
    }
  }
  if (cmap_off == 0 || cmap_off + 4 > n)
    return false;
  const uint16_t subtables = u16(d + cmap_off + 2);
  bool parsed = false;
  for (uint16_t i = 0;
       i < subtables && cmap_off + 4 + static_cast<size_t>(i) * 8 + 8 <= n;
       i++) {
    const unsigned char *rec = d + cmap_off + 4 + static_cast<size_t>(i) * 8;
    Ranges sub;
    if (parse_subtable(d, n, cmap_off + u32(rec + 4), sub)) {
      out.insert(out.end(), sub.begin(), sub.end());
      parsed = true;
    }
  }
  if (!parsed)
    return false;
  std::sort(out.begin(), out.end());
  Ranges merged;
  for (const auto &r : out) {
    if (!merged.empty() && r.first <= merged.back().second + 1)
      merged.back().second = std::max(merged.back().second, r.second);
    else
      merged.push_back(r);
  }
  out = std::move(merged);
  return true;
}

inline bool parse_file(const char *file, Ranges &out) {
  if (!file)
    return false;
  std::ifstream in(file, std::ios::binary | std::ios::ate);
  if (!in)
    return false;
  const std::streamsize size = in.tellg();
  if (size <= 0)
    return false;
  std::vector<unsigned char> bytes(static_cast<size_t>(size));
  in.seekg(0);
  if (!in.read(reinterpret_cast<char *>(bytes.data()), size))
    return false;
  return parse_cmap(bytes.data(), bytes.size(), out);
}

inline bool covers(const Ranges &ranges, uint32_t cp) {
  size_t lo = 0, hi = ranges.size();
  while (lo < hi) {
    const size_t mid = (lo + hi) / 2;
    if (cp < ranges[mid].first)
      hi = mid;
    else if (cp > ranges[mid].second)
      lo = mid + 1;
    else
      return true;
  }
  return false;
}

} // namespace afterhours::font_coverage
