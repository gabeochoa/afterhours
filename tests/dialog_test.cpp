// dialog_test.cpp
// Regression tests for the modal dialog family (confirm / confirm_danger /
// prompt). Locks the two layout bugs fixed during the dialog overhaul:
//   1. message body overlapping the button row (expand() was greedy), and
//   2. the rightmost action button escaping the panel (children()-width +
//      padding rendered wider than its allocated box).
//
// Build (from the afterhours repo root):
//   clang++ -std=c++23 -I.. -Ivendor tests/dialog_test.cpp -o /tmp/t && /tmp/t

#include "ui_test_harness.h"

#include <afterhours/src/plugins/modal.h>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

// Dialogs need the ModalRoot singleton (modal_impl throws without it). Put it on
// a permanent entity so it survives the per-test UI-collection cleanup.
static void ensure_modal_singleton() {
  if (!EntityHelper::has_singleton<modal::ModalRoot>()) {
    Entity &e = EntityHelper::createPermanentEntity();
    modal::detail::init_singleton(e);
  }
}

// Assert the standard dialog layout invariants: message sits fully above the
// button row (no overlap) and every action button stays inside the panel.
static void check_dialog_layout(ImmTestHarness &h, size_t expected_buttons) {
  UIComponent *panel = h.find("modal");
  UIComponent *msg = h.find("dialog_message");
  UIComponent *row = h.find("dialog_buttons");
  CHECK(panel != nullptr);
  CHECK(msg != nullptr);
  CHECK(row != nullptr);
  if (!panel || !msg || !row)
    return;

  // 1. Message ends at or above the button row (no overlap).
  CHECK(msg->rect().y + msg->rect().height <= row->rect().y + 2.f);

  // 2. Expected number of buttons, each within the panel's right edge.
  CHECK(row->children.size() == expected_buttons);
  float panel_right = panel->rect().x + panel->rect().width;
  for (EntityID cid : row->children) {
    UIComponent &b = AutoLayout::to_cmp_static(cid);
    CHECK(b.rect().x + b.rect().width <= panel_right + 2.f);
  }

  // 3. The button row fits vertically too. Only the horizontal edge was
  // checked before, so a dialog declared shorter than its own content -- which
  // every variant was -- passed while the row hung out of the bottom.
  float panel_bottom = panel->rect().y + panel->rect().height;
  CHECK(row->rect().y + row->rect().height <= panel_bottom + 2.f);
}

TEST(confirm_dialog_message_above_buttons) {
  ImmTestHarness h;
  ensure_modal_singleton();
  bool open = true;
  modal::confirm(h.context(), mk(h.root(), 0), open, "Apply changes?",
                 "This is a reasonably long confirmation message that should "
                 "wrap across several lines inside the dialog panel.",
                 "Apply", "Cancel");
  h.layout_only();
  check_dialog_layout(h, /*expected_buttons=*/2);
}

TEST(confirm_danger_lays_out_two_buttons) {
  ImmTestHarness h;
  ensure_modal_singleton();
  bool open = true;
  modal::confirm_danger(h.context(), mk(h.root(), 0), open, "Delete save?",
                        "This permanently deletes the file and cannot be undone.",
                        "Delete", "Cancel");
  h.layout_only();
  check_dialog_layout(h, /*expected_buttons=*/2);
}

// NOTE: prompt() embeds a text_input, which requires a fuller InputAction enum
// (TextEnd/MenuBack/... for cursor + editing) than the harness's minimal
// TestInputAction. Its layout reuses the same dialog_message/dialog_button
// helpers exercised above, and it is validated visually by the dialog_prompt
// headless showcase, so it isn't unit-tested here.


