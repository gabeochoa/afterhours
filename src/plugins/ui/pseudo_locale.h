#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "ui_core_components.h"

namespace afterhours {
namespace ui {

// Pseudo-localization for layout stress. Real translations run longer than
// the source and some run right to left; shipping a screen that only ever
// saw its own short LTR strings is how clipped labels and crushed buttons
// reach users. These built-in fake languages let a developer flip every
// label in the app at once and watch what breaks:
//
//   DoubleWords  "Save changes" -> "Save changes Save changes"
//   RtlWords     "the quick fox" -> "fox quick the", and labels whose
//                alignment was never set align right instead of left.
//
// The transform runs where labels are built (apply_label), on the config's
// original text each frame, so measurement, wrapping and overflow all see
// the stressed string, and turning the mode off restores the original text
// exactly. Editable text (field values) is user data, not copy, and is not
// transformed.
enum class PseudoLocale {
  None,
  DoubleWords,
  RtlWords,
};

namespace detail {

inline std::vector<std::string_view> split_words(std::string_view line) {
  std::vector<std::string_view> words;
  size_t i = 0;
  while (i < line.size()) {
    while (i < line.size() && line[i] == ' ')
      i++;
    const size_t start = i;
    while (i < line.size() && line[i] != ' ')
      i++;
    if (i > start)
      words.push_back(line.substr(start, i - start));
  }
  return words;
}

inline std::string pseudo_localize_line(std::string_view line,
                                        PseudoLocale mode) {
  if (mode == PseudoLocale::DoubleWords) {
    if (line.empty())
      return {};
    return std::string(line) + " " + std::string(line);
  }
  if (mode == PseudoLocale::RtlWords) {
    const auto words = split_words(line);
    std::string out;
    for (size_t i = words.size(); i-- > 0;) {
      if (!out.empty())
        out += ' ';
      out += words[i];
    }
    return out;
  }
  return std::string(line);
}

} // namespace detail

// Per line, so a multi-line label keeps its line count: hard breaks are
// structure, not copy.
[[nodiscard]] inline std::string pseudo_localize(std::string_view text,
                                                 PseudoLocale mode) {
  if (mode == PseudoLocale::None)
    return std::string(text);
  std::string out;
  size_t start = 0;
  while (true) {
    const size_t nl = text.find('\n', start);
    const std::string_view line =
        text.substr(start, nl == std::string_view::npos ? nl : nl - start);
    out += detail::pseudo_localize_line(line, mode);
    if (nl == std::string_view::npos)
      break;
    out += '\n';
    start = nl + 1;
  }
  return out;
}

// Spans keep their colours: doubling appends a copy of the run list (the
// join carries a leading space on its first run), and RtlWords reverses
// the run order and the words inside each run.
[[nodiscard]] inline std::vector<TextSpan>
pseudo_localize_spans(const std::vector<TextSpan> &spans, PseudoLocale mode) {
  if (mode == PseudoLocale::None || spans.empty())
    return spans;
  if (mode == PseudoLocale::DoubleWords) {
    std::vector<TextSpan> out = spans;
    out.reserve(spans.size() * 2);
    for (size_t i = 0; i < spans.size(); i++) {
      TextSpan copy = spans[i];
      if (i == 0)
        copy.text = " " + copy.text;
      out.push_back(std::move(copy));
    }
    return out;
  }
  std::vector<TextSpan> out;
  out.reserve(spans.size());
  for (size_t i = spans.size(); i-- > 0;) {
    TextSpan copy = spans[i];
    // An edge space separates this run from its neighbour; after the
    // reversal the neighbour is on the other side, so the space moves too.
    const bool lead = !copy.text.empty() && copy.text.front() == ' ';
    const bool trail = !copy.text.empty() && copy.text.back() == ' ';
    copy.text = pseudo_localize(copy.text, PseudoLocale::RtlWords);
    if (copy.text.empty())
      copy.text = spans[i].text;
    else {
      if (trail)
        copy.text = " " + copy.text;
      if (lead)
        copy.text += ' ';
    }
    out.push_back(std::move(copy));
  }
  return out;
}

} // namespace ui
} // namespace afterhours
