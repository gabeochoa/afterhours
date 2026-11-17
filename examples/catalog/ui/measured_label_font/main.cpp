#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

#include "../../../../ah.h"
#include "../../../../src/plugins/autolayout.h"

using namespace afterhours;
using namespace afterhours::ui;

int main(int, char **) {
  auto &fonts_entity = EntityHelper::createPermanentEntity();
  auto &fonts = fonts_entity.addComponent<FontManager>();
  fonts.load_font("body", Font{});
  fonts.load_font("body@bold", Font{});
  EntityHelper::registerSingleton<FontManager>(fonts_entity);

  auto &label = EntityHelper::createEntity();
  auto &cmp = label.addComponent<UIComponent>(label.id);
  cmp.set_desired_width(Size{.dim = Dim::Text, .value = 0.f})
      .set_desired_height(Size{.dim = Dim::Text, .value = 0.f});
  cmp.enable_font("body", pixels(20.f), true, colors::FontWeight::Bold);
  label.addComponent<HasLabel>("Bold", false).set_letter_spacing(2.f);

  std::vector<Entity *> mapping(static_cast<size_t>(label.id) + 1, nullptr);
  mapping[static_cast<size_t>(label.id)] = &label;
  AutoLayout layout({1280, 720}, mapping);
  layout.set_measure_text_fn([](const std::string &font, const std::string &s,
                                float size, float spacing) {
    const float per_glyph = font == "body@bold" ? 0.6f : 0.5f;
    const float n = static_cast<float>(s.size());
    std::printf("measuring \"%s\" in %s, spacing %.1f\n", s.c_str(),
                font.c_str(), static_cast<double>(spacing));
    return Vector2Type{n * size * per_glyph + (n - 1.f) * spacing, size};
  });
  layout.build_cmp_cache();
  layout.reset_and_calculate_standalone(cmp);

  std::printf("label width %.1f\n", static_cast<double>(cmp.computed[Axis::X]));
  assert(cmp.computed[Axis::X] >= 4 * 20.f * 0.6f + 3 * 3.f);
  std::printf("measured_label_font: ok\n");
  return 0;
}
