#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

#include "../../../../ah.h"
#include "../../../../src/font_helper.h"
#include "../../../../src/plugins/ui/text_units.h"

int main(int, char **) {
  using afterhours::ui::ellipsize_text;
  using afterhours::ui::fit_text_prefix;

  afterhours::set_measure_text_fn([](const char *s, float size, float) {
    return Vector2Type{static_cast<float>(std::strlen(s)) * size * 0.5f, size};
  });

  const afterhours::Font font{};
  const float font_size = 16.f;
  const float column = 120.f;
  const auto width = [&](const std::string &s) {
    return afterhours::measure_text(font, s.c_str(), font_size, 1.f).x;
  };

  const char *rows[] = {"Play", "Settings and controls",
                        ("Caf\xC3\xA9 cr\xC3\xA8me br\xC3\xBBl\xC3\xA9" "e")};
  for (const char *row : rows) {
    const std::string shown = ellipsize_text(row, column, width);
    std::printf("%-28s -> \"%s\" (%.0fpx)\n", row, shown.c_str(),
                static_cast<double>(width(shown)));
    assert(width(shown) <= column);
  }

  const std::string title = "oncall sweep finished";
  const size_t fits = fit_text_prefix(title, column, width);
  std::printf("\"%s\" fits %zu of %zu bytes in %.0fpx\n", title.c_str(), fits,
              title.size(), static_cast<double>(column));

  assert(ellipsize_text("Play", column, width) == "Play");
  assert(ellipsize_text(title, column, width, "\xE2\x80\xA6").size() <
         title.size());
  std::printf("ellipsize_text: ok\n");
  return 0;
}
