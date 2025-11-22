#pragma once

#include <map>
#include <set>
#include <vector>

#include "../../ecs.h"
#include "../../logging.h"
#include "../autolayout.h"
#include "../input_system.h"
#include "../window_manager.h"
#include "components.h"
#include "context.h"
#include "theme_defaults.h"
#if __has_include(<magic_enum/magic_enum.hpp>)
#include <magic_enum/magic_enum.hpp>
#else
#include "../../../vendor/magic_enum/magic_enum.hpp"
#endif

namespace afterhours {

namespace ui {

template <typename InputAction>
struct BeginUIContextManager : System<UIContext<InputAction>> {
  using InputBits = UIContext<InputAction>::InputBitset;

  // TODO this should live inside input_system
  // but then it would require magic_enum as a dependency
  InputBits inputs_as_bits(const std::vector<input::ActionDone> &inputs) const {
    InputBits output;
    for (auto &input : inputs) {
      if (input.amount_pressed <= 0.f)
        continue;
      output[magic_enum::enum_index<InputAction>(
                 static_cast<InputAction>(input.action))
                 .value()] = true;
    }
    return output;
  }

  virtual void for_each_with(Entity &, UIContext<InputAction> &context,
                             float) override {
    // Apply theme defaults first
    auto &theme_defaults = imm::ThemeDefaults::get();
    context.theme = theme_defaults.get_theme();

    context.mouse_pos = input::get_mouse_position();
    context.mouseLeftDown = input::is_mouse_button_down(0);

    {
      input::PossibleInputCollector inpc = input::get_input_collector();
      if (inpc.has_value()) {
        context.all_actions = inputs_as_bits(inpc.inputs());
        for (auto &actions_done : inpc.inputs_pressed()) {
          context.last_action = static_cast<InputAction>(actions_done.action);
        }
      }
    }

    context.hot_id = context.ROOT;
  }
};

struct ClearVisibity : System<UIComponent> {
  virtual void for_each_with(Entity &, UIComponent &cmp, float) override {
    cmp.was_rendered_to_screen = false;
  }
};

struct ClearUIComponentChildren : System<UIComponent> {
  virtual void for_each_with(Entity &, UIComponent &cmp, float) override {
    cmp.children.clear();
  }
};

static void print_debug_autolayout_tree(Entity &entity, UIComponent &cmp,
                                        size_t tab = 0) {

  for (size_t i = 0; i < tab; i++)
    std::cout << "  ";

  std::cout << cmp.id << " : ";
  std::cout << cmp.rect().x << ",";
  std::cout << cmp.rect().y << ",";
  std::cout << cmp.rect().width << ",";
  std::cout << cmp.rect().height << " ";
  if (entity.has<UIComponentDebug>())
    std::cout << entity.get<UIComponentDebug>().name() << " ";
  std::cout << std::endl;

  for (EntityID child_id : cmp.children) {
    print_debug_autolayout_tree(AutoLayout::to_ent_static(child_id),
                                AutoLayout::to_cmp_static(child_id), tab + 1);
  }
}

struct RunAutoLayout : System<AutoLayoutRoot, UIComponent> {

  std::map<EntityID, RefEntity> components;
  window_manager::Resolution resolution;

  virtual void once(float) override {
    components.clear();
    auto comps = EntityQuery().whereHasComponent<UIComponent>().gen();
    for (Entity &entity : comps) {
      components.emplace(entity.id, entity);
    }

    Entity &e = EntityHelper::get_singleton<
        window_manager::ProvidesCurrentResolution>();

    resolution =
        e.get<window_manager::ProvidesCurrentResolution>().current_resolution;
  }

  virtual void for_each_with(Entity &, AutoLayoutRoot &, UIComponent &cmp,
                             float) override {

    AutoLayout::autolayout(cmp, resolution, components);

    // print_debug_autolayout_tree(entity, cmp);
    // log_error("");
  }
};

template <typename InputAction>
struct TrackIfComponentWillBeRendered : System<> {

  void set_visibility(UIComponent &cmp) {
    // Early exit if already processed or hidden
    if (cmp.was_rendered_to_screen || cmp.should_hide)
      return;

    // Process children first (bottom-up approach for better early exits)
    for (EntityID child : cmp.children) {
      set_visibility(AutoLayout::to_cmp_static(child));
    }

    // Only mark visible if component has valid dimensions
    if (cmp.width() >= 0 && cmp.height() >= 0) {
      cmp.was_rendered_to_screen = true;
    }
  }

