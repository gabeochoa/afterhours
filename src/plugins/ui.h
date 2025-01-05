
#pragma once

#include <algorithm>
#include <atomic>
#include <format>
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
  int layer = 0;
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
struct SkipWhenTabbing : BaseComponent {};

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

namespace imm {
struct ElementResult {
  // no explicit on purpose
  ElementResult(bool val, Entity &ref) : result(val), element(ref) {}
  ElementResult(bool val, Entity &ref, float data_)
      : result(val), element(ref), data(data_) {}
  ElementResult(bool val, Entity &ref, int data_)
      : result(val), element(ref), data(data_) {}
  ElementResult(bool val, Entity &ref, size_t data_)
      : result(val), element(ref), data((int)data_) {}

  ElementResult(Entity &ent) : result(true), element(ent) {}

  operator bool() const { return result; }
  EntityID id() const { return element.id; }
  Entity &ent() const { return element; }
  UIComponent &cmp() const { return element.get<UIComponent>(); }

  template <typename T> T as() const { return std::get<T>(data); }

private:
  bool result = false;
  Entity &element;
  std::variant<float, int> data = 0.f;
};

using UI_UUID = size_t;
static std::map<UI_UUID, EntityID> existing_ui_elements;

using EntityParent = std::pair<RefEntity, RefEntity>;

static EntityParent
mk(Entity &parent, EntityID otherID = -1,
   const std::source_location location = std::source_location::current()) {

  std::stringstream pre_hash;
  pre_hash << parent.id << otherID << "file: " << location.file_name() << '('
           << location.line() << ':' << location.column() << ") `"
           << location.function_name() << "`: " << '\n';

  UI_UUID hash = std::hash<std::string>{}(pre_hash.str());

  if (existing_ui_elements.contains(hash)) {
    auto entityID = existing_ui_elements.at(hash);
    log_trace("Reusing element {} for {}", hash, entityID);
    return {EntityHelper::getEntityForIDEnforce(entityID), parent};
  }

  Entity &entity = EntityHelper::createEntity();
  existing_ui_elements[hash] = entity.id;
  log_info("Creating element {} for {}", hash, entity.id);
  return {entity, parent};
}

template <typename T>
concept HasUIContext = requires(T a) {
  {
    std::is_same_v<T, UIContext<typename decltype(a)::value_type>>
  } -> std::convertible_to<bool>;
};

struct ComponentConfig {
  ComponentSize size;
  Padding padding;
  Margin margin;
  std::string label;
  bool is_absolute = false;

  // inheritable options
  bool skip_when_tabbing = false;

