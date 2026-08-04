#pragma once

// Text selection geometry, with no graphics backend.
//
// Everything here works off `measure`, a callable from string to pixel width,
// rather than a Font. That keeps the header buildable under any backend --
// including the recording one used by the tests, where text_input's own caret
// and selection code cannot go, since that code needs a FontManager the test
// harness has no way to construct.
//
// Offsets are byte offsets into the JOINED text of the wrapped lines: the
// lines concatenated with '\n' between them, which is what joined_text()
// returns. For content whose line breaks are all hard ('\n' in the source, as
// in a diff) that is byte-identical to the source text. Soft wrapping inserts
// breaks the source did not have -- see the note on joined_text.

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include "../../ecs.h"
#include "ui_core_components.h"

namespace afterhours {
namespace ui {

// Byte length of the UTF-8 character starting at `pos`. Lives here rather than
// in text_input/utils.h so that selection geometry does not have to depend on
// the editing module; text_input pulls it back in with a using-declaration.
inline size_t utf8_char_length(const std::string &str, size_t pos) {
  if (pos >= str.size())
    return 0;
  unsigned char c = static_cast<unsigned char>(str[pos]);
  if ((c & 0x80) == 0)
    return 1; // ASCII
  if ((c & 0xE0) == 0xC0)
    return 2; // 2-byte
  if ((c & 0xF0) == 0xE0)
    return 3; // 3-byte (CJK)
  if ((c & 0xF8) == 0xF0)
    return 4; // 4-byte (emoji)
  return 1;
}

namespace detail {

// One visual line, as coloured runs.
using TextRunLine = std::vector<TextSpan>;

// Greedy word-wrap over coloured runs. Over-wide words get their own line;
// '\n' is a hard break, so '\n\n' leaves a blank line.
//
// `measure` sizes the whole joined candidate line, not a sum of per-word
// widths -- measure_text spaces BETWEEN characters, so a per-word sum drifts
// further off with every word.
//
// The single wrapping primitive: wrap_text_to_width is one colourless run
// through here, so plain and styled break identically by construction.
template <typename MeasureFn>
static inline std::vector<TextRunLine>
wrap_runs_to_width(const std::vector<TextSpan> &runs, float max_width,
                   MeasureFn &&measure) {
  // A word is a run of non-space characters, which may cross colour
  // boundaries: {"foo", red}, {"(bar)", blue} is ONE word in two colours, not
  // two words. Tokenising per-run instead used to insert a space at every
  // colour change, so syntax-highlighted "foo(bar)" rendered as "foo (bar)".
  struct Word {
    TextRunLine parts;
    std::string text;
    bool empty() const { return text.empty(); }
  };

  std::vector<Word> words;   // one entry per word; a null word marks a break
  std::vector<bool> breaks;  // hard break AFTER words[i]
  Word pending;

  const auto end_word = [&]() {
    words.push_back(pending);
    breaks.push_back(false);
    pending = Word{};
  };
  const auto hard_break = [&]() {
    if (!pending.empty())
      end_word();
    else {
      words.push_back(Word{});
      breaks.push_back(false);
    }
    breaks.back() = true;
  };

  for (const auto &run : runs) {
    size_t i = 0;
    while (i < run.text.size()) {
      const char c = run.text[i];
      if (c == '\n') {
        hard_break();
        i++;
        continue;
      }
      if (c == ' ') {
        if (!pending.empty())
          end_word();
        i++;
        continue;
      }
      size_t j = i;
      while (j < run.text.size() && run.text[j] != ' ' && run.text[j] != '\n')
        j++;
      const std::string frag = run.text.substr(i, j - i);
      pending.parts.push_back(TextSpan{frag, run.color});
      pending.text += frag;
      i = j;
    }
    // Run ended without a separator: the next run continues the SAME word, so
    // `pending` is intentionally left open across the boundary.
  }
  if (!pending.empty())
    end_word();

  const auto same_color = [](const Color &a, const Color &b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
  };

  std::vector<TextRunLine> lines;
  TextRunLine current;
  std::string current_text;

  const auto flush = [&]() {
    lines.push_back(current);
    current.clear();
    current_text.clear();
  };
  const auto push_parts = [&](const TextRunLine &parts, bool with_space) {
    bool first = true;
    for (const auto &part : parts) {
      const std::string text =
          (first && with_space) ? " " + part.text : part.text;
      if (!current.empty() && same_color(current.back().color, part.color))
        current.back().text += text;
      else
        current.push_back(TextSpan{text, part.color});
      first = false;
    }
  };

  for (size_t w = 0; w < words.size(); w++) {
    const Word &word = words[w];
    if (!word.empty()) {
      if (current_text.empty()) {
        current_text = word.text;
        push_parts(word.parts, false);
      } else {
        const std::string candidate = current_text + " " + word.text;
        if (measure(candidate) > max_width) {
          flush();
          current_text = word.text;
          push_parts(word.parts, false);
        } else {
          current_text = candidate;
          push_parts(word.parts, true);
        }
      }
    }
    if (breaks[w])
      flush();
  }
  if (!current.empty() || lines.empty())
    flush();
  return lines;
}

} // namespace detail

namespace text_selection {

// An ordered byte range. start <= end always; use Selection below for the
// anchor/cursor pair that can be dragged in either direction.
struct Range {
  size_t start = 0;
  size_t end = 0;

