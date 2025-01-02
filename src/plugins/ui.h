
#pragma once

#include <algorithm>
#include <atomic>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <optional>
#include <source_location>
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

struct RenderInfo {
  EntityID id;
};

template <typename InputAction> struct UIContext : BaseComponent {
  using value_type = InputAction;

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

  [[nodiscard]] bool is_hot(EntityID id) const { return hot_id == id; };
  [[nodiscard]] bool is_active(EntityID id) const { return active_id == id; };
  void set_hot(EntityID id) { hot_id = id; }
  void set_active(EntityID id) { active_id = id; }

  bool has_focus(EntityID id) const { return focus_id == id; }
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
    render_cmds.clear();
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

  mutable std::vector<RenderInfo> render_cmds;

  void queue_render(RenderInfo &&info) { render_cmds.emplace_back(info); }
};

struct UIComponentDebug : BaseComponent {
  enum struct Type {
    custom,
    //
    div,
    button,
    checkbox,
    dropdown,
    slider,
  } type;

  std::string name;

  UIComponentDebug(Type type_) : type(type_) {}
  UIComponentDebug(const std::string &name_)
      : type(Type::custom), name(name_) {}

  void set(Type type_, const std::string &name_) {
    type = type_;
    name = name_;
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

struct HasCheckboxState : BaseComponent {
  bool changed_since = false;
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

#ifdef AFTER_HOURS_USE_RAYLIB
// TODO add a non raylib version...
struct HasColor : BaseComponent {
  raylib::Color color;
  HasColor(raylib::Color c) : color(c) {}
};
#endif

struct SliderConfig {
  Vector2Type size;
  float starting_pct;
  std::function<std::optional<std::string>(float)> on_slider_changed = nullptr;
  std::string label = "";
  bool label_is_format_string = false;
};

#ifdef AFTER_HOURS_IMM_UI

namespace imm {
struct ElementResult {
  // no explicit on purpose
  ElementResult(bool val, Entity &ref) : result(val), element(ref) {}
  ElementResult(bool val, Entity &ref, float data_)
      : result(val), element(ref), data(data_) {}

  ElementResult(Entity &ent) : result(true), element(ent) {}

  operator bool() const { return result; }
  EntityID id() const { return element.id; }
  Entity &ent() const { return element; }

  template <typename T> T as() const { return std::get<T>(data); }

private:
  bool result = false;
  Entity &element;
  std::variant<float> data = 0.f;
};

using UI_UUID = size_t;
std::map<UI_UUID, EntityID> existing_ui_elements;

using EntityParent = std::pair<RefEntity, RefEntity>;

EntityParent
mk(Entity &parent, EntityID otherID = 0,
   const std::source_location location = std::source_location::current()) {

  std::stringstream pre_hash;
  pre_hash << parent.id << otherID << "file: " << location.file_name() << '('
           << location.line() << ':' << location.column() << ") `"
           << location.function_name() << "`: " << '\n';

  UI_UUID hash = std::hash<std::string>{}(pre_hash.str());

  if (existing_ui_elements.contains(hash)) {
    auto entityID = existing_ui_elements.at(hash);
    // std::cout << "Reusing element " << hash << " for " << entityID << "\n";
    return {EntityHelper::getEntityForIDEnforce(entityID), parent};
  }

  Entity &entity = EntityHelper::createEntity();
  existing_ui_elements[hash] = entity.id;
  // std::cout << "Creating element " << hash << " for " << entity.id << "\n";
  return {entity, parent};
}

template <typename T>
concept HasUIContext = requires(T a) {
  {
    std::is_same_v<T, UIContext<typename decltype(a)::value_type>>
  } -> std::convertible_to<bool>;
};

ElementResult div(HasUIContext auto &ctx, EntityParent ep_pair) {
  Entity &entity = ep_pair.first;
  Entity &parent = ep_pair.second;

  const auto make_div = [&]() -> UIComponent & {
    if (entity.is_missing<UIComponent>()) {
      entity.addComponent<UIComponentDebug>(UIComponentDebug::Type::div);
      entity.addComponent<ui::UIComponent>(entity.id)
          .set_desired_width(children())
          .set_desired_height(children())
          // .set_desired_padding(padding.left, ui::Axis::left)
          // .set_desired_padding(padding.right, ui::Axis::right)
          // .set_desired_padding(padding.top, ui::Axis::top)
          // .set_desired_padding(padding.bottom, ui::Axis::bottom)
          // .set_desired_margin(margin.left, ui::Axis::left)
          // .set_desired_margin(margin.right, ui::Axis::right)
          // .set_desired_margin(margin.top, ui::Axis::top)
          // .set_desired_margin(margin.bottom, ui::Axis::bottom)
          .set_parent(parent.id);

      Entity &parent_ent = EntityHelper::getEntityForIDEnforce(parent.id);
      UIComponent &parent_cmp = parent_ent.get<UIComponent>();
      parent_cmp.add_child(entity.id);
    }
    return entity.get<UIComponent>();
  };

  make_div();

  return {true, entity};
}

Vector2Type button_size = {150.f, 50.f};
ElementResult button(HasUIContext auto &ctx, EntityParent ep_pair) {
  Entity &entity = ep_pair.first;
  Entity &parent = ep_pair.second;

  const auto make_button = [&]() -> UIComponent & {
    if (entity.is_missing<UIComponent>()) {
      entity.addComponent<UIComponentDebug>(UIComponentDebug::Type::button);
      entity.addComponent<UIComponent>(entity.id)
          .set_desired_width(pixels(button_size.x))
          .set_desired_height(pixels(button_size.y))
          .set_parent(parent.id);
      entity.addComponent<HasClickListener>([](Entity &) {});

      entity.addComponent<ui::HasLabel>("Label");
#ifdef AFTER_HOURS_USE_RAYLIB
      entity.addComponent<HasColor>(raylib::BLUE);
#endif

      UIComponent &parent_cmp = parent.get<UIComponent>();
      parent_cmp.add_child(entity.id);
    }
    return entity.get<UIComponent>();
  };

  UIComponent &cmp = make_button();

  ctx.queue_render(RenderInfo{entity.id});

  return ElementResult{entity.get<HasClickListener>().down, entity};
}

ElementResult slider(HasUIContext auto &ctx, EntityParent ep_pair,
                     float &owned_value, std::string label = "") {
  Entity &entity = ep_pair.first;
  Entity &parent = ep_pair.second;

  ElementResult result = {false, entity};

  Vector2Type size = button_size;
  if (!label.empty()) {
    size.x /= 2.f;
    size.y /= 2.f;
  }

  if (entity.is_missing<ui::HasSliderState>())
    entity.addComponent<ui::HasSliderState>(owned_value);

  HasSliderState &sliderState = entity.get<ui::HasSliderState>();

  const auto on_drag = [](Entity &draggable) {
    float mnf = 0.f;
    float mxf = 1.f;

    UIComponent &cmp = draggable.get<UIComponent>();
    Rectangle rect = cmp.rect();

    HasSliderState &state = draggable.get<HasSliderState>();

    auto mouse_position = input::get_mouse_position();
    float v = (mouse_position.x - rect.x) / rect.width;
    if (v < mnf)
      v = mnf;
    if (v > mxf)
      v = mxf;
    if (v != state.value) {
      state.value = v;
      state.changed_since = true;
    }

    auto opt_child =
        EntityQuery()
            .whereID(draggable.get<ui::HasChildrenComponent>().children[0])
            .gen_first();

    UIComponent &child_cmp = opt_child->get<UIComponent>();
    child_cmp.set_desired_padding(pixels(state.value * 0.75f * rect.width),
                                  Axis::left);
  };

  const auto make_slider = [size, &on_drag,
                            &owned_value](Entity &slider_entity,
                                          EntityID parent_id) -> Entity & {
    if (slider_entity.is_missing<UIComponent>()) {
      slider_entity.addComponent<UIComponentDebug>(
          UIComponentDebug::Type::slider);
      slider_entity.addComponent<UIComponent>(slider_entity.id)
          .set_desired_width(pixels(size.x))
          .set_desired_height(pixels(size.y))
          .set_parent(parent_id);

      // slider_entity.addComponent<ui::HasLabel>("Label");
      slider_entity.addComponent<ui::HasDragListener>(on_drag);

      auto &handle = EntityHelper::createEntity();
      handle.addComponent<UIComponent>(handle.id)
          .set_desired_width(ui::Size{
              .dim = ui::Dim::Pixels,
              .value = size.x * 0.25f,
          })
          .set_desired_height(ui::Size{
              .dim = ui::Dim::Pixels,
              .value = size.y,
          })
          .set_desired_padding(pixels(owned_value * size.x), Axis::left)
          .set_parent(slider_entity.id);
      handle.addComponent<UIComponentDebug>("slider_handle");
      slider_entity.get<ui::UIComponent>().add_child(handle.id);

#ifdef AFTER_HOURS_USE_RAYLIB
      slider_entity.addComponent<ui::HasColor>(raylib::GREEN);
      handle.addComponent<ui::HasColor>(raylib::BLUE);
#endif

      slider_entity.addComponent<ui::HasChildrenComponent>();
      slider_entity.get<ui::HasChildrenComponent>().add_child(handle);

      Entity &parent_ent = EntityHelper::getEntityForIDEnforce(parent_id);
      UIComponent &parent_cmp = parent_ent.get<UIComponent>();
      parent_cmp.add_child(slider_entity.id);
    }
    return slider_entity;
  };

  if (label.empty()) {
    make_slider(entity, parent.id);

    ctx.queue_render(RenderInfo{entity.id});

    owned_value = sliderState.value;
    return ElementResult{sliderState.changed_since, entity, sliderState.value};
  }

  auto elem_pair = div(ctx, mk(parent, entity.id));
  Entity &div_ent = elem_pair.ent();
  div_ent.get<UIComponent>().set_flex_direction(FlexDirection::Row);
  div_ent.get<UIComponentDebug>().set(UIComponentDebug::Type::custom,
                                      "slider_group");

  // label
  {
    Entity &label_holder = div(ctx, mk(div_ent, entity.id)).ent();
    label_holder.get<UIComponent>()
        .set_desired_width(pixels(size.x))
        .set_desired_height(pixels(size.y));
    label_holder.get<UIComponentDebug>().set(UIComponentDebug::Type::custom,
                                             "slider_label");
    label_holder.addComponent<HasLabel>(label);

    // TODO right now we require color to render,
    // but we should require color or label
#ifdef AFTER_HOURS_USE_RAYLIB
    label_holder.addComponent<ui::HasColor>(raylib::BLUE);
#endif
  }

  make_slider(entity, div_ent.id);

  ctx.queue_render(RenderInfo{div_ent.id});

  owned_value = sliderState.value;
  return ElementResult{sliderState.changed_since, div_ent, sliderState.value};
}

} // namespace imm

#endif

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
    cmp.was_rendered_to_screen = false;
  }
};

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