TEST(custom_panel_keeps_modal_lifecycle_and_focus_restore) {
  ImmTestHarness h;
  ensure_modal_singleton();
  auto &stack = modal::detail::get_modal_root().modal_stack;
  stack.clear();
  auto trigger = button(h.context(), mk(h.root(), 9), ComponentConfig{}.with_label("Open"));
  h.context().set_focus(trigger.ent().id);
  bool open = true;
  auto config = ModalConfig{}.with_show_close_button(false).with_panel(
      ComponentConfig{}.with_size({pixels(320), pixels(160)})
          .with_absolute_position(40, 60).with_corner_radius(0)
          .with_padding(Padding::all(pixels(0))).with_debug_name("custom_panel"));
  auto emit = [&] { return afterhours::modal(h.context(), mk(h.root(), 0), open, config); };
  auto panel = emit();
  h.layout_only();
  CHECK(modal::top_modal() == panel.ent().id);
  CHECK(!h.context().is_input_allowed(trigger.ent().id));
  CHECK(h.context().is_input_allowed(panel.ent().id));
  CHECK_APPROX(panel.cmp().rect().x, 40.f);
  CHECK_APPROX(panel.cmp().rect().y, 60.f);
  CHECK_APPROX(panel.cmp().rect().width, 320.f);
  auto action = button(h.context(), mk(panel.ent(), 1), ComponentConfig{}.with_label("Close"));
  h.context().set_focus(action.ent().id);
  panel.cmp().children.clear();
  CHECK(modal::detail::is_entity_in_tree(panel.ent().id, action.ent().id));
  open = false;
  h.begin_frame();
  emit();
  CHECK(stack.empty());
  CHECK(h.context().is_input_allowed(trigger.ent().id));
  CHECK(h.context().focus_id == trigger.ent().id);
}

TEST(closing_a_covered_modal_preserves_top_modal_focus) {
  ImmTestHarness h;
  ensure_modal_singleton();
  modal::detail::get_modal_root().modal_stack.clear();
  bool lower_open = true, upper_open = true;
  auto config = ModalConfig{}.with_show_close_button(false);
  auto emit_lower = [&] { return afterhours::modal(h.context(), mk(h.root(), 1), lower_open, config); };
  auto lower = emit_lower();
  auto upper = afterhours::modal(h.context(), mk(h.root(), 2), upper_open, config);
  auto action = button(h.context(), mk(upper.ent(), 0), ComponentConfig{}.with_label("Close"));
  h.context().set_focus(action.ent().id);
  h.layout_only();
  h.begin_frame();
  lower_open = false;
  emit_lower();
  CHECK(modal::top_modal() == upper.ent().id);
  CHECK(h.context().focus_id == action.ent().id);
  CHECK(!h.context().is_input_allowed(lower.ent().id));
  CHECK(h.context().is_input_allowed(action.ent().id));
}

TEST(omitted_modal_releases_its_input_gate_and_focus) {
  ImmTestHarness h;
  ensure_modal_singleton();
  modal::detail::get_modal_root().modal_stack.clear();
  auto trigger = button(h.context(), mk(h.root(), 9), ComponentConfig{}.with_label("Open"));
  h.context().set_focus(trigger.ent().id);
  bool open = true;
  auto panel = afterhours::modal(h.context(), mk(h.root(), 0), open,
                                ModalConfig{}.with_show_close_button(false));
  auto action = button(h.context(), mk(panel.ent(), 1), ComponentConfig{}.with_label("Close"));
  h.context().set_focus(action.ent().id);
  h.layout_only();
  EntityHelper::registerSingleton<UIContext<ui_test::TestInputAction>>(h.context_entity());
  panel.cmp().was_rendered_to_screen = false;
  modal::ModalInputBlockSystem<ui_test::TestInputAction> block;
  block.once(0);
  CHECK(!modal::is_active());
  CHECK(h.context().is_input_allowed(trigger.ent().id));
  CHECK(h.context().focus_id == trigger.ent().id);
  CHECK(!panel.ent().get<modal::Modal>().was_open_last_frame);
  EntityHelper::get_default_collection().singletonMap.erase(
      components::get_type_id<UIContext<ui_test::TestInputAction>>());
}

