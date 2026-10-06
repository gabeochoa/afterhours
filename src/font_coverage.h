#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
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

inline bool read_file(const char *file, std::vector<unsigned char> &out) {
  if (!file)
    return false;
  std::ifstream in(file, std::ios::binary | std::ios::ate);
  if (!in)
    return false;
  const std::streamsize size = in.tellg();
  if (size <= 0)
    return false;
  out.resize(static_cast<size_t>(size));
  in.seekg(0);
  return static_cast<bool>(
      in.read(reinterpret_cast<char *>(out.data()), size));
}

inline bool parse_file(const char *file, Ranges &out) {
  std::vector<unsigned char> bytes;
  return read_file(file, bytes) &&
         parse_cmap(bytes.data(), bytes.size(), out);
}

inline bool is_collection(const unsigned char *d, size_t n) {
  return d && n >= 12 && std::memcmp(d, "ttcf", 4) == 0;
}

inline int face_count(const unsigned char *d, size_t n) {
  if (!is_collection(d, n))
    return d && n >= 12 ? 1 : 0;
  return static_cast<int>(u32(d + 8));
}

// One face of a TrueType/OpenType collection (.ttc) as a standalone font.
// stb_truetype, under both fontstash and raylib, reads the face at offset 0
// and neither loader takes an index, so a collection loads as nothing.
// Table offsets in a collection are from the start of the file, so the face's
// tables are copied out behind a fresh directory. A plain font comes back
// unchanged for index 0; anything unreadable comes back empty.
inline std::vector<unsigned char>
extract_face(const std::vector<unsigned char> &d, int index) {
  const size_t n = d.size();
  if (!is_collection(d.data(), n))
    return index == 0 ? d : std::vector<unsigned char>{};
  if (index < 0 || index >= face_count(d.data(), n) ||
      12 + 4 * static_cast<size_t>(index) + 4 > n)
    return {};
  const size_t face = u32(d.data() + 12 + 4 * static_cast<size_t>(index));
  if (face + 12 > n)
    return {};
  const size_t tables = u16(d.data() + face + 4);
  const size_t dir = 12 + 16 * tables;
  if (face + dir > n)
    return {};
  std::vector<unsigned char> out(d.begin() + static_cast<long>(face),
                                 d.begin() + static_cast<long>(face + dir));
  for (size_t t = 0; t < tables; t++) {
    unsigned char *rec = out.data() + 12 + 16 * t;
    const size_t off = u32(rec + 8), len = u32(rec + 12);
    if (off + len > n)
      return {};
    out.resize((out.size() + 3) & ~size_t{3});
    const uint32_t at = static_cast<uint32_t>(out.size());
    out.insert(out.end(), d.begin() + static_cast<long>(off),
               d.begin() + static_cast<long>(off + len));
    rec = out.data() + 12 + 16 * t; // insert may have moved the buffer
    rec[8] = static_cast<unsigned char>(at >> 24);
    rec[9] = static_cast<unsigned char>(at >> 16);
    rec[10] = static_cast<unsigned char>(at >> 8);
    rec[11] = static_cast<unsigned char>(at);
  }
  return out;
}

