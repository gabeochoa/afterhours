#include <cstdio>
#include <string_view>

#include "../../../../ah.h"
#include "../../../../src/plugins/ui/text_measure.h"

int main() {
  afterhours::set_measure_text_fn(
      [](const char *text, float font_size, float) {
        return Vector2Type{
            static_cast<float>(
                std::string_view(text ? text : "").size()) *
                font_size * 0.5f,
            font_size};
      });

  afterhours::Entity &entity =
      afterhours::EntityHelper::createPermanentEntity();
  auto &fonts = entity.addComponent<afterhours::ui::FontManager>();
  fonts.load_font("mono", afterhours::Font{});
  afterhours::EntityHelper::registerSingleton<afterhours::ui::FontManager>(
      entity);

  const float cell = afterhours::ui::monospace_cell_advance("mono", 16.f);
  const int digits = 4;
  const float gutter_width = static_cast<float>(digits) * cell;

  std::printf("monospace_cell: cell=%.1f digits=%d gutter=%.1f\n", cell,
              digits, gutter_width);
  return cell == 8.f && gutter_width == 32.f ? 0 : 1;
}
