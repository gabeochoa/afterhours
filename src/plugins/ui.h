
#pragma once

#include <algorithm>
#include <atomic>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <optional>
#include <utility>
//
#include "autolayout.h"
#include <magic_enum/magic_enum.hpp>

#include "../developer.h"
#include "input_system.h"

namespace afterhours {
namespace ui {

static bool is_mouse_inside(const input::MousePosition &mouse_pos,
                            const RectangleType &rect) {
  return mouse_pos.x >= rect.x && mouse_pos.x <= rect.x + rect.width &&
         mouse_pos.y >= rect.y && mouse_pos.y <= rect.y + rect.height;
}

template <typename InputAction> struct UIContext : BaseComponent {
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

struct HasChildrenComponent : BaseComponent {
  std::vector<EntityID> children;
  std::function<void(Entity &)> on_child_add;

  HasChildrenComponent() {}

  void add_child(Entity &child) {
    children.push_back(child.id);
    if (on_child_add)
      on_child_add(child);
  }

  auto &register_on_child_add(const std::function<void(Entity &)> &cb) {
    on_child_add = cb;
    return *this;
  }
};

struct HasDropdownState : ui::HasCheckboxState {
  using Options = std::vector<std::string>;
  Options options;
  std::function<Options(HasDropdownState &)> fetch_options;
  std::function<void(size_t)> on_option_changed;
  size_t last_option_clicked = 0;

  HasDropdownState(
      const Options &opts,
      const std::function<Options(HasDropdownState &)> fetch_opts = nullptr,
      const std::function<void(size_t)> opt_changed = nullptr)
      : HasCheckboxState(false), options(opts), fetch_options(fetch_opts),
        on_option_changed(opt_changed) {}

  HasDropdownState(const std::function<Options(HasDropdownState &)> fetch_opts)
      : HasDropdownState(fetch_opts(*this), fetch_opts, nullptr) {}
};

//// /////
////  Systems
//// /////

template <typename InputAction>
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

  virtual void for_each_with(Entity &, UIContext<InputAction> &context,
                             float) override {
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

struct ClearVisibity : System<UIComponent> {
  virtual void for_each_with(Entity &, UIComponent &cmp, float) override {
    cmp.is_visible = false;
  }
};

struct RunAutoLayout : System<AutoLayoutRoot, UIComponent> {
  virtual void for_each_with(Entity &, AutoLayoutRoot &, UIComponent &cmp,
                             float) override {

    AutoLayout::autolayout(cmp);

    // AutoLayout::print_tree(cmp);
    // log_error("");
  }
};

struct SetVisibity : System<AutoLayoutRoot, UIComponent> {

  void set_visibility(UIComponent &cmp) {
    if (cmp.width() < 0 || cmp.height() < 0) {
      return;
    }
    for (EntityID child : cmp.children) {
      set_visibility(AutoLayout::to_cmp(child));
    }
    cmp.is_visible = true;
  }

  virtual void for_each_with(Entity &, AutoLayoutRoot &, UIComponent &cmp,
                             float) override {
    set_visibility(cmp);
  }
};

template <typename InputAction>
struct EndUIContextManager : System<UIContext<InputAction>> {

  virtual void for_each_with(Entity &, UIContext<InputAction> &context,
                             float) override {

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

// TODO i like this but for Tags, i wish
// the user of this didnt have to add UIComponent to their for_each_with
template <typename... Components>
struct SystemWithUIContext : System<UIComponent, Components...> {};

template <typename InputAction>
struct HandleClicks : SystemWithUIContext<ui::HasClickListener> {

  Entity *context_entity;
  virtual void once(float) override {
    OptEntity opt_context =
        EntityQuery()                                        //
            .whereHasComponent<ui::UIContext<InputAction>>() //
            .gen_first();
    this->context_entity = opt_context.value();
  }

  virtual ~HandleClicks() {}

  virtual void for_each_with(Entity &entity, UIComponent &component,
                             HasClickListener &hasClickListener,
                             float) override {
    if (!this->context_entity)
      return;

    UIContext<InputAction> &context =
        this->context_entity->template get<UIContext<InputAction>>();

    context.active_if_mouse_inside(entity.id, component.rect());

    if (context.has_focus(entity.id) &&
        context.pressed(InputAction::WidgetPress)) {
      context.set_focus(entity.id);
      hasClickListener.cb(entity);
    }

    if (context.is_mouse_click(entity.id)) {
      context.set_focus(entity.id);
      hasClickListener.cb(entity);
    }
  }
};

template <typename InputAction> struct HandleTabbing : SystemWithUIContext<> {
  Entity *context_entity;
  virtual void once(float) override {
    OptEntity opt_context =
        EntityQuery()                                        //
            .whereHasComponent<ui::UIContext<InputAction>>() //
            .gen_first();
    this->context_entity = opt_context.value();
  }

  virtual ~HandleTabbing() {}

  virtual void for_each_with(Entity &entity, UIComponent &component,
                             float) override {
    if (!this->context_entity)
      return;

    UIContext<InputAction> &context =
        this->context_entity->template get<UIContext<InputAction>>();

    if (entity.is_missing<HasClickListener>()) {
      return;
    }
    if (!component.is_visible)
      return;

    // Valid things to tab to...
    context.try_to_grab(entity.id);
    context.process_tabbing(entity.id);
  }
};

template <typename InputAction>
struct HandleDrags : SystemWithUIContext<ui::HasDragListener> {
  Entity *context_entity;
  virtual void once(float) override {
    OptEntity opt_context =
        EntityQuery()                                        //
            .whereHasComponent<ui::UIContext<InputAction>>() //
            .gen_first();
    this->context_entity = opt_context.value();
  }
  virtual ~HandleDrags() {}

  virtual void for_each_with(Entity &entity, UIComponent &component,
                             HasDragListener &hasDragListener, float) override {
    if (!this->context_entity)
      return;
    UIContext<InputAction> &context =
        this->context_entity->template get<UIContext<InputAction>>();

    context.active_if_mouse_inside(entity.id, component.rect());

    if (context.has_focus(entity.id) &&
        context.pressed(InputAction::WidgetPress)) {
      context.set_focus(entity.id);
      hasDragListener.cb(entity);
    }

    if (context.is_active(entity.id)) {
      context.set_focus(entity.id);
      hasDragListener.cb(entity);
    }
  }
};

template <typename InputAction>
struct UpdateDropdownOptions
    : SystemWithUIContext<HasDropdownState, HasChildrenComponent> {

  Entity *context_entity;
  virtual void once(float) override {
    OptEntity opt_context =
        EntityQuery()                                        //
            .whereHasComponent<ui::UIContext<InputAction>>() //
            .gen_first();
    this->context_entity = opt_context.value();
  }

  UpdateDropdownOptions()
      : SystemWithUIContext<HasDropdownState, HasChildrenComponent>() {
    this->include_derived_children = true;
  }

  virtual void for_each_with_derived(Entity &entity, UIComponent &component,
                                     HasDropdownState &hasDropdownState,
                                     HasChildrenComponent &hasChildren, float) {
    component.children.clear();
    hasDropdownState.options = hasDropdownState.fetch_options(hasDropdownState);

    if (hasChildren.children.size() == 0) {
      // no children and no options :)
      if (hasDropdownState.options.size() == 0) {
        log_warn("You have a dropdown with no options");
        return;
      }

      auto &options = hasDropdownState.options;
      for (size_t i = 0; i < options.size(); i++) {
        Entity &child = EntityHelper::createEntity();
        child.addComponent<UIComponent>(child.id)
            .set_desired_width(ui::Size{
                .dim = ui::Dim::Percent,
                .value = 1.f,
            })
            .set_desired_height(ui::Size{
                .dim = ui::Dim::Pixels,
                .value = component.desired[1].value,
            })
            .set_parent(entity.id);
        child.addComponent<ui::HasLabel>(options[i]);
        child.addComponent<ui::HasClickListener>([i, &entity](Entity &) {
          log_info("clicked {}", i);
          // Parent stuff.

          ui::HasDropdownState &hds = entity.get_with_child<HasDropdownState>();
          hds.on = !hds.on;

          if (hds.on_option_changed)
            hds.on_option_changed(i);
          hds.last_option_clicked = i;

          return;

          OptEntity opt_context =
              EntityQuery()
                  .whereHasComponent<UIContext<InputAction>>()
                  .gen_first();
          opt_context->get<ui::UIContext<InputAction>>().set_focus(entity.id);
        });
        hasChildren.add_child(child);
      }
    }
    // If we get here, we should have num_options children...

    if (!hasDropdownState.on) {
      // just draw the selected one...
      EntityID child_id =
          hasChildren.children[hasDropdownState.last_option_clicked];
      component.add_child(child_id);
    } else {
      for (EntityID child_id : hasChildren.children) {
        component.add_child(child_id);
      }
    }
  }
};

//// /////
////  Plugin Info
//// /////

template <typename InputAction>
static void add_singleton_components(Entity &entity) {
  entity.addComponent<UIContext<InputAction>>();
}

template <typename InputAction>
static void enforce_singletons(SystemManager &sm) {
  sm.register_update_system(
      std::make_unique<developer::EnforceSingleton<UIContext<InputAction>>>());
}

template <typename InputAction>
static void register_update_systems(SystemManager &sm) {
  sm.register_update_system(
      std::make_unique<ui::BeginUIContextManager<InputAction>>());
  {
    sm.register_update_system(
        std::make_unique<ui::UpdateDropdownOptions<InputAction>>());

    //
    sm.register_update_system(std::make_unique<ui::ClearVisibity>());
    sm.register_update_system(std::make_unique<ui::RunAutoLayout>());
    sm.register_update_system(std::make_unique<ui::SetVisibity>());
    //
    sm.register_update_system(
        std::make_unique<ui::HandleTabbing<InputAction>>());
    sm.register_update_system(
        std::make_unique<ui::HandleClicks<InputAction>>());
    sm.register_update_system(std::make_unique<ui::HandleDrags<InputAction>>());
  }
  sm.register_update_system(
      std::make_unique<ui::EndUIContextManager<InputAction>>());
}

} // namespace ui

} // namespace afterhours
