#pragma once

#include <algorithm>
#include <bitset>
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

namespace afterhours {

namespace ui {

static inline bool is_mouse_inside(const input::MousePosition &mouse_pos,
                                   const RectangleType &rect) {
  return mouse_pos.x >= rect.x && mouse_pos.x <= rect.x + rect.width &&
         mouse_pos.y >= rect.y && mouse_pos.y <= rect.y + rect.height;
}

struct RenderInfo {
  EntityID id;
  int layer = 0;
};

struct MousePointerState {
  input::MousePosition pos{};
  bool left_down = false;
  bool just_pressed = false;
  bool just_released = false;
  input::MousePosition press_pos{};
  bool press_moved = false;
  static constexpr float press_drag_threshold_px = 6.0f;
};

template <typename InputAction> struct UIContext : BaseComponent {
  using value_type = InputAction;

  // TODO move to input system
  using InputBitset = std::bitset<magic_enum::enum_count<InputAction>()>;

  EntityID ROOT = -1;
  EntityID FAKE = -2;

  std::set<EntityID> focused_ids;

  EntityID hot_id = ROOT;          // hot means the mouse is over this element
  EntityID focus_id = ROOT;        // current actual focused element
  EntityID visual_focus_id = ROOT; // the element the ring should be drawn on
  EntityID active_id =
      ROOT; // active means the element is being interacted with
  EntityID last_processed =
      ROOT; // last element that was processed (used for reverse tabbing)

  MousePointerState mouse;
  InputAction last_action;
  InputBitset all_actions;

  Theme theme;
  // TODO: Add styling defaults back when circular dependency is resolved
  // imm::UIStylingDefaults styling_defaults;

  [[nodiscard]] bool is_hot(EntityID id) const { return hot_id == id; };
  [[nodiscard]] bool is_active(EntityID id) const { return active_id == id; };
  void set_hot(EntityID id) { hot_id = id; }
  void set_active(EntityID id) { active_id = id; }

  bool has_focus(EntityID id) const { return focus_id == id; }
  void set_focus(EntityID id) { focus_id = id; }

  void active_if_mouse_inside(EntityID id, RectangleType rect) {
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
  }

  void try_to_grab(EntityID id) {
    focused_ids.insert(id);
    if (has_focus(ROOT)) {
      set_focus(id);
    }
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
    if (OptEntity opt_ent = EntityHelper::getEntityForID(id);
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

  [[nodiscard]] bool is_held_down(const InputAction &name) {
    bool a = all_actions[magic_enum::enum_index<InputAction>(name).value()];
    if (a) {
      // ui::sounds::select();
      all_actions[magic_enum::enum_index<InputAction>(name).value()] = false;
    }
    return a;
  }

  void process_tabbing(EntityID id) {
    // TODO How do we handle something that wants to use
    // Widget Value Down/Up to control the value?
    // Do we mark the widget type with "nextable"? (tab will always work but
    // not very discoverable

    if (has_focus(id)) {
      if constexpr (magic_enum::enum_contains<InputAction>("WidgetNext")) {
        if (pressed(InputAction::WidgetNext)) {
          set_focus(ROOT);
          if constexpr (magic_enum::enum_contains<InputAction>("WidgetMod")) {
            if (is_held_down(InputAction::WidgetMod)) {
              set_focus(last_processed);
            }
          }
        }
      }
      if constexpr (magic_enum::enum_contains<InputAction>("WidgetBack")) {
        if (pressed(InputAction::WidgetBack)) {
          set_focus(last_processed);
        }
      }
    }
    // before any returns
    last_processed = id;
  }

  mutable std::vector<RenderInfo> render_cmds;

  void queue_render(RenderInfo &&info) { render_cmds.emplace_back(info); }
};

} // namespace ui

} // namespace afterhours
