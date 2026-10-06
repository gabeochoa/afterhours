#pragma once

// Where an OS keeps faces worth chaining behind a UI font (set_font_fallbacks
// / FontManager::set_fallbacks). Only paths that exist are returned, in
// preference order; load each with load_font_face(path, face_index), which
// handles collections.
//
// Only the macOS list is verified (on macOS 26): every face there loads and
// draws through stb_truetype. The Linux and Windows lists are the
// distributions' and Windows' documented install paths, untested here.

#include <filesystem>
#include <string>
#include <vector>

namespace afterhours::system_fonts {

struct Face {
  std::string path;
  int face_index = 0;
};

namespace detail {
inline std::vector<Face> existing(std::initializer_list<Face> candidates) {
  std::vector<Face> out;
  std::error_code ec;
  for (const Face &f : candidates)
    if (std::filesystem::is_regular_file(f.path, ec))
      out.push_back(f);
  return out;
}
} // namespace detail

// Faces covering Han ideographs and kana; the first is the usual choice.
// Hiragino has no Hangul, so a UI expecting Korean should also chain the
// first face that has it (Apple SD Gothic Neo here; Arial Unicode covers
// all three but is an older design).
inline std::vector<Face> cjk() {
#if defined(__APPLE__)
  return detail::existing({
      {"/System/Library/Fonts/\xE3\x83\x92\xE3\x83\xA9\xE3\x82\xAE\xE3\x83\x8E"
       "\xE8\xA7\x92\xE3\x82\xB4\xE3\x82\xB7\xE3\x83\x83\xE3\x82\xAF W3.ttc",
       0}, // Hiragino Sans W3 (Japanese)
      {"/System/Library/Fonts/Hiragino Sans GB.ttc", 0},
      {"/System/Library/Fonts/PingFang.ttc", 0}, // before macOS 12
      {"/System/Library/Fonts/AppleSDGothicNeo.ttc", 0}, // Hangul too
      {"/System/Library/Fonts/Supplemental/Arial Unicode.ttf", 0},
      {"/Library/Fonts/Arial Unicode.ttf", 0},
  });
#elif defined(_WIN32)
  return detail::existing({
      {"C:\\Windows\\Fonts\\YuGothR.ttc", 0}, // Yu Gothic
      {"C:\\Windows\\Fonts\\msyh.ttc", 0},    // Microsoft YaHei
      {"C:\\Windows\\Fonts\\malgun.ttf", 0},  // Malgun Gothic (Hangul)
      {"C:\\Windows\\Fonts\\simsun.ttc", 0},
  });
#else
  // Noto Sans CJK collections order JP, KR, SC, TC, HK; face 0 is JP.
  return detail::existing({
      {"/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc", 0},
      {"/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc", 0},
      {"/usr/share/fonts/google-noto-cjk/NotoSansCJK-Regular.ttc", 0},
      {"/usr/share/fonts/truetype/wqy/wqy-microhei.ttc", 0},
      {"/usr/share/fonts/wenquanyi/wqy-zenhei/wqy-zenhei.ttc", 0},
  });
#endif
}

// Emoji faces with OUTLINE glyphs, which stb_truetype can draw (in one
// colour, like any glyph). Colour-bitmap emoji -- Apple Color Emoji (sbix),
// Noto Color Emoji (CBDT) -- have no outlines and draw nothing, so they are
// not listed. macOS ships no outline emoji face: this is empty there, and
// emoji draw as one replacement glyph per sequence (font_coverage::
// visible_text). Segoe UI Emoji's COLR base glyphs are outlines.
inline std::vector<Face> emoji() {
#if defined(__APPLE__)
  return {};
#elif defined(_WIN32)
  return detail::existing({{"C:\\Windows\\Fonts\\seguiemj.ttf", 0}});
#else
  return detail::existing({
      {"/usr/share/fonts/truetype/noto/NotoEmoji-Regular.ttf", 0},
      {"/usr/share/fonts/noto/NotoEmoji-Regular.ttf", 0},
      {"/usr/share/fonts/google-noto-emoji/NotoEmoji-Regular.ttf", 0},
  });
#endif
}

// Symbols outside a UI face (dingbats, arrows, U+FFFD) that a chain often
// lacks; Arial Unicode has U+FFFD, which makes replacements read as such.
inline std::vector<Face> symbols() {
#if defined(__APPLE__)
  return detail::existing({
      {"/System/Library/Fonts/Supplemental/Arial Unicode.ttf", 0},
      {"/Library/Fonts/Arial Unicode.ttf", 0},
      {"/System/Library/Fonts/Apple Symbols.ttf", 0},
  });
#elif defined(_WIN32)
  return detail::existing({{"C:\\Windows\\Fonts\\seguisym.ttf", 0}});
#else
  return detail::existing({
      {"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 0},
      {"/usr/share/fonts/dejavu/DejaVuSans.ttf", 0},
  });
#endif
}

} // namespace afterhours::system_fonts
