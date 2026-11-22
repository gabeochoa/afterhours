#include <cassert>
#include <cstdio>
#include <string>

#include "../../../../ah.h"
#include "../../../../src/plugins/ui/text_selection.h"

int main(int, char **) {
  const auto monospace = [](const std::string &s) {
    return static_cast<float>(s.size()) * 8.f;
  };
  const std::string text =
      "Saved to /Users/someone/Library/Application_Support/app/settings.json";

  const auto lines = afterhours::ui::detail::wrap_lines(text, 160.f, monospace);
  for (const auto &line : lines) {
    std::printf("[%2zu, %2zu) %5.1fpx \"%s\"\n", line.begin, line.end,
                static_cast<double>(line.width),
                text.substr(line.begin, line.end - line.begin).c_str());
    assert(line.width <= 160.f);
  }
  std::printf("wrap_long_words: ok (%zu lines)\n", lines.size());
  return 0;
}
