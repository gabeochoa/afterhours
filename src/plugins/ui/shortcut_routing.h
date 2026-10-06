#pragma once

// Ordered shortcut dispatch: the focused control, then the scopes containing
// it, then the app. The first taker consumes the shortcut, once per frame.
//
// Polling raw keys in app systems bypasses focus, so the same chord can run a
// global command while the user types into a field, or reach a control under
// an open menu. Here a chord -- a mapped InputAction or a raw key -- is
// offered along the focus chain instead:
//
//   1. If an input gate blocks the focused control, it is swallowed.
//   2. From the focused entity up, each ShortcutScope may take it. A control
//      editing text keeps typing and editing keys for itself; a control with
//      ConsumesDirectionalInput keeps Up/Down. An exclusive scope (a dialog)
//      stops it from going any further out.
//   3. Then ShortcutRouter::app, the host command callback, unless an input
//      gate is installed (a modal is open) and app_when_gated is false.
//
// A taken mapped action is cleared from UIContext, so a widget polling
// pressed() later in the frame does not run it again, and a second dispatch
// of the same chord in the frame reports Repeated. Native menus and test
// injection call dispatch() or `app` directly, so every path reaches the
// same callback. What a chord means is the app's business.
//
//   router.app = [&](const ShortcutEvent &e) {
//     if (e.is_key(keys::F, KeyChord::MOD_SUPER)) { open_find(); return true; }
//     return false;
//   };
//   canvas.addComponent<ShortcutScope>([&](const ShortcutEvent &e) {
//     return e.is_key(keys::F, 0) && fire();
//   });

#include <algorithm>
#include <functional>
#include <string_view>
#include <vector>

#include "../../ecs.h"
#include "../input_system.h"
#include "context.h"
#include "text_input/state.h"
#include "text_input/text_area_state.h"

namespace afterhours {
namespace ui {

struct ShortcutEvent {
  int action = -1; // a mapped InputAction's value, or -1 for a raw key
  int key = 0;     // raw key code when action is -1
  uint8_t modifiers = 0; // input::KeyChord::MOD_* held
  bool repeat = false;

  template <typename Action>
  static ShortcutEvent of(Action a, uint8_t modifiers = 0, bool repeat = false) {
    return {static_cast<int>(a), 0, modifiers, repeat};
  }
  static ShortcutEvent raw(int key, uint8_t modifiers = 0, bool repeat = false) {
    return {-1, key, modifiers, repeat};
  }
  template <typename Action> [[nodiscard]] bool is(Action a) const {
    return action == static_cast<int>(a);
  }
  [[nodiscard]] bool is_key(int k, uint8_t mods) const {
    return action < 0 && key == k && modifiers == mods;
  }
  bool operator==(const ShortcutEvent &) const = default;
};

/// Who took a shortcut. Control, Scope, App and Blocked consume it; the rest
/// leave a mapped action in UIContext for widgets to poll.
enum struct ShortcutRoute {
  Unhandled, // nobody took it
  Control,   // the focused control's own scope
  Scope,     // a scope containing the focused control
  App,       // the app callback
  Editing,   // focus is editing text or owns the arrows: left for that widget
  Contained, // an exclusive scope stopped it, untaken, inside itself
  Blocked,   // an input gate owns input; swallowed
  Repeated,  // already dispatched this frame
};

/// Offers shortcuts to the entity's subtree. Return true to take one.
struct ShortcutScope : BaseComponent {
  std::function<bool(const ShortcutEvent &)> handle;
  // A dialog or overlay: untaken shortcuts stop here rather than reaching
  // the scopes and app behind it.
  bool exclusive = false;

  ShortcutScope() = default;
  explicit ShortcutScope(std::function<bool(const ShortcutEvent &)> fn,
                         bool exclusive_ = false)
      : handle(std::move(fn)), exclusive(exclusive_) {}
};

namespace detail {
// Keys a focused text field means for itself: typing (no Ctrl/Super), and
// caret movement and deletion with any modifier, Option+Left included.
inline bool text_editing_key(const ShortcutEvent &e) {
  if (e.key == keys::LEFT || e.key == keys::RIGHT || e.key == keys::UP ||
      e.key == keys::DOWN || e.key == keys::HOME || e.key == keys::END ||
      e.key == keys::PAGE_UP || e.key == keys::PAGE_DOWN ||
      e.key == keys::BACKSPACE || e.key == keys::DELETE_KEY)
    return true;
  const uint8_t command = input::KeyChord::MOD_CTRL | input::KeyChord::MOD_SUPER;
  return e.key >= 32 && e.key < 256 && !(e.modifiers & command);
}

template <typename Action>
bool owns_shortcut(const Entity &entity, bool focused, const ShortcutEvent &e) {
  const std::string_view name =
      e.action >= 0 ? magic_enum::enum_name(static_cast<Action>(e.action)) : "";
  if (entity.has<text_input::HasTextInputState>() ||
      entity.has<text_input::HasTextAreaState>()) {
    if (e.action < 0 ? text_editing_key(e)
                     : name.starts_with("Text") || name == "WidgetLeft" ||
                           name == "WidgetRight")
      return true;
  }
  return focused && entity.has<ConsumesDirectionalInput>() &&
         (e.action < 0 ? e.key == keys::UP || e.key == keys::DOWN
                       : name == "WidgetUp" || name == "WidgetDown");
}
} // namespace detail

struct ShortcutRouter : BaseComponent {
  // The host command callback: the last stop, and what native menus call.
  std::function<bool(const ShortcutEvent &)> app;
  // Let app shortcuts through while an input gate (a modal) is installed.
  bool app_when_gated = false;
  // Raw chords pressed_chords() polls for dispatch_frame.
  std::vector<input::KeyChord> watched;
  std::vector<ShortcutEvent> dispatched; // this frame

