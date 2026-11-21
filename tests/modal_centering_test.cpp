// modal_centering_test.cpp
// Regression tests for modal centering across scaling mode and ui_scale.
// modal_impl used to center in unscaled screen space, then hand the origin to
// with_absolute_position, whose translate is multiplied by ui_scale in
// Adaptive mode -- so at ui_scale 2 the origin doubled AND the size doubled,
// putting the modal center at screen_center * ui_scale (off-screen at 200%
// zoom; floatinghotel measured left edge 1180 on an 1800 viewport).
//
// The harness lays out at 800x600 (ProvidesCurrentResolution singleton), so
// for a pixels(400) x pixels(200) modal (panel size is not ui_scale-scaled
// in this layout path; only the absolute translate is):
//   Proportional (any ui_scale) / Adaptive 1/1.5/2: origin (200, 200)

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

// Emit a pixels(400)xPixels(200) centered modal at the given scale and return
// the panel rect. Anchor is Center unless a test passes another one.
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
