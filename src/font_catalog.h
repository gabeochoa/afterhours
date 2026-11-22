#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "font_coverage.h"
#include "warn_once.h"

namespace afterhours::font_catalog {

struct FontFaceInfo {
  std::string path;
  int face_index = 0;
  bool collection = false;
  std::string family; // typographic family (name ID 16) else family (1)
  std::string style;  // typographic style (17) else subfamily (2), e.g. Bold
};

struct Catalog {
  std::vector<FontFaceInfo> faces;
  // False = no platform scan; empty + true = scanned, found none.
  bool supported = true;
};

namespace detail {
using font_coverage::u16;
using font_coverage::u32;

inline std::string utf16be_to_utf8(const unsigned char *p, size_t bytes) {
  std::string out;
  for (size_t i = 0; i + 1 < bytes; i += 2) {
    uint32_t cp = (static_cast<uint32_t>(p[i]) << 8) | p[i + 1];
    if (cp >= 0xD800 && cp <= 0xDBFF && i + 3 < bytes) {
      const uint32_t lo = (static_cast<uint32_t>(p[i + 2]) << 8) | p[i + 3];
      if (lo >= 0xDC00 && lo <= 0xDFFF) {
        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
        i += 2;
      }
    }
    if (cp < 0x80) {
      out += static_cast<char>(cp);
    } else if (cp < 0x800) {
      out += static_cast<char>(0xC0 | (cp >> 6));
      out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
      out += static_cast<char>(0xE0 | (cp >> 12));
      out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
      out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
      out += static_cast<char>(0xF0 | (cp >> 18));
      out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
      out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
      out += static_cast<char>(0x80 | (cp & 0x3F));
    }
  }
  return out;
}

inline const unsigned char *table_at(const unsigned char *d, size_t n,
                                     size_t face, const char *tag) {
  if (face + 12 > n)
    return nullptr;
  const uint16_t tables = u16(d + face + 4);
  for (uint16_t i = 0; i < tables; i++) {
    const size_t rec = face + 12 + static_cast<size_t>(i) * 16;
    if (rec + 16 > n || std::memcmp(d + rec, tag, 4) != 0)
      continue;
    const size_t off = u32(d + rec + 8);
    return off < n ? d + off : nullptr;
  }
  return nullptr;
}

// One name-table string for `id`; prefers Unicode over MacRoman.
inline std::string name_string(const unsigned char *d, size_t n, size_t face,
                               uint16_t id) {
  if (face >= n)
    return {};
  const unsigned char *name = table_at(d, n, face, "name");
  if (!name || static_cast<size_t>(name - d) + 6 > n)
    return {};
  const size_t base = static_cast<size_t>(name - d);
  const uint16_t count = u16(d + base + 2);
  const size_t storage = base + u16(d + base + 4);
  std::string best;
  int best_score = -1;
  for (uint16_t i = 0; i < count; i++) {
    const size_t rec = base + 6 + static_cast<size_t>(i) * 12;
    if (rec + 12 > n || u16(d + rec + 6) != id)
      continue;
    const uint16_t platform = u16(d + rec);
    const size_t off = storage + u16(d + rec + 10);
    const size_t len = u16(d + rec + 8);
    if (off + len > n)
      continue;
    const uint16_t language = u16(d + rec + 4);
    const int score = ((platform == 3 || platform == 0) ? 2
                       : platform == 1                  ? 1
                                                        : 0) *
                          2 +
                      (language == 0 || language == 0x0409 ? 1 : 0);
    if (score <= best_score)
      continue;
    best = platform == 1
               ? std::string(reinterpret_cast<const char *>(d + off), len)
               : utf16be_to_utf8(d + off, len);
    best_score = score;
  }
  return best;
}

inline bool is_font_file(const std::filesystem::path &p) {
  std::string ext = p.extension().string();
  for (char &c : ext)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return ext == ".ttf" || ext == ".otf" || ext == ".ttc";
}
}

[[nodiscard]] inline std::vector<FontFaceInfo>
faces_in_bytes(const unsigned char *d, size_t n, const std::string &path = {}) {
  std::vector<FontFaceInfo> out;
  if (!d || n < 12)
    return out;
  const bool collection = std::memcmp(d, "ttcf", 4) == 0;
  const bool sfnt = detail::u32(d) == 0x00010000 ||
                    std::memcmp(d, "OTTO", 4) == 0 ||
                    std::memcmp(d, "true", 4) == 0;
  if (!collection && !sfnt)
    return out;
  const uint32_t count = collection ? detail::u32(d + 8) : 1;
  if (count == 0 || count > 1024)
    return out;
  for (uint32_t i = 0; i < count; i++) {
    const size_t face =
        collection ? detail::u32(d + 12 + static_cast<size_t>(i) * 4) : 0;
    if (face + 6 > n)
      continue;
    FontFaceInfo info;
    info.path = path;
    info.face_index = static_cast<int>(i);
    info.collection = collection;
    info.family = detail::name_string(d, n, face, 16);
    if (info.family.empty())
      info.family = detail::name_string(d, n, face, 1);
    info.style = detail::name_string(d, n, face, 17);
    if (info.style.empty())
      info.style = detail::name_string(d, n, face, 2);
    out.push_back(std::move(info));
  }
  return out;
}

[[nodiscard]] inline std::vector<FontFaceInfo>
faces_for_file(const std::string &path) {
  std::vector<unsigned char> bytes;
  if (!font_coverage::read_file(path.c_str(), bytes))
    return {};
  return faces_in_bytes(bytes.data(), bytes.size(), path);
}

// Faces in file at `path`; 0 when unreadable or not a font.
[[nodiscard]] inline int face_count_for_file(const std::string &path) {
  return static_cast<int>(faces_for_file(path).size());
}

[[nodiscard]] inline Catalog
list_fonts_in(const std::vector<std::string> &dirs) {
  Catalog catalog;
  for (const std::string &dir : dirs) {
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec))
      continue;
    for (std::filesystem::recursive_directory_iterator
             it(dir,
                std::filesystem::directory_options::skip_permission_denied,
                ec),
             end;
         it != end; it.increment(ec)) {
      if (ec || !it->is_regular_file(ec) || !detail::is_font_file(it->path()))
        continue;
      auto faces = faces_for_file(it->path().string());
      catalog.faces.insert(catalog.faces.end(), faces.begin(), faces.end());
    }
  }
  std::sort(catalog.faces.begin(), catalog.faces.end(),
            [](const FontFaceInfo &a, const FontFaceInfo &b) {
              if (a.family != b.family)
                return a.family < b.family;
              if (a.style != b.style)
                return a.style < b.style;
              if (a.path != b.path)
                return a.path < b.path;
              return a.face_index < b.face_index;
            });
  catalog.faces.erase(
      std::unique(catalog.faces.begin(), catalog.faces.end(),
                  [](const FontFaceInfo &a, const FontFaceInfo &b) {
                    return a.path == b.path && a.face_index == b.face_index;
                  }),
      catalog.faces.end());
  return catalog;
}