TEST(modal_backdrop_and_default_panel_ignore_offset_parent) {
  ImmTestHarness h;
  ensure_modal_singleton();
  modal::detail::get_modal_root().modal_stack.clear();
  auto parent = div(h.context(), mk(h.root(), 0), ComponentConfig{}
      .with_size({pixels(600), pixels(400)}).with_absolute_position(180, 90)
      .with_padding(Padding::all(pixels(24))));
  bool open = true;
  auto panel = afterhours::modal(h.context(), mk(parent.ent(), 0), open,
      ModalConfig{}.with_size(pixels(320), pixels(160)).with_show_close_button(false));
  h.layout_only();
  auto *backdrop = h.find("modal_backdrop");
  CHECK(backdrop != nullptr);
  CHECK_APPROX(backdrop->rect().x, 0.f);
  CHECK_APPROX(backdrop->rect().y, 0.f);
  CHECK_APPROX(backdrop->rect().width, 800.f);
  CHECK_APPROX(backdrop->rect().height, 600.f);
  CHECK_APPROX(panel.cmp().rect().x, 240.f);
  CHECK_APPROX(panel.cmp().rect().y, 220.f);
}

TEST(backdrop_release_belongs_to_the_modal_present_on_press) {
  for (bool close_between : {false, true}) {
    ImmTestHarness h;
    ensure_modal_singleton();
    modal::detail::get_modal_root().modal_stack.clear();
    EntityHelper::registerSingleton<UIContext<ui_test::TestInputAction>>(h.context_entity());
    modal::ModalCloseWatcherSystem<ui_test::TestInputAction> watcher;
    auto config = ModalConfig{}.with_size(pixels(320), pixels(160))
        .with_closed_by(ClosedBy::Any).with_show_close_button(false);
    bool first_open = true;
    auto emit_first = [&] { return afterhours::modal(h.context(), mk(h.root(), 0), first_open, config); };
    emit_first(); h.layout_only(); watcher.once(0);
    h.context().mouse.pos = {10, 10};
    h.context().mouse.just_pressed = true;
    watcher.once(0);
    h.context().mouse.just_pressed = false;
    if (close_between) {
      first_open = false;
      h.begin_frame(); emit_first(); h.layout_only(); watcher.once(0);
    }
    bool next_open = true;
    auto next = afterhours::modal(h.context(), mk(h.root(), 1), next_open, config);
    h.layout_only();
    h.context().mouse.just_released = true;
    watcher.once(0);
    CHECK(!next.ent().get<modal::Modal>().pending_close);
    h.context().mouse.just_released = false;
    h.context().mouse.just_pressed = true;
    watcher.once(0);
    h.context().mouse.just_pressed = false;
    h.context().mouse.just_released = true;
    watcher.once(0);
    CHECK(next.ent().get<modal::Modal>().pending_close);
    EntityHelper::get_default_collection().singletonMap.erase(
        components::get_type_id<UIContext<ui_test::TestInputAction>>());
  }
}

TEST(opening_press_does_not_light_dismiss_its_new_modal) {
  ImmTestHarness h;
  ensure_modal_singleton();
  modal::detail::get_modal_root().modal_stack.clear();
  EntityHelper::registerSingleton<UIContext<ui_test::TestInputAction>>(h.context_entity());
  modal::ModalCloseWatcherSystem<ui_test::TestInputAction> watcher;
  watcher.once(0);
  h.context().mouse.pos = {10, 10};
  h.context().mouse.just_pressed = true;
  bool open = true;
  auto panel = afterhours::modal(h.context(), mk(h.root(), 0), open,
      ModalConfig{}.with_size(pixels(320), pixels(160))
          .with_closed_by(ClosedBy::Any).with_show_close_button(false));
  h.layout_only(); watcher.once(0);
  h.context().mouse.just_pressed = false;
  h.context().mouse.just_released = true;
  watcher.once(0);
  CHECK(!panel.ent().get<modal::Modal>().pending_close);
  EntityHelper::get_default_collection().singletonMap.erase(
      components::get_type_id<UIContext<ui_test::TestInputAction>>());
}

