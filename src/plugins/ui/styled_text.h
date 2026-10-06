#pragma once

// Styled text laid out once, for everything that needs it.
//
// layout_styled_text turns runs (TextSpan: text, colour, weight, face, size)
// into placed runs. Wrapping, ellipsis, line width, per-run x and the
// selection geometry in text_selection.h all come from detail::line_x_at
// over the same `metrics`, and both label renderers draw the placed runs as
// given -- so what is measured is what is drawn, at any UI scale and with
// fallback glyphs mixed in (the backend resolves those inside measure_text
// and draw_text_ex alike).
//
// `metrics` needs three calls, each per run style:
//   metrics(text, span) -> advance width
//   metrics.height(span) -> line height
//   metrics.baseline(span) -> top of line to baseline
// StyledFonts below answers them from a FontManager; tests may pass their own.

#include <algorithm>
#include <string>
#include <vector>

#include "text_selection.h"
#include "ui_core_components.h"

namespace afterhours {
namespace ui {

// One run ready to draw: `rect.x/y` is where draw_text_ex puts its top left,
// `rect.width` its advance, `rect.height` its own line height.
struct PlacedRun {
  std::string text;
  std::string font_name;
  float font_size = 0.f;
  Color color{};
  RectangleType rect{};
  size_t line = 0;
};

struct StyledLayout {
  std::vector<detail::TextRunLine> lines;
  std::vector<float> line_top; // relative to the block's top
  std::vector<float> line_height;
  std::vector<PlacedRun> runs; // relative to the block's top left
  float width = 0.f;           // widest line
  float height = 0.f;          // all lines
};

// FontManager-backed metrics: a run's face is span_face(base, base_weight),
// its size font_size * size_scale.
struct StyledFonts {
  const FontManager &fm;
  std::string base;
  colors::FontWeight base_weight = colors::FontWeight::Regular;
  float font_size = 0.f;
  float spacing = 1.f;

  [[nodiscard]] std::string face(const TextSpan &s) const {
    return fm.span_face(base, base_weight, s);
  }
  [[nodiscard]] float size(const TextSpan &s) const {
    return font_size * s.size_scale;
  }
  float operator()(const std::string &text, const TextSpan &s) const {
    return measure_text(fm.get_font(face(s)), text.c_str(), size(s), spacing)
        .x;
  }
  [[nodiscard]] float height(const TextSpan &s) const {
    return measure_text(fm.get_font(face(s)), "Ag", size(s), spacing).y;
  }
  [[nodiscard]] float baseline(const TextSpan &s) const {
    return font_baseline(fm.get_font(face(s)), size(s));
  }
  // What the wrap depends on beyond the runs, width, size and spacing.
  [[nodiscard]] std::string memo_name() const {
    return base + "@" + std::to_string(static_cast<int>(base_weight));
  }
};

// Lines of `runs`: soft-wrapped at `wrap_width` (pass a huge width for hard
// breaks only), each then cut to `ellipsis_width` with "..." when that is
// positive. Lines share a left edge; runs in a line share a baseline, the
// lowest any of them needs, and a line is as tall as its tallest run.
// `align_width` positions each line for Center/Right.
template <typename Metrics>
inline StyledLayout
layout_styled_text(const std::vector<TextSpan> &runs, const Metrics &metrics,
                   float wrap_width, float ellipsis_width = 0.f,
                   TextAlignment alignment = TextAlignment::Left,
                   float align_width = 0.f) {
  StyledLayout out;
  // Memoised when the metrics can name themselves: labels re-lay out
  // unchanged text every frame, and the wrap is the allocation-heavy part.
  if constexpr (requires { metrics.memo_name(); }) {
    const std::uint64_t key =
        detail::wrap_memo::key_for(runs, wrap_width, metrics.font_size,
                                   metrics.spacing, metrics.memo_name());
    const auto *cached = detail::wrap_memo::lookup(key);
    out.lines = cached ? *cached
                       : detail::wrap_memo::store(
                             key, detail::wrap_runs_to_width(runs, wrap_width,
                                                             metrics));
  } else {
    out.lines = detail::wrap_runs_to_width(runs, wrap_width, metrics);
  }
  if (ellipsis_width > 0.f)
    for (auto &line : out.lines)
      line = detail::ellipsize_line(line, ellipsis_width, metrics);
  // An empty line ("\n\n") is as tall as the first run's style.
  const TextSpan blank = runs.empty() ? TextSpan{} : runs.front();
  float y = 0.f;
  for (size_t li = 0; li < out.lines.size(); li++) {
    const auto &line = out.lines[li];
    float base = line.empty() ? metrics.baseline(blank) : 0.f;
    float h = line.empty() ? metrics.height(blank) : 0.f;
    for (const auto &run : line)
      base = std::max(base, metrics.baseline(run));
    for (const auto &run : line)
      h = std::max(h, base - metrics.baseline(run) + metrics.height(run));
    float w = 0.f;
    const std::vector<float> offsets =
        detail::text_run_offsets(line, metrics, w);
    float x = 0.f;
    if (alignment == TextAlignment::Center)
      x = std::max(0.f, (align_width - w) / 2.f);
    else if (alignment == TextAlignment::Right)
      x = std::max(0.f, align_width - w);
    for (size_t ri = 0; ri < line.size(); ri++) {
      const auto &run = line[ri];
      out.runs.push_back(PlacedRun{
          run.text, std::string{}, 0.f, run.color,
          RectangleType{x + offsets[ri], y + base - metrics.baseline(run),
                        metrics(run.text, run), metrics.height(run)},
          li});
      if constexpr (requires { metrics.face(run); metrics.size(run); }) {
        out.runs.back().font_name = metrics.face(run);
        out.runs.back().font_size = metrics.size(run);
      }
    }
    out.line_top.push_back(y);
    out.line_height.push_back(h);
    out.width = std::max(out.width, w);
    y += h;
  }
  out.height = y;
  return out;
}

// Selection rects over a styled layout: text_selection::selection_rects,
// but each line at its own top and height, which differ once runs change
// size. `origin` is where the block's top left was drawn.
template <typename Metrics>
inline std::vector<RectangleType>
styled_selection_rects(const StyledLayout &layout, text_selection::Range range,
                       Vector2Type origin, const Metrics &metrics) {
  std::vector<RectangleType> rects;
  const auto starts = text_selection::line_start_offsets(layout.lines);
  for (size_t row = 0; row < layout.lines.size(); row++) {
    const auto &line = layout.lines[row];
    const size_t len = text_selection::line_text(line).size();
    const size_t from = std::max(range.start, starts[row]) - starts[row];
    const size_t to = std::min(range.end, starts[row] + len);
    if (range.empty() || to <= starts[row] + from)
      continue;
    const float x0 = detail::line_x_at(line, from, metrics);
    const float x1 = detail::line_x_at(line, to - starts[row], metrics);
    float x = 0.f; // the line's alignment offset: where its first run went
    for (const auto &run : layout.runs)
      if (run.line == row) {
        x = run.rect.x;
        break;
      }
    rects.push_back(RectangleType{origin.x + x + x0,
                                  origin.y + layout.line_top[row], x1 - x0,
                                  layout.line_height[row]});
  }
  return rects;
}

} // namespace ui
} // namespace afterhours
