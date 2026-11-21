#include "ui_test_harness.h"

#include <afterhours/src/plugins/modal.h>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

static void ensure_modal_singleton() {
  if (!EntityHelper::has_singleton<modal::ModalRoot>()) {
    Entity &e = EntityHelper::createPermanentEntity();
    modal::detail::init_singleton(e);
  }
}

static RectangleType modal_rect(ImmTestHarness &h, ScalingMode mode,
                                float ui_scale,
                                ModalAnchor anchor = ModalAnchor::Center) {
  h.context().scaling_mode = mode;
  h.context().theme.ui_scale = ui_scale;
  bool open = true;
  afterhours::modal(h.context(), mk(h.root(), 0), open,
      ModalConfig{}.with_size(pixels(400), pixels(200))
          .with_anchor(anchor)
          .with_show_close_button(false));
  h.layout_only();
  UIComponent *found = h.find("modal");
  CHECK(found != nullptr);
  return found ? found->rect() : RectangleType{};
}

TEST(modal_centered_in_proportional_mode) {
  ImmTestHarness h;
  ensure_modal_singleton();
  RectangleType r = modal_rect(h, ScalingMode::Proportional, 2.0f);
  CHECK_APPROX(r.x, 200.f);
  CHECK_APPROX(r.y, 200.f);
  CHECK_APPROX(r.width, 400.f);
  CHECK_APPROX(r.height, 200.f);
}

TEST(modal_centered_in_adaptive_at_scale_1) {
  ImmTestHarness h;
  ensure_modal_singleton();
  RectangleType r = modal_rect(h, ScalingMode::Adaptive, 1.0f);
  CHECK_APPROX(r.x, 200.f);
  CHECK_APPROX(r.y, 200.f);
  CHECK_APPROX(r.width, 400.f);
  CHECK_APPROX(r.height, 200.f);
}

TEST(modal_centered_in_adaptive_at_scale_1_5) {
  ImmTestHarness h;
  ensure_modal_singleton();
  RectangleType r = modal_rect(h, ScalingMode::Adaptive, 1.5f);
  CHECK_APPROX(r.x, 200.f);
  CHECK_APPROX(r.y, 200.f);
  CHECK_APPROX(r.width, 400.f);
  CHECK_APPROX(r.height, 200.f);
}

TEST(modal_centered_in_adaptive_at_scale_2) {
  ImmTestHarness h;
  ensure_modal_singleton();
  RectangleType r = modal_rect(h, ScalingMode::Adaptive, 2.0f);
  CHECK_APPROX(r.x, 200.f);
  CHECK_APPROX(r.y, 200.f);
  CHECK_APPROX(r.width, 400.f);
  CHECK_APPROX(r.height, 200.f);
}

int main() { return ui_test::run_registered_tests("modal centering tests"); }
