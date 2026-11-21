#define AFTER_HOURS_ENABLE_E2E_TESTING
#include "ui_test_harness.h"
using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;
using ui_test::TestInputAction;
namespace {
const HasAccessibility *a11y_of(ImmTestHarness &h, const std::string &debug) {
  for (const auto &e : h.coll.get_entities()) {
    if (e && e->has<UIComponentDebug>() &&
        e->get<UIComponentDebug>().name() == debug &&
        e->has<HasAccessibility>())
      return &e->get<HasAccessibility>();
  }
  return nullptr;
}
testing::PendingE2ECommand run_expect(ImmTestHarness &h,
                                      std::vector<std::string> args) {
  testing::PendingE2ECommand cmd;
  cmd.name = "expect_accessible";
  cmd.args = std::move(args);
  testing::ui_commands::HandleExpectAccessibleCommand<TestInputAction>{}
      .for_each_with(h.root(), cmd, 0.f);
  return cmd;
}
std::string dump_of(ImmTestHarness &h) {
  std::string xml;
  testing::ui_commands::dump_ui_node(xml, h.root(), 0);
  return xml;
}
}
TEST(explicit_name_beats_label_and_tooltip) {
  ImmTestHarness h;
  button(h.context(), mk(h.root(), 0),
         ComponentConfig{}
             .with_label("Save")
             .with_tooltip("Save the document")
             .with_accessible_name("Save document")
             .with_debug_name("btn"));
  h.layout_only();
  auto *a = a11y_of(h, "btn");
  CHECK(a != nullptr);
  if (a) {
    CHECK(a->role == AccessibleRole::Button);
    CHECK(a->name == "Save document");
    CHECK(a->description == "Save the document");
  }
}
TEST(label_is_the_default_name) {
  ImmTestHarness h;
  button(h.context(), mk(h.root(), 0),
         ComponentConfig{}.with_label("Cancel").with_debug_name("btn"));
  h.layout_only();
  auto *a = a11y_of(h, "btn");
  CHECK(a != nullptr);
  if (a) CHECK(a->name == "Cancel");
}
TEST(space_only_label_falls_back_to_tooltip) {
  ImmTestHarness h;
  button(h.context(), mk(h.root(), 0),
         ComponentConfig{}
             .with_label(" ")
             .with_tooltip("Close")
             .with_debug_name("icon_btn"));
  h.layout_only();
  auto *a = a11y_of(h, "icon_btn");
  CHECK(a != nullptr);
  if (a) {
    CHECK(a->name == "Close");
    CHECK(a->name != " ");
  }
}
TEST(no_name_anywhere_carries_no_component) {
  ImmTestHarness h;
  div(h.context(), mk(h.root(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(50), pixels(20)})
          .with_debug_name("plain"));
  h.layout_only();
  CHECK(a11y_of(h, "plain") == nullptr);
}
TEST(checkbox_role_name_and_on_off_value) {
  ImmTestHarness h;
  bool on = true;
  checkbox(h.context(), mk(h.root(), 0), on,
           ComponentConfig{}.with_label("Mute").with_debug_name("cb"));
  h.layout_only();
  auto *a = a11y_of(h, "cb");
  CHECK(a != nullptr);
  if (a) {
    CHECK(a->role == AccessibleRole::Checkbox);
    CHECK(a->name == "Mute");
    CHECK(a->value == "on");
  }
}
TEST(toggle_switch_role_and_value) {
  ImmTestHarness h;
  bool on = false;
  toggle_switch(h.context(), mk(h.root(), 0), on,
                ComponentConfig{}.with_label("Wi-Fi").with_debug_name("tg"));
  h.layout_only();
  auto *a = a11y_of(h, "tg");
  CHECK(a != nullptr);
  if (a) {
    CHECK(a->role == AccessibleRole::Switch);
    CHECK(a->name == "Wi-Fi");
    CHECK(a->value == "off");
  }
}
TEST(slider_role_name_and_percent_value) {
  ImmTestHarness h;
  float v = 0.42f;
  slider(h.context(), mk(h.root(), 0), v,
         ComponentConfig{}
             .with_label("Volume")
             .with_size(ComponentSize{pixels(200), pixels(30)})
             .with_debug_name("sl"));
  h.layout_only();
  auto *a = a11y_of(h, "sl");
  CHECK(a != nullptr);
  if (a) {
    CHECK(a->role == AccessibleRole::Slider);
    CHECK(a->name == "Volume");
    CHECK(a->value == "42%");
  }
}
TEST(progress_bar_role_and_percent_value) {
  ImmTestHarness h;
  progress_bar(h.context(), mk(h.root(), 0), 0.75f,
               ComponentConfig{}
                   .with_label("Loading")
                   .with_size(ComponentSize{pixels(200), pixels(20)})
                   .with_debug_name("pb"));
  h.layout_only();
  auto *a = a11y_of(h, "pb");
  CHECK(a != nullptr);
  if (a) {
    CHECK(a->role == AccessibleRole::ProgressBar);
    CHECK(a->value == "75%");
  }
}
TEST(stepper_value_is_current_option) {
  ImmTestHarness h;
  std::vector<std::string> opts{"Low", "Medium", "High"};
  size_t idx = 1;
  stepper(h.context(), mk(h.root(), 0), opts, idx,
          ComponentConfig{}
              .with_size(ComponentSize{pixels(400), pixels(56)})
              .with_font(UIComponent::DEFAULT_FONT, pixels(18))
              .with_debug_name("st"));
  h.layout_only();
  auto *a = a11y_of(h, "st");
  CHECK(a != nullptr);
  if (a) {
    CHECK(a->role == AccessibleRole::Stepper);
    CHECK(a->value == "Medium");
  }
}
TEST(dropdown_value_is_current_option) {
  ImmTestHarness h;
  std::vector<std::string> opts{"Red", "Green"};
  size_t idx = 1;
  dropdown(h.context(), mk(h.root(), 0), opts, idx,
           ComponentConfig{}.with_label("Colour").with_debug_name("dd"));
  h.layout_only();
  auto *a = a11y_of(h, "dd");
  CHECK(a != nullptr);
  if (a) {
    CHECK(a->role == AccessibleRole::Dropdown);
    CHECK(a->name == "Colour");
    CHECK(a->value == "Green");
  }
}
TEST(explicit_role_and_value_beat_widget_defaults) {
  ImmTestHarness h;
  bool on = false;
  checkbox(h.context(), mk(h.root(), 0), on,
           ComponentConfig{}
               .with_label("Mute")
               .with_accessible(AccessibleRole::Switch, "Mute audio")
               .with_accessible_value("muted")
               .with_debug_name("cb"));
  h.layout_only();
  auto *a = a11y_of(h, "cb");
  CHECK(a != nullptr);
  if (a) {
    CHECK(a->role == AccessibleRole::Switch);
    CHECK(a->name == "Mute audio");
    CHECK(a->value == "muted");
  }
}
TEST(dump_ui_writes_accessible_fields) {
  ImmTestHarness h;
  button(h.context(), mk(h.root(), 0),
         ComponentConfig{}
             .with_label(" ")
             .with_tooltip("Close")
             .with_debug_name("icon_btn"));
  h.layout_only();
  std::string xml = dump_of(h);
  CHECK(xml.find("role=\"button\"") != std::string::npos);
  CHECK(xml.find("accessible_name=\"Close\"") != std::string::npos);
}
TEST(expect_accessible_by_debug_name) {
  ImmTestHarness h;
  float v = 0.5f;
  slider(h.context(), mk(h.root(), 0), v,
         ComponentConfig{}
             .with_label("Volume")
             .with_size(ComponentSize{pixels(200), pixels(30)})
             .with_debug_name("sl"));
  h.layout_only();
  auto ok = run_expect(h, {"sl", "role=slider", "name=Volume", "value=50%"});
  CHECK(ok.is_consumed() && ok.succeeded());
  auto bad = run_expect(h, {"sl", "value=51%"});
  CHECK(bad.is_retry());
  CHECK(bad.last_mismatch.find("51%") != std::string::npos);
}
TEST(expect_accessible_by_accessible_name) {
  ImmTestHarness h;
  button(h.context(), mk(h.root(), 0),
         ComponentConfig{}
             .with_label(" ")
             .with_tooltip("Close")
             .with_debug_name("icon_btn"));
  h.layout_only();
  auto ok = run_expect(h, {"name=Close", "role=button"});
  CHECK(ok.is_consumed() && ok.succeeded());
  auto missing = run_expect(h, {"name=Nope"});
  CHECK(missing.is_retry());
}
int main() { return ui_test::run_registered_tests("accessibility"); }
