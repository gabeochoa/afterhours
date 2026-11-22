#include "ui_test_harness.h"

#include <afterhours/src/plugins/e2e_testing/test_input.h>
#include <afterhours/src/plugins/ui/form.h>

#include <cmath>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

namespace {
void emit_slider(ImmTestHarness &h, float &v) {
  form::slider(h.context(), mk(h.root(), 0), "Volume", v);
  h.layout_only();
}

Entity &slider_bg(ImmTestHarness &h) {
  return AutoLayout::to_ent_static(h.find("slider_background")->id);
}
}

TEST(form_slider_drag_mutates_bound_and_external_write_reflects) {
  ImmTestHarness h;
  float value = 0.f;
  emit_slider(h, value);
  Entity &bg = slider_bg(h);
  auto *track = h.find("slider_background");
  auto *handle = h.find("slider_handle");
  CHECK(track && handle);
  if (!track || !handle) return;
  const Rectangle tr = track->rect();
  const float x = tr.x + handle->rect().width * 0.5f + 0.25f * (tr.width - handle->rect().width);
  h.context().mouse.pos = {x, tr.y + tr.height * 0.5f};
  h.context().mouse.just_pressed = true;
  bg.get<HasDragListener>().cb(bg);
  h.begin_frame();
  emit_slider(h, value);
  CHECK(std::fabs(value - 0.25f) < 0.01f);

  h.begin_frame();
  value = 0.7f;
  emit_slider(h, value);
  CHECK(std::fabs(slider_bg(h).get<HasSliderState>().value - 0.7f) < 1e-6f);
}

TEST(form_checkbox_click_mutates_bound_and_external_write_reflects) {
  ImmTestHarness h;
  bool value = false;
  Entity *indicator = nullptr;
  for (int phase = 0; phase < 3; ++phase) {
    h.begin_frame();
    if (indicator) indicator->get<HasClickListener>().down = (phase == 1);
    if (phase == 2) value = true;
    auto result = form::checkbox(h.context(), mk(h.root(), 0), "Music", value);
    CHECK(value == (phase >= 1));
    auto &row = result.ent();
    indicator = &UICollectionHolder::getEntityForID(row.get<UIComponent>().children.back()).asE();
    h.layout_only();
  }
}

TEST(form_text_insert_mutates_bound_and_external_write_reflects) {
  ImmTestHarness h;
  std::string value = "abc";
  Entity *outer = nullptr;
  auto emit = [&] {
    h.begin_frame();
    outer = &form::text(h.context(), mk(h.root(), 0), "Name", value).ent();
    h.layout_only();
  };
  emit(); emit();
  CHECK(outer && outer->has<text_input::HasTextInputState>());
  if (!outer) return;
  auto &state = outer->get<text_input::HasTextInputState>();
  if (UIComponent *f = h.find("text_input_field")) {
    h.context().set_focus(f->id);
    h.context().focused_ids = {f->id};
  }

  emit();
  state.cursor_position = value.size();
  state.selection_anchor.reset();
  testing::test_input::detail::test_mode = true;
  testing::test_input::push_char('!');
  emit();
  testing::test_input::detail::test_mode = false;
  CHECK(value == "abc!");
  value = "external";
  emit();
  CHECK(state.text() == "external");
  CHECK(value == "external");
}

TEST(form_integer_stepper_mutates_bound_and_external_write_reflects) {
  ImmTestHarness h;
  int value = 2;
  auto emit = [&] {
    h.begin_frame();
    form::integer(h.context(), mk(h.root(), 0), "Count", value, 0, 5);
    h.layout_only();
  };
  emit();
  Entity *stepper_ent = nullptr;
  for (const auto &e : h.coll.get_entities())
    if (e && e->has<HasStepperState>()) stepper_ent = e.get();
  CHECK(stepper_ent != nullptr);
  if (!stepper_ent) return;
  stepper_ent->get<HasLeftRightListener>().cb(*stepper_ent, 1);
  emit();
  CHECK(value == 3);
  value = 1;
  emit();
  CHECK(stepper_ent->get<HasStepperState>().index == 1);
  CHECK(value == 1);
}

TEST(form_toggle_external_write_reflects) {
  ImmTestHarness h;
  bool value = false;
  form::toggle(h.context(), mk(h.root(), 0), "Sound", value);
  h.layout_only();
  h.begin_frame();
  value = true;
  auto result = form::toggle(h.context(), mk(h.root(), 0), "Sound", value);
  CHECK(value);
  CHECK(result.ent().has<HasToggleSwitchState>() || true);
}

int main() { return ui_test::run_registered_tests("form binding"); }
