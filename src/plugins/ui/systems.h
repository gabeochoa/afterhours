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
#include "theme.h"
#include "ui_collection.h"
#if __has_include(<magic_enum/magic_enum.hpp>)
#include <magic_enum/magic_enum.hpp>
#else
#include "../../../vendor/magic_enum/magic_enum.hpp"
#endif

namespace afterhours {

namespace ui {

/// Singleton component that caches entity mappings for fast lookups during
/// UI tree traversal. Populated once per frame by BuildUIEntityMapping system.
struct UIEntityMappingCache : BaseComponent {
  std::map<EntityID, RefEntity> components;

  Entity &to_ent(EntityID id) {
    auto it = components.find(id);
    if (it == components.end()) {
      log_error("UIEntityMappingCache: entity {} not in mapping", id);
    }
    return it->second.get();
  }

  UIComponent &to_cmp(EntityID id) {
    return to_ent(id).template get<UIComponent>();
  }
};

/// System that builds the entity mapping cache once per frame.
/// Must run before RunAutoLayout and TrackIfComponentWillBeRendered.
struct BuildUIEntityMapping : System<> {
  virtual void once(float) override {
    auto *cache = EntityHelper::get_singleton_cmp<UIEntityMappingCache>();
    if (!cache) {
      return; // Singleton not created yet
    }

    cache->components.clear();
    auto &ui_coll = UICollectionHolder::get().collection;
    auto ui_entities = EntityQuery(ui_coll, {.ignore_temp_warning = true})
        .whereHasComponent<UIComponent>().gen();
    for (Entity &entity : ui_entities) {
      cache->components.emplace(entity.id, entity);
    }
  }
};

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