    // AutoLayout::print_tree(cmp);
    // log_error("");
  }
};

struct TrackIfComponentWasRendered : System<AutoLayoutRoot, UIComponent> {

  void set_visibility(UIComponent &cmp) {
    // Hiding hides children
    if (cmp.should_hide)
      return;
    for (EntityID child : cmp.children) {
      set_visibility(AutoLayout::to_cmp_static(child));
    }
    // you might still have visible children even if you arent big (i think)
    if (cmp.width() < 0 || cmp.height() < 0) {
      return;
    }
    cmp.was_rendered_to_screen = true;
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

  UIContext<InputAction> *context;

  virtual void once(float) override {
    this->context =
        EntityHelper::get_singleton_cmp<ui::UIContext<InputAction>>();
    this->include_derived_children = true;
  }

  virtual ~HandleClicks() {}

  virtual void for_each_with_derived(Entity &entity, UIComponent &component,
                                     HasClickListener &hasClickListener,
                                     float) {
    hasClickListener.down = false;

    if (!component.was_rendered_to_screen)
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
    if (entity.is_missing<HasClickListener>())
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
struct UpdateDropdownOptions
    : SystemWithUIContext<HasDropdownState, HasChildrenComponent> {

  UIContext<InputAction> *context;

  virtual void once(float) override {
    this->context =
        EntityHelper::get_singleton_cmp<ui::UIContext<InputAction>>();
  }

  UpdateDropdownOptions()
      : SystemWithUIContext<HasDropdownState, HasChildrenComponent>() {
    this->include_derived_children = true;
  }

  virtual void for_each_with_derived(Entity &entity, UIComponent &component,
                                     HasDropdownState &hasDropdownState,
                                     HasChildrenComponent &hasChildren, float) {
    auto options = hasDropdownState.options;
    hasDropdownState.options = hasDropdownState.fetch_options(hasDropdownState);

    // detect if the options changed or if the state changed
    // and if so, we should refresh
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

      if (!changed)
        return;
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
        child.addComponent<ui::HasLabel>(options[i]);
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
  }
};

#ifdef AFTER_HOURS_USE_RAYLIB

static void draw_text(const ui::FontManager &fm, const std::string &text,
                      vec2 position, int font_size, raylib::Color color) {
  DrawTextEx(fm.get_active_font(), text.c_str(), position, font_size, 1.f,
             color);
}

template <typename InputAction>
struct RenderAutoLayoutRoots : SystemWithUIContext<AutoLayoutRoot> {

  virtual ~RenderAutoLayoutRoots() {}

  ui::FontManager *font_manager;
  ui::UIContext<InputAction> *ui_context;

  virtual void once(float) override {
    this->ui_context =
        EntityHelper::get_singleton_cmp<ui::UIContext<InputAction>>();
    this->font_manager = EntityHelper::get_singleton_cmp<ui::FontManager>();

    this->include_derived_children = true;
  }

  void render_me(const Entity &entity) const {
    const UIComponent &cmp = entity.get<UIComponent>();

    raylib::Color col = entity.get<HasColor>().color;

    if (ui_context->is_hot(entity.id)) {
      col = raylib::RED;
    }

    if (ui_context->has_focus(entity.id)) {
      raylib::DrawRectangleRec(cmp.focus_rect(), raylib::PINK);
    }
    raylib::DrawRectangleRec(cmp.rect(), col);
    if (entity.has<HasLabel>()) {
      draw_text(*font_manager, entity.get<HasLabel>().label.c_str(),
                vec2{cmp.x(), cmp.y()}, (int)(cmp.height() / 2.f),
                raylib::RAYWHITE);
    }
  }

  void render(const Entity &entity) const {
    const UIComponent &cmp = entity.get<UIComponent>();
    if (cmp.should_hide)
      return;

    if (cmp.font_name != UIComponent::UNSET_FONT)
      font_manager->set_active(cmp.font_name);

    if (entity.has<HasColor>()) {
      render_me(entity);
    }

    for (EntityID child : cmp.children) {
      render(AutoLayout::to_ent_static(child));
    }
  }

  virtual void for_each_with_derived(const Entity &entity, const UIComponent &,
                                     const AutoLayoutRoot &,
                                     float) const override {
    render(entity);
  }
};

template <typename InputAction>
struct RenderDebugAutoLayoutRoots : SystemWithUIContext<AutoLayoutRoot> {

  InputAction toggle_action;
  bool enabled = false;
  float enableCooldown = 0.f;
  float enableCooldownReset = 0.2f;

  UIContext<InputAction> *context;

  mutable int level = 0;
  mutable int indent = 0;

  RenderDebugAutoLayoutRoots(InputAction toggle_kp)
      : toggle_action(toggle_kp) {}

  virtual ~RenderDebugAutoLayoutRoots() {}

  virtual bool should_run(float dt) override {
    enableCooldown -= dt;

    if (enableCooldown < 0) {
      enableCooldown = enableCooldownReset;
      input::PossibleInputCollector<InputAction> inpc =
          input::get_input_collector<InputAction>();
      for (auto &actions_done : inpc.inputs()) {
        if (actions_done.action == toggle_action) {
          enabled = !enabled;
          break;
        }
      }
    }
    return enabled;
  }

  virtual void once(float) override {
    this->context =
        EntityHelper::get_singleton_cmp<ui::UIContext<InputAction>>();
    this->include_derived_children = true;
    this->level = 0;
    this->indent = 0;
  }

  void render_me(const Entity &entity) const {
    const UIComponent &cmp = entity.get<UIComponent>();

    float fontSize = 20;
    float x = 10 * indent;
    float y = (fontSize * level) + fontSize / 2.f;

    std::string component_name = "Unknown";
    if (entity.has<UIComponentDebug>()) {
      const auto &cmpdebug = entity.get<UIComponentDebug>();
      auto type = cmpdebug.type;
      if (type == UIComponentDebug::Type::custom) {
        component_name = cmpdebug.name;
      } else {
        component_name =
            magic_enum::enum_name<UIComponentDebug::Type>(cmpdebug.type);
      }
    }

    std::string widget_str = fmt::format(
        "{:03} (x{:05.2f} y{:05.2f}) w{:05.2f}xh{:05.2f} {}", (int)entity.id,
        cmp.x(), cmp.y(), cmp.rect().width, cmp.rect().height, component_name);

    DrawText(widget_str.c_str(), x, y, fontSize, raylib::RAYWHITE);
  }

  void render(const Entity &entity) const {
    const UIComponent &cmp = entity.get<UIComponent>();
    if (cmp.should_hide)
      return;

    render_me(entity);
    level++;

    indent++;
    for (EntityID child : cmp.children) {
      render(AutoLayout::to_ent_static(child));
    }
    indent--;
  }

  virtual void for_each_with_derived(const Entity &entity, const UIComponent &,
                                     const AutoLayoutRoot &,
                                     float) const override {
    render(entity);
    level += 2;
    indent = 0;
  }
};

template <typename InputAction>
struct RenderImm : System<UIContext<InputAction>, FontManager> {

  virtual void once(float) override { this->include_derived_children = true; }

  void render_me(const UIContext<InputAction> &context,
                 const FontManager &font_manager, const Entity &entity) const {
    const UIComponent &cmp = entity.get<UIComponent>();

    raylib::Color col = entity.template get<HasColor>().color;

    if (context.is_hot(entity.id)) {
      col = raylib::RED;
    }

    if (context.has_focus(entity.id)) {
      raylib::DrawRectangleRec(cmp.focus_rect(), raylib::PINK);
    }
    raylib::DrawRectangleRec(cmp.rect(), col);
    if (entity.has<HasLabel>()) {
      draw_text(font_manager, entity.get<HasLabel>().label.c_str(),
                vec2{cmp.x(), cmp.y()}, (int)(cmp.height() / 2.f),
                raylib::RAYWHITE);
    }
  }

  void render(const UIContext<InputAction> &context,
              const FontManager &font_manager, const Entity &entity) const {
    const UIComponent &cmp = entity.get<UIComponent>();
    if (cmp.should_hide)
      return;

    if (cmp.font_name != UIComponent::UNSET_FONT)
      const_cast<FontManager &>(font_manager).set_active(cmp.font_name);

    if (entity.has<HasColor>()) {
      render_me(context, font_manager, entity);
    }

    for (EntityID child : cmp.children) {
      render(context, font_manager, AutoLayout::to_ent_static(child));
    }
  }

  virtual void for_each_with(const Entity &,
                             const UIContext<InputAction> &context,
                             const FontManager &font_manager,
                             float) const override {
    for (auto &cmd : context.render_cmds) {
      auto id = cmd.id;
      auto &ent = EntityHelper::getEntityForIDEnforce(id);
      render(context, font_manager, ent);
    }
    context.render_cmds.clear();
  }
};

#endif

// TODO i really wanted to statically validate this
// so that any developer would get a reasonable compiler msg
// but i couldnt get it working with both trivial types and struct types
// https://godbolt.org/z/7v4n7s1Kn
/*
template <typename T, typename = void>
struct is_vector_data_convertible_to_string : std::false_type {};
template <typename T>
using stringable = std::is_convertible<T, std::string>;

template <typename T>
struct is_vector_data_convertible_to_string;

template <typename T>
struct is_vector_data_convertible_to_string<std::vector<T>> : stringable<T> {};

template <typename T>
struct is_vector_data_convertible_to_string<
    T, std::void_t<decltype(std::to_string(
           std::declval<typename T::value_type>()))>> : std::true_type {};
*/

template <typename Derived, typename DataStorage, typename ProvidedType,
          typename ProviderComponent>
struct ProviderConsumer : public DataStorage {

  struct has_fetch_data_member {
    template <typename U>
    static auto test(U *)
        -> decltype(std::declval<U>().fetch_data(), void(), std::true_type{});

    template <typename U> static auto test(...) -> std::false_type;

    static constexpr bool value = decltype(test<ProviderComponent>(0))::value;
  };

  struct has_on_data_changed_member {
    template <typename U>
    static auto test(U *) -> decltype(std::declval<U>().on_data_changed(0),
                                      void(), std::true_type{});

    template <typename U> static auto test(...) -> std::false_type;

    static constexpr bool value = decltype(test<ProviderComponent>(0))::value;
  };

  struct return_type {
    using type = decltype(std::declval<ProviderComponent>().fetch_data());
  };

  ProviderConsumer(const std::function<void(size_t)> &on_change)
      : DataStorage({{}}, get_data_from_provider, on_change) {}

  virtual void write_value_change_to_provider(const DataStorage &storage) = 0;
  virtual ProvidedType convert_from_fetch(return_type::type data) const = 0;

  static ProvidedType get_data_from_provider(const DataStorage &storage) {
    // log_info("getting data from provider");
    static_assert(std::is_base_of_v<BaseComponent, ProviderComponent>,
                  "ProviderComponent must be a child of BaseComponent");
    // Convert the data in the provider component to a list of options
    auto &provider_entity = EntityQuery()
                                .whereHasComponent<ProviderComponent>()
                                .gen_first_enforce();
    ProviderComponent &pComp =
        provider_entity.template get<ProviderComponent>();

    static_assert(has_fetch_data_member::value,
                  "ProviderComponent must have fetch_data function");

    auto fetch_data_result = pComp.fetch_data();
    return static_cast<const Derived &>(storage).convert_from_fetch(
        fetch_data_result);
  }
};

template <typename ProviderComponent>
struct HasDropdownStateWithProvider
    : public ProviderConsumer<HasDropdownStateWithProvider<ProviderComponent>,
                              HasDropdownState, HasDropdownState::Options,
                              ProviderComponent> {

  using PC = ProviderConsumer<HasDropdownStateWithProvider<ProviderComponent>,
                              HasDropdownState, HasDropdownState::Options,
                              ProviderComponent>;

  int max_num_options = -1;
  HasDropdownStateWithProvider(const std::function<void(size_t)> &on_change,
                               int max_options_ = -1)
      : PC(on_change), max_num_options(max_options_) {}

  virtual HasDropdownState::Options
  convert_from_fetch(PC::return_type::type fetched_data) const override {

    // TODO see message above
    // static_assert(is_vector_data_convertible_to_string<
    // decltype(fetch_data_result)>::value, "ProviderComponent::fetch_data must
    // return a vector of items " "convertible to string");
    HasDropdownState::Options new_options;
    for (auto &data : fetched_data) {
      if (max_num_options > 0 && (int)new_options.size() > max_num_options)
        continue;
      new_options.push_back((std::string)data);
    }
    return new_options;
  }

  virtual void
  write_value_change_to_provider(const HasDropdownState &hdswp) override {
    size_t index = hdswp.last_option_clicked;

    auto &entity = EntityQuery()
                       .whereHasComponent<ProviderComponent>()
                       .gen_first_enforce();
    ProviderComponent &pComp = entity.template get<ProviderComponent>();

    static_assert(
        PC::has_on_data_changed_member::value,
        "ProviderComponent must have on_data_changed(index) function");
    pComp.on_data_changed(index);
  }
};

//// /////
////  UI Component Makers
//// /////

template <typename ProviderComponent>
Entity &make_dropdown(Entity &parent, int max_options = -1) {
  using WRDS = ui::HasDropdownStateWithProvider<ProviderComponent>;
  auto &dropdown = EntityHelper::createEntity();
  dropdown.addComponent<UIComponentDebug>(UIComponentDebug::Type::dropdown);

  dropdown.addComponent<UIComponent>(dropdown.id).set_parent(parent.id);
  parent.get<ui::UIComponent>().add_child(dropdown.id);

  dropdown.addComponent<ui::HasChildrenComponent>();

  dropdown.addComponent<WRDS>(
      [&dropdown](size_t) {
        WRDS &wrds = dropdown.get<WRDS>();
        if (!wrds.on) {
          wrds.write_value_change_to_provider(wrds);
        }
      },
      max_options);

#ifdef AFTER_HOURS_USE_RAYLIB
  dropdown.addComponent<ui::HasColor>(raylib::BLUE);
#endif

  return dropdown;
}

static void force_layout_and_print(
    Entity &root,
    window_manager::Resolution resolution = window_manager::Resolution()) {
  std::map<EntityID, RefEntity> components;
  auto comps = EntityQuery().whereHasComponent<ui::UIComponent>().gen();
  for (Entity &entity : comps) {
    components.emplace(entity.id, entity);
  }

  if (resolution.width == 0 || resolution.height == 0) {
    Entity &e =
        EntityQuery()
            .whereHasComponent<window_manager::ProvidesCurrentResolution>()
            .gen_first_enforce();
    resolution =
        e.get<window_manager::ProvidesCurrentResolution>().current_resolution;
  }

  ui::AutoLayout::autolayout(root.get<ui::UIComponent>(), resolution,
                             components);
  ui::AutoLayout::print_tree(root.get<ui::UIComponent>());
}

static Size padding_(float v, float strict = 0.5f) {
  return ui::Size{
      .dim = ui::Dim::Percent,
      .value = v,
      .strictness = strict,
  };
}

struct Padding {
  Size top;
  Size left;
  Size bottom;
  Size right;
};

struct Margin {
  Size top;
  Size bottom;
  Size left;
  Size right;
};

static Entity &make_div(Entity &parent, ComponentSize cz,
                        Padding padding = Padding(), Margin margin = Margin()) {
  auto &div = EntityHelper::createEntity();
  {
    div.addComponent<UIComponentDebug>(UIComponentDebug::Type::div);
    div.addComponent<ui::UIComponent>(div.id)
        .set_desired_width(cz.first)
        .set_desired_height(cz.second)
        .set_desired_padding(padding.left, ui::Axis::left)
        .set_desired_padding(padding.right, ui::Axis::right)
        .set_desired_padding(padding.top, ui::Axis::top)
        .set_desired_padding(padding.bottom, ui::Axis::bottom)
        .set_desired_margin(margin.left, ui::Axis::left)
        .set_desired_margin(margin.right, ui::Axis::right)
        .set_desired_margin(margin.top, ui::Axis::top)
        .set_desired_margin(margin.bottom, ui::Axis::bottom)
        .set_parent(parent.id);
    parent.get<ui::UIComponent>().add_child(div.id);
  }
  return div;
}

Entity &make_button(Entity &parent, const std::string &label,
                    Vector2Type button_size,
                    const std::function<void(Entity &)> &click = {},
                    Padding padding = Padding(), Margin margin = Margin()) {
  auto &entity = EntityHelper::createEntity();
  entity.addComponent<UIComponentDebug>(UIComponentDebug::Type::button);
  entity.addComponent<UIComponent>(entity.id)
      .set_desired_width(ui::Size{
          .dim = ui::Dim::Pixels,
          .value = button_size.x,
      })
      .set_desired_height(ui::Size{
          .dim = ui::Dim::Pixels,
          .value = button_size.y,
      })
      .set_desired_padding(padding.left, ui::Axis::left)
      .set_desired_padding(padding.right, ui::Axis::right)
      .set_desired_padding(padding.top, ui::Axis::top)
      .set_desired_padding(padding.bottom, ui::Axis::bottom)
      .set_desired_margin(margin.left, ui::Axis::left)
      .set_desired_margin(margin.right, ui::Axis::right)
      .set_desired_margin(margin.top, ui::Axis::top)
      .set_desired_margin(margin.bottom, ui::Axis::bottom)
      .set_parent(parent.id);

  parent.get<ui::UIComponent>().add_child(entity.id);

  entity.addComponent<ui::HasLabel>(label);
  entity.addComponent<ui::HasClickListener>(click);
#ifdef AFTER_HOURS_USE_RAYLIB
  entity.addComponent<ui::HasColor>(raylib::BLUE);
#endif
  return entity;
}

Entity &internal_make_slider(Entity &parent, SliderConfig config,
                             Entity &label) {
  // TODO add vertical slider

  auto &background = EntityHelper::createEntity();
  background.addComponent<UIComponentDebug>(UIComponentDebug::Type::slider);
  background.addComponent<UIComponent>(background.id)
      .set_desired_width(pixels(config.size.x))
      .set_desired_height(pixels(config.size.y))
      .set_parent(parent.id);
  parent.get<ui::UIComponent>().add_child(background.id);

  background.addComponent<ui::HasSliderState>(0.5f);
#ifdef AFTER_HOURS_USE_RAYLIB
  background.addComponent<ui::HasColor>(raylib::GREEN);
#endif
  background.addComponent<ui::HasDragListener>(
      [config, &label](Entity &entity) {
        float mnf = 0.f;
        float mxf = 1.f;

        UIComponent &cmp = entity.get<UIComponent>();
        Rectangle rect = cmp.rect();
        HasSliderState &sliderState = entity.get<ui::HasSliderState>();
        float &value = sliderState.value;

        auto mouse_position = input::get_mouse_position();
        float v = (mouse_position.x - rect.x) / rect.width;
        if (v < mnf)
          v = mnf;
        if (v > mxf)
          v = mxf;
        if (v != value) {
          value = v;
          sliderState.changed_since = true;
        }

        auto opt_child =
            EntityQuery()
                .whereID(entity.get<ui::HasChildrenComponent>().children[0])
                .gen_first();

        UIComponent &child_cmp = opt_child->get<UIComponent>();
        child_cmp.set_desired_padding(pixels(value * 0.75f * rect.width),
                                      Axis::left);

        if (sliderState.changed_since && config.on_slider_changed) {
          std::optional<std::string> val_str = config.on_slider_changed(value);
          if (val_str.has_value() && !config.label.empty()) {
            std::string str;
            if (config.label_is_format_string) {
#ifdef AFTER_HOURS_USE_RAYLIB
              fmt::format_args args = fmt::make_format_args(val_str.value());
              str = fmt::vformat(config.label, args);
#endif
            } else {
              str = config.label;
            }
            label.get<HasLabel>().label = str;
          }
        }
      });

  auto &handle = EntityHelper::createEntity();
  handle.addComponent<UIComponent>(handle.id)
      .set_desired_width(ui::Size{
          .dim = ui::Dim::Pixels,
          .value = config.size.x * 0.25f,
      })
      .set_desired_height(ui::Size{
          .dim = ui::Dim::Pixels,
          .value = config.size.y,
      })
      .set_desired_padding(pixels(config.starting_pct * config.size.x),
                           Axis::left)
      .set_parent(background.id);
  handle.addComponent<UIComponentDebug>("slider_handle");
  background.get<ui::UIComponent>().add_child(handle.id);

#ifdef AFTER_HOURS_USE_RAYLIB
  handle.addComponent<ui::HasColor>(raylib::BLUE);
#endif

  background.addComponent<ui::HasChildrenComponent>();
  background.get<ui::HasChildrenComponent>().add_child(handle);
  return background;
}

Entity &make_slider(Entity &parent, SliderConfig config) {
  if (config.label.empty())
    return internal_make_slider(parent, config, parent);

  //
  auto &div = make_div(parent, children_xy());
  div.get<ui::UIComponent>().flex_direction = FlexDirection::Row;

  auto &label = ui::make_div(div, {
                                      pixels(config.size.x / 2.f),
                                      pixels(config.size.y),
                                  });
#ifdef AFTER_HOURS_USE_RAYLIB
  label.addComponent<ui::HasColor>(raylib::GRAY);
#endif

  std::string str;
  if (config.label_is_format_string) {
#ifdef AFTER_HOURS_USE_RAYLIB
    fmt::format_args args;
    if (config.on_slider_changed) {
      auto value = config.on_slider_changed(config.starting_pct);
      args = fmt::make_format_args(value.value());
    } else {
      args = fmt::make_format_args(config.starting_pct);
    }
    str = fmt::vformat(config.label, args);
#endif
  } else {
    str = config.label;
  }

  label.addComponent<ui::HasLabel>(str);

  config.size.x /= 2.f;
  internal_make_slider(div, config, label);

  return div;
}

Entity &make_checkbox(Entity &parent, Vector2Type button_size) {
  auto &entity = EntityHelper::createEntity();
  entity.addComponent<UIComponentDebug>(UIComponentDebug::Type::checkbox);
  entity.addComponent<UIComponent>(entity.id)
      .set_desired_width(ui::Size{
          .dim = ui::Dim::Pixels,
          .value = button_size.x,
      })
      .set_desired_height(ui::Size{
          .dim = ui::Dim::Pixels,
          .value = button_size.y,
      })
      .set_parent(parent.id);
  parent.get<ui::UIComponent>().add_child(entity.id);

#ifdef AFTER_HOURS_USE_RAYLIB
  entity.addComponent<ui::HasColor>(raylib::BLUE);
#endif
  entity.addComponent<ui::HasCheckboxState>(false);
  entity.addComponent<ui::HasLabel>(" ");
  entity.addComponent<ui::HasClickListener>([](Entity &checkbox) {
    log_info("I clicked the checkbox {}", checkbox.id);
    ui::HasCheckboxState &hcs = checkbox.get<ui::HasCheckboxState>();
    hcs.on = !hcs.on;
    checkbox.get<ui::HasLabel>().label = hcs.on ? "X" : " ";
  });
  return entity;
}

//// /////
////  Plugin Info
//// /////

template <typename InputAction>
static void add_singleton_components(Entity &entity) {
  entity.addComponent<UIContext<InputAction>>();
  EntityHelper::registerSingleton<UIContext<InputAction>>(entity);

#ifdef AFTER_HOURS_USE_RAYLIB
  entity.addComponent<ui::FontManager>()
      .load_font(UIComponent::DEFAULT_FONT, raylib::GetFontDefault())
      .load_font(UIComponent::UNSET_FONT, raylib::GetFontDefault());
  EntityHelper::registerSingleton<ui::FontManager>(entity);
#endif
}

template <typename InputAction>
static void enforce_singletons(SystemManager &sm) {
  sm.register_update_system(
      std::make_unique<developer::EnforceSingleton<UIContext<InputAction>>>());
#ifdef AFTER_HOURS_USE_RAYLIB
  sm.register_update_system(
      std::make_unique<developer::EnforceSingleton<ui::FontManager>>());
#endif
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
    sm.register_update_system(
        std::make_unique<ui::TrackIfComponentWasRendered>());
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

#ifdef AFTER_HOURS_USE_RAYLIB
template <typename InputAction>
static void
register_render_systems(SystemManager &sm,
                        InputAction toggle_debug = InputAction::None) {
  sm.register_render_system(
      std::make_unique<ui::RenderAutoLayoutRoots<InputAction>>());
  sm.register_render_system(
      std::make_unique<ui::RenderDebugAutoLayoutRoots<InputAction>>(
          toggle_debug));
  sm.register_render_system(std::make_unique<ui::RenderImm<InputAction>>());
}
#endif

} // namespace ui

} // namespace afterhours