  virtual void for_each_with(Entity &entity, float) override {
    if (entity.is_missing<UIContext<InputAction>>())
      return;

    const UIContext<InputAction> &context =
        entity.get<UIContext<InputAction>>();

    for (auto &cmd : context.render_cmds) {
      auto id = cmd.id;
      Entity &ent = EntityHelper::getEntityForIDEnforce(id);
      set_visibility(ent.get<UIComponent>());
    }
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

template <typename InputAction> struct ComputeVisualFocusId : System<> {
  virtual void for_each_with(Entity &, float) override {
    auto ctx = EntityHelper::get_singleton_cmp<ui::UIContext<InputAction>>();
    ctx->visual_focus_id = ctx->ROOT;
    if (ctx->focus_id == ctx->ROOT)
      return;
    OptEntity focused = EntityHelper::getEntityForID(ctx->focus_id);
    if (!focused.has_value())
      return;
    Entity &fe = focused.asE();
    // climb to nearest FocusClusterRoot if member of a cluster
    Entity *current = &fe;
    int guard = 0;
    while (current->has<ui::InFocusCluster>()) {
      const UIComponent &cmp = current->get<UIComponent>();
      if (cmp.parent < 0 || ++guard > 64)
        break;
      Entity &parent = EntityHelper::getEntityForIDEnforce(cmp.parent);
      if (parent.has<ui::FocusClusterRoot>()) {
        ctx->visual_focus_id = parent.id;
        return;
      }
      current = &parent;
    }
    ctx->visual_focus_id = fe.id;
  }
};

// TODO i like this but for Tags, i wish
// the user of this didnt have to add UIComponent to their for_each_with
template <typename... Components>
struct SystemWithUIContext : System<UIComponent, Components...> {};

template <typename InputAction>
struct HandleClicks : SystemWithUIContext<ui::HasClickListener> {
  UIContext<InputAction> *context;

  virtual void once(float) override {
    this->context =
        EntityHelper::get_singleton_cmp<ui::UIContext<InputAction>>();
    // this->include_derived_children = true;
  }

  virtual ~HandleClicks() {}

  virtual void for_each_with(Entity &entity, UIComponent &component,
                             HasClickListener &hasClickListener, float) {
    hasClickListener.down = false;

    if (!component.was_rendered_to_screen)
      return;

    if (component.should_hide || entity.has<ShouldHide>())
      return;

    if (entity.has<HasLabel>() && entity.get<HasLabel>().is_disabled)
      return;

    context->active_if_mouse_inside(entity.id, component.rect());

    if (context->has_focus(entity.id) &&
        context->pressed(InputAction::WidgetPress)) {
      context->set_focus(entity.id);
      hasClickListener.cb(entity);
      hasClickListener.down = true;
    }

    if (context->is_mouse_click(entity.id)) {
      context->set_focus(entity.id);
      hasClickListener.cb(entity);
      hasClickListener.down = true;
    }

    process_derived_children(entity, component);
  }

private:
  void process_derived_children(Entity &parent, UIComponent &parent_component) {
    if (!parent.has<UIComponent>()) {
      return;
    }

    auto &ui_component = parent.get<UIComponent>();
    for (auto child_id : ui_component.children) {
      auto child_entity = EntityHelper::getEntityForID(child_id);
      if (!child_entity.has_value()) {
        continue;
      }

      Entity &child = child_entity.asE();
      if (!child.has<UIComponent>() || !child.has<ui::HasClickListener>()) {
        continue;
      }

      auto &child_component = child.get<UIComponent>();
      auto &child_hasClickListener = child.get<ui::HasClickListener>();

      child_hasClickListener.down = false;

      if (!child_component.was_rendered_to_screen)
        continue;

      if (child.has<HasLabel>() && child.get<HasLabel>().is_disabled)
        continue;

      context->active_if_mouse_inside(child.id, child_component.rect());

      if (context->has_focus(child.id) &&
          context->pressed(InputAction::WidgetPress)) {
        context->set_focus(child.id);
        child_hasClickListener.cb(child);
        child_hasClickListener.down = true;
      }

      if (context->is_mouse_click(child.id)) {
        context->set_focus(child.id);
        child_hasClickListener.cb(child);
        child_hasClickListener.down = true;
      }

      process_derived_children(child, child_component);
    }
  }
};

template <typename InputAction> struct HandleTabbing : SystemWithUIContext<> {
  UIContext<InputAction> *context;

  virtual void once(float) override {
    this->context =
        EntityHelper::get_singleton_cmp<ui::UIContext<InputAction>>();
  }

  virtual ~HandleTabbing() {}

  virtual void for_each_with(Entity &entity, UIComponent &component,
                             float) override {
    if (entity.is_missing<HasClickListener>() &&
        entity.is_missing<HasDragListener>())
      return;
    if (entity.has<SkipWhenTabbing>())
      return;
    if (entity.has<ShouldHide>())
      return;
    if (!component.was_rendered_to_screen)
      return;

    // Valid things to tab to...
    context->try_to_grab(entity.id);
    context->process_tabbing(entity.id);
  }
};

template <typename InputAction>
struct HandleDrags : SystemWithUIContext<ui::HasDragListener> {
  UIContext<InputAction> *context;

  virtual void once(float) override {
    this->context =
        EntityHelper::get_singleton_cmp<ui::UIContext<InputAction>>();
  }
  virtual ~HandleDrags() {}

  virtual void for_each_with(Entity &entity, UIComponent &component,
                             HasDragListener &hasDragListener, float) override {

    context->active_if_mouse_inside(entity.id, component.rect());

    if (context->has_focus(entity.id) &&
        context->pressed(InputAction::WidgetPress)) {
      context->set_focus(entity.id);
      hasDragListener.cb(entity);
    }

    if (context->is_active(entity.id)) {
      context->set_focus(entity.id);
      hasDragListener.cb(entity);
    }
  }
};

template <typename InputAction>
struct HandleLeftRight : SystemWithUIContext<ui::HasLeftRightListener> {
  UIContext<InputAction> *context;

  virtual void once(float) override {
    this->context =
        EntityHelper::get_singleton_cmp<ui::UIContext<InputAction>>();
    // this->include_derived_children = true;
  }
  virtual ~HandleLeftRight() {}

  virtual void for_each_with(Entity &entity, UIComponent &component,
                             HasLeftRightListener &listener, float) {
    if (!component.was_rendered_to_screen)
      return;

    if (!context->has_focus(entity.id))
      return;

    // TODO consider using a different repeat rate
    if (context->pressed(InputAction::WidgetLeft) ||
        context->is_held_down(InputAction::WidgetLeft)) {
      listener.cb(entity, -1);
    }
    if (context->pressed(InputAction::WidgetRight) ||
        context->is_held_down(InputAction::WidgetRight)) {
      listener.cb(entity, +1);
    }

    process_derived_children(entity, component);
  }

private:
  void process_derived_children(Entity &parent, UIComponent &parent_component) {
    if (!parent.has<UIComponent>()) {
      return;
    }

    auto &ui_component = parent.get<UIComponent>();
    for (auto child_id : ui_component.children) {
      auto child_entity = EntityHelper::getEntityForID(child_id);
      if (!child_entity.has_value()) {
        continue;
      }

      Entity &child = child_entity.asE();
      if (!child.has<UIComponent>() || !child.has<ui::HasLeftRightListener>()) {
        continue;
      }

      auto &child_component = child.get<UIComponent>();
      auto &child_listener = child.get<ui::HasLeftRightListener>();

      if (!child_component.was_rendered_to_screen)
        continue;

      if (!context->has_focus(child.id))
        continue;

      // TODO consider using a different repeat rate
      if (context->pressed(InputAction::WidgetLeft) ||
          context->is_held_down(InputAction::WidgetLeft)) {
        child_listener.cb(child, -1);
      }
      if (context->pressed(InputAction::WidgetRight) ||
          context->is_held_down(InputAction::WidgetRight)) {
        child_listener.cb(child, +1);
      }

      process_derived_children(child, child_component);
    }
  }
};

template <typename InputAction>
struct HandleSelectOnFocus
    : SystemWithUIContext<ui::SelectOnFocus, ui::HasClickListener> {
  UIContext<InputAction> *context;
  std::set<EntityID> last_focused_ids;

  virtual void once(float) override {
    this->context =
        EntityHelper::get_singleton_cmp<ui::UIContext<InputAction>>();
    // this->include_derived_children = true;
  }

  virtual ~HandleSelectOnFocus() {}

  virtual void for_each_with(Entity &entity, UIComponent &component,
                             SelectOnFocus &selectOnFocus,
                             HasClickListener &hasClickListener, float) {
    if (!component.was_rendered_to_screen)
      return;

    // Check if this entity just gained focus
    bool currently_focused = context->has_focus(entity.id);
    bool was_focused = last_focused_ids.contains(entity.id);

    if (currently_focused && !was_focused) {
      // Component just gained focus, trigger the click
      hasClickListener.cb(entity);
      hasClickListener.down = true;
    }

    // Update our tracking
    if (currently_focused) {
      last_focused_ids.insert(entity.id);
    } else {
      last_focused_ids.erase(entity.id);
    }

    process_derived_children(entity, component);
  }

private:
  void process_derived_children(Entity &parent, UIComponent &parent_component) {
    if (!parent.has<UIComponent>()) {
      return;
    }

    auto &ui_component = parent.get<UIComponent>();
    for (auto child_id : ui_component.children) {
      auto child_entity = EntityHelper::getEntityForID(child_id);
      if (!child_entity.has_value()) {
        continue;
      }

      Entity &child = child_entity.asE();
      if (!child.has<UIComponent>() || !child.has<ui::SelectOnFocus>() ||
          !child.has<ui::HasClickListener>()) {
        continue;
      }

      auto &child_component = child.get<UIComponent>();
      auto &child_selectOnFocus = child.get<ui::SelectOnFocus>();
      auto &child_hasClickListener = child.get<ui::HasClickListener>();

      if (!child_component.was_rendered_to_screen)
        continue;

      // Check if this entity just gained focus
      bool currently_focused = context->has_focus(child.id);
      bool was_focused = last_focused_ids.contains(child.id);

      if (currently_focused && !was_focused) {
        // Component just gained focus, trigger the click
        child_hasClickListener.cb(child);
        child_hasClickListener.down = true;
      }

      // Update our tracking
      if (currently_focused) {
        last_focused_ids.insert(child.id);
      } else {
        last_focused_ids.erase(child.id);
      }

      process_derived_children(child, child_component);
    }
  }
};

template <typename InputAction>
struct UpdateDropdownOptions
    : SystemWithUIContext<HasDropdownState, HasChildrenComponent> {

  UIContext<InputAction> *context;

  virtual void once(float) override {
    this->context =
        EntityHelper::get_singleton_cmp<ui::UIContext<InputAction>>();
  }

  UpdateDropdownOptions()
      : SystemWithUIContext<HasDropdownState, HasChildrenComponent>() {
// TODO figure out if this actually will cause trouble
// Remove include_derived_children since we want to process entities with direct
// components
#if __WIN32
// this->include_derived_children = true;
#else
// this->include_derived_children = true;
#endif
  }

#if __WIN32
  virtual void for_each_with(Entity &entity, UIComponent &component,
                             HasDropdownState &hasDropdownState,
                             HasChildrenComponent &hasChildren, float){

#else
  virtual void for_each_with(Entity &entity, UIComponent &component,
                             HasDropdownState &hasDropdownState,
                             HasChildrenComponent &hasChildren, float) {
#endif
      // The system should automatically filter entities to only those with
      // required components No need to manually check since
      // SystemWithUIContext<HasDropdownState, HasChildrenComponent> handles
      // this

      auto options = hasDropdownState.options;
  hasDropdownState.options = hasDropdownState.fetch_options(hasDropdownState);

  // Validate returned options
  // TODO replace with a pound define
  if (hasDropdownState.options.size() > 100) {
    log_error("UpdateDropdownOptions: Entity {} - fetch_options returned {} "
              "options - ABORTING",
              entity.id, hasDropdownState.options.size());
    return;
  }

  // detect if the options changed or if the state changed
  {
    bool changed = false;
    if (options.size() != hasDropdownState.options.size()) {
      changed = true;
    }

    // Validate the order and which strings have changed
    for (size_t i = 0; i < options.size(); ++i) {
      if (i >= hasDropdownState.options.size() ||
          options[i] != hasDropdownState.options[i]) {
        changed = true;
      }
    }

    // Check for new options
    if (hasDropdownState.options.size() > options.size()) {
      for (size_t i = options.size(); i < hasDropdownState.options.size();
           ++i) {
        changed = true;
      }
    }

    if (hasDropdownState.changed_since) {
      changed = true;
      hasDropdownState.changed_since = false;
    }

    if (!changed) {
      return;
    }
  }

  options = hasDropdownState.options;
  component.children.clear();

  if (hasChildren.children.size() == 0) {
    // no children and no options :)
    if (hasDropdownState.options.size() == 0) {
      log_warn("You have a dropdown with no options");
      return;
    }

    for (size_t i = 0; i < options.size(); i++) {
      Entity &child = EntityHelper::createEntity();
      child.addComponent<UIComponentDebug>("dropdown_option");
      child.addComponent<UIComponent>(child.id)
          .set_desired_width(ui::Size{
              .dim = ui::Dim::Percent,
              .value = 1.f,
          })
          .set_desired_height(ui::Size{
              .dim = ui::Dim::Pixels,
              .value = component.desired[Axis::Y].value,
          })
          .set_parent(entity.id);
      child.addComponent<ui::HasLabel>(options[i], false);
      child.addComponent<ui::HasClickListener>([i, &entity](Entity &) {
        ui::HasDropdownState &hds = entity.get_with_child<HasDropdownState>();
        hds.changed_since = true;
        hds.on = !hds.on;

        hds.last_option_clicked = i;
        if (hds.on_option_changed)
          hds.on_option_changed(i);
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

  process_derived_children(entity, component, hasDropdownState, hasChildren);
}

private
    : void
      process_derived_children(Entity &parent, UIComponent &parent_component,
                               HasDropdownState &parent_hasDropdownState,
                               HasChildrenComponent &parent_hasChildren) {
  if (!parent.has<UIComponent>()) {
    return;
  }

  auto &ui_component = parent.get<UIComponent>();
  for (auto child_id : ui_component.children) {
    auto child_entity = EntityHelper::getEntityForID(child_id);
    if (!child_entity.has_value()) {
      continue;
    }

    Entity &child = child_entity.asE();
    if (!child.has<UIComponent>() || !child.has<HasDropdownState>() ||
        !child.has<HasChildrenComponent>()) {
      continue;
    }

    auto &child_component = child.get<UIComponent>();
    auto &child_hasDropdownState = child.get<HasDropdownState>();
    auto &child_hasChildren = child.get<HasChildrenComponent>();

    auto options = child_hasDropdownState.options;
    child_hasDropdownState.options =
        child_hasDropdownState.fetch_options(child_hasDropdownState);

    // Validate returned options
    if (child_hasDropdownState.options.size() > 100) {
      log_error("UpdateDropdownOptions: Entity {} - fetch_options returned {} "
                "options - ABORTING",
                child.id, child_hasDropdownState.options.size());
      continue;
    }

    // detect if the options changed or if the state changed
    {
      bool changed = false;
      if (options.size() != child_hasDropdownState.options.size()) {
        changed = true;
      }

      // Validate the order and which strings have changed
      for (size_t i = 0; i < options.size(); ++i) {
        if (i >= child_hasDropdownState.options.size() ||
            options[i] != child_hasDropdownState.options[i]) {
          changed = true;
        }
      }

      // Check for new options
      if (child_hasDropdownState.options.size() > options.size()) {
        for (size_t i = options.size();
             i < child_hasDropdownState.options.size(); ++i) {
          changed = true;
        }
      }

      if (child_hasDropdownState.changed_since) {
        changed = true;
        child_hasDropdownState.changed_since = false;
      }

      if (!changed) {
        continue;
      }
    }

    options = child_hasDropdownState.options;
    child_component.children.clear();

    if (child_hasChildren.children.size() == 0) {
      // no children and no options :)
      if (child_hasDropdownState.options.size() == 0) {
        log_warn("You have a dropdown with no options");
        continue;
      }

      for (size_t i = 0; i < options.size(); i++) {
        Entity &grandchild = EntityHelper::createEntity();
        grandchild.addComponent<UIComponentDebug>("dropdown_option");
        grandchild.addComponent<UIComponent>(grandchild.id)
            .set_desired_width(ui::Size{
                .dim = ui::Dim::Percent,
                .value = 1.f,
            })
            .set_desired_height(ui::Size{
                .dim = ui::Dim::Pixels,
                .value = child_component.desired[Axis::Y].value,
            })
            .set_parent(child.id);
        grandchild.addComponent<ui::HasLabel>(options[i], false);
        grandchild.addComponent<ui::HasClickListener>([i, &child](Entity &) {
          ui::HasDropdownState &hds = child.get_with_child<HasDropdownState>();
          hds.changed_since = true;
          hds.on = !hds.on;

          hds.last_option_clicked = i;
          if (hds.on_option_changed)
            hds.on_option_changed(i);
        });
        child_hasChildren.add_child(grandchild);
      }
    }
    // If we get here, we should have num_options children...

    if (!child_hasDropdownState.on) {
      // just draw the selected one...
      EntityID grandchild_id =
          child_hasChildren
              .children[child_hasDropdownState.last_option_clicked];
      child_component.add_child(grandchild_id);
    } else {
      for (EntityID grandchild_id : child_hasChildren.children) {
        child_component.add_child(grandchild_id);
      }
    }

    process_derived_children(child, child_component, child_hasDropdownState,
                             child_hasChildren);
  }
}
}; // namespace ui

} // namespace afterhours

} // namespace afterhours