TEST(modal_inherits_the_app_family_and_uses_a_pixel_panel_radius) {
  ui_test::ImmTestHarness h;
  auto &defaults = UIStylingDefaults::get();
  const auto saved_name = defaults.default_font_name;
  const auto saved_size = defaults.default_font_size;
  UIStylingDefaults::get().set_default_font("Interface", pixels(20));
  ensure_modal_singleton();
  bool open = true;
  auto panel = afterhours::modal(h.context(), mk(h.root(), 0), open,
      ModalConfig{}.with_title("Settings"));
  h.layout_only();
  const auto *title = h.find("modal_title");
  CHECK(title != nullptr);
  if (title) {
    CHECK(title->font_name == "Interface");
    CHECK(title->font_weight == colors::FontWeight::Bold);
  }
  CHECK(panel.ent().get<HasRoundedCorners>().radius_px.has_value());
  CHECK_APPROX(panel.ent().get<HasRoundedCorners>().radius_px.value_or(-1.f),
               h.context().theme.panel_corner_radius);
  CHECK(panel.ent().has<HasBorder>());
  defaults.set_default_font(saved_name, saved_size);
}

TEST(explicit_modal_panel_styling_is_preserved) {
  ui_test::ImmTestHarness h;
  ensure_modal_singleton();
  bool open = true;
  const Color custom{80, 34, 62, 255};
  auto panel = afterhours::modal(h.context(), mk(h.root(), 0), open,
      ModalConfig{}.with_panel(ComponentConfig{}
          .with_size({pixels(300), pixels(200)})
          .with_custom_background(custom).with_corner_radius(3.f)));
  CHECK_APPROX(panel.ent().get<HasRoundedCorners>().radius_px.value_or(-1.f), 3.f);
  const auto actual = panel.ent().get<HasColor>().color();
  CHECK(actual.r == custom.r && actual.g == custom.g && actual.b == custom.b &&
        actual.a == custom.a);
  CHECK(!panel.ent().has<HasBorder>());
}

TEST(modal_chrome_inherits_or_overrides_adaptive_scaling) {
  auto &defaults = UIStylingDefaults::get();
  const auto saved_mode = defaults.scaling_mode;
  for (bool context_override : {false, true}) {
    ImmTestHarness h;
    defaults.scaling_mode = context_override ? ScalingMode::Proportional : ScalingMode::Adaptive;
    h.context().scaling_mode = context_override
        ? std::optional{ScalingMode::Adaptive} : std::nullopt;
    auto *resolution = EntityHelper::get_singleton_cmp<window_manager::ProvidesCurrentResolution>();
    const auto saved_resolution = resolution->current_resolution;
    resolution->current_resolution = {1920, 1080};
    ensure_modal_singleton();
    modal::detail::get_modal_root().modal_stack.clear();
    bool open = true;
    afterhours::modal(h.context(), mk(h.root(), 0), open,
        ModalConfig{}.with_size(pixels(420), pixels(400)).with_title("About this theme"));
    h.layout_only(false, {1920, 1080});
    auto *header = h.find("modal_header");
    auto *close = h.find("modal_close");
    auto *title = h.find("modal_title");
    CHECK(header && close && title);
    if (header && close && title) {
      CHECK_APPROX(header->rect().height, 36.f);
      CHECK_APPROX(close->rect().width, 36.f);
      CHECK_APPROX(close->rect().height, 36.f);
      CHECK(title->font_size.dim == Dim::Pixels);
      CHECK_APPROX(title->font_size.value, 25.f);
      CHECK(title->rect().x + title->rect().width <= close->rect().x + 1.f);
    }
    resolution->current_resolution = saved_resolution;
  }
  defaults.scaling_mode = saved_mode;
}

int main() { return ui_test::run_registered_tests("dialog tests"); }
