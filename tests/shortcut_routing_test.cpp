// shortcut_routing_test.cpp
// One chord, one owner, once per frame: the focused control, then its
// scopes, then the app, with typing, arrows, dialogs and input gates each
// keeping what is theirs.

#include "ui_test_harness.h"
#include <afterhours/src/plugins/ui/shortcut_routing.h>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;
using Action = ui_test::TestInputAction;
using Chord = input::KeyChord;

namespace {
struct Counter {
  std::vector<ShortcutEvent> seen;
  std::function<bool(const ShortcutEvent &)> take(std::function<bool(const ShortcutEvent &)> wants) {
    return [this, wants](const ShortcutEvent &e) {
      if (!wants(e)) return false;
      seen.push_back(e);
      return true;
    };
  }
};

EntityID node(ImmTestHarness &h, Entity &parent, int slot) {
  return div(h.context(), mk(parent, slot), ComponentConfig{}.with_size({pixels(50), pixels(20)}))
      .ent().id;
}
Entity &ent(EntityID id) { return UICollectionHolder::getEntityForIDEnforce(id); }
} // namespace

TEST(typing_in_a_field_over_a_reader_or_canvas_keeps_its_keys) {
  ImmTestHarness h;
  auto &ctx = h.context();
  const EntityID canvas = node(h, h.root(), 0);
  const EntityID input = node(h, ent(canvas), 0); // a field drawn over the canvas
  const EntityID field = node(h, ent(input), 0);  // the focusable inner field
  ent(input).addComponent<text_input::HasTextInputState>();
  const EntityID overlay = node(h, h.root(), 1);  // a field in a sibling panel
  const EntityID area = node(h, ent(overlay), 0);
  ent(area).addComponent<text_input::HasTextAreaState>();

  Counter fire, find, back;
  ent(canvas).addComponent<ShortcutScope>(fire.take([](auto &e) { return e.is_key(keys::F, 0); }));
  ShortcutRouter router;
  router.app = [&](const ShortcutEvent &e) {
    if (e.is_key(keys::F, Chord::MOD_SUPER)) { find.seen.push_back(e); return true; }
    if (e.is_key(keys::LEFT, Chord::MOD_ALT)) { back.seen.push_back(e); return true; }
    return false;
  };

  for (const EntityID typing_in : {field, area}) {
    ctx.focus_id = typing_in;
    router.begin_frame();
    CHECK(router.dispatch(ctx, ShortcutEvent::raw(keys::F)) == ShortcutRoute::Editing);
    CHECK(router.dispatch(ctx, ShortcutEvent::raw(keys::F, Chord::MOD_SHIFT)) == ShortcutRoute::Editing);
    // Option+Left is word-left while editing, not the app's Back.
    CHECK(router.dispatch(ctx, ShortcutEvent::raw(keys::LEFT, Chord::MOD_ALT)) == ShortcutRoute::Editing);
    // A held command modifier makes it a command again.
    CHECK(router.dispatch(ctx, ShortcutEvent::raw(keys::F, Chord::MOD_SUPER)) == ShortcutRoute::App);
    // Editing actions stay queued for the field to poll.
    ctx.last_action = Action::TextWordLeft;
    router.dispatch_frame(ctx, {}, Chord::MOD_ALT);
    CHECK(ctx.pressed(Action::TextWordLeft));
  }
  CHECK(fire.seen.empty());
  CHECK(back.seen.empty());
  CHECK(find.seen.size() == 2);

  ctx.focus_id = canvas;
  router.begin_frame();
  CHECK(router.dispatch(ctx, ShortcutEvent::raw(keys::F)) == ShortcutRoute::Control);
  CHECK(router.dispatch(ctx, ShortcutEvent::raw(keys::LEFT, Chord::MOD_ALT)) == ShortcutRoute::App);
  CHECK(fire.seen.size() == 1);
  CHECK(back.seen.size() == 1);
}

TEST(a_taken_action_runs_once_per_frame) {
  ImmTestHarness h;
  auto &ctx = h.context();
  const EntityID viewer = node(h, h.root(), 0);
  const EntityID later_field = node(h, h.root(), 1);
  Counter copy;
  ent(viewer).addComponent<ShortcutScope>(copy.take([](auto &e) { return e.is(Action::TextCopy); }));
  ShortcutRouter router;
  int app_calls = 0;
  router.app = [&](const ShortcutEvent &) { app_calls++; return true; };

  ctx.focus_id = viewer;
  ctx.last_action = Action::TextCopy;
  router.begin_frame();
  router.dispatch_frame(ctx, {}, Chord::MOD_SUPER);
  // Another system offering the same chord this frame gets nothing.
  CHECK(router.dispatch(ctx, ShortcutEvent::of(Action::TextCopy, Chord::MOD_SUPER)) ==
        ShortcutRoute::Repeated);
  // A field focused later in the frame cannot run the copy again.
  ctx.focus_id = later_field;
  CHECK(!ctx.pressed(Action::TextCopy));
  CHECK(copy.seen.size() == 1);
  CHECK(app_calls == 0);

  // Next frame is a new press.
  ctx.focus_id = viewer;
  ctx.last_action = Action::TextCopy;
  router.begin_frame();
  router.dispatch_frame(ctx, {}, Chord::MOD_SUPER);
  CHECK(copy.seen.size() == 2);

  // Held actions repeat through the same route, flagged as repeats.
  Counter step;
  ent(viewer).get<ShortcutScope>().handle = step.take([](auto &e) { return e.is(Action::WidgetDown); });
  ctx.last_action = Action::None;
  ctx.all_actions_repeat[*magic_enum::enum_index(Action::WidgetDown)] = true;
  router.begin_frame();
  router.dispatch_frame(ctx, {}, 0);
  CHECK(step.seen.size() == 1 && step.seen[0].repeat);
  CHECK(!ctx.pressed_or_repeat(Action::WidgetDown));
}

