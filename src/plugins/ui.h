
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

  std::map<EntityID, RefEntity> components;

  virtual void once(float) override {
    components.clear();
    auto comps = EntityQuery().whereHasComponent<UIComponent>().gen();
    for (Entity &entity : comps) {
      components.emplace(entity.id, entity);
    }
  }
  virtual void for_each_with(Entity &, AutoLayoutRoot &, UIComponent &cmp,
                             float) override {

    AutoLayout::autolayout(cmp, components);

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
      set_visibility(AutoLayout::to_cmp_static(child));
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
    this->include_derived_children = true;
  }

  virtual ~HandleClicks() {}

  virtual void for_each_with_derived(Entity &entity, UIComponent &component,
                                     HasClickListener &hasClickListener,
                                     float) {
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
// TODO add a non raylib version...
struct HasColor : BaseComponent {
  raylib::Color color;
  HasColor(raylib::Color c) : color(c) {}
};

template <typename InputAction>
struct RenderAutoLayoutRoots : SystemWithUIContext<AutoLayoutRoot> {

  virtual ~RenderAutoLayoutRoots() {}

  Entity *context_entity;
  virtual void once(float) override {
    OptEntity opt_context =
        EntityQuery()                                        //
            .whereHasComponent<ui::UIContext<InputAction>>() //
            .gen_first();
    this->context_entity = opt_context.value();
    this->include_derived_children = true;
  }

  void render_me(const Entity &entity) const {
    const UIComponent &cmp = entity.get<UIComponent>();

    UIContext<InputAction> &context =
        this->context_entity->template get<UIContext<InputAction>>();

    raylib::Color col = entity.get<HasColor>().color;

    if (context.is_hot(entity.id)) {
      col = raylib::RED;
    }

    if (context.has_focus(entity.id)) {
      raylib::DrawRectangleRec(cmp.focus_rect(), raylib::PINK);
    }
    raylib::DrawRectangleRec(cmp.rect(), col);
    if (entity.has<HasLabel>()) {
      DrawText(entity.get<HasLabel>().label.c_str(), (int)cmp.x(), (int)cmp.y(),
               (int)(cmp.height() / 2.f), raylib::RAYWHITE);
    }
  }

  void render(const Entity &entity) const {
    const UIComponent &cmp = entity.get<UIComponent>();

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

Entity &make_button(Entity &parent, const std::string &label,
                    Vector2Type button_size) {
  auto &entity = EntityHelper::createEntity();
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

  entity.addComponent<ui::HasLabel>(label);
  entity.addComponent<ui::HasClickListener>(
      [](Entity &button) { log_info("I clicked the button {}", button.id); });
#ifdef AFTER_HOURS_USE_RAYLIB
  entity.addComponent<ui::HasColor>(raylib::BLUE);
#endif
  return entity;
}

Entity &make_slider(Entity &parent, Vector2Type button_size) {
  // TODO add vertical slider

  auto &background = EntityHelper::createEntity();
  background.addComponent<UIComponent>(background.id)
      .set_desired_width(ui::Size{
          .dim = ui::Dim::Pixels,
          .value = button_size.x,
      })
      .set_desired_height(ui::Size{
          .dim = ui::Dim::Pixels,
          .value = button_size.y,
      })
      .set_parent(parent.id);
  parent.get<ui::UIComponent>().add_child(background.id);

  background.addComponent<ui::HasSliderState>(0.5f);
#ifdef AFTER_HOURS_USE_RAYLIB
  background.addComponent<ui::HasColor>(raylib::GREEN);
#endif
  background.addComponent<ui::HasDragListener>([](Entity &entity) {
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
    child_cmp.set_desired_width(
        ui::Size{.dim = ui::Dim::Percent, .value = value * 0.75f});

    // log_info("I clicked the slider {} {}", entity.id, value);
  });

  // TODO replace when we have actual padding later
  auto &left_padding = EntityHelper::createEntity();
  left_padding.addComponent<UIComponent>(left_padding.id)
      .set_desired_width(ui::Size{
          .dim = ui::Dim::Percent,
          .value = 0.f,
      })
      .set_desired_height(ui::Size{
          .dim = ui::Dim::Pixels,
          .value = button_size.y,
      })
      .set_parent(background.id);
  background.get<ui::UIComponent>().add_child(left_padding.id);

  auto &handle = EntityHelper::createEntity();
  handle.addComponent<UIComponent>(handle.id)
      .set_desired_width(ui::Size{
          .dim = ui::Dim::Pixels,
          .value = button_size.x * 0.25f,
      })
      .set_desired_height(ui::Size{
          .dim = ui::Dim::Pixels,
          .value = button_size.y,
      })
      .set_parent(background.id);
  background.get<ui::UIComponent>().add_child(handle.id);

#ifdef AFTER_HOURS_USE_RAYLIB
  handle.addComponent<ui::HasColor>(raylib::BLUE);
#endif

  background.addComponent<ui::HasChildrenComponent>();
  background.get<ui::HasChildrenComponent>().add_child(left_padding);
  background.get<ui::HasChildrenComponent>().add_child(handle);
  return background;
}

Entity &make_checkbox(Entity &parent, Vector2Type button_size) {
  auto &entity = EntityHelper::createEntity();
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

#ifdef AFTER_HOURS_USE_RAYLIB
template <typename InputAction>
static void register_render_systems(SystemManager &sm) {
  sm.register_render_system(
      std::make_unique<ui::RenderAutoLayoutRoots<InputAction>>());
}
#endif

} // namespace ui

} // namespace afterhours