// Top of the line box to the baseline, as a fraction of the font size:
// hhea ascender / (ascender - descender). stb_truetype sizes a face by that
// same span, so fontstash's ALIGN_TOP and raylib's glyph offsets both put the
// baseline here. 0 when the table is missing.
inline float baseline_ratio(const unsigned char *d, size_t n) {
  if (!d || n < 12)
    return 0.f;
  const uint16_t tables = u16(d + 4);
  for (uint16_t i = 0;
       i < tables && 12 + static_cast<size_t>(i) * 16 + 16 <= n; i++) {
    const unsigned char *rec = d + 12 + static_cast<size_t>(i) * 16;
    if (std::memcmp(rec, "hhea", 4) != 0)
      continue;
    const size_t off = u32(rec + 8);
    if (off + 8 > n)
      return 0.f;
    const float asc = static_cast<int16_t>(u16(d + off + 4));
    const float desc = static_cast<int16_t>(u16(d + off + 6));
    return asc - desc > 0.f ? asc / (asc - desc) : 0.f;
  }
  return 0.f;
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

// Codepoints that draw nothing on their own: joiners, variation selectors,
// tag characters, skin-tone modifiers and the keycap mark. A face without
// them would draw a missing glyph between the characters they modify.
inline bool is_invisible_modifier(uint32_t cp) {
  return (cp >= 0x200B && cp <= 0x200F) || (cp >= 0x2060 && cp <= 0x2064) ||
         (cp >= 0xFE00 && cp <= 0xFE0F) || cp == 0xFEFF || cp == 0x20E3 ||
         (cp >= 0x1F3FB && cp <= 0x1F3FF) || (cp >= 0xE0000 && cp <= 0xE0FFF);
}

// Codepoint and byte length at `s`; length 0 for a malformed sequence.
inline uint32_t decode_utf8(const unsigned char *s, size_t &len) {
  len = 0;
  uint32_t cp = s[0];
  size_t need = 0;
  if (cp < 0x80)
    need = 1;
  else if ((cp & 0xE0) == 0xC0)
    cp &= 0x1F, need = 2;
  else if ((cp & 0xF0) == 0xE0)
    cp &= 0x0F, need = 3;
  else if ((cp & 0xF8) == 0xF0)
    cp &= 0x07, need = 4;
  else
    return 0;
  for (size_t k = 1; k < need; k++) {
    if ((s[k] & 0xC0) != 0x80)
      return 0;
    cp = (cp << 6) | (s[k] & 0x3F);
  }
  len = need;
  return cp;
}

// What a chain of faces can draw for `text`. Each codepoint `covered`
// rejects becomes one visible replacement -- U+FFFD when the chain has it,
// otherwise '?' -- and a ZWJ emoji sequence or a flag pair becomes ONE
// replacement, not one per codepoint. Uncovered modifiers (above) vanish.
//
// Measurement and drawing both pass text through this, so a replacement is
// measured exactly as it is drawn and layout never sizes a glyph that will
// not appear. Returns false and leaves `out` alone when nothing changed,
// which is every all-ASCII string.
template <typename Covered>
inline bool visible_text(const char *text, Covered &&covered,
                         std::string &out) {
  if (!text)
    return false;
  size_t i = 0;
  while (text[i] && static_cast<unsigned char>(text[i]) < 0x80)
    i++;
  if (!text[i])
    return false;
  const auto *s = reinterpret_cast<const unsigned char *>(text);
  std::string res(text, i);
  const char *replacement = covered(0xFFFDu) ? "\xEF\xBF\xBD" : "?";
  bool changed = false, joining = false, half_flag = false;
  while (s[i]) {
    size_t len = 0;
    const uint32_t cp = decode_utf8(s + i, len);
    if (len == 0) { // malformed: pass the byte through, as both backends do
      res += text[i++];
      joining = false;
      continue;
    }
    const char *start = text + i;
    i += len;
    const bool have = cp < 0x80 || covered(cp);
    if (cp == 0x200D) { // a joiner continues the cluster either way
      joining = true;
      if (have)
        res.append(start, len);
      else
        changed = true;
      continue;
    }
    if (have) {
      res.append(start, len);
      joining = half_flag = false;
      continue;
    }
    changed = true;
    if (is_invisible_modifier(cp))
      continue;
    if (joining) {
      joining = false;
      continue;
    }
    if (cp >= 0x1F1E6 && cp <= 0x1F1FF) { // regional indicators pair up
      half_flag = !half_flag;
      if (!half_flag)
        continue;
    } else {
      half_flag = false;
    }
    res += replacement;
  }
  if (!changed)
    return false;
  out = std::move(res);
  return true;
}

} // namespace afterhours::font_coverage
