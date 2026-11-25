#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

namespace {
texture_manager::Texture tex() { return TextureType{64, 48}; }

Rectangle build_and_find(ImmTestHarness &h, float bw, float bh,
                         ComponentSize icon_sz, IconPosition pos,
                         Rectangle *btn_rect) {
  auto btn = button(h.context(), mk(h.root(), 0),
                    ComponentConfig{"Lbl"}
                        .with_size(ComponentSize{pixels(bw), pixels(bh)})
                        .with_icon(tex(), {0, 0, 64, 64})
                        .with_icon_position(pos)
                        .with_icon_size(icon_sz)
                        .with_debug_name("btn"));
  h.layout_only();
  UIComponent *icon = h.find("btn_icon");
  if (btn_rect)
    *btn_rect = btn.ent().get<UIComponent>().rect();
  CHECK(icon != nullptr);
  return icon ? icon->rect() : Rectangle{};
}
} // namespace

TEST(configured_size_is_the_icon_size) {
  ImmTestHarness h;
  Rectangle b{};
  Rectangle r = build_and_find(h, 200.f, 48.f,
                               ComponentSize{pixels(24), pixels(24)},
                               IconPosition::Left, &b);
  CHECK_APPROX(r.width, 24.f);
  CHECK_APPROX(r.height, 24.f);
  CHECK_APPROX(r.y + r.height / 2, b.y + b.height / 2);
}

TEST(configured_size_is_honored_on_the_right_too) {
  ImmTestHarness h;
  Rectangle b{};
  Rectangle r = build_and_find(h, 200.f, 48.f,
                               ComponentSize{pixels(16), pixels(16)},
                               IconPosition::Right, &b);
  CHECK_APPROX(r.width, 16.f);
  CHECK_APPROX(r.height, 16.f);
  CHECK_APPROX(r.x + r.width + (b.x + b.width - r.x - r.width), b.x + b.width);
  CHECK_APPROX(r.y + r.height / 2, b.y + b.height / 2);
}

TEST(the_built_in_size_still_applies_by_default) {
  ImmTestHarness h;
  auto btn = button(h.context(), mk(h.root(), 0),
                    ComponentConfig{"Lbl"}
                        .with_size(ComponentSize{pixels(200), pixels(48)})
                        .with_icon(tex(), {0, 0, 64, 64})
                        .with_debug_name("btn_dflt"));
  h.layout_only();
  UIComponent *icon = h.find("btn_icon");
  CHECK(icon != nullptr);
  if (icon) {
    CHECK_APPROX(icon->rect().width, 28.8f);
    CHECK_APPROX(icon->rect().height, 28.8f);
  }
}

TEST(configured_size_survives_both_renderers) {
  for (int renderer = 0; renderer < 2; ++renderer) {
    ImmTestHarness h;
    button(h.context(), mk(h.root(), 0),
           ComponentConfig{"Lbl"}
               .with_size(ComponentSize{pixels(200), pixels(48)})
               .with_icon(tex(), {0, 0, 64, 64})
               .with_icon_size(ComponentSize{pixels(24), pixels(24)})
               .with_debug_name("btn_draw"));
    if (renderer == 0)
      h.render();
    else
      h.render_batched();
    UIComponent *icon = h.find("btn_icon");
    CHECK(icon != nullptr);
    if (icon) {
      CHECK_APPROX(icon->rect().width, 24.f);
      CHECK_APPROX(icon->rect().height, 24.f);
    }
  }
}

int main() { return ui_test::run_registered_tests("icon sizing"); }
