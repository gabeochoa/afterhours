#pragma once

// TODO: Consider using C++20 concepts for type constraints in this plugin.
// See e2e_testing/concepts.h for examples (HasPosition, MouseStateLike, etc.)
// Potential uses:
// - Concept for mouse position types (instead of hard-coding
// input::MousePosition)
// - Concept for rectangle types (instead of RectangleType)
// - Concept for input action enums (instead of template parameter)

#include <algorithm>
#include <bitset>
#include <cmath>
#include <functional>
#if __has_include(<magic_enum/magic_enum.hpp>)
#include <magic_enum/magic_enum.hpp>
#else
#include "../../../vendor/magic_enum/magic_enum.hpp"
#endif
#include <set>
#include <vector>

#include "../../drawing_helpers.h"
#include "../../ecs.h"
#include "../../logging.h"
#include "../input_system.h"
#include "components.h"
#include "theme.h"
#include "ui_collection.h"
// UIComponent, for the parent-chain walk in contains_in_subtree. Not pulled in
// transitively: a TU including ui.h directly reaches context.h first.
#include "ui_core_components.h"

namespace afterhours {

namespace ui {

/// Every action name the UI plugin looks up. Apps supplying their own
/// InputAction enum must define all of these; use this one unless you need UI
/// actions fused with game bindings. Pinned by tests/setup_test.cpp.
enum struct DefaultAction {
  None,
  // Focus movement and activation.
  WidgetMod,
  WidgetNext,
  WidgetBack,
  WidgetPress,
  WidgetLeft,
  WidgetRight,
  WidgetUp,
  WidgetDown,
  MenuBack,
  // Text editing — referenced by the text_input/text_area systems.
  TextBackspace,
  TextCopy,
  TextCut,
  TextDelete,
  TextDeleteWordBack,
  TextDeleteWordForward,
  TextEnd,
  TextHome,
  TextPaste,
  TextRedo,
  TextSelectAll,
  TextSelectLeft,
  TextSelectRight,
  TextUndo,
  TextWordLeft,
  TextWordRight,
};

static inline bool is_mouse_inside(const input::MousePosition &mouse_pos,
                                   const RectangleType &rect) {
  return mouse_pos.x >= rect.x && mouse_pos.x <= rect.x + rect.width &&
         mouse_pos.y >= rect.y && mouse_pos.y <= rect.y + rect.height;
}

struct RenderInfo {
  EntityID id;
  int layer = 0;
};

// Mark an element as owning WidgetUp/WidgetDown while it is focused, so the
// arrows do whatever the widget means by them -- adjust a value (spinbox),
// step a list (tray) -- instead of moving focus off it.
//
// NOTE: there is no ValueUp/ValueDown action pair. Navigation and value
// adjustment are the SAME two keys, and this component is the only thing
// telling them apart. If dedicated value actions are ever added, they collide
// with this and the collision is silent, because process_tabbing resolves
// actions with `if constexpr` on their names.
struct ConsumesDirectionalInput : BaseComponent {};

// Former name, from when a tray stepping its children had to pretend to be a
// value widget to get the arrow keys.
using AcceptsValueInput = ConsumesDirectionalInput;

struct MousePointerState {
  // pos and delta are in the same space as UIComponent::rect() -- letterbox
  // and resolution scaling are already applied. Callers reaching for the
  // backend's raw mouse instead have to redo that conversion by hand.
  input::MousePosition pos{};
  // Movement since the previous frame. Derived from pos rather than the
  // backend's mouse delta, which is raw window pixels. Zero when there is no
  // cursor, and no dead zone, unlike moved_this_frame below.
  input::MousePosition delta{};
  bool left_down = false;
  bool just_pressed = false;
  bool just_released = false;
  // Secondary button. Kept separate rather than generalising to an array: the
  // left button drives hot/active/drag and the right one only ever asks "was
  // there a click on this, and where", so sharing the machinery would mean
  // teaching all of it which button it is talking about.
  bool right_down = false;
  bool right_just_pressed = false;
  bool right_just_released = false;
  input::MousePosition press_pos{};
  bool press_moved = false;
  bool moved_this_frame = false; // pos changed since last frame (any button)
  static constexpr float press_drag_threshold_px = 6.0f;
  // Below this, the cursor is parked. An exact compare made sub-pixel jitter
  // (a hi-DPI position rescaled to render space flickers in the last mantissa
  // bit) read as intent, which is enough to yank focus away from the keyboard.
  static constexpr float move_dead_zone_px = 2.0f;

