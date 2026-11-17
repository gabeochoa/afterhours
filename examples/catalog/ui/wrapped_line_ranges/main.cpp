#include <algorithm>
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

#include "../../../../ah.h"
#include "../../../../src/plugins/ui/text_selection.h"

int main(int, char **) {
  using afterhours::ui::detail::wrap_lines;
  using afterhours::ui::detail::WrappedLine;

  const std::string text =
      "The quick brown fox jumps over the lazy dog.\nCaf\xC3\xA9 menus "
      "list cr\xC3\xA8me br\xC3\xBBl\xC3\xA9" "e under desserts.";
  const auto monospace = [](const std::string &s) {
    return static_cast<float>(s.size()) * 8.f;
  };

  const std::vector<WrappedLine> lines = wrap_lines(text, 160.f, monospace);
  for (size_t i = 0; i < lines.size(); i++) {
    const WrappedLine &line = lines[i];
    std::printf("line %zu [%2zu, %2zu) width %5.1f  \"%s\"\n", i, line.begin,
                line.end, static_cast<double>(line.width),
                text.substr(line.begin, line.end - line.begin).c_str());
  }

  const auto line_of = [&](size_t offset) {
    size_t found = 0;
    for (size_t i = 0; i < lines.size() && lines[i].begin <= offset; i++)
      found = i;
    return found;
  };

  const std::string needle = "lazy dog";
  const size_t match = text.find(needle);
  assert(match != std::string::npos);
  const size_t match_end = match + needle.size();
  const size_t first = line_of(match);
  const size_t last = line_of(match_end - 1);
  std::printf("\"%s\" at byte %zu spans lines %zu..%zu\n", needle.c_str(),
              match, first, last);
  for (size_t i = first; i <= last; i++) {
    const size_t begin = std::max(match, lines[i].begin);
    const size_t end = std::min(match_end, lines[i].end);
    std::printf("  highlight line %zu bytes [%zu, %zu)\n", i, begin, end);
  }

  assert(lines.size() > 2);
  assert(text[lines.back().end - 1] == '.');
  std::printf("wrapped_line_ranges: ok (%zu lines)\n", lines.size());
  return 0;
}
