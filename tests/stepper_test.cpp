// stepper_test.cpp
// Regression tests for the imm::stepper widget.
//
// Build (from the afterhours repo root):
//   clang++ -std=c++23 -I.. -Ivendor examples/stepper_test.cpp -o /tmp/t && /tmp/t
//
// Covers the multi-visible label separation bug: a stepper showing
// prev/current/next placed its labels in a children()-sized container with
// SpaceAround; with no free space the labels butt together ("Healer" +
// "Warrior" + "Mage" -> "HealerWarriorMage"). The fix adds a gap when
// num_visible > 1; this also exercised the Dim::Children + flex_gap sizing fix
// (a children()-sized container must include gaps in its width).

#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

// With num_visible == 3 the label container is wider than the sum of its
// labels (i.e. there is real separation between them).
TEST(stepper_multi_visible_labels_separated) {
  ImmTestHarness h;
  std::vector<std::string> opts = {"Healer", "Warrior", "Mage"};
  size_t idx = 1;

  stepper(h.context(), mk(h.root(), 0), opts, idx,
          ComponentConfig{}
              .with_size(ComponentSize{pixels(400), pixels(56)})
              // ComponentConfig defaults to a 50px font; three labels at that
              // size do not fit in 400px and the stepper legitimately reports
              // an overflow. Pick a size a real caller would use.
              .with_font(UIComponent::DEFAULT_FONT, pixels(18))
              .with_debug_name("st"),
          /*num_visible=*/3);
  h.layout_only();

  UIComponent *labels = h.find("stepper_labels");
  CHECK(labels != nullptr);
  if (labels) {
    float sum_children = 0.f;
    for (EntityID cid : labels->children) {
      auto &c = AutoLayout::to_cmp_static(cid);
      sum_children += c.rect().width;
    }
    // Container is wider than the labels by the inter-label gaps.
    CHECK(labels->rect().width > sum_children + 1.f);
    // Three labels are visible (prev/current/next).
    CHECK(labels->children.size() == 3);
  }
}

// Default single-visible stepper shows exactly one value and adds no gap.
TEST(stepper_single_visible_no_gap) {
  ImmTestHarness h;
  std::vector<std::string> opts = {"Low", "Medium", "High"};
  size_t idx = 1;

  stepper(h.context(), mk(h.root(), 0), opts, idx,
          ComponentConfig{}
              .with_size(ComponentSize{pixels(300), pixels(48)})
              .with_debug_name("st"));
  h.layout_only();

  UIComponent *labels = h.find("stepper_labels");
  CHECK(labels != nullptr);
  if (labels) {
    CHECK(labels->children.size() == 1); // only the current value
    float sum_children = 0.f;
    for (EntityID cid : labels->children) {
      auto &c = AutoLayout::to_cmp_static(cid);
      sum_children += c.rect().width;
    }
    // No gap for a single label: container width == the one label's width.
    CHECK_APPROX(labels->rect().width, sum_children);
  }
}

TEST(stepper_frame_border_is_not_repeated_on_values_or_arrows) {
  ImmTestHarness h;
  std::vector<std::string> options{"Low", "Medium", "High"};
  size_t index = 1;
  auto result = stepper(h.context(), mk(h.root(), 0), options, index,
      ComponentConfig{}.with_size({pixels(360), pixels(48)})
          .with_font(UIComponent::DEFAULT_FONT, pixels(18))
          .with_border(Color{220, 170, 70, 255}, 2), 3);
  h.layout_only();
  CHECK(result.ent().has<HasBorder>());
  CHECK(result.ent().get<HasBorder>().border.has_border());
  const auto check_children = [&](auto &&self, Entity &parent) -> void {
    for (const auto id : parent.get<UIComponent>().children) {
      auto &child = AutoLayout::to_ent_static(id);
      CHECK(!child.has<HasBorder>() || !child.get<HasBorder>().border.has_border());
      self(self, child);
    }
  };
  check_children(check_children, result.ent());
}