TEST(nested_dialogs_close_one_at_a_time_and_contain_their_input) {
  ImmTestHarness h;
  auto &ctx = h.context();
  const EntityID outer = node(h, h.root(), 0);
  const EntityID outer_button = node(h, ent(outer), 0);
  const EntityID inner = node(h, ent(outer), 1);
  const EntityID inner_button = node(h, ent(inner), 0);
  Counter close_outer, close_inner;
  const auto escape = [](const ShortcutEvent &e) { return e.is(Action::MenuBack); };
  ent(outer).addComponent<ShortcutScope>(close_outer.take(escape), true);
  ent(inner).addComponent<ShortcutScope>(close_inner.take(escape), true);
  ShortcutRouter router;
  int app_calls = 0;
  router.app = [&](const ShortcutEvent &) { app_calls++; return true; };

  ctx.focus_id = inner_button;
  ctx.last_action = Action::MenuBack;
  router.begin_frame();
  router.dispatch_frame(ctx, {}, 0);
  CHECK(close_inner.seen.size() == 1);
  CHECK(close_outer.seen.empty());

  ctx.focus_id = outer_button; // the inner dialog closed and returned focus
  ctx.last_action = Action::MenuBack;
  router.begin_frame();
  router.dispatch_frame(ctx, {}, 0);
  CHECK(close_outer.seen.size() == 1);

  // Tab is no dialog's: it stops inside and stays queued for tabbing.
  ctx.last_action = Action::WidgetNext;
  router.begin_frame();
  CHECK(router.dispatch(ctx, ShortcutEvent::of(Action::WidgetNext)) == ShortcutRoute::Contained);
  CHECK(ctx.last_action == Action::WidgetNext);
  CHECK(app_calls == 0);
}

TEST(input_gates_and_directional_owners_hold_their_input) {
  ImmTestHarness h;
  auto &ctx = h.context();
  const EntityID page = node(h, h.root(), 0);
  const EntityID behind = node(h, ent(page), 0);
  const EntityID menu = node(h, h.root(), 1);
  const EntityID spinner = node(h, ent(page), 1);
  Counter page_keys;
  ent(page).addComponent<ShortcutScope>(page_keys.take([](auto &) { return true; }));
  ShortcutRouter router;
  int app_calls = 0;
  router.app = [&](const ShortcutEvent &) { app_calls++; return true; };

  // A menu gate: focus stranded behind it must not activate anything.
  ctx.add_input_gate("menu", [&](EntityID id) { return ctx.contains_in_subtree(menu, id); });
  ctx.focus_id = behind;
  ctx.last_action = Action::WidgetPress;
  router.begin_frame();
  router.dispatch_frame(ctx, {}, 0);
  CHECK(!ctx.pressed(Action::WidgetPress));
  CHECK(page_keys.seen.empty());
  // Nothing focused, gate installed: app commands wait for the modal...
  ctx.focus_id = ctx.ROOT;
  CHECK(router.dispatch(ctx, ShortcutEvent::raw(keys::F, Chord::MOD_SUPER)) == ShortcutRoute::Unhandled);
  CHECK(app_calls == 0);
  // ...unless the app says its commands work under one.
  router.app_when_gated = true;
  CHECK(router.dispatch(ctx, ShortcutEvent::raw(keys::Q, Chord::MOD_SUPER)) == ShortcutRoute::App);
  ctx.remove_input_gate("menu");

  ent(spinner).addComponent<ConsumesDirectionalInput>();
  ctx.focus_id = spinner;
  router.begin_frame();
  CHECK(router.dispatch(ctx, ShortcutEvent::of(Action::WidgetDown)) == ShortcutRoute::Editing);
  CHECK(router.dispatch(ctx, ShortcutEvent::raw(keys::UP)) == ShortcutRoute::Editing);
  CHECK(router.dispatch(ctx, ShortcutEvent::of(Action::WidgetNext)) == ShortcutRoute::Scope);
  CHECK(page_keys.seen.size() == 1);
}

// The per-frame system has to compile against a real InputAction enum; it
// polls the backend for modifiers, which the headless backend cannot answer.
[[maybe_unused]] static RouteShortcuts<Action> route_system;

int main() { return ui_test::run_registered_tests("shortcut_routing"); }