  void begin_frame() { dispatched.clear(); }

  template <typename Action>
  ShortcutRoute route(UIContext<Action> &ctx, const ShortcutEvent &e) const {
    if (ctx.focus_id != ctx.ROOT && !ctx.is_input_allowed(ctx.focus_id))
      return ShortcutRoute::Blocked;
    EntityID current = ctx.focus_id;
    for (int depth = 0; current >= 0 && depth < 64; depth++) {
      OptEntity opt = UICollectionHolder::getEntityForID(current);
      if (!opt.valid())
        break;
      Entity &entity = opt.asE();
      const bool focused = current == ctx.focus_id;
      if (entity.has<ShortcutScope>() && ctx.is_input_allowed(current)) {
        auto &scope = entity.get<ShortcutScope>();
        if (scope.handle && scope.handle(e))
          return focused ? ShortcutRoute::Control : ShortcutRoute::Scope;
      }
      if (detail::owns_shortcut<Action>(entity, focused, e))
        return ShortcutRoute::Editing;
      if (entity.has<ShortcutScope>() && entity.get<ShortcutScope>().exclusive)
        return ShortcutRoute::Contained;
      if (!entity.has<UIComponent>())
        break;
      current = entity.get<UIComponent>().parent;
    }
    if (app && (ctx.input_gates.empty() || app_when_gated) && app(e))
      return ShortcutRoute::App;
    return ShortcutRoute::Unhandled;
  }

  /// Offer one shortcut along the focus chain.
  template <typename Action>
  ShortcutRoute dispatch(UIContext<Action> &ctx, const ShortcutEvent &e) {
    if (std::find(dispatched.begin(), dispatched.end(), e) != dispatched.end())
      return ShortcutRoute::Repeated;
    dispatched.push_back(e);
    const ShortcutRoute taken = route(ctx, e);
    if (e.action >= 0 && (taken == ShortcutRoute::Control || taken == ShortcutRoute::Scope ||
                          taken == ShortcutRoute::App || taken == ShortcutRoute::Blocked)) {
      const auto action = static_cast<Action>(e.action);
      if (ctx.last_action == action)
        ctx.last_action = Action::None;
      if (const auto idx = magic_enum::enum_index(action))
        ctx.all_actions_repeat[*idx] = false;
    }
    return taken;
  }

  /// Offer this frame's input: the mapped action pressed, held actions
  /// repeating, then the raw chords in `raw` (see pressed_chords).
  template <typename Action>
  void dispatch_frame(UIContext<Action> &ctx, const std::vector<ShortcutEvent> &raw,
                      uint8_t held_modifiers) {
    const Action pressed = ctx.last_action;
    if (pressed != Action::None)
      dispatch(ctx, ShortcutEvent::of(pressed, held_modifiers));
    for (const Action a : magic_enum::enum_values<Action>())
      if (a != pressed && ctx.all_actions_repeat[*magic_enum::enum_index(a)])
        dispatch(ctx, ShortcutEvent::of(a, held_modifiers, true));
    for (const auto &e : raw)
      dispatch(ctx, e);
  }
};

/// The watched chords pressed (or repeating) this frame, as raw events.
inline std::vector<ShortcutEvent> pressed_chords(const std::vector<input::KeyChord> &watched,
                                                 uint8_t held_modifiers) {
  std::vector<ShortcutEvent> out;
  for (const auto &chord : watched) {
    if (chord.has_explicit_modifiers && chord.required_modifiers != held_modifiers)
      continue;
    if (input::is_key_pressed(chord.key))
      out.push_back(ShortcutEvent::raw(chord.key, held_modifiers));
    else if (input::is_key_pressed_repeat(chord.key))
      out.push_back(ShortcutEvent::raw(chord.key, held_modifiers, true));
  }
  return out;
}

/// Runs the router on the context entity each frame. Register it after
/// BeginUIContextManager and before the systems that build screens, so
/// shortcuts are taken before widgets poll the same actions.
template <typename Action>
struct RouteShortcuts : System<UIContext<Action>, ShortcutRouter> {
  void for_each_with(Entity &, UIContext<Action> &ctx, ShortcutRouter &router,
                     float) override {
    router.begin_frame();
    const uint8_t held = input::get_current_modifiers();
    router.dispatch_frame(ctx, pressed_chords(router.watched, held), held);
  }
};

} // namespace ui
} // namespace afterhours
