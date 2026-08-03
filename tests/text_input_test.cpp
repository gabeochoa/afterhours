// text_input_test.cpp
// D11: text_input derives its font size from field height and forces its fill
// to Theme::Usage::Secondary, so with_font_size and with_custom_background are
// both silently dropped.
//
// Raylib rather than the recording backend: text_input pulls in graphics.h,
// which requires a real backend macro. These assert on component state after
// layout, so no draw calls are needed.

#include "ui_test_harness.h"

#include <afterhours/src/plugins/ui/text_input/component.h>

#include <functional>
#include <string>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

namespace {
bool same_color(const Color &a, const Color &b) {
  return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

// text_input sizes its font from the PREVIOUS frame's height, so a single
// emit+layout never exercises that path. Run two frames like a real app does.
void two_frames(ImmTestHarness &h, const std::function<void()> &emit) {
  emit();
  h.layout_only();
  emit();
  h.layout_only();
}

Entity *find_field_entity() {
  for (const auto &e : UICollectionHolder::get().collection.get_entities()) {
    if (e && e->has<UIComponentDebug>() &&
        e->get<UIComponentDebug>().name() == "text_input_field")
      return e.get();
  }
  return nullptr;
}
} // namespace

// ===========================================================================
// D11 -- text_input ignores with_font_size and with_custom_background
//
// The field derives its font size from its computed height and forces its fill
// to Theme::Usage::Secondary, so both config calls are silently dropped: a tall
// field renders an oversized font that overflows, and the field stays dark even
// under a light app theme. The forced fill is also what blocks a placeholder,
// since hint text drawn behind the field is covered by it.
// ===========================================================================

namespace {
// The field is the child the widget builds; the outer entity is the row.
UIComponent *find_field(ImmTestHarness &h) {
  return h.find("text_input_field");
}
} // namespace

TEST(d11_text_input_honours_explicit_font_size) {
  ImmTestHarness h;
  std::string text = "hello";
  // Tall field: the height-derived size would be 40, so 14 only appears if the
  // explicit size is honoured.
  two_frames(h, [&] {
    imm::text_input(h.context(), mk(h.root(), 0), text,
                    ComponentConfig{}
                        .with_size(ComponentSize{pixels(220), pixels(80)})
                        .with_font(UIComponent::DEFAULT_FONT, 14.f));
  });

  UIComponent *field = find_field(h);
  ui_test::check(field != nullptr, "the field exists", __FILE__, __LINE__);
  if (field)
    CHECK_APPROX(field->font_size.value, 14.f);
}

TEST(d11_text_input_derives_font_size_when_unset) {
  ImmTestHarness h;
  std::string text = "hello";
  two_frames(h, [&] {
    imm::text_input(h.context(), mk(h.root(), 0), text,
                    ComponentConfig{}.with_size(
                        ComponentSize{pixels(220), pixels(80)}));
  });

  UIComponent *field = find_field(h);
  ui_test::check(field != nullptr, "the field exists", __FILE__, __LINE__);
  // Half the field height, the long-standing behaviour when nothing is set.
  if (field)
    CHECK_APPROX(field->font_size.value, 40.f);
}

TEST(d11_text_input_honours_custom_background) {
  const Color custom{30, 90, 60, 255};
  ImmTestHarness h;
  std::string text = "hello";
  two_frames(h, [&] {
    imm::text_input(h.context(), mk(h.root(), 0), text,
                    ComponentConfig{}
                        .with_size(ComponentSize{pixels(220), pixels(40)})
                        .with_custom_background(custom));
  });

  Entity *field = find_field_entity();
  ui_test::check(field != nullptr && field->has<HasColor>(),
                 "found the field's color", __FILE__, __LINE__);
  if (field && field->has<HasColor>())
    ui_test::check(same_color(field->get<HasColor>().color(), custom),
                   "the field uses the caller's background", __FILE__,
                   __LINE__);
}

int main() { return ui_test::run_registered_tests("text_input"); }