  // A distance test rather than `!=`, because the exact compare counted
  // sub-pixel jitter as intent. The two non-finite cases have to be handled
  // explicitly or NaN decides by accident: with no cursor at all (headless)
  // `!=` reported movement on every frame forever, while a plain distance test
  // would let one NaN frame poison `prev` and report *no* movement forever.
  [[nodiscard]] bool moved_since(const input::MousePosition &prev) const {
    const bool now_ok = std::isfinite(pos.x) && std::isfinite(pos.y);
    if (!now_ok)
      return false; // no cursor: not moving
    if (!std::isfinite(prev.x) || !std::isfinite(prev.y))
      return true; // a cursor just appeared
    const float dx = pos.x - prev.x, dy = pos.y - prev.y;
    return dx * dx + dy * dy > move_dead_zone_px * move_dead_zone_px;
  }
};

// Who last claimed focus this frame. Hover-follow must not overwrite a focus
// move that something asked for on purpose (a game's own arrow-key nav, a test
// harness, process_tabbing) -- but it must still win over `Grab`, the ordinary
// per-frame re-grab in try_to_grab, which happens every frame regardless.
enum struct FocusSource {
  Grab,
  Pointer,
  Explicit
};

// Forward declaration - full type in styling_defaults.h
namespace imm {
struct UIStylingDefaults;
} // namespace imm

template <typename InputAction> struct UIContext : BaseComponent {
  using value_type = InputAction;

  // TODO move to input system
  using InputBitset = std::bitset<magic_enum::enum_count<InputAction>()>;

  EntityID ROOT = -1;
  EntityID FAKE = -2;

  std::set<EntityID> focused_ids;

  EntityID hot_id = ROOT;          // hot means the mouse is over this element
  // Elements already warned about in is_right_click. Mutable so the check can
  // stay const, like the rest of the query helpers around it.
  mutable std::set<EntityID> right_click_warned;
  EntityID prev_hot_id = ROOT;     // previous frame's hot_id (for animations)
  EntityID focus_id = ROOT;        // current actual focused element
  EntityID visual_focus_id = ROOT; // the element the ring should be drawn on
  EntityID active_id =
      ROOT; // active means the element is being interacted with
  EntityID prev_active_id = ROOT; // previous frame's active_id (for animations)
  EntityID last_processed =
      ROOT; // last element that was processed (used for reverse tabbing)
  // Reset to Grab each frame in BeginUIContextManager; see FocusSource.
  FocusSource focus_source = FocusSource::Grab;

  MousePointerState mouse;
  InputAction last_action;
  uint8_t last_action_modifiers = 0;
  InputBitset all_actions;
  InputBitset all_actions_repeat;

  // Screen dimensions in pixels, set each frame by the layout system.
  // Used by rendering to resolve ScreenPercent font sizes.
  float screen_width = 0.f;
  float screen_height = 0.f;

  Theme theme;

  // Per-screen scaling mode override. If set, overrides the app-wide default
  // from UIStylingDefaults. Components can further override via
  // ComponentConfig::with_scaling_mode().
  std::optional<ScalingMode> scaling_mode;

  // Convenience accessor to the UIStylingDefaults singleton.
  // Defined in component_init.h (which has the full type).
  static imm::UIStylingDefaults &styling_defaults();

  // Delta time for animation updates (set each frame)
  float dt = 0.0f;

  // Input gates - systems can register functions that control whether
  // an entity should receive input. All gates must return true for input
  // to be allowed. Use add_input_gate/remove_input_gate to manage.
  using InputGate = std::function<bool(EntityID)>;
  std::vector<std::pair<std::string, InputGate>> input_gates;

  // Add a named input gate (returns false to block input for an entity)
  void add_input_gate(const std::string &name, InputGate gate) {
    // Remove existing gate with same name first to avoid duplicates
    remove_input_gate(name);
    input_gates.emplace_back(name, std::move(gate));
  }

  // Remove an input gate by name
  void remove_input_gate(const std::string &name) {
    input_gates.erase(
        std::remove_if(input_gates.begin(), input_gates.end(),
                       [&](const auto &pair) { return pair.first == name; }),
        input_gates.end());
  }

  // Check if all gates allow input for this entity
  [[nodiscard]] bool is_input_allowed(EntityID id) const {
    return std::all_of(input_gates.begin(), input_gates.end(),
                       [id](const auto &pair) { return pair.second(id); });
  }

  [[nodiscard]] bool is_hot(EntityID id) const { return hot_id == id; };
  [[nodiscard]] bool is_active(EntityID id) const { return active_id == id; };
  // For animations: check previous frame's state (since current frame state
  // isn't set until HandleClicks runs after screen rendering)
  [[nodiscard]] bool was_hot(EntityID id) const { return prev_hot_id == id; };
  [[nodiscard]] bool was_active(EntityID id) const {
    return prev_active_id == id;
  };
  void set_hot(EntityID id) { hot_id = id; }
  void set_active(EntityID id) { active_id = id; }

  bool has_focus(EntityID id) const { return focus_id == id; }
  void set_focus(EntityID id, FocusSource src = FocusSource::Explicit) {
    focus_id = id;
    focus_source = src;
  }

  // Walks UP: the tree is cleared each frame, so a caller asking before it has
  // re-added its children would see an empty subtree. Bounded to avoid a hang.
  [[nodiscard]] bool contains_in_subtree(EntityID ancestor,
                                         EntityID descendant) const {
    EntityID cur = descendant;
    for (int depth = 0; depth < 64 && cur != -1; depth++) {
      if (cur == ancestor)
        return true;
      OptEntity opt = UICollectionHolder::getEntityForID(cur);
      if (!opt.has_value() || !opt.asE().template has<UIComponent>())
        return false;
      cur = opt.asE().template get<UIComponent>().parent;
    }
    return false;
  }

  /// Mouse over this element or anything inside it. One global hot_id means a
  /// hoverable child otherwise steals its parent row's hover fill.
  [[nodiscard]] bool mouse_in_subtree(EntityID id) const {
    return contains_in_subtree(id, hot_id);
  }
  /// A secondary click finished over this element or anything inside it --
  /// what a context menu opens on. Pair it with `mouse.pos` for the anchor:
  ///
  ///   if (ctx.is_right_click(row.ent().id)) { at = ctx.mouse.pos; open = true; }
  ///
  /// Uses last frame's hot, like mouse_was_in_subtree, because a screen asks
  /// this while it is being rebuilt and hot_id is not resolved until after.
  ///
  /// The element or a descendant must be hit-testable -- carry a click or drag
  /// listener -- because that is what hot resolves against. Asking about a
  /// plain div is the obvious first thing to try and would otherwise never
  /// fire, so it says so rather than quietly answering no.
  [[nodiscard]] bool is_right_click(EntityID id) const {
    if (!mouse.right_just_released)
      return false;
    if (contains_in_subtree(id, prev_hot_id))
      return true;
    warn_if_not_hit_testable(id);
    return false;
  }
  /// Previous frame's answer. Use this while building a screen: hot_id is not
  /// resolved until after.
  [[nodiscard]] bool mouse_was_in_subtree(EntityID id) const {
    return contains_in_subtree(id, prev_hot_id);
  }
  /// Does focus sit on this element or anything inside it?
  [[nodiscard]] bool focus_in_subtree(EntityID id) const {
    return contains_in_subtree(id, focus_id);
  }

  void active_if_mouse_inside(EntityID id, RectangleType rect) {
    // Check if input is blocked for this element (e.g., by a modal)
    if (!is_input_allowed(id)) {
      return;
    }

    if (is_mouse_inside(mouse.pos, rect)) {
      set_hot(id);
      if (is_active(ROOT) && mouse.left_down) {
        set_active(id);
      }
    }
  }

  void reset() {
    focus_id = ROOT;
    visual_focus_id = ROOT;
    last_processed = ROOT;
    hot_id = ROOT;
    active_id = ROOT;
    focused_ids.clear();
    render_cmds.clear();
    input_gates.clear();
  }

  void try_to_grab(EntityID id) {
    focused_ids.insert(id);
    if (has_focus(ROOT)) {
      // Grab, not Explicit: this runs every frame for whichever widget happens
      // to be first, so counting it as intent would disable hover-follow.
      set_focus(id, FocusSource::Grab);
    }
  }

  /// Once per element. A right-click on a non-hit-testable element is a silent
  /// no-op otherwise, and the caller has nothing to go on.
  void warn_if_not_hit_testable(EntityID id) const {
    OptEntity opt = UICollectionHolder::getEntityForID(id);
    if (!opt.has_value())
      return;
    const Entity &e = opt.asE();
    if (e.has<HasClickListener>() || e.has<HasDragListener>())
      return; // hot could have landed here; the click was simply elsewhere
    if (!right_click_warned.insert(id).second)
      return;
    log_warn("is_right_click({}) will never fire: this element has no click or "
             "drag listener, so hit-testing never selects it. Put the check on "
             "the button/row the user actually clicks.",
             id);
  }

  [[nodiscard]] bool is_mouse_press(EntityID id) const {
    bool was_press =
        mouse.just_pressed && is_active(id) && is_hot(id) && !mouse.press_moved;
    return was_press;
  }

  [[nodiscard]] bool is_mouse_click(EntityID id) const {
    bool was_click = mouse.just_released && is_active(id) && is_hot(id) &&
                     !mouse.press_moved;
    // if(was_click){play_sound();}
    return was_click;
  }

  [[nodiscard]] bool mouse_activates(EntityID id) const {
    ClickActivationMode activation_mode = theme.click_activation_mode;
    if (OptEntity opt_ent = UICollectionHolder::getEntityForID(id);
        opt_ent.has_value() && opt_ent.asE().has<HasClickActivationMode>()) {
      activation_mode = opt_ent.asE().get<HasClickActivationMode>().mode;
    }
    return activation_mode == ClickActivationMode::Press ? is_mouse_press(id)
                                                         : is_mouse_click(id);
  }

  [[nodiscard]] bool pressed(const InputAction &name) {
    bool a = last_action == name;
    if (a) {
      // ui::sounds::select();
      last_action = InputAction::None;
    }
    return a;
  }

  [[nodiscard]] bool pressed_exact(const InputAction &name) {
    if (last_action != name) return false;
    if (input::get_current_modifiers() != last_action_modifiers) return false;
    last_action = InputAction::None;
    return true;
  }

  [[nodiscard]] bool pressed_or_repeat(const InputAction &name) {
    if (pressed(name)) return true;
    size_t idx = magic_enum::enum_index<InputAction>(name).value();
    bool a = all_actions_repeat[idx];
    if (a) all_actions_repeat[idx] = false;
    return a;
  }

  [[nodiscard]] bool is_held_down(const InputAction &name) {
    bool a = all_actions[magic_enum::enum_index<InputAction>(name).value()];
    if (a) {
      // ui::sounds::select();
      all_actions[magic_enum::enum_index<InputAction>(name).value()] = false;
    }
    return a;
  }

  // Does the focused element own WidgetUp/WidgetDown? When true they belong to
  // it and must not move focus.
  [[nodiscard]] bool focused_consumes_directional_input() const {
    if (focus_id == ROOT)
      return false;
    OptEntity opt = UICollectionHolder::getEntityForID(focus_id);
    return opt.valid() &&
           opt.asE().template has<ConsumesDirectionalInput>();
  }

  void process_tabbing(EntityID id) {
    if (has_focus(id)) {
      // Arrow Up/Down move focus just like Tab / Shift+Tab, EXCEPT when the
      // focused element owns them (a tray stepping its children, a spinbox
      // adjusting a value) or the app has taken them over wholesale.
      const bool arrows_navigate =
          theme.arrows_tab && !focused_consumes_directional_input();
      if constexpr (magic_enum::enum_contains<InputAction>("WidgetNext")) {
        bool forward = pressed(InputAction::WidgetNext);
        if constexpr (magic_enum::enum_contains<InputAction>("WidgetDown")) {
          if (arrows_navigate && pressed(InputAction::WidgetDown))
            forward = true;
        }
        if (forward) {
          set_focus(ROOT);
          if constexpr (magic_enum::enum_contains<InputAction>("WidgetMod")) {
            if (is_held_down(InputAction::WidgetMod)) {
              set_focus(last_processed);
            }
          }
        }
      }
      if constexpr (magic_enum::enum_contains<InputAction>("WidgetBack")) {
        bool backward = pressed(InputAction::WidgetBack);
        if constexpr (magic_enum::enum_contains<InputAction>("WidgetUp")) {
          if (arrows_navigate && pressed(InputAction::WidgetUp))
            backward = true;
        }
        if (backward) {
          set_focus(last_processed);
        }
      }
    }
    // before any returns
    last_processed = id;
  }

  std::vector<RenderInfo> render_cmds;

  void queue_render(RenderInfo &&info) { render_cmds.emplace_back(info); }
};

using DefaultUIContext = UIContext<DefaultAction>;

} // namespace ui

} // namespace afterhours
