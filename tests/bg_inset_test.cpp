
#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

namespace {
template <typename F> void measure_fill(ComponentConfig cfg, F &&fn) {
  ImmTestHarness h;
  h.begin_frame();
  auto d = div(h.context(), mk(h.root(), 0), cfg);
  h.render();
  const RectangleType r = d.ent().get<UIComponent>().rect();
  const auto fills = h.drawn("rectangle_rounded");
  fn(r, fills);
}

TEST(fill_is_widget_rect_by_default) {
  measure_fill(ComponentConfig{}
                   .with_size({pixels(200), pixels(28)})
                   .with_padding(Padding::all(pixels(10)))
                   .with_custom_background(Color{40, 40, 40, 255}),
               [](const RectangleType &r, const auto &fills) {
                 CHECK(fills.size() == 1);
                 if (fills.empty())
                   return;
                 CHECK_APPROX(fills[0].rect.x, r.x);
                 CHECK_APPROX(fills[0].rect.y, r.y);
                 CHECK_APPROX(fills[0].rect.width, r.width);
                 CHECK_APPROX(fills[0].rect.height, r.height);
               });
}

TEST(inset_shrinks_fill_only) {
  measure_fill(ComponentConfig{}
                   .with_size({pixels(200), pixels(28)})
                   .with_background_inset(3.f, 4.f)
                   .with_custom_background(Color{40, 40, 40, 255}),
               [](const RectangleType &r, const auto &fills) {
                 CHECK(fills.size() == 1);
                 if (fills.empty())
                   return;
                 CHECK_APPROX(fills[0].rect.x, r.x + 3.f);
                 CHECK_APPROX(fills[0].rect.y, r.y + 4.f);
                 CHECK_APPROX(fills[0].rect.width, r.width - 6.f);
                 CHECK_APPROX(fills[0].rect.height, r.height - 8.f);
               });
}

TEST(hover_fill_uses_same_inset_rect) {
  measure_fill(ComponentConfig{}
                   .with_size({pixels(200), pixels(28)})
                   .with_background_inset(2.f, 2.f)
                   .with_custom_background(Color{40, 40, 40, 255})
                   .with_custom_hover_bg(Color{90, 90, 90, 255}),
               [](const RectangleType &r, const auto &fills) {
                 CHECK(fills.size() == 1);
                 if (fills.empty())
                   return;
                 CHECK_APPROX(fills[0].rect.x, r.x + 2.f);
                 CHECK_APPROX(fills[0].rect.y, r.y + 2.f);
                 CHECK_APPROX(fills[0].rect.width, r.width - 4.f);
                 CHECK_APPROX(fills[0].rect.height, r.height - 4.f);
               });
}

TEST(inset_composes_with_padding) {
  measure_fill(ComponentConfig{}
                   .with_size({pixels(200), pixels(28)})
                   .with_padding(Padding{.top = pixels(6),
                                        .left = pixels(14),
                                        .bottom = pixels(2),
                                        .right = pixels(4)})
                   .with_background_inset(5.f, 1.f)
                   .with_custom_background(Color{40, 40, 40, 255}),
               [](const RectangleType &r, const auto &fills) {
                 CHECK(fills.size() == 1);
                 if (fills.empty())
                   return;
                 CHECK_APPROX(fills[0].rect.x, r.x + 5.f);
                 CHECK_APPROX(fills[0].rect.y, r.y + 1.f);
                 CHECK_APPROX(fills[0].rect.width, r.width - 10.f);
                 CHECK_APPROX(fills[0].rect.height, r.height - 2.f);
               });
}

TEST(oversized_inset_collapses) {
  measure_fill(ComponentConfig{}
                   .with_size({pixels(200), pixels(28)})
                   .with_background_inset(150.f, 20.f)
                   .with_custom_background(Color{40, 40, 40, 255}),
               [](const RectangleType &, const auto &fills) {
                 CHECK(fills.size() == 1);
                 if (fills.empty())
                   return;
                 CHECK_APPROX(fills[0].rect.width, 0.f);
                 CHECK_APPROX(fills[0].rect.height, 0.f);
               });
}

} // namespace

int main() { return ui_test::run_registered_tests("bg_inset"); }