TEST(stepper_keyboard_changes_survive_the_next_build_and_are_consumed_once) {
  ImmTestHarness h;
  std::vector<std::string> options{"Top left", "Top right", "Bottom left",
                                   "Bottom right", "Bottom center"};
  size_t index = 4;
  const auto build = [&] {
    h.begin_frame();
    auto result = stepper(h.context(), mk(h.root(), 0), options, index,
        ComponentConfig{}.with_size({pixels(360), pixels(48)})
            .with_font(UIComponent::DEFAULT_FONT, pixels(18)));
    h.layout_only();
    return result;
  };
  auto first = build();
  CHECK(!first);
  auto &entity = first.ent();
  CHECK(can_be_focused(h.context(), entity));
  auto &listener = entity.get<HasLeftRightListener>();
  listener.cb(entity, 1);
  CHECK(index == 4);
  CHECK(entity.get<HasStepperState>().index == 0);
  auto right = build();
  CHECK(right);
  CHECK(index == 0);
  CHECK(!entity.get<HasStepperState>().changed_since);
  auto *value = h.find("stepper_value");
  CHECK(value != nullptr);
  if (value)
    CHECK(AutoLayout::to_ent_static(value->id).get<HasLabel>().label == "Top left");
  CHECK(!build());
  CHECK(index == 0);
  listener.cb(entity, -1);
  CHECK(build());
  CHECK(index == 4);
  CHECK(!build());
  index = 2;
  CHECK(!build());
  CHECK(entity.get<HasStepperState>().index == 2);
  listener.cb(entity, 1);
  listener.cb(entity, 1);
  CHECK(build());
  CHECK(index == 4);
  listener.cb(entity, -1);
  options.resize(2);
  CHECK(build());
  CHECK(index == 1);
  CHECK(entity.get<HasStepperState>().num_options == 2);
}

TEST(stepper_system_uses_press_and_configured_repeat_instead_of_every_held_frame) {
  ImmTestHarness h;
  using Action = ui_test::TestInputAction;
  std::vector<std::string> options{"Top left", "Top right", "Bottom left",
                                   "Bottom right", "Bottom center"};
  size_t index = 4;
  const auto build = [&] {
    h.begin_frame();
    auto result = stepper(h.context(), mk(h.root(), 0), options, index,
        ComponentConfig{}.with_size({pixels(360), pixels(48)})
            .with_font(UIComponent::DEFAULT_FONT, pixels(18)));
    h.layout_only();
    return result;
  };
  auto &entity = build().ent();
  auto &context = h.context();
  context.set_focus(entity.id);
  HandleLeftRight<Action> system;
  system.context = &context;
  const auto frame = [&](Action action, bool pressed, bool held, bool repeat) {
    const auto bit = magic_enum::enum_index(action).value();
    context.last_action = pressed ? action : Action::None;
    context.all_actions.reset();
    context.all_actions_repeat.reset();
    context.all_actions[bit] = held;
    context.all_actions_repeat[bit] = repeat;
    entity.get<UIComponent>().was_rendered_to_screen = true;
    system.for_each_with(entity, entity.get<UIComponent>(),
                         entity.get<HasLeftRightListener>(), 1.f / 60);
    const bool changed = build();
    CHECK(context.has_focus(entity.id));
    return changed;
  };
  CHECK(frame(Action::WidgetRight, true, true, false));
  CHECK(index == 0);
  for (int i = 0; i < 4; ++i) {
    CHECK(!frame(Action::WidgetRight, false, true, false));
    CHECK(index == 0);
  }
  CHECK(frame(Action::WidgetRight, false, true, true));
  CHECK(index == 1);
  CHECK(!context.all_actions_repeat[magic_enum::enum_index(Action::WidgetRight).value()]);
  CHECK(!frame(Action::WidgetRight, false, true, false));
  CHECK(index == 1);
  CHECK(!frame(Action::WidgetRight, false, false, false));
  CHECK(frame(Action::WidgetLeft, true, true, false));
  CHECK(index == 0);
  CHECK(!frame(Action::WidgetLeft, false, true, false));
  CHECK(frame(Action::WidgetLeft, false, true, true));
  CHECK(index == 4);
}

TEST(slider_system_still_advances_on_every_held_frame) {
  ImmTestHarness h;
  using Action = ui_test::TestInputAction;
  float value = .5f;
  slider(h.context(), mk(h.root(), 0), value,
      ComponentConfig{}.with_size({pixels(300), pixels(40)}));
  h.layout_only();
  Entity *control = nullptr;
  for (const auto &entity : h.coll.get_entities()) {
    if (entity && entity->has<HasSliderState>() && entity->has<HasLeftRightListener>()) {
      control = entity.get();
      break;
    }
  }
  CHECK(control != nullptr);
  if (!control) return;
  auto &context = h.context();
  context.set_focus(control->id);
  HandleLeftRight<Action> system;
  system.context = &context;
  control->get<UIComponent>().was_rendered_to_screen = true;
  for (int frame = 0; frame < 3; ++frame) {
    context.last_action = Action::None;
    context.all_actions[magic_enum::enum_index(Action::WidgetRight).value()] = true;
    context.all_actions_repeat.reset();
    system.for_each_with(*control, control->get<UIComponent>(),
                         control->get<HasLeftRightListener>(), 1.f / 60);
    CHECK(std::abs(control->get<HasSliderState>().value - (.51f + frame * .01f)) < .0001f);
  }
}

int main() { return ui_test::run_registered_tests("stepper tests"); }