  // debugs
  std::string debug_name;
  int render_layer = 0;

#ifdef AFTER_HOURS_USE_RAYLIB
  std::optional<raylib::Color> color;
#endif
};

static Vector2Type default_component_size = {150.f, 50.f};

static bool _init_component(Entity &entity, Entity &parent,
                            ComponentConfig config, UIComponentDebug::Type type,
                            const std::string debug_name = "") {

  bool created = false;

  // only once on startup
  if (entity.is_missing<UIComponent>()) {
    entity.addComponent<ui::UIComponent>(entity.id).set_parent(parent.id);

    entity
        .addComponent<UIComponentDebug>(type) //
        .set(type, debug_name);

    if (!config.label.empty())
      entity.addComponent<ui::HasLabel>(config.label);

#ifdef AFTER_HOURS_USE_RAYLIB
    if (config.color.has_value()) {
      entity.addComponent<HasColor>(config.color.value());
    }
#endif

    if (config.skip_when_tabbing)
      entity.addComponent<SkipWhenTabbing>();

    UIComponent &parent_cmp = parent.get<UIComponent>();
    parent_cmp.add_child(entity.id);
    created = true;
  }

  // things that happen every frame

  entity.get<UIComponent>()
      .set_desired_width(config.size.first)
      .set_desired_height(config.size.second)
      .set_desired_padding(config.padding)
      .set_desired_margin(config.margin);

  if (config.is_absolute)
    entity.get<UIComponent>().make_absolute();

  if (!config.debug_name.empty()) {
    entity.get<UIComponentDebug>().set(UIComponentDebug::Type::custom,
                                       config.debug_name);
  }

#ifdef AFTER_HOURS_USE_RAYLIB
  if (config.color.has_value()) {
    entity.get<HasColor>().color = config.color.value();
  }
#endif

  if (!config.label.empty())
    entity.get<ui::HasLabel>().label = config.label;

  return created;
}

ElementResult div(HasUIContext auto &ctx, EntityParent ep_pair,
                  ComponentConfig config = ComponentConfig()) {
  Entity &entity = ep_pair.first;
  Entity &parent = ep_pair.second;

  config.size = ComponentSize{children(), children()};

  _init_component(entity, parent, config, UIComponentDebug::Type::div);

  ctx.queue_render(RenderInfo{entity.id, config.render_layer});

  return {true, entity};
}

ElementResult button(HasUIContext auto &ctx, EntityParent ep_pair,
                     ComponentConfig config = ComponentConfig()) {
  Entity &entity = ep_pair.first;
  Entity &parent = ep_pair.second;

#ifdef AFTER_HOURS_USE_RAYLIB
  config.color = raylib::BLUE;
#endif

  config.size = ComponentSize{pixels(default_component_size.x),
                              pixels(default_component_size.y)};

  _init_component(entity, parent, //
                  config, UIComponentDebug::Type::button);

  entity.addComponentIfMissing<HasClickListener>([](Entity &) {});

  ctx.queue_render(RenderInfo{entity.id, config.render_layer});

  return ElementResult{entity.get<HasClickListener>().down, entity};
}

/*
    if (imm::checkbox(context, mk(button_group.ent()), test)) {
      log_info("checkbox {}", test);
    }
*/
ElementResult checkbox(HasUIContext auto &ctx, EntityParent ep_pair,
                       bool &value,
                       ComponentConfig config = ComponentConfig()) {

  Entity &entity = ep_pair.first;
  Entity &parent = ep_pair.second;

  if (entity.is_missing<ui::HasCheckboxState>())
    entity.addComponent<ui::HasCheckboxState>(value);
  HasCheckboxState &checkboxState = entity.get<ui::HasCheckboxState>();

  config.label = checkboxState.on ? "X" : " ";
#ifdef AFTER_HOURS_USE_RAYLIB
  config.color = raylib::BLUE;
#endif

  config.size = ComponentSize{pixels(default_component_size.x),
                              pixels(default_component_size.y)};

  _init_component(entity, parent, //
                  config, UIComponentDebug::Type::checkbox);

  entity.addComponentIfMissing<HasClickListener>([](Entity &checkbox) {
    ui::HasCheckboxState &hcs = checkbox.get<ui::HasCheckboxState>();
    hcs.on = !hcs.on;
    checkbox.get<ui::HasLabel>().label = hcs.on ? "X" : " ";
  });

  ctx.queue_render(RenderInfo{entity.id, config.render_layer});

  value = checkboxState.on;
  return ElementResult{checkboxState.on, entity};
}

ElementResult slider(HasUIContext auto &ctx, EntityParent ep_pair,
                     float &owned_value,
                     ComponentConfig config = ComponentConfig()) {
  Entity &entity = ep_pair.first;
  Entity &parent = ep_pair.second;

  Vector2Type size = default_component_size;
  if (!config.label.empty()) {
    size.x /= 2.f;
    size.y /= 2.f;
  }
  ElementResult result = {false, entity};

#ifdef AFTER_HOURS_USE_RAYLIB
  config.color = raylib::BLUE;
#endif

  config.size = ComponentSize{pixels(size.x), pixels(size.y)};

  _init_component(entity, parent, //
                  config, UIComponentDebug::Type::custom, "slider");

  auto elem = div(ctx, mk(parent, entity.id + 0),
                  ComponentConfig{
                      .size =
                          ComponentSize{
                              pixels(size.x),
                              pixels(size.y),
                          },
                      // inheritables
                      .skip_when_tabbing = config.skip_when_tabbing,
                      // debugs
                      .debug_name = "slider_background",
                      .render_layer = config.render_layer,
#ifdef AFTER_HOURS_USE_RAYLIB
                      .color = raylib::RED,
#endif
                  });

  // TODO why do we need to do this?
  // why isnt this covered by the pixels(size.x) above?
  elem.ent().template get<UIComponent>().set_desired_width(pixels(size.x));

  Entity &slider_bg = elem.ent();
  if (slider_bg.is_missing<ui::HasSliderState>())
    slider_bg.addComponent<ui::HasSliderState>(owned_value);

  HasSliderState &sliderState = slider_bg.get<ui::HasSliderState>();
  // sliderState.value = owned_value;
  sliderState.changed_since = true;

  {
    slider_bg.addComponentIfMissing<ui::HasDragListener>(
        [&sliderState](Entity &draggable) {
          float mnf = 0.f;
          float mxf = 1.f;

          UIComponent &cmp = draggable.get<UIComponent>();
          Rectangle rect = cmp.rect();

          auto mouse_position = input::get_mouse_position();
          float v = (mouse_position.x - rect.x) / rect.width;
          if (v < mnf)
            v = mnf;
          if (v > mxf)
            v = mxf;

          if (v != sliderState.value) {
            sliderState.value = v;
            sliderState.changed_since = true;
          }

          auto opt_child =
              EntityQuery()
                  .whereID(draggable.get<UIComponent>().children[0])
                  .gen_first();

          UIComponent &child_cmp = opt_child->get<UIComponent>();
          child_cmp.set_desired_padding(
              pixels(sliderState.value * 0.75f * rect.width), Axis::left);
        });
  }

  auto handle = div(ctx, mk(slider_bg),
                    ComponentConfig{
                        .size = {pixels(size.x * 0.25f), pixels(size.y)},
                        .padding =
                            Padding{
                                .left = pixels(owned_value * 0.75f * size.x),
                            },
                        // inheritables
                        .skip_when_tabbing = config.skip_when_tabbing,
                        // debugs
                        .debug_name = "slider_handle",
                        .render_layer = config.render_layer + 1,
#ifdef AFTER_HOURS_USE_RAYLIB
                        .color = raylib::GREEN,
#endif
                    });

  handle.cmp()
      .set_desired_width(pixels(size.x * 0.25f))
      .set_desired_height(pixels(size.y));

  // TODO tabbing when dragged should go to handle

  ctx.queue_render(RenderInfo{entity.id, config.render_layer});

  owned_value = sliderState.value;
  return ElementResult{sliderState.changed_since, entity, sliderState.value};
}

/*
    if (imm::dropdown(context, mk(button_group.ent()), data, option_index)) {
      log_info("dropdown {}", option_index);
    }
    */

ElementResult dropdown(HasUIContext auto &ctx, EntityParent ep_pair,
                       const std::vector<std::string> &options,
                       int &option_index,
                       ComponentConfig config = ComponentConfig()) {

  Entity &entity = ep_pair.first;
  Entity &parent = ep_pair.second;

  if (options.empty())
    return {false, entity};

  if (entity.is_missing<ui::HasDropdownState>())
    entity.addComponent<ui::HasDropdownState>(
        options, nullptr, [&](size_t opt) {
          HasDropdownState &ds = entity.get<ui::HasDropdownState>();
          if (!ds.on) {
            ds.last_option_clicked = opt;
          }
        });
  HasDropdownState &dropdownState = entity.get<ui::HasDropdownState>();
  dropdownState.last_option_clicked = option_index;
  dropdownState.changed_since = false;

  const auto toggle_visibility = [&ctx](Entity &dd) {
    HasDropdownState &ds = dd.get<ui::HasDropdownState>();
    ds.on = !ds.on;
    ds.changed_since = true;

    auto children = dd.get<UIComponent>().children;
    if (ds.on) {
      for (size_t i = 0; i < children.size(); i++) {
        EntityID id = children[i];
        Entity &opt = EntityHelper::getEntityForIDEnforce(id);
        if (i != 0 && opt.has<UIComponent>())
          opt.get<UIComponent>().should_hide = false;

        if (i == ds.last_option_clicked) {
          ctx.set_focus(id);
        }
      }
    } else {
      for (size_t i = 0; i < children.size(); i++) {
        EntityID id = children[i];
        Entity &opt = EntityHelper::getEntityForIDEnforce(id);
        if (i != 0 && opt.has<UIComponent>())
          opt.get<UIComponent>().should_hide = true;
      }
    }
  };

  const auto make_dropdown = [&]() -> UIComponent & {
    if (entity.is_missing<UIComponent>()) {
      entity.addComponent<UIComponentDebug>(UIComponentDebug::Type::dropdown);
      entity.addComponent<UIComponent>(entity.id)
          .set_desired_width(pixels(default_component_size.x))
          .set_desired_height(children(default_component_size.y))
          .set_desired_padding(config.padding)
          .set_desired_margin(config.margin)
          .set_parent(parent.id);

      UIComponent &parent_cmp = parent.get<UIComponent>();
      parent_cmp.add_child(entity.id);
    }

    return entity.get<UIComponent>();
  };

  UIComponent &cmp = make_dropdown();

  const auto on_option_click = [toggle_visibility, options, &ctx](Entity &dd,
                                                                  size_t i) {
    toggle_visibility(dd);

    HasDropdownState &ds = dd.get<ui::HasDropdownState>();
    ds.last_option_clicked = i;

    EntityID id = dd.get<UIComponent>().children[0];
    Entity &first_child = EntityHelper::getEntityForIDEnforce(id);

    first_child.get<ui::HasLabel>().label = (options[ds.last_option_clicked]);
    ctx.set_focus(first_child.id);
  };

  if (button(
          ctx, mk(entity, 0),
          ComponentConfig{
              .label =
                  options[dropdownState.on ? 0
                                           : dropdownState.last_option_clicked],
              // inheritables
              .skip_when_tabbing = config.skip_when_tabbing,
              // debugs
          })) {

    dropdownState.on // when closed we dont want to "select" the visible option
        ? on_option_click(entity, 0)
        : toggle_visibility(entity);
  }

  if (dropdownState.on) {
    for (size_t i = 1; i < options.size(); i++) {
      if (button(ctx, mk(entity, i),
                 ComponentConfig{
                     .label = options[i],
                     // inheritables
                     .skip_when_tabbing = config.skip_when_tabbing,
                     // debugs
                     .render_layer = (config.render_layer + 1),
                 })) {
        on_option_click(entity, i);
      }
    }
  } else {
  }

  ctx.queue_render(RenderInfo{entity.id, config.render_layer});

  option_index = dropdownState.last_option_clicked;
  return ElementResult{dropdownState.changed_since, entity,
                       dropdownState.last_option_clicked};
}

} // namespace imm

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

