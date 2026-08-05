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

// Greedy word-wrap over styled runs. Over-wide words get their own line;
// '\n' is a hard break, so '\n\n' leaves a blank line.
//
// `measure(text, weight)` returns the pixel width of `text` in that weight.
// It is called on the largest same-weight stretch of a candidate line rather
// than per word -- measure_text spaces BETWEEN characters, so summing pieces
// loses one spacing per join and drifts further off with every piece. A line
// of uniform weight is therefore still measured in a single call, exactly as
// before weights existed; only a genuine weight boundary costs a join.
//
// The single wrapping primitive: wrap_text_to_width is one colourless
// regular-weight run through here, so plain and styled break identically by
// construction.
template <typename MeasureFn>
static inline std::vector<TextRunLine>
wrap_runs_to_width(const std::vector<TextSpan> &runs, float max_width,
                   MeasureFn &&measure) {
  // Hard breaks are split off first so soft wrapping only ever sees one source
  // line, and whitespace decisions stay local to it.
  std::vector<TextRunLine> source_lines{TextRunLine{}};
  for (const auto &run : runs) {
    size_t start = 0;
    while (true) {
      const size_t nl = run.text.find('\n', start);
      const size_t end = nl == std::string::npos ? run.text.size() : nl;
      if (end > start)
        source_lines.back().push_back(TextSpan{
            run.text.substr(start, end - start), run.color, run.weight});
      if (nl == std::string::npos)
        break;
      source_lines.push_back(TextRunLine{});
      start = nl + 1;
    }
  }

  // A chunk is a maximal run of either spaces or non-spaces, and may cross
  // colour boundaries: {"foo", red}, {"(bar)", blue} is ONE word in two
  // colours. Tokenising per-run instead inserted a space at every colour
  // change, so syntax-highlighted "foo(bar)" rendered as "foo (bar)".
  struct Chunk {
    TextRunLine parts;
    std::string text;
    bool is_space = false;
  };

  const auto same_style = [](const TextSpan &a, const TextSpan &b) {
    return a.color.r == b.color.r && a.color.g == b.color.g &&
           a.color.b == b.color.b && a.color.a == b.color.a &&
           a.weight == b.weight;
  };

  std::vector<TextRunLine> lines;
  TextRunLine current;
  std::string current_text;

  const auto push_parts = [&](const TextRunLine &parts) {
    for (const auto &part : parts) {
      // Merge only when colour AND weight match: coalescing across a weight
      // boundary would silently render the bold half regular.
      if (!current.empty() && same_style(current.back(), part))
        current.back().text += part.text;
      else
        current.push_back(part);
    }
  };

  // Width of a candidate line given as up to three part lists in order.
  // Adjacent parts of equal weight are measured together, so a uniform line
  // is a single measure call.
  const auto measure_candidate = [&](const TextRunLine &a,
                                     const TextRunLine &b,
                                     const TextRunLine &c) {
    float total = 0.f;
    std::string seg;
    bool have = false;
    colors::FontWeight seg_w = colors::FontWeight::Regular;
    const auto flush_seg = [&]() {
      if (have && !seg.empty())
        total += measure(seg, seg_w);
      seg.clear();
      have = false;
    };
    for (const TextRunLine *list : {&a, &b, &c}) {
      for (const auto &part : *list) {
        if (have && part.weight != seg_w)
          flush_seg();
        if (!have) {
          seg_w = part.weight;
          have = true;
        }
        seg += part.text;
      }
    }
    flush_seg();
    return total;
  };

  for (const auto &src : source_lines) {
    std::vector<Chunk> chunks;
    for (const auto &part : src) {
      size_t i = 0;
      while (i < part.text.size()) {
        const bool sp = part.text[i] == ' ';
        size_t j = i;
        while (j < part.text.size() && (part.text[j] == ' ') == sp)
          j++;
        const std::string frag = part.text.substr(i, j - i);
        if (!chunks.empty() && chunks.back().is_space == sp) {
          chunks.back().parts.push_back(
              TextSpan{frag, part.color, part.weight});
          chunks.back().text += frag;
        } else {
          chunks.push_back(Chunk{
              TextRunLine{TextSpan{frag, part.color, part.weight}}, frag, sp});
        }
        i = j;
      }
    }

    current.clear();
    current_text.clear();
    // Whitespace is held until we know whether the next word fits: if it does
    // the spacing is kept verbatim (so indentation and column alignment
    // survive), and if it does not the break consumes it.
    Chunk pending_ws;

    for (const auto &chunk : chunks) {
      if (chunk.is_space) {
        pending_ws.parts.insert(pending_ws.parts.end(), chunk.parts.begin(),
                                chunk.parts.end());
        pending_ws.text += chunk.text;
        continue;
      }
      const std::string candidate =
          current_text + pending_ws.text + chunk.text;
      if (!current_text.empty() &&
          measure_candidate(current, pending_ws.parts, chunk.parts) >
              max_width) {
        lines.push_back(current);
        current.clear();
        push_parts(chunk.parts);
        current_text = chunk.text;
      } else {
        // An empty line here means the start of the source line, not a wrap,
        // so its leading whitespace is indentation and is kept.
        push_parts(pending_ws.parts);
        push_parts(chunk.parts);
        current_text = candidate;
      }
      pending_ws = Chunk{};
    }
    // Trailing spaces are kept too, so hard-broken text round-trips byte for
    // byte through joined_text().
    if (!pending_ws.text.empty()) {
      push_parts(pending_ws.parts);
      current_text += pending_ws.text;
    }
    lines.push_back(current);
  }

  current.clear();
  return lines;
}

