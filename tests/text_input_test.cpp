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

// ---------------------------------------------------------------------------
// D26: control characters in the char queue
// ---------------------------------------------------------------------------

// macOS delivers Backspace as a CHAR event carrying DEL (0x7F), so a field that
// only filters `< 32` types a blank glyph on every backspace. Driven through
// insert_char because that is the one path a CHAR event and a paste share.
TEST(d26_del_is_not_insertable_text) {
  text_input::HasTextInputState s("ab");
  s.cursor_position = 2;
  CHECK(!text_input::insert_char(s, 0x7F));
  CHECK(s.text() == std::string("ab"));
}

// The other end of the same guard: printable input still goes in, or the fix
// would pass by rejecting everything.
TEST(d26_printable_text_still_inserts) {
  text_input::HasTextInputState s("ab");
  s.cursor_position = 2;
  CHECK(text_input::insert_char(s, 'c'));
  CHECK(s.text() == std::string("abc"));
  // Tab is deliberately allowed; it is the one C0 code that is text.
  CHECK(text_input::insert_char(s, '\t'));
  CHECK(s.text() == std::string("abc\t"));
}

// C1 (0x80-0x9F) are controls too, and reach the field via paste rather than a
// keystroke -- text copied out of a Windows-1252 source is the usual carrier.
TEST(d26_c1_controls_are_rejected) {
  text_input::HasTextInputState s("");
  CHECK(!text_input::insert_char(s, 0x85)); // NEL
  CHECK(!text_input::insert_char(s, 0x9F));
  CHECK(s.text() == std::string(""));
  // 0xA0 is the first printable above the C1 block, so the range stops there.
  CHECK(text_input::insert_char(s, 0xA0));
}

// ---------------------------------------------------------------------------
// D26: the field clips its own text
// ---------------------------------------------------------------------------

// Text longer than the field must not paint past the end of it. The field's
// label is drawn as part of the element, not as a child, so a plain
// clip-children rule would miss it -- Overflow::Hidden has to clip the element
// itself. Asserted on compute_intersected_clip_rect because that is the one
// definition both the render scissor and hit-testing read.
TEST(d26_field_clips_its_own_overflowing_text) {
  ImmTestHarness h;
  std::string text = "a very long value that runs well past the right edge";
  two_frames(h, [&] {
    imm::text_input(h.context(), mk(h.root(), 0), text,
                    ComponentConfig{}.with_size(
                        ComponentSize{pixels(120), pixels(40)}));
  });

  Entity *field = find_field_entity();
  ui_test::check(field != nullptr, "the field exists", __FILE__, __LINE__);
  if (!field)
    return;
  CHECK(field->has<HasClipChildren>());

  auto [clipped, clip] = ui::detail::compute_intersected_clip_rect(*field);
  CHECK(clipped);
  const RectangleType fr = field->get<UIComponent>().rect();
  CHECK_APPROX(clip.x, fr.x);
  CHECK_APPROX(clip.width, fr.width);
}

// Pins that the test above measures the flag: an ordinary div holding the same
// overlong label is not clipped, so nothing about it clips by default.
TEST(d26_a_plain_div_is_not_clipped) {
  ImmTestHarness h;
  h.begin_frame();
  auto d = div(h.context(), mk(h.root(), 0),
               ComponentConfig{}
                   .with_size(ComponentSize{pixels(120), pixels(40)})
                   .with_label("a very long value that runs past the edge"));
  h.layout_only();
  CHECK(!d.ent().has<HasClipChildren>());
  auto [clipped, clip] = ui::detail::compute_intersected_clip_rect(d.ent());
  (void)clip;
  CHECK(!clipped);
}

// ---------------------------------------------------------------------------
// D26: clicking positions the caret
// ---------------------------------------------------------------------------

// The state lives on the widget's OUTER entity while the click listener hangs
// off the inner field, its child. The listener asked its own entity for the
// state, found none, and returned -- so clicking a text field had never once
// moved the caret, in any app, on any build.
TEST(d26_clicking_the_field_reaches_the_widget_state) {
  ImmTestHarness h;
  std::string text = "hello world";
  two_frames(h, [&] {
    imm::text_input(h.context(), mk(h.root(), 0), text,
                    ComponentConfig{}.with_size(
                        ComponentSize{pixels(300), pixels(40)}));
  });

  Entity *field = find_field_entity();
  ui_test::check(field != nullptr && field->has<HasClickListener>(),
                 "the field has a click listener", __FILE__, __LINE__);
  if (!field || !field->has<HasClickListener>())
    return;

  // The field itself must NOT hold the state -- that is the whole trap.
  CHECK(!field->has<text_input::HasTextInputState>());
  CHECK(text_input::state_for_field<text_input::HasTextInputState>(*field) !=
        nullptr);

  // Clicking has to reach the state and act on it. Without a FontManager the
  // callback stops before the caret maths -- no font, no measurement -- but it
  // still clears the selection and resets the blink on its way out, and those
  // are past the lookup that used to fail.
  auto *s = text_input::state_for_field<text_input::HasTextInputState>(*field);
  ui_test::check(s != nullptr, "state reachable", __FILE__, __LINE__);
  if (!s)
    return;
  s->selection_anchor = 3;
  s->cursor_blink_timer = 0.4f;

  const RectangleType fr = field->get<UIComponent>().rect();
  h.context().mouse.pos = Vector2Type{fr.x + fr.width * 0.5f,
                                      fr.y + fr.height * 0.5f};
  field->get<HasClickListener>().cb(*field);

  CHECK(!s->selection_anchor.has_value());
  CHECK_APPROX(s->cursor_blink_timer, 0.f);
}

int main() { return ui_test::run_registered_tests("text_input"); }