template <typename InputAction>
struct TrackIfComponentWillBeRendered : System<> {

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
    if (entity.is_missing<HasClickListener>() &&
        entity.is_missing<HasDragListener>())
      return;
    if (entity.has<SkipWhenTabbing>())
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
                                     const AutoLayoutRoot &, float) const {
    render(entity);
    level += 2;
    indent = 0;
  }
};

template <typename InputAction> struct RenderImm : System<> {

  RenderImm() : System<>() { this->include_derived_children = true; }

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

    // NOTE: i dont think we need this TODO
    // for (EntityID child : cmp.children) {
    // render(context, font_manager, AutoLayout::to_ent_static(child));
    // }
  }

  virtual void for_each_with(const Entity &entity,
                             // TODO why if we add these to the filter,
                             // this doesnt return any entities?
                             //
                             // const UIContext<InputAction> &context,
                             // const FontManager &font_manager,
                             float) const override {

    if (entity.is_missing<UIContext<InputAction>>())
      return;
    if (entity.is_missing<FontManager>())
      return;

    const UIContext<InputAction> &context =
        entity.get<UIContext<InputAction>>();
    const FontManager &font_manager = entity.get<FontManager>();

    std::ranges::sort(context.render_cmds, [](RenderInfo a, RenderInfo b) {
      return a.layer < b.layer;
    });

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

//// /////
////  Plugin Info
//// /////

enum struct InputValidationMode { None, LogOnly, Assert };

constexpr static InputValidationMode validation_mode =
#if defined(AFTER_HOURS_INPUT_VALIDATION_ASSERT)
    InputValidationMode::Assert;
#elif defined(AFTER_HOURS_INPUT_VALIDATION_LOG_ONLY)
    InputValidationMode::LogOnly;
#elif defined(AFTER_HOURS_INPUT_VALIDATION_NONE)
    InputValidationMode::None;
#else
    InputValidationMode::LogOnly;
#endif

// NOTE: i tried to write this as a constexpr function but
// the string joining wasnt working for me for some reason
#define validate_enum_has_value(enum_name, name, reason)                       \
  do {                                                                         \
    if constexpr (validation_mode == InputValidationMode::None) {              \
      log_error("validation mode none");                                       \
      return;                                                                  \
    }                                                                          \
                                                                               \
    if constexpr (!magic_enum::enum_contains<enum_name>(name)) {               \
      if constexpr (validation_mode == InputValidationMode::Assert) {          \
        static_assert(false, "InputAction missing value '" name                \
                             "'. Input used to " reason);                      \
      } else if constexpr (validation_mode == InputValidationMode::LogOnly) {  \
        log_warn("InputAction missing value '" name                            \
                 "'. Input used to " reason);                                  \
      } else {                                                                 \
      }                                                                        \
    }                                                                          \
  } while (0);

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

  validate_enum_has_value(InputAction, "None", "any unmapped input");
  validate_enum_has_value(InputAction, "WidgetMod",
                          "while held, press WidgetNext to do WidgetBack");
  validate_enum_has_value(InputAction, "WidgetNext",
                          "'tab' forward between ui elements");
  validate_enum_has_value(InputAction, "WidgetBack",
                          "'tab' back between ui elements");
  validate_enum_has_value(InputAction, "WidgetPress", "click on element");

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
        std::make_unique<ui::TrackIfComponentWillBeRendered<InputAction>>());
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
      std::make_unique<ui::RenderDebugAutoLayoutRoots<InputAction>>(
          toggle_debug));
  sm.register_render_system(std::make_unique<ui::RenderImm<InputAction>>());
}
#endif

} // namespace ui

} // namespace afterhours