  virtual void for_each_with(Entity &entity, UIContext<InputAction> &context,
                             float) override {
    // Apply theme defaults first
    auto &theme_defaults = imm::ThemeDefaults::get();
    context.theme = theme_defaults.get_theme();

    // Mouse input handling
    {
      context.mouse.pos = input::get_mouse_position();
      const bool prev_mouse_down = context.mouse.left_down;
      context.mouse.left_down = input::is_mouse_button_down(0);
      context.mouse.just_pressed = !prev_mouse_down && context.mouse.left_down;
      context.mouse.just_released = prev_mouse_down && !context.mouse.left_down;

      if (context.mouse.just_pressed) {
        context.mouse.press_pos = context.mouse.pos;
        context.mouse.press_moved = false;
      }

      if (!context.mouse.left_down) {
        context.mouse.press_moved = false;
      } else if (!context.mouse.press_moved) {
        const float dx = context.mouse.pos.x - context.mouse.press_pos.x;
        const float dy = context.mouse.pos.y - context.mouse.press_pos.y;
        const float dist_sq = (dx * dx) + (dy * dy);
        const float threshold = MousePointerState::press_drag_threshold_px;
        if (dist_sq > (threshold * threshold)) {
          context.mouse.press_moved = true;
        }
      }
    }

    {
      input::PossibleInputCollector inpc = input::get_input_collector();
      if (inpc.has_value()) {
        context.all_actions = inputs_as_bits(inpc.inputs());
        for (auto &actions_done : inpc.inputs_pressed()) {
          context.last_action = static_cast<InputAction>(actions_done.action);
        }
      }
    }

    // Update screen dimensions for font size resolution
    {
      Entity &res_entity = EntityHelper::get_singleton<
          window_manager::ProvidesCurrentResolution>();
      auto &res =
          res_entity.get<window_manager::ProvidesCurrentResolution>()
              .current_resolution;
      context.screen_width = static_cast<float>(res.width);
      context.screen_height = static_cast<float>(res.height);
    }

    // Save previous frame's state for animations before resetting
    context.prev_hot_id = context.hot_id;
    context.prev_active_id = context.active_id;
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

/// Returns the element's rect with translate offsets applied (the actual
/// on-screen position). Use this instead of cmp.rect() when you need the
/// final rendered position including with_translate() offsets.
static RectangleType get_final_rect(const Entity &entity,
                                    const UIComponent &cmp) {
  RectangleType r = cmp.rect();
  if (entity.has<HasUIModifiers>()) {
    r = entity.get<HasUIModifiers>().apply_modifier(r);
  }
  return r;
}

static void print_debug_autolayout_tree(Entity &entity, UIComponent &cmp,
                                        size_t tab = 0) {
  for (size_t i = 0; i < tab; i++)
    std::cout << "  ";

  std::cout << "ID:" << cmp.id << " ";

  if (entity.has<UIComponentDebug>())
    std::cout << "[" << entity.get<UIComponentDebug>().name() << "] ";

  std::cout << "Rect(" << cmp.rect().x << "," << cmp.rect().y << " "
            << cmp.rect().width << "x" << cmp.rect().height << ") ";

  // Show translate offset and final position when modifiers are present
  if (entity.has<HasUIModifiers>()) {
    auto &mods = entity.get<HasUIModifiers>();
    if (mods.translate_x != 0.f || mods.translate_y != 0.f) {
      auto final_r = get_final_rect(entity, cmp);
      std::cout << "Translate(" << mods.translate_x << ","
                << mods.translate_y << ") ";
      std::cout << "FinalPos(" << final_r.x << "," << final_r.y << ") ";
    }
  }

  std::cout << "Computed(" << cmp.computed[Axis::X] << "x"
            << cmp.computed[Axis::Y] << ") ";

  std::cout << "RelPos(" << cmp.computed_rel[Axis::X] << ","
            << cmp.computed_rel[Axis::Y] << ") ";

  std::cout << "Padding(" << cmp.computed_padd[Axis::left] << ","
            << cmp.computed_padd[Axis::top] << ","
            << cmp.computed_padd[Axis::right] << ","
            << cmp.computed_padd[Axis::bottom] << ") ";

  std::cout << "Margin(" << cmp.computed_margin[Axis::left] << ","
            << cmp.computed_margin[Axis::top] << ","
            << cmp.computed_margin[Axis::right] << ","
            << cmp.computed_margin[Axis::bottom] << ") ";

  std::cout << "Desired(" << cmp.desired[Axis::X] << "," << cmp.desired[Axis::Y]
            << ") ";

  if (cmp.absolute)
    std::cout << "[ABS] ";

  std::cout << std::endl;

  for (EntityID child_id : cmp.children) {
    print_debug_autolayout_tree(AutoLayout::to_ent_static(child_id),
                                AutoLayout::to_cmp_static(child_id), tab + 1);
  }
}

struct RunAutoLayout : System<AutoLayoutRoot, UIComponent> {
  UIEntityMappingCache *cache = nullptr;
  window_manager::Resolution resolution;

  virtual void once(float) override {
    cache = EntityHelper::get_singleton_cmp<UIEntityMappingCache>();

    Entity &e = EntityHelper::get_singleton<
        window_manager::ProvidesCurrentResolution>();

    resolution =
        e.get<window_manager::ProvidesCurrentResolution>().current_resolution;
  }

  virtual void for_each_with(Entity &, AutoLayoutRoot &, UIComponent &cmp,
                             float) override {
    if (!cache) {
      return; // Cache not ready yet
    }

    auto &styling_defaults = imm::UIStylingDefaults::get();
    bool enable_grid = styling_defaults.enable_grid_snapping;

    // Get ui_scale from ThemeDefaults (set each frame from the active theme).
    float ui_scale = imm::ThemeDefaults::get().theme.ui_scale;

    AutoLayout::autolayout(cmp, resolution, cache->components, enable_grid,
                           ui_scale);

    // print_debug_autolayout_tree(entity, cmp);
    // log_error("");
  }
};

template <typename InputAction>
struct TrackIfComponentWillBeRendered : System<> {
  UIEntityMappingCache *cache = nullptr;

  virtual void once(float) override {
    cache = EntityHelper::get_singleton_cmp<UIEntityMappingCache>();
  }

  void set_visibility(UIComponent &cmp) {
    // Early exit if already processed or hidden
    if (cmp.was_rendered_to_screen || cmp.should_hide)
      return;

    // Process children first (bottom-up approach for better early exits)
    for (EntityID child : cmp.children) {
      set_visibility(cache->to_cmp(child));
    }

    // Only mark visible if component has valid dimensions
    if (cmp.width() >= 0 && cmp.height() >= 0) {
      cmp.was_rendered_to_screen = true;
    }
  }

  virtual void for_each_with(Entity &entity, float) override {
    // TODO move to a system filter
    if (entity.is_missing<UIContext<InputAction>>())
      return;

    if (!cache) {
      return; // Cache not ready yet
    }

    auto &context = entity.get<UIContext<InputAction>>();

    // Only mark entities as visible if they were queued for render this frame.
    // This ensures that UI elements from inactive screens (which don't call
    // their div/button functions) are not marked as visible.
    for (const auto &cmd : context.render_cmds) {
      OptEntity opt_ent = UICollectionHolder::getEntityForID(cmd.id);
      if (!opt_ent.valid())
        continue;
      Entity &ent = opt_ent.asE();
      if (ent.has<UIComponent>()) {
        set_visibility(ent.get<UIComponent>());
      }
    }
  }
};

template <typename InputAction>
struct EndUIContextManager : System<UIContext<InputAction>> {
  virtual void for_each_with(Entity &, UIContext<InputAction> &context,
                             float) override {
    if (context.focus_id == context.ROOT)
      return;

    if (context.mouse.left_down) {
      if (context.is_active(context.ROOT)) {
        context.set_active(context.FAKE);
      }
    } else {
      context.set_active(context.ROOT);
    }
    if (!context.focused_ids.contains(context.focus_id))
      context.focus_id = context.ROOT;
    context.focused_ids.clear();

    if (auto *text_cache =
            EntityHelper::get_singleton_cmp<ui::TextMeasureCache>()) {
      text_cache->end_frame();
    }
  }
};

template <typename InputAction> struct ComputeVisualFocusId : System<> {
  virtual void for_each_with(Entity &, float) override {
    auto ctx = EntityHelper::get_singleton_cmp<ui::UIContext<InputAction>>();
    ctx->visual_focus_id = ctx->ROOT;
    if (ctx->focus_id == ctx->ROOT)
      return;
    OptEntity focused = UICollectionHolder::getEntityForID(ctx->focus_id);
    if (!focused.has_value())
      return;
    Entity &fe = focused.asE();
    // If focused entity has HasTray, show focus on the selected child
    if (fe.has<ui::HasTray>()) {
      const auto &tray = fe.get<ui::HasTray>();
      if (!tray.navigable_children.empty()) {
        int idx = std::clamp(tray.selection_index, 0,
                             (int)tray.navigable_children.size() - 1);
        ctx->visual_focus_id = tray.navigable_children[idx];
        return;
      }
    }
    // climb to nearest FocusClusterRoot if member of a cluster
    Entity *current = &fe;
    int guard = 0;
    while (current->has<ui::InFocusCluster>()) {
      const UIComponent &cmp = current->get<UIComponent>();
      if (cmp.parent < 0 || ++guard > 64)
        break;
      Entity &parent = UICollectionHolder::getEntityForIDEnforce(cmp.parent);
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

    // Apply translation if present (with_translate applies via
    // HasUIModifiers)
    RectangleType rect = component.rect();
    if (entity.has<HasUIModifiers>()) {
      rect = entity.get<HasUIModifiers>().apply_modifier(rect);
    }

    context->active_if_mouse_inside(entity.id, rect);

    if (context->has_focus(entity.id) &&
        context->pressed(InputAction::WidgetPress)) {
      context->set_focus(entity.id);
      hasClickListener.cb(entity);
      hasClickListener.down = true;
    }

    if (context->mouse_activates(entity.id)) {
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
      auto child_entity = UICollectionHolder::getEntityForID(child_id);
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

      if (context->mouse_activates(child.id)) {
        context->set_focus(child.id);
        child_hasClickListener.cb(child);
        child_hasClickListener.down = true;
      }

      process_derived_children(child, child_component);
    }
  }
};

// Helper function to check if a point is inside an entity's rect (including
// children)
inline bool is_point_inside_entity_tree(EntityID entity_id,
                                        const input::MousePosition &pos) {
  OptEntity opt = UICollectionHolder::getEntityForID(entity_id);
  if (!opt.has_value())
    return false;

  Entity &entity = opt.asE();
  if (!entity.has<UIComponent>())
    return false;

  const UIComponent &cmp = entity.get<UIComponent>();

  // Check if point is inside this entity's rect
  RectangleType rect = cmp.rect();
  if (entity.has<HasUIModifiers>()) {
    rect = entity.get<HasUIModifiers>().apply_modifier(rect);
  }
  if (is_mouse_inside(pos, rect))
    return true;

  // Check children recursively
  for (EntityID child_id : cmp.children) {
    if (is_point_inside_entity_tree(child_id, pos))
      return true;
  }

  return false;
}

template <typename InputAction>
struct CloseDropdownOnClickOutside : System<HasDropdownState, UIComponent> {
  UIContext<InputAction> *context;
  bool prev_mouse_down = false;
  bool should_close_dropdowns = false;
  input::MousePosition click_pos;

  virtual void once(float) override {
    this->context =
        EntityHelper::get_singleton_cmp<ui::UIContext<InputAction>>();

    // Detect click: mouse was down last frame, now it's up
    should_close_dropdowns = prev_mouse_down && !context->mouse.left_down;
    click_pos = context->mouse.pos;

    // Track mouse state for next frame
    prev_mouse_down = context->mouse.left_down;
  }

  virtual void for_each_with(Entity &entity, HasDropdownState &dropdownState,
                             UIComponent &, float) override {
    // Only process open dropdowns
    if (!dropdownState.on)
      return;

    // Only process if a click just happened
    if (!should_close_dropdowns)
      return;

    // Check if the click was inside this dropdown or any of its children
    if (is_point_inside_entity_tree(entity.id, click_pos))
      return;

    // Click was outside - close the dropdown
    dropdownState.on = false;
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
struct HandleTrayNavigation : SystemWithUIContext<ui::HasTray> {
  UIContext<InputAction> *context;

  virtual void once(float) override {
    context = EntityHelper::get_singleton_cmp<ui::UIContext<InputAction>>();
  }

  virtual void for_each_with(Entity &entity, UIComponent &component,
                             HasTray &tray, float dt) {
    if (!component.was_rendered_to_screen) return;

    // Rebuild navigable children list each frame
    tray.navigable_children.clear();
    for (auto child_id : component.children) {
      auto child_opt = UICollectionHolder::getEntityForID(child_id);
      if (!child_opt.has_value()) continue;
      Entity &child = child_opt.asE();
      // Mark all children as skip-tabbing
      child.addComponentIfMissing<SkipWhenTabbing>();
      // Only navigable if it has a click listener and is rendered
      if (child.has<HasClickListener>() &&
          child.has<UIComponent>() &&
          child.get<UIComponent>().was_rendered_to_screen) {
        tray.navigable_children.push_back(child_id);
      }
    }

    if (tray.navigable_children.empty()) return;

    // Clamp selection index
    int count = (int)tray.navigable_children.size();
    tray.selection_index = std::clamp(tray.selection_index, 0, count - 1);

    if (!context->has_focus(entity.id)) return;

    // Determine axis from flex direction
    bool horizontal = (component.flex_direction == FlexDirection::Row);

    // Arrow key navigation with key repeat
    auto fwd = horizontal ? InputAction::WidgetRight : InputAction::WidgetDown;
    auto bck = horizontal ? InputAction::WidgetLeft : InputAction::WidgetUp;

    int dir = 0;
    bool held = context->is_held_down(fwd) || context->is_held_down(bck);

    if (context->pressed(fwd)) {
      dir = 1;
      tray.repeat_timer = 0.f;
      tray.was_held = false;
    } else if (context->pressed(bck)) {
      dir = -1;
      tray.repeat_timer = 0.f;
      tray.was_held = false;
    } else if (held) {
      tray.repeat_timer += dt;
      float threshold = tray.was_held ? tray.repeat_interval : tray.repeat_delay;
      if (tray.repeat_timer >= threshold) {
        tray.repeat_timer = 0.f;
        tray.was_held = true;
        // Determine direction from whichever key is held
        dir = context->is_held_down(fwd) ? 1 : -1;
      }
    } else {
      tray.repeat_timer = 0.f;
      tray.was_held = false;
    }

    if (dir != 0) {
      tray.selection_index = (tray.selection_index + dir + count) % count;
    }

    // WidgetPress activates selected child
    // The tray's own HasClickListener is handled by HandleClicks (which runs
    // before this system). If HandleClicks set the tray's .down flag, we
    // propagate that activation to the currently selected child.
    if (entity.has<HasClickListener>() &&
        entity.get<HasClickListener>().down) {
      auto sel_opt = UICollectionHolder::getEntityForID(
          tray.navigable_children[tray.selection_index]);
      if (sel_opt.has_value()) {
        Entity &sel = sel_opt.asE();
        if (sel.has<HasClickListener>()) {
          sel.get<HasClickListener>().cb(sel);
          sel.get<HasClickListener>().down = true;
        }
      }
    }
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
      auto child_entity = UICollectionHolder::getEntityForID(child_id);
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
      auto child_entity = UICollectionHolder::getEntityForID(child_id);
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
      Entity &child = UICollectionHolder::get().collection.createEntity();
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
    auto child_entity = UICollectionHolder::getEntityForID(child_id);
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
        Entity &grandchild = UICollectionHolder::get().collection.createEntity();
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

/// Query the UI collection for the first entity carrying the given DragTag.
// TODO should we inline these? 
inline OptEntity find_drag_tagged(DragTag tag) {
  auto &ui_coll = UICollectionHolder::get().collection;
  return EntityQuery(ui_coll, {.ignore_temp_warning = true})
      .whereHasTag(tag)
      .gen_first();
}

/// Mark every entity carrying the given DragTag for cleanup.
inline void cleanup_drag_tagged(DragTag tag) {
  auto &ui_coll = UICollectionHolder::get().collection;
  for (Entity &e :
       EntityQuery(ui_coll, {.ignore_temp_warning = true})
           .whereHasTag(tag)
           .gen()) {
    e.cleanup = true;
  }
}

/// Clear the given DragTag from every entity that has it.
inline void untag_all(DragTag tag) {
  auto &ui_coll = UICollectionHolder::get().collection;
  for (Entity &e :
       EntityQuery(ui_coll, {.ignore_temp_warning = true})
           .whereHasTag(tag)
           // TODO we need gen_for_each() or something
           .gen()) {
    e.disableTag(tag);
  }
}

/// Create or update the floating overlay entity at the given position.
/// On the first call (no existing overlay), creates a new entity and copies
/// visual properties from the DragTag::DraggedItem entity.
/// On subsequent calls, just updates the position of the existing overlay.
inline void create_or_update_drag_overlay(DragGroupState &state, float mouse_x,
                                          float mouse_y) {
  // If an overlay already exists, just reposition it.
  auto existing = find_drag_tagged(DragTag::Overlay);
  if (existing && existing.asE().has<UIComponent>()) {
    auto &cmp = existing.asE().get<UIComponent>();
    cmp.computed_rel[Axis::X] = mouse_x - state.dragged_width / 2.0f;
    cmp.computed_rel[Axis::Y] = mouse_y - state.dragged_height / 2.0f;
    return;
  }

  // First frame of drag: create overlay from scratch.
  auto dragged_opt = find_drag_tagged(DragTag::DraggedItem);
  if (!dragged_opt) return;

  auto &ui_coll = UICollectionHolder::get().collection;
  Entity &overlay = ui_coll.createEntity();
  overlay.enableTag(DragTag::Overlay);
  overlay.addComponent<UIComponentDebug>("drag_overlay");
  auto &overlay_cmp = overlay.addComponent<UIComponent>(overlay.id);
  {
    overlay_cmp.absolute = true;
    overlay_cmp.render_layer = 1000;
    overlay_cmp.was_rendered_to_screen = true;

    // Position directly (layout already ran).
    overlay_cmp.computed[Axis::X] = state.dragged_width;
    overlay_cmp.computed[Axis::Y] = state.dragged_height;
    overlay_cmp.computed_rel[Axis::X] = mouse_x - state.dragged_width / 2.0f;
    overlay_cmp.computed_rel[Axis::Y] = mouse_y - state.dragged_height / 2.0f;
  }

  // Copy visual properties from the dragged entity.
  // TODO: Only flat properties (HasLabel, HasColor) are copied. Dragged items
  //       with children (nested divs, icons, etc.) won't render correctly in
  //       the overlay. Consider deep-cloning the subtree or re-parenting.
  Entity &d = dragged_opt.asE();
  if (d.has<HasLabel>()) {
    auto &src_label = d.get<HasLabel>();
    overlay.addComponent<HasLabel>(src_label.label, src_label.is_disabled);
    auto &src_cmp = d.get<UIComponent>();
    overlay_cmp.enable_font(src_cmp.font_name, src_cmp.font_size);
  }
  if (d.has<HasColor>()) {
    overlay.addComponent<HasColor>(d.get<HasColor>().color());
  }
}

/// Create or reuse a spacer entity sized to match the dragged item, then
/// insert it into the hover group's children list at the correct position.
/// The spacer entity is kept alive for the duration of the drag (same
/// pattern as the overlay) so it stays in the merged entity list and is
/// always discoverable via getEntityForID.
inline void create_or_update_drag_spacer(DragGroupState &state) {
  auto hover_opt = find_drag_tagged(DragTag::HoverGroup);
  if (!hover_opt || !hover_opt.asE().has<UIComponent>()) return;

  auto dragged_opt = find_drag_tagged(DragTag::DraggedItem);
  EntityID dragged_id = dragged_opt ? dragged_opt.asE().id : (EntityID)-1;

  auto &group_cmp = hover_opt.asE().get<UIComponent>();

  // --- Reuse existing spacer or create a new one ---
  auto existing = find_drag_tagged(DragTag::Spacer);
  Entity *spacer_ptr = nullptr;
  if (existing && existing.asE().has<UIComponent>()) {
    spacer_ptr = &existing.asE();
    auto &spacer_cmp = spacer_ptr->get<UIComponent>();
    spacer_cmp.set_parent(hover_opt.asE().id);
    spacer_cmp.set_desired_width(pixels(state.dragged_width));
    spacer_cmp.set_desired_height(pixels(state.dragged_height));
  } else {
    auto &ui_coll = UICollectionHolder::get().collection;
    Entity &spacer = ui_coll.createEntity();
    spacer.enableTag(DragTag::Spacer);
    spacer.addComponent<UIComponentDebug>("drag_spacer");
    auto &spacer_cmp = spacer.addComponent<UIComponent>(spacer.id);
    spacer_cmp.set_parent(hover_opt.asE().id);
    spacer_cmp.set_desired_width(pixels(state.dragged_width));
    spacer_cmp.set_desired_height(pixels(state.dragged_height));
    spacer_ptr = &spacer;
  }

  // Map hover_index (among visible children) to children list position,
  // skipping the hidden dragged entity.
  int target_pos = 0;
  int visible = 0;
  for (int i = 0; i < static_cast<int>(group_cmp.children.size()); i++) {
    if (group_cmp.children[i] == dragged_id) {
      target_pos++;
      continue;
    }
    if (visible == state.hover_index) break;
    visible++;
    target_pos++;
  }
  target_pos =
      std::min(target_pos, static_cast<int>(group_cmp.children.size()));
  group_cmp.children.insert(group_cmp.children.begin() + target_pos,
                            spacer_ptr->id);
}

template <typename InputAction>
struct HandleDragGroupsPreLayout : System<> {
/// runs BEFORE RunAutoLayout.
/// - Hides the dragged entity (should_hide) so layout skips it.
/// - Inserts a spacer entity at the current hover position so the layout
///   reserves a gap.
  virtual ~HandleDragGroupsPreLayout() {}

  virtual void once(float) override {
    auto *state = EntityHelper::get_singleton_cmp<DragGroupState>();
    if (!state || !state->dragging) return;

    // --- Hide the dragged entity ---
    auto dragged_opt = find_drag_tagged(DragTag::DraggedItem);
    if (!dragged_opt || !dragged_opt.asE().has<UIComponent>()) {
      cleanup_drag_tagged(DragTag::Spacer);
      cleanup_drag_tagged(DragTag::Overlay);
      untag_all(DragTag::DraggedItem);
      untag_all(DragTag::SourceGroup);
      untag_all(DragTag::HoverGroup);
      state->reset_drag();
      return;
    }
    dragged_opt.asE().get<UIComponent>().should_hide = true;

    // --- Reuse (or create) spacer and insert at hover position ---
    // The spacer entity is kept alive for the duration of the drag;
    // cleanup happens in clear_all_drag_tags when the drag ends.
    create_or_update_drag_spacer(*state);
  }
};

template <typename InputAction>
struct HandleDragGroupsPostLayout : System<> {
/// runs AFTER RunAutoLayout and HandleDrags.
/// - Detects drag start (mouse press inside a drag_group child).
/// - While dragging: updates hover_group / hover_index, creates an overlay
///   entity at the mouse cursor (queued to render_cmds for this frame).
/// - On mouse release: emits a DragGroupState::Event and cleans up.

  virtual ~HandleDragGroupsPostLayout() {}

  /// Remove all drag tags and clean up ephemeral entities.
  void clear_all_drag_tags(DragGroupState *state) {
    cleanup_drag_tagged(DragTag::Spacer);
    cleanup_drag_tagged(DragTag::Overlay);
    untag_all(DragTag::DraggedItem);
    untag_all(DragTag::SourceGroup);
    untag_all(DragTag::HoverGroup);
    state->reset_drag();
  }

  virtual void once(float) override {
    auto *state = EntityHelper::get_singleton_cmp<DragGroupState>();
    if (!state) return;

    auto *ctx = EntityHelper::get_singleton_cmp<UIContext<InputAction>>();
    if (!ctx) return;

    auto &ui_coll = UICollectionHolder::get().collection;

    // Note: overlay is reused across frames (not recreated each frame)
    // because newly created entities aren't in the slot map until the next
    // merge, making them invisible to getEntityForID at render time.

    // --- Query all drag groups ---
    auto groups = EntityQuery(ui_coll, {.ignore_temp_warning = true})
                      .whereHasTag(DragTag::Group)
                      .whereHasComponent<UIComponent>()
                      .gen();

    // ---- Not dragging: check for drag start ---------------------------------
    if (!state->dragging) {
      if (!ctx->mouse.just_pressed) return;

      for (Entity &group : groups) {
        auto &group_cmp = group.get<UIComponent>();
        for (int i = 0; i < static_cast<int>(group_cmp.children.size()); i++) {
          auto child_opt =
              EntityQuery(ui_coll, {.ignore_temp_warning = true})
                  .whereID(group_cmp.children[i])
                  .whereHasComponent<UIComponent>()
                  .whereLambda([](const Entity &e) {
                    return !e.get<UIComponent>().should_hide;
                  })
                  .gen_first();
          if (!child_opt) continue;

          Entity &child = child_opt.asE();
          auto &child_cmp = child.get<UIComponent>();

          if (is_mouse_inside(ctx->mouse.pos, child_cmp.rect())) {
            state->dragging = true;
            state->drag_source_index = i;
            state->hover_index = i;
            state->dragged_width = child_cmp.rect().width;
            state->dragged_height = child_cmp.rect().height;

            // Tag the participants
            child.enableTag(DragTag::DraggedItem);
            group.enableTag(DragTag::SourceGroup);
            group.enableTag(DragTag::HoverGroup);

            // Hide immediately so this frame's render skips it.
            child_cmp.should_hide = true;
            break;
          }
        }
        if (state->dragging) break;
      }
      return; // overlay will appear next frame
    }

    // ---- Mouse released: emit event and clean up ----------------------------
    if (!ctx->mouse.left_down) {
      auto source_opt = find_drag_tagged(DragTag::SourceGroup);
      auto hover_opt = find_drag_tagged(DragTag::HoverGroup);

      if (source_opt && hover_opt) {
        EntityID source_id = source_opt.asE().id;
        EntityID hover_id = hover_opt.asE().id;
        if (hover_id != source_id ||
            state->hover_index != state->drag_source_index) {
          state->events.push_back(DragGroupState::Event{
              .source_group = source_id,
              .source_index = state->drag_source_index,
              .target_group = hover_id,
              .target_index = state->hover_index,
          });
        }
      }

      // Unhide dragged entity
      auto dragged_opt = find_drag_tagged(DragTag::DraggedItem);
      if (dragged_opt && dragged_opt.asE().has<UIComponent>()) {
        dragged_opt.asE().get<UIComponent>().should_hide = false;
      }

      clear_all_drag_tags(state);
      return;
    }

    // ---- Still dragging: update hover + create overlay ----------------------
    auto dragged_opt = find_drag_tagged(DragTag::DraggedItem);
    if (!dragged_opt) {
      clear_all_drag_tags(state);
      return;
    }

    EntityID dragged_id = dragged_opt.asE().id;

    // Default hover back to source group
    auto source_opt = find_drag_tagged(DragTag::SourceGroup);
    untag_all(DragTag::HoverGroup);
    if (source_opt) {
      source_opt.asE().enableTag(DragTag::HoverGroup);
      state->hover_index = state->drag_source_index;
    }

    for (Entity &group : groups) {
      auto &group_cmp = group.get<UIComponent>();
      RectangleType group_rect = group_cmp.rect();

      if (!is_mouse_inside(ctx->mouse.pos, group_rect)) continue;

      // Move hover tag to this group
      untag_all(DragTag::HoverGroup);
      group.enableTag(DragTag::HoverGroup);

      // Determine insertion index among visible children.
      // Use the group's flex direction to pick the correct axis.
      bool horizontal = group_cmp.flex_direction == FlexDirection::Row;
      int insert_idx = 0;
      int visible_count = 0;
      for (int i = 0; i < static_cast<int>(group_cmp.children.size()); i++) {
        EntityID child_id = group_cmp.children[i];
        if (child_id == dragged_id) continue;

        auto child_opt =
            EntityQuery(ui_coll, {.ignore_temp_warning = true})
                .whereID(child_id)
                .whereHasComponent<UIComponent>()
                .whereLambda([](const Entity &e) {
                  return !e.hasTag(DragTag::Spacer) &&
                         !e.get<UIComponent>().should_hide;
                })
                .gen_first();
        if (!child_opt) continue;
        auto &child_cmp = child_opt.asE().template get<UIComponent>();
        auto r = child_cmp.rect();
        float child_mid = horizontal ? (r.x + r.width / 2.0f)
                                     : (r.y + r.height / 2.0f);
        float mouse_pos = horizontal ? ctx->mouse.pos.x : ctx->mouse.pos.y;
        if (mouse_pos > child_mid) {
          insert_idx = visible_count + 1;
        }
        visible_count++;
      }

      state->hover_index = insert_idx;
      break;
    }

    // --- Create or update floating overlay at cursor ---
    create_or_update_drag_overlay(*state, ctx->mouse.pos.x, ctx->mouse.pos.y);

    // Queue for rendering at a high layer.
    auto overlay_opt = find_drag_tagged(DragTag::Overlay);
    if (overlay_opt) {
      ctx->queue_render(RenderInfo{overlay_opt.asE().id, 1000});
    }
  }
};

/// Processes mouse wheel input for all entities with HasScrollView.
/// Runs after RunAutoLayout so that entity rects reflect the current frame.
/// Replaces the inline wheel handling that was previously in scroll_view().
template <typename InputAction>
struct HandleScrollInput : SystemWithUIContext<HasScrollView> {
  UIContext<InputAction> *context;

  virtual ~HandleScrollInput() {}

  virtual void once(float) override {
    this->context =
        EntityHelper::get_singleton_cmp<UIContext<InputAction>>();
  }

  virtual void for_each_with(Entity &entity, UIComponent &cmp,
                             HasScrollView &scroll_state, float) {
    // TODO can we make this a tag? 
    if (!cmp.was_rendered_to_screen)
      return;
    // TODO can we combine these? 
    if (cmp.should_hide || entity.has<ShouldHide>())
      return;

    // Update viewport size from computed layout
    scroll_state.viewport_size = {cmp.computed[Axis::X], cmp.computed[Axis::Y]};

    // In auto mode, skip scroll input when content fits in viewport
    if (scroll_state.auto_overflow) {
      bool needs_v = scroll_state.vertical_enabled && scroll_state.needs_scroll_y();
      bool needs_h = scroll_state.horizontal_enabled && scroll_state.needs_scroll_x();
      if (!needs_v && !needs_h) {
        scroll_state.scroll_offset = {0, 0};
        return;
      }
    }

    // Skip input on the first frame when rect hasn't been computed yet
    RectangleType rect = cmp.rect();
    if (rect.width <= 0.0f || rect.height <= 0.0f)
      return;

    if (!is_mouse_inside(context->mouse.pos, rect))
      return;

    Vector2Type wheel_v = input::get_mouse_wheel_move_v();

    // TODO add support for customizing this for "natural" scroll
    // Direction multiplier: natural scrolling (default) vs inverted
    float direction = scroll_state.invert_scroll ? 1.0f : -1.0f;

    // Vertical scrolling
    if (scroll_state.vertical_enabled && wheel_v.y != 0.0f) {
      scroll_state.scroll_offset.y +=
          direction * wheel_v.y * scroll_state.scroll_speed;
      if (scroll_state.scroll_offset.y < 0.0f) {
        scroll_state.scroll_offset.y = 0.0f;
      }
    }

    // Horizontal scrolling
    if (scroll_state.horizontal_enabled && wheel_v.x != 0.0f) {
      scroll_state.scroll_offset.x +=
          direction * wheel_v.x * scroll_state.scroll_speed;
      if (scroll_state.scroll_offset.x < 0.0f) {
        scroll_state.scroll_offset.x = 0.0f;
      }
    }
  }
};

} // namespace afterhours

} // namespace afterhours
