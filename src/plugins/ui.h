

#include "autolayout.h"

#include "../developer.h"
#include "input_system.h"
#include <magic_enum/magic_enum.hpp>

namespace afterhours {
namespace ui {

static bool is_mouse_inside(const input::MousePosition &mouse_pos,
                            const RectangleType &rect) {
  return mouse_pos.x >= rect.x && mouse_pos.x <= rect.x + rect.width &&
         mouse_pos.y >= rect.y && mouse_pos.y <= rect.y + rect.height;
}


template<typename InputAction> 
struct UIContext : BaseComponent {
  // TODO move to input system 
  using InputBitset = std::bitset<magic_enum::enum_count<InputAction>()>;

  EntityID ROOT = -1;
  EntityID FAKE = -2;

  std::set<EntityID> focused_ids;

  EntityID hot_id = ROOT;
  EntityID focus_id = ROOT;
  EntityID active_id = ROOT;
  EntityID last_processed = ROOT;

  input::MousePosition mouse_pos;
  bool mouseLeftDown;
  InputAction last_action;
  InputBitset all_actions;

  [[nodiscard]] bool is_hot(EntityID id) { return hot_id == id; };
  [[nodiscard]] bool is_active(EntityID id) { return active_id == id; };
  void set_hot(EntityID id) { hot_id = id; }
  void set_active(EntityID id) { active_id = id; }

  bool has_focus(EntityID id) { return focus_id == id; }
  void set_focus(EntityID id) { focus_id = id; }

  void active_if_mouse_inside(EntityID id, Rectangle rect) {
    if (is_mouse_inside(mouse_pos, rect)) {
      set_hot(id);
      if (is_active(ROOT) && mouseLeftDown) {
        set_active(id);
      }
    }
  }

  void reset() {
    focus_id = ROOT;
    last_processed = ROOT;
    hot_id = ROOT;
    active_id = ROOT;
    focused_ids.clear();
  }

  void try_to_grab(EntityID id) {
    focused_ids.insert(id);
    if (has_focus(ROOT)) {
      set_focus(id);
    }
  }

  [[nodiscard]] bool is_mouse_click(EntityID id) {
    bool let_go = !mouseLeftDown;
    bool was_click = let_go && is_active(id) && is_hot(id);
    // if(was_click){play_sound();}
    return was_click;
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
      if (
          //
          pressed(InputAction::WidgetNext) || pressed(InputAction::ValueDown)
          // TODO add support for holding down tab
          // get().is_held_down_debounced(InputAction::WidgetNext) ||
          // get().is_held_down_debounced(InputAction::ValueDown)
      ) {
        set_focus(ROOT);
        if (is_held_down(InputAction::WidgetMod)) {
          set_focus(last_processed);
        }
      }
      if (pressed(InputAction::ValueUp)) {
        set_focus(last_processed);
      }
      // if (pressed(InputAction::WidgetBack)) {
      // set_focus(last_processed);
      // }
    }
    // before any returns
    last_processed = id;
  }
};


struct HasClickListener : BaseComponent {
  bool down = false;
  std::function<void(Entity &)> cb;
  HasClickListener(const std::function<void(Entity &)> &callback)
      : cb(callback) {}
};

struct HasDragListener : BaseComponent {
  bool down = false;
  std::function<void(Entity &)> cb;
  HasDragListener(const std::function<void(Entity &)> &callback)
      : cb(callback) {}
};

struct HasLabel : BaseComponent {
  std::string label;
  HasLabel(const std::string &str) : label(str) {}
  HasLabel() : label("") {}
};

struct HasCheckboxState : BaseComponent {
  bool on;
  HasCheckboxState(bool b) : on(b) {}
};

struct HasSliderState : BaseComponent {
  bool changed_since = false;
  float value;
  HasSliderState(float val) : value(val) {}
};

struct ShouldHide : BaseComponent {};

//// /////
////  Systems
//// /////

template<typename InputAction>
struct BeginUIContextManager : System<UIContext<InputAction>> {
  using InputBits = UIContext<InputAction>::InputBitset;

  // TODO this should live inside input_system
  // but then it would require magic_enum as a dependency
  InputBits inputs_as_bits(
      const std::vector<input::ActionDone<InputAction>> &inputs) const {
    InputBits output;
    for (auto &input : inputs) {
      if (input.amount_pressed <= 0.f)
        continue;
      output[magic_enum::enum_index<InputAction>(input.action).value()] = true;
    }
    return output;
  }

  virtual void for_each_with(Entity &, UIContext<InputAction> &context, float) override {
    context.mouse_pos = input::get_mouse_position();
    context.mouseLeftDown = input::is_mouse_button_down(0);

    {
      input::PossibleInputCollector<InputAction> inpc =
          input::get_input_collector<InputAction>();
      if (inpc.has_value()) {
        context.all_actions = inputs_as_bits(inpc.inputs());
        for (auto &actions_done : inpc.inputs_pressed()) {
          context.last_action = actions_done.action;
        }
      }
    }

    context.hot_id = context.ROOT;
  }
};

struct RunAutoLayout : System<AutoLayoutRoot, UIComponent> {
  virtual void for_each_with(Entity &, AutoLayoutRoot &, UIComponent& cmp, float) override {
      AutoLayout::autolayout(cmp);
  }
};

template<typename InputAction>
struct EndUIContextManager : System<UIContext<InputAction>> {

  virtual void for_each_with(Entity &, UIContext<InputAction> &context, float) override {

    if (context.focus_id == context.ROOT)
      return;

    if (context.mouseLeftDown) {
      if (context.is_active(context.ROOT)) {
        context.set_active(context.FAKE);
      }
    } else {
      context.set_active(context.ROOT);
    }
    if (!context.focused_ids.contains(context.focus_id))
      context.focus_id = context.ROOT;
    context.focused_ids.clear();
  }
};

//// /////
////  Plugin Info 
//// /////

  template<typename InputAction>
  static void add_singleton_components(Entity &entity){
      entity.addComponent<UIContext<InputAction>>();
  }

  template<typename InputAction>
  static void enforce_singletons(SystemManager &sm){
    sm.register_update_system(
        std::make_unique<
            developer::EnforceSingleton<UIContext<InputAction>>>());
  }

  // static void register_update_systems(SystemManager &){}
 

} // namespace ui

} // namespace afterhours