  [[nodiscard]] bool empty() const { return start >= end; }
  [[nodiscard]] size_t length() const { return empty() ? 0 : end - start; }
};

// Anchor is where the drag began, cursor is where it is now, so a selection
// dragged right-to-left is anchor > cursor. Order only at the point of use.
struct Selection {
  size_t anchor = 0;
  size_t cursor = 0;

  [[nodiscard]] bool has_selection() const { return anchor != cursor; }
  [[nodiscard]] Range range() const {
    return Range{std::min(anchor, cursor), std::max(anchor, cursor)};
  }
  void collapse_to(size_t pos) { anchor = cursor = pos; }
  void set_cursor(size_t pos, bool extend) {
    cursor = pos;
    if (!extend)
      anchor = pos;
  }
  void select_all(size_t length) {
    anchor = 0;
    cursor = length;
  }
};

// The text of a wrapped line, runs concatenated.
inline std::string line_text(const detail::TextRunLine &line) {
  std::string out;
  for (const auto &run : line)
    out += run.text;
  return out;
}

// The full text these lines represent, lines joined by '\n'. All offsets in
// this header index into this string.
//
// For hard-broken content this reproduces the source exactly. Soft wrapping
// does not round-trip: the wrap consumes the space it broke at, so a soft
// break becomes '\n' here where the source had ' '. A caller that needs the
// original bytes back should either wrap at a width nothing reaches (pass a
// huge max_width, leaving only hard breaks) or keep its own mapping.
inline std::string joined_text(const std::vector<detail::TextRunLine> &lines) {
  std::string out;
  for (size_t i = 0; i < lines.size(); i++) {
    if (i)
      out += '\n';
    out += line_text(lines[i]);
  }
  return out;
}

// Byte offset of the first character of each line, into joined_text().
inline std::vector<size_t>
line_start_offsets(const std::vector<detail::TextRunLine> &lines) {
  std::vector<size_t> starts;
  starts.reserve(lines.size());
  size_t off = 0;
  for (size_t i = 0; i < lines.size(); i++) {
    starts.push_back(off);
    off += line_text(lines[i]).size() + 1; // +1 for the '\n'
  }
  return starts;
}

namespace detail_sel {
// Byte offset within `text` whose left edge is nearest `x`. Walks UTF-8
// characters so a multi-byte glyph is never split.
template <typename MeasureFn>
inline size_t offset_nearest_x(const std::string &text, float x,
                               MeasureFn &&measure) {
  size_t best = 0;
  float best_dist = std::abs(x);
  for (size_t i = 0; i < text.size();) {
    const size_t len = utf8_char_length(text, i);
    const size_t next = i + (len > 0 ? len : 1);
    const float w = measure(text.substr(0, next));
    const float dist = std::abs(x - w);
    if (dist < best_dist) {
      best_dist = dist;
      best = next;
    }
    i = next;
  }
  return best;
}
} // namespace detail_sel

// Which byte a point lands on. `local` is relative to the text block's top
// left. Above the first line clamps to 0; below the last clamps to the end,
// which is what a drag that runs off the bottom should do.
template <typename MeasureFn>
inline size_t byte_offset_at(const std::vector<detail::TextRunLine> &lines,
                             Vector2Type local, float line_height,
                             MeasureFn &&measure) {
  if (lines.empty() || line_height <= 0.f)
    return 0;

  const auto starts = line_start_offsets(lines);
  int row = static_cast<int>(local.y / line_height);
  row = std::clamp(row, 0, static_cast<int>(lines.size()) - 1);

  const std::string text = line_text(lines[static_cast<size_t>(row)]);
  const size_t within =
      detail_sel::offset_nearest_x(text, local.x, measure);
  return starts[static_cast<size_t>(row)] + within;
}

// One rect per line the range covers. A range inside a single line yields one
// rect; a range spanning three yields three, with the first and last partial.
// `origin` is the block's top left.
template <typename MeasureFn>
inline std::vector<RectangleType>
selection_rects(const std::vector<detail::TextRunLine> &lines, Range range,
                Vector2Type origin, float line_height, MeasureFn &&measure) {
  std::vector<RectangleType> rects;
  if (range.empty() || lines.empty())
    return rects;

  const auto starts = line_start_offsets(lines);
  for (size_t row = 0; row < lines.size(); row++) {
    const std::string text = line_text(lines[row]);
    const size_t line_start = starts[row];
    const size_t line_end = line_start + text.size();

    // Skip lines entirely before or after the range. The '\n' itself is not
    // drawn, so a range covering it contributes nothing on its own.
    if (line_end <= range.start || line_start >= range.end)
      continue;

    const size_t from = range.start > line_start ? range.start - line_start : 0;
    const size_t to =
        range.end < line_end ? range.end - line_start : text.size();
    if (to <= from)
      continue;

    const float x0 = measure(text.substr(0, from));
    const float x1 = measure(text.substr(0, to));
    rects.push_back(RectangleType{origin.x + x0,
                                  origin.y + line_height *
                                                 static_cast<float>(row),
                                  x1 - x0, line_height});
  }
  return rects;
}

// The selected text, for the clipboard.
inline std::string substring(const std::vector<detail::TextRunLine> &lines,
                             Range range) {
  if (range.empty())
    return "";
  const std::string all = joined_text(lines);
  if (range.start >= all.size())
    return "";
  return all.substr(range.start, std::min(range.length(),
                                          all.size() - range.start));
}

} // namespace text_selection
} // namespace ui
} // namespace afterhours
