// text_input_on_submit_test.cpp -- R-031: with_on_submit fires synchronously
// during the widget call with this frame's captures, and is never stored.
#include "ui_test_harness.h"

#include <afterhours/src/plugins/ui/text_input/component.h>
#include <afterhours/src/plugins/ui/text_input/text_area.h>

#include <string>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

namespace {
Entity *find_by_name(const char *name) {
  for (const auto &e : UICollectionHolder::get().collection.get_entities())
    if (e && e->has<UIComponentDebug>() &&
        e->get<UIComponentDebug>().name() == name)
      return e.get();
  return nullptr;
}

void two_frames(ImmTestHarness &h, const std::function<void()> &emit) {
  for (int i = 0; i < 2; i++) {
    h.begin_frame();
    emit();
    h.layout_only();
  }
}
} // namespace

TEST(on_submit_sees_this_frames_captures_not_the_first_frames) {
  ImmTestHarness h;
  std::string text = "hello";
  bool kickoff = true; // the stale first-frame value from the hanabi bug
  bool saw_kickoff = true;
  std::string saw_text;
  int calls = 0;
  auto emit = [&] {
    auto r = imm::text_input(
        h.context(), mk(h.root(), 0), text,
        ComponentConfig{}
            .with_size({pixels(220), pixels(40)})
            .with_on_submit([&](std::string_view t) {
              calls++;
              saw_kickoff = kickoff;
              saw_text = std::string(t);
            }));
    CHECK(!r.submitted || calls > 0);
  };
  two_frames(h, emit);
  Entity *field = find_by_name("text_input_field");
  CHECK(field != nullptr);
  if (!field) return;
  kickoff = false; // thread opened since the field was created
  h.context().set_focus(field->id);
  h.context().last_action = ui_test::TestInputAction::WidgetPress;
  h.begin_frame();
  emit();
  h.layout_only();
  CHECK(calls == 1);
  CHECK(!saw_kickoff);
  CHECK(saw_text == "hello");
}

TEST(submitted_flag_and_no_fire_without_enter) {
  ImmTestHarness h;
  std::string text = "hi";
  int calls = 0;
  bool submitted = true;
  auto emit = [&] {
    auto r = imm::text_input(
        h.context(), mk(h.root(), 0), text,
        ComponentConfig{}
            .with_size({pixels(220), pixels(40)})
            .with_on_submit([&](std::string_view) { calls++; }));
    submitted = r.submitted;
  };
  two_frames(h, emit);
  CHECK(!submitted);
  CHECK(calls == 0);
  // Nothing was stored on the entity that a later frame could fire.
  Entity *field = find_by_name("text_input_field");
  CHECK(field != nullptr);
  h.begin_frame();
  emit();
  h.layout_only();
  CHECK(calls == 0);
}

TEST(text_area_on_submit_fires_without_newline) {
  ImmTestHarness h;
  std::string text = "ab";
  std::string saw_text;
  bool submitted = false;
  auto emit = [&] {
    auto r = text_input::text_area(
        h.context(), mk(h.root(), 0), text,
        ComponentConfig{}
            .with_size({pixels(200), pixels(100)})
            .with_submit_on_enter()
            .with_on_submit(
                [&](std::string_view t) { saw_text = std::string(t); }));
    submitted = r.submitted;
  };
  two_frames(h, emit);
  Entity *field = find_by_name("text_area_field");
  CHECK(field != nullptr);
  if (!field) return;
  h.context().set_focus(field->id);
  h.context().last_action = ui_test::TestInputAction::WidgetPress;
  h.begin_frame();
  emit();
  h.layout_only();
  CHECK(submitted);
  CHECK(saw_text == "ab");
  CHECK(text == "ab");
}

int main() { return ui_test::run_registered_tests("text_input_on_submit"); }
