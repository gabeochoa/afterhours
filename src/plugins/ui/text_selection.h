#pragma once

#include <list>
#include <unordered_map>

#include "text_units.h"

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
#include <cctype>
#include <cstddef>
#include <string>
#include <vector>

#include "../../ecs.h"
#include "../../measure_memo.h"
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

// Greedy word-wrap over styled runs. Over-wide words break between graphemes;
// '\n' is a hard break, so '\n\n' leaves a blank line.
//
// `measure(text, weight)` returns the pixel width of `text` in that weight.
// It is called on the largest same-weight stretch of a candidate line rather
// than per word -- measure_text spaces BETWEEN characters, so summing pieces
// loses one spacing per join and drifts further off with every piece. A line
// of uniform weight is therefore still measured in a single call, exactly as
// before weights existed; only a genuine weight boundary costs a join.
//
// The single wrapping primitive is wrap_ranges, over source byte offsets;
// this and wrap_text_to_width both slice its ranges, so plain and styled break
// identically by construction.
namespace wrap_memo {

// LRU on content + width, same shape as measure_memo. The draw path otherwise
// re-wraps unchanged text every frame, which hanabi measured as their biggest
// allocation site.
constexpr std::size_t kMaxEntries = 512;

struct Entry {
  std::uint64_t key;
  std::uint64_t generation = 0;
  std::vector<TextRunLine> lines;
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

// Everything the wrap depends on is in the key: the runs, the width, the
// face, the size and the spacing. Omitting the last three served one size's
// line breaks to every other size of the same text.
inline std::uint64_t key_for(const std::vector<TextSpan> &runs,
                             float max_width, float font_size, float spacing,
                             const std::string &font_name) {
  std::uint64_t h = 1469598103934665603ull;
  const auto mix = [&h](std::uint64_t v) {
    h ^= v;
    h *= 1099511628211ull;
  };
  for (const auto &r : runs) {
    for (unsigned char c : r.text)
      mix(c);
    mix(0xff); // run boundary, so ["ab"]["c"] and ["a"]["bc"] differ
    mix(static_cast<std::uint64_t>(r.weight));
    mix(r.color.r | (r.color.g << 8) | (r.color.b << 16) | (r.color.a << 24));
  }
  for (unsigned char c : font_name)
    mix(c);
  mix(static_cast<std::uint64_t>(max_width * 64.f));
  mix(static_cast<std::uint64_t>(font_size * 64.f));
  mix(static_cast<std::uint64_t>(spacing * 64.f));
  return h;
}

inline const std::vector<TextRunLine> *lookup(std::uint64_t key) {
  auto &idx = index();
  const auto it = idx.find(key);
  if (it == idx.end()) {
    misses()++;
    return nullptr;
  }
  if (it->second->generation != measure_memo::generation()) {
    lru().erase(it->second);
    idx.erase(it);
    misses()++;
    return nullptr;
  }
  lru().splice(lru().begin(), lru(), it->second);
  hits()++;
  return &it->second->lines;
}

inline const std::vector<TextRunLine> &store(std::uint64_t key,
                                             std::vector<TextRunLine> lines) {
  auto &idx = index();
  const auto dup = idx.find(key);
  if (dup != idx.end()) {
    lru().erase(dup->second);
    idx.erase(dup);
  }
  while (idx.size() >= kMaxEntries) {
    idx.erase(lru().back().key);
    lru().pop_back();
  }
  lru().push_front({key, measure_memo::generation(), std::move(lines)});
  idx[key] = lru().begin();
  return lru().front().lines;
}

inline void clear() {
  lru().clear();
  index().clear();
}

} // namespace wrap_memo

struct WrappedLine {
  size_t begin = 0;
  size_t end = 0;
  float width = 0.f;
};

template <typename MeasureRangeFn>
static inline std::vector<WrappedLine>
wrap_ranges(const std::string &text, float max_width,
            MeasureRangeFn &&measure_range) {
  std::vector<WrappedLine> lines;
  size_t source_begin = 0;
  while (true) {
    const size_t nl = text.find('\n', source_begin);
    const size_t source_end = nl == std::string::npos ? text.size() : nl;
    if (measure_range(source_begin, source_end) <= max_width) {
      lines.push_back({source_begin, source_end});
      if (nl == std::string::npos)
        return lines;
      source_begin = nl + 1;
      continue;
    }
    size_t begin = source_begin;
    size_t end = source_begin;
    size_t i = source_begin;
    while (i < source_end) {
      size_t word = i;
      while (word < source_end && text[word] == ' ')
        word++;
      if (word == source_end)
        break;
      size_t word_end = word;
      while (word_end < source_end && text[word_end] != ' ')
        word_end++;
      if (end > begin && measure_range(begin, word_end) > max_width) {
        lines.push_back({begin, end});
        begin = word;
      }
      if ((end <= begin || begin == word) &&
          measure_range(begin, word_end) > max_width) {
        const size_t base = begin;
        std::vector<UnitSpan> graphemes;
        split_graphemes(std::string_view(text).substr(base, word_end - base),
                        graphemes);
        size_t first = 0;
        while (measure_range(base + graphemes[first].begin, word_end) >
               max_width) {
          size_t lo = first + 1;
          size_t hi = graphemes.size();
          while (lo < hi) {
            const size_t mid = lo + (hi - lo + 1) / 2;
            if (measure_range(base + graphemes[first].begin,
                              base + graphemes[mid - 1].end) <= max_width)
              lo = mid;
            else
              hi = mid - 1;
          }
          if (lo == graphemes.size())
            break;
          lines.push_back({base + graphemes[first].begin,
                           base + graphemes[lo - 1].end});
          first = lo;
        }
        begin = base + graphemes[first].begin;
      }
      end = word_end;
      i = word_end;
    }
    lines.push_back({begin, source_end});
    if (nl == std::string::npos)
      return lines;
    source_begin = nl + 1;
  }
}

template <typename MeasureFn>
static inline std::vector<TextRunLine>
wrap_runs_to_width(const std::vector<TextSpan> &runs, float max_width,
                   MeasureFn &&measure) {
  std::string text;
  std::vector<size_t> run_ends;
  for (const auto &run : runs) {
    text += run.text;
    run_ends.push_back(text.size());
  }
  const auto run_at = [&](size_t pos) {
    return static_cast<size_t>(
        std::upper_bound(run_ends.begin(), run_ends.end(), pos) -
        run_ends.begin());
  };

  const auto measure_range = [&](size_t begin, size_t end) {
    float total = 0.f;
    size_t seg = begin;
    for (size_t r = run_at(begin); seg < end; r++) {
      const size_t stop = std::min(end, run_ends[r]);
      const colors::FontWeight weight = runs[run_at(seg)].weight;
      if (stop > seg &&
          (stop == end || runs[run_at(stop)].weight != weight)) {
        total += measure(text.substr(seg, stop - seg), weight);
        seg = stop;
      }
    }
    return total;
  };

  const auto same_style = [](const TextSpan &a, const TextSpan &b) {
    return a.color.r == b.color.r && a.color.g == b.color.g &&
           a.color.b == b.color.b && a.color.a == b.color.a &&
           a.weight == b.weight;
  };

  std::vector<TextRunLine> lines;
  for (const WrappedLine &range : wrap_ranges(text, max_width, measure_range)) {
    TextRunLine line;
    for (size_t pos = range.begin, r = run_at(range.begin); pos < range.end;
         r++) {
      const size_t stop = std::min(range.end, run_ends[r]);
      if (stop == pos)
        continue;
      const TextSpan part{text.substr(pos, stop - pos), runs[r].color,
                          runs[r].weight};
      if (!line.empty() && same_style(line.back(), part))
        line.back().text += part.text;
      else
        line.push_back(part);
      pos = stop;
    }
    lines.push_back(std::move(line));
  }
  return lines;
}

// Where each run of a laid-out line starts, and how wide the line is.
// Measured on the JOINED text the way wrap_runs_to_width measures it --
// maximal same-weight stretches, one call each -- because backend
// measurement charges between characters: summing per-run widths loses a
// spacing at every colour boundary and a monospace block's columns drift
// further with every run. Within a stretch a run starts at the stretch's
// own prefix measurement, so a run's offset is where its byte range sits
// in the measurement of the whole line.
template <typename MeasureFn>
static inline std::vector<float>
text_run_offsets(const TextRunLine &line, MeasureFn &&measure,
                 float &total_width) {
  std::vector<float> offsets(line.size(), 0.f);
  float stretch_x = 0.f;
  for (size_t i = 0; i < line.size();) {
    size_t j = i;
    std::string joined;
    while (j < line.size() && line[j].weight == line[i].weight) {
      joined += line[j].text;
      j++;
    }
    size_t prefix = 0;
    for (size_t k = i; k < j; k++) {
      offsets[k] = stretch_x + (prefix == 0 ? 0.f : measure(joined.substr(0, prefix), line[i].weight));
      prefix += line[k].text.size();
    }
    stretch_x += measure(joined, line[i].weight);
    i = j;
  }
  total_width = stretch_x;
  return offsets;
}

template <typename MeasureFn>
static inline std::vector<std::string>
wrap_text_to_width(const std::string &text, float max_width,
                   MeasureFn &&measure) {
  if (text.empty() || max_width <= 0.f)
    return {text};
  std::vector<std::string> lines;
  for (const WrappedLine &line :
       wrap_ranges(text, max_width, [&](size_t begin, size_t end) {
         return measure(text.substr(begin, end - begin));
       }))
    lines.push_back(text.substr(line.begin, line.end - line.begin));
  return lines;
}

template <typename MeasureFn>
static inline std::vector<WrappedLine>
wrap_lines(const std::string &text, float max_width, MeasureFn &&measure) {
  std::vector<WrappedLine> lines =
      max_width <= 0.f
          ? std::vector<WrappedLine>{{0, text.size()}}
          : wrap_ranges(text, max_width, [&](size_t begin, size_t end) {
              return measure(text.substr(begin, end - begin));
            });
  for (WrappedLine &line : lines)
    if (line.end > line.begin)
      line.width = measure(text.substr(line.begin, line.end - line.begin));
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
    const size_t next = next_grapheme_end(text, i);
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

inline constexpr float kMultiClickTime = 0.4f;
inline constexpr size_t kMultiClickDist = 3;

struct ClickStreak {
  float last_time = -1.f;
  size_t last_pos = 0;
  int count = 0;
};

inline int register_click(ClickStreak &streak, float now, size_t pos) {
  const size_t dist = pos > streak.last_pos ? pos - streak.last_pos
                                            : streak.last_pos - pos;
  if (streak.last_time >= 0.f && (now - streak.last_time) < kMultiClickTime &&
      dist <= kMultiClickDist)
    streak.count++;
  else
    streak.count = 1;
  streak.last_time = now;
  streak.last_pos = pos;
  return streak.count;
}

namespace detail_sel {
inline bool is_word_separator(char c) {
  return std::isspace(static_cast<unsigned char>(c)) ||
         std::ispunct(static_cast<unsigned char>(c));
}
}

inline Range word_range_at(const std::string &text, size_t pos) {
  if (text.empty())
    return Range{0, 0};
  pos = std::min(pos, text.size() - 1);
  if (detail_sel::is_word_separator(text[pos]))
    return Range{pos, pos + 1};
  size_t start = pos;
  while (start > 0 && !detail_sel::is_word_separator(text[start - 1]))
    start--;
  size_t end = pos;
  while (end < text.size() && !detail_sel::is_word_separator(text[end]))
    end++;
  return Range{start, end};
}

inline Range line_range_at(const std::string &text, size_t pos) {
  pos = std::min(pos, text.size());
  size_t start = pos;
  while (start > 0 && text[start - 1] != '\n')
    start--;
  size_t end = pos;
  while (end < text.size() && text[end] != '\n')
    end++;
  return Range{start, end};
}

} // namespace text_selection
} // namespace ui
} // namespace afterhours