// Greedy word-wrap: split `text` into lines each no wider than `max_width`,
// using `measure` to size candidate lines. Words wider than max_width are
// placed on their own line (not character-split). Returns at least one line
// for non-empty input. `measure` returns the pixel width of a string.
template <typename MeasureFn>
static inline std::vector<std::string>
wrap_text_to_width(const std::string &text, float max_width,
                   MeasureFn &&measure) {
  if (text.empty() || max_width <= 0.f)
    return {text};
  std::vector<std::string> lines;
  // One regular-weight run, so the weight argument is never anything else and
  // callers here keep the plain measure(text) signature.
  const auto measure_span = [&](const std::string &s, colors::FontWeight) {
    return measure(s);
  };
  for (const auto &line : wrap_runs_to_width({TextSpan{text, Color{}}},
                                             max_width, measure_span)) {
    std::string joined;
    for (const auto &run : line)
      joined += run.text;
    lines.push_back(std::move(joined));
  }
  if (lines.empty())
    lines.push_back(text);
  return lines;
}

// Wrap-aware text measurement: the pixel width/height and line count of `text`
// laid out within `max_width` (wrap_text_to_width already honors hard newlines).
// The reusable answer to "how tall is this wrapped paragraph?" so apps stop
// hand-rolling height estimates. `measure2d(str)` returns the {w,h} of a line.
struct WrappedTextMetrics {
  float width = 0.f;  // widest resulting line
  float height = 0.f; // line_count * single-line height
  int line_count = 0;
};
template <typename Measure2DFn>
static inline WrappedTextMetrics
measure_wrapped(const std::string &text, float max_width,
                Measure2DFn &&measure2d) {
  std::vector<std::string> lines = wrap_text_to_width(
      text, max_width, [&](const std::string &s) { return measure2d(s).x; });
  WrappedTextMetrics m;
  m.line_count = static_cast<int>(lines.size());
  // Line height from a representative non-empty line (blank lines still occupy
  // one line of height); fall back to the whole-text measure.
  float line_h = 0.f;
  for (const auto &l : lines) {
    Vector2Type ls = measure2d(l);
    m.width = std::max(m.width, ls.x);
    if (!l.empty())
      line_h = std::max(line_h, ls.y);
  }
  if (line_h <= 0.f)
    line_h = measure2d(text).y;
  m.height = line_h * static_cast<float>(m.line_count);
  return m;
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
