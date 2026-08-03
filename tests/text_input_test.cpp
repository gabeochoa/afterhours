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
  for (int i = 0; i < 2; i++) {
    h.begin_frame();
    emit();
    h.layout_only();
  }
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

// D12: no placeholder. An empty field renders as a bare box, so hanabi's
// sidebar search read as an unlabelled rectangle. Painting hint text BEHIND the
// field did not work either -- D11's forced fill covered it -- which is why the
// app resorted to an absolutely-positioned overlay with hand-derived geometry.

TEST(d12_placeholder_shows_when_empty) {
  ImmTestHarness h;
  std::string text;
  two_frames(h, [&] {
    imm::text_input(h.context(), mk(h.root(), 0), text,
                    ComponentConfig{}
                        .with_size(ComponentSize{pixels(220), pixels(40)})
                        .with_placeholder("Search conversations"));
  });

  Entity *field = find_field_entity();
  ui_test::check(field != nullptr && field->has<HasLabel>(), "field exists",
                 __FILE__, __LINE__);
  if (field && field->has<HasLabel>())
    ui_test::check(field->get<HasLabel>().label == "Search conversations",
                   "an empty field shows the placeholder", __FILE__, __LINE__);
}

TEST(d12_placeholder_hidden_once_there_is_text) {
  ImmTestHarness h;
  std::string text = "hello";
  two_frames(h, [&] {
    imm::text_input(h.context(), mk(h.root(), 0), text,
                    ComponentConfig{}
                        .with_size(ComponentSize{pixels(220), pixels(40)})
                        .with_placeholder("Search conversations"));
  });

  Entity *field = find_field_entity();
  if (field && field->has<HasLabel>())
    ui_test::check(field->get<HasLabel>().label == "hello",
                   "typed text replaces the placeholder", __FILE__, __LINE__);
}

TEST(d12_placeholder_is_muted_and_real_text_is_not) {
  {
    ImmTestHarness h;
    std::string text;
    two_frames(h, [&] {
      imm::text_input(h.context(), mk(h.root(), 0), text,
                      ComponentConfig{}
                          .with_size(ComponentSize{pixels(220), pixels(40)})
                          .with_placeholder("hint"));
    });
    Entity *field = find_field_entity();
    if (field && field->has<HasLabel>())
      ui_test::check(field->get<HasLabel>().explicit_text_color.has_value() &&
                         same_color(*field->get<HasLabel>().explicit_text_color,
                                    h.context().theme.font_muted),
                     "the placeholder draws muted", __FILE__, __LINE__);
  }
  {
    ImmTestHarness h;
    std::string text = "typed";
    two_frames(h, [&] {
      imm::text_input(h.context(), mk(h.root(), 0), text,
                      ComponentConfig{}
                          .with_size(ComponentSize{pixels(220), pixels(40)})
                          .with_placeholder("hint"));
    });
    Entity *field = find_field_entity();
    if (field && field->has<HasLabel>())
      ui_test::check(!field->get<HasLabel>().explicit_text_color.has_value(),
                     "real text is not left muted", __FILE__, __LINE__);
  }
}

// The placeholder must not feed the cursor maths, or the caret would sit after
// the hint instead of where typing starts.
TEST(d12_placeholder_does_not_move_the_cursor) {
  auto cursor_x_for = [](bool with_placeholder) {
    ImmTestHarness h;
    std::string text;
    auto emit = [&] {
      auto cfg = ComponentConfig{}
                     .with_size(ComponentSize{pixels(220), pixels(40)});
      if (with_placeholder)
        cfg.with_placeholder("a very long placeholder string");
      imm::text_input(h.context(), mk(h.root(), 0), text, cfg);
    };
    // The caret only exists while focused, so focus the field between frames.
    h.begin_frame();
    emit();
    h.layout_only();
    if (Entity *field = find_field_entity())
      h.context().focus_id = field->id;
    h.begin_frame();
    emit();
    h.layout_only();
    UIComponent *c = h.find("cursor");
    return c ? c->rect().x : -1.f;
  };
  const float without = cursor_x_for(false);
  const float with = cursor_x_for(true);
  ui_test::check(without >= 0.f && with >= 0.f, "found the cursor", __FILE__,
                 __LINE__);
  CHECK_APPROX(with, without);
}

int main() { return ui_test::run_registered_tests("text_input"); }