// Standard per-platform font directories that list_system_fonts() scans.
[[nodiscard]] inline std::vector<std::string> system_font_dirs() {
  std::vector<std::string> dirs;
  const auto add_home = [&dirs](const char *suffix) {
    if (const char *home = std::getenv("HOME"))
      dirs.push_back(std::string(home) + suffix);
  };
#if defined(__APPLE__)
  dirs = {"/System/Library/Fonts", "/System/Library/Fonts/Supplemental",
          "/Library/Fonts"};
  add_home("/Library/Fonts");
#elif defined(_WIN32)
  if (const char *windir = std::getenv("WINDIR"))
    dirs.push_back(std::string(windir) + "\\Fonts");
  if (const char *local = std::getenv("LOCALAPPDATA"))
    dirs.push_back(std::string(local) + "\\Microsoft\\Windows\\Fonts");
#elif defined(__linux__)
  dirs = {"/usr/share/fonts", "/usr/local/share/fonts"};
  add_home("/.local/share/fonts");
  add_home("/.fonts");
#endif
  return dirs;
}

[[nodiscard]] inline Catalog list_system_fonts() {
#if defined(__APPLE__) || defined(_WIN32) || defined(__linux__)
  return list_fonts_in(system_font_dirs());
#else
  warn_once(0, "@notimplemented font_catalog::list_system_fonts on this "
               "platform; returning an unsupported, empty catalog");
  return Catalog{{}, false};
#endif
}

}
