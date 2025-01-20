
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
#include "color.h"
#include "input_system.h"
#include "texture_manager.h"

#include "../drawing_helpers.h"

namespace afterhours {

namespace ui {

struct Theme {
  enum struct Usage {
    Font,
    DarkFont,
    Background,
    Primary,
    Secondary,
    Accent,
    Error,

    //
    Custom,
    Default,
  };

  static bool is_valid(Usage cu) {
    switch (cu) {
    case Usage::Font:
    case Usage::DarkFont:
    case Usage::Background:
    case Usage::Primary:
    case Usage::Secondary:
    case Usage::Accent:
    case Usage::Error:
      return true;
    case Usage::Custom:
    case Usage::Default:
      return false;
    };
    return false;
  }

  Color font;
  Color darkfont;
  Color background;

  Color primary;
  Color secondary;
  Color accent;
  Color error;

  Theme()
      : font(colors::isabelline), darkfont(colors::oxford_blue),
        background(colors::oxford_blue), primary(colors::pacific_blue),
        secondary(colors::tea_green), accent(colors::orange_soda),
        // TODO find a better error color
        error(colors::red) {}

  Theme(Color f, Color df, Color bg, Color p, Color s, Color a, Color e)
      : font(f), darkfont(df), background(bg), primary(p), secondary(s),
        accent(a), error(e) {}

  Color from_usage(Usage cu, bool disabled = false) const {
    Color color;
    switch (cu) {
    case Usage::Font:
      color = font;
      break;
    case Usage::DarkFont:
      color = darkfont;
      break;
    case Usage::Background:
      color = background;
      break;
    case Usage::Primary:
      color = primary;
      break;
    case Usage::Secondary:
      color = secondary;
      break;
    case Usage::Accent:
      color = accent;
      break;
    case Usage::Error:
      color = error;
      break;
    case Usage::Default:
      log_warn("You should not be fetching 'default' color usage from theme, "
               "UI library should handle this??");
      color = primary;
      break;
    case Usage::Custom:
      log_warn("You should not be fetching 'custom' color usage from theme, "
               "UI library should handle this??");
      color = primary;
      break;
    }
    if (disabled) {
      return colors::darken(color, 0.5f);
    }
    return color;
  }

  std::bitset<4> rounded_corners = std::bitset<4>().set();
};

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

  Theme theme;

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
    unknown,
    custom,
  } type;

  std::string name_value;

  UIComponentDebug(Type type_) : type(type_) {}
  UIComponentDebug(const std::string &name_)
      : type(Type::custom), name_value(name_) {}

  void set(const std::string &name_) {
    if (name_ == "") {
      type = Type::unknown;
      return;
    }
    type = Type::custom;
    name_value = name_;
  }

  std::string name() const {
    if (type == UIComponentDebug::Type::custom) {
      return name_value;
    }
    return std::string(magic_enum::enum_name<UIComponentDebug::Type>(type));
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

  template <size_t N>
  HasDropdownState(
      const std::array<std::string_view, N> &opts,
      const std::function<Options(HasDropdownState &)> fetch_opts = nullptr,
      const std::function<void(size_t)> opt_changed = nullptr)
      : HasDropdownState(Options(opts.begin(), opts.end()), fetch_opts,
                         opt_changed) {}
};

struct HasRoundedCorners : BaseComponent {
  std::bitset<4> rounded_corners = std::bitset<4>().reset();
  auto &set(std::bitset<4> input) {
    rounded_corners = input;
    return *this;
  }
  auto &get() const { return rounded_corners; }
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
  ElementResult(bool val, Entity &ref, bool data_)
      : result(val), element(ref), data((bool)data_) {}

  template <size_t Size>
  ElementResult(bool val, Entity &ref, std::bitset<Size> data_)
      : result(val), element(ref), data(data_.to_ulong()) {}

  ElementResult(Entity &ent) : result(true), element(ent) {}

  operator bool() const { return result; }
  EntityID id() const { return element.id; }
  Entity &ent() const { return element; }
  UIComponent &cmp() const { return element.get<UIComponent>(); }

  template <typename T> T as() const { return std::get<T>(data); }

private:
  bool result = false;
  Entity &element;
  std::variant<float, int, bool, unsigned long> data = 0.f;
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

static Vector2Type default_component_size = {200.f, 50.f};

/*

  auto tex = TextureLibrary::get().get("xbox_button_color_a");
  settings_button_config.texture_config = TextureConfig{
      .texture = tex,
      .alignment = texture_manager::HasTexture::Alignment::Center,
  };

 * */
struct TextureConfig {
  texture_manager::Texture texture;
  texture_manager::HasTexture::Alignment alignment =
      texture_manager::HasTexture::Alignment::None;
};

struct ComponentConfig {
  ComponentSize size = ComponentSize(pixels(default_component_size.x),
                                     pixels(default_component_size.y), true);
  Padding padding;
  Margin margin;
  std::string label;
  bool is_absolute = false;
  FlexDirection flex_direction = FlexDirection::Column;

  Theme::Usage color_usage = Theme::Usage::Default;
  std::optional<Color> custom_color;

  std::optional<TextureConfig> texture_config;
  std::optional<std::bitset<4>> rounded_corners;

  // inheritable options
  TextAlignment label_alignment = TextAlignment::None;
  bool skip_when_tabbing = false;
  bool disabled = false;
  bool hidden = false;

  // debugs
  std::string debug_name = "";
  int render_layer = 0;
};

/*


   div
   button
   checkbox
   checkbox group
   dropdown
   slider


   todo
   button group
   rounded checkbox when max set to 1
   focus ring should wrap label when label exists
   toggle button (basically a slider than can only be full or empty)
   pagination with only one visible option (but wraps around when going off


 * */

static bool _init_component(HasUIContext auto &ctx, Entity &entity,
                            Entity &parent, ComponentConfig config,
                            const std::string &debug_name = "") {
  bool created = false;

  // only once on startup
  if (entity.is_missing<UIComponent>()) {
    entity.addComponent<ui::UIComponent>(entity.id).set_parent(parent.id);

    entity.addComponent<UIComponentDebug>(debug_name);

    if (!config.label.empty())
      entity.addComponent<ui::HasLabel>(config.label, config.disabled)
          .set_alignment(config.label_alignment);

    if (Theme::is_valid(config.color_usage)) {
      entity.addComponent<HasColor>(
          ctx.theme.from_usage(config.color_usage, config.disabled));

      if (config.custom_color.has_value()) {
        log_warn("You have custom color set on {} but didnt set "
                 "config.color_usage = Custom",
                 debug_name.c_str());
      }
    }

    if (config.color_usage == Theme::Usage::Custom) {
      if (config.custom_color.has_value()) {
        entity.addComponentIfMissing<HasColor>(config.custom_color.value());
      } else {
        log_warn("You have custom color usage selected on {} but didnt set "
                 "config.custom_color",
                 debug_name.c_str());
        entity.addComponentIfMissing<HasColor>(colors::UI_PINK);
      }
    }

    if (config.skip_when_tabbing)
      entity.addComponent<SkipWhenTabbing>();

    if (config.texture_config.has_value()) {
      const TextureConfig &conf = config.texture_config.value();
      entity.addComponent<texture_manager::HasTexture>(conf.texture,
                                                       conf.alignment);
    }

    created = true;
  }

  UIComponent &parent_cmp = parent.get<UIComponent>();
  parent_cmp.add_child(entity.id);

  // things that happen every frame

  if (config.hidden) {
    entity.addComponentIfMissing<ShouldHide>();
  } else {
    entity.removeComponentIfExists<ShouldHide>();
  }

  entity.get<UIComponent>()
      .set_desired_width(config.size.x_axis)
      .set_desired_height(config.size.y_axis)
      .set_desired_padding(config.padding)
      .set_desired_margin(config.margin);

  if (config.rounded_corners.has_value() &&
      config.rounded_corners.value().any()) {
    entity.addComponentIfMissing<HasRoundedCorners>().set(
        config.rounded_corners.value());
  }

  if (!config.label.empty())
    entity.get<ui::HasLabel>()
        .set_label(config.label)
        .set_disabled(config.disabled)
        .set_alignment(config.label_alignment);

  if (config.is_absolute)
    entity.get<UIComponent>().make_absolute();

  if (!config.debug_name.empty()) {
    entity.get<UIComponentDebug>().set(config.debug_name);
  }

  if (Theme::is_valid(config.color_usage)) {
    entity.get<HasColor>().set(
        ctx.theme.from_usage(config.color_usage, config.disabled));
  }

  if (config.color_usage == Theme::Usage::Custom) {
    if (config.custom_color.has_value()) {
      entity.get<HasColor>().set(config.custom_color.value());
    } else {
      // no warning on this to avoid spamming log
      entity.get<HasColor>().set(colors::UI_PINK);
    }
  }

  if (!config.label.empty())
    entity.get<ui::HasLabel>().label = config.label;

  return created;
}

ElementResult div(HasUIContext auto &ctx, EntityParent ep_pair,
                  ComponentConfig config = ComponentConfig()) {
  Entity &entity = ep_pair.first;
  Entity &parent = ep_pair.second;

  if (config.size.is_default && config.label.empty())
    config.size = ComponentSize{children(), children()};
  if (config.size.is_default && !config.label.empty())
    config.size = ComponentSize{children(default_component_size.x),
                                children(default_component_size.y)};

  // Load from theme when not passed in
  if (!config.rounded_corners.has_value()) {
    config.rounded_corners = ctx.theme.rounded_corners;
  }

  _init_component(ctx, entity, parent, config);

  ctx.queue_render(RenderInfo{entity.id, config.render_layer});

  return {true, entity};
}

ElementResult button(HasUIContext auto &ctx, EntityParent ep_pair,
                     ComponentConfig config = ComponentConfig()) {
  Entity &entity = ep_pair.first;
  Entity &parent = ep_pair.second;

  if (config.color_usage == Theme::Usage::Default)
    config.color_usage = Theme::Usage::Primary;

  // By default buttons have centered text if user didnt specify anything
  if (config.label_alignment == TextAlignment::None) {
    config.label_alignment = TextAlignment::Center;
  }

  // Load from theme when not passed in
  if (!config.rounded_corners.has_value()) {
    config.rounded_corners = ctx.theme.rounded_corners;
  }

  _init_component(ctx, entity, parent, config, "button");

  entity.addComponentIfMissing<HasClickListener>([](Entity &) {});

  ctx.queue_render(RenderInfo{entity.id, config.render_layer});

  return ElementResult{entity.get<HasClickListener>().down, entity};
}

/*
  {
    std::array<std::string, 4> labels = {{
        "play",
        "about",
        "settings",
        "exit",
    }};

    if (auto result = button_group(context, mk(button_group_e.ent()), labels);
        result) {
      log_info("button clicked {}", result.as<int>());
    }
  }
  */

// TODO add support for padding
template <typename Container>
ElementResult button_group(HasUIContext auto &ctx, EntityParent ep_pair,
                           const Container &labels,
                           ComponentConfig config = ComponentConfig()) {

  Entity &entity = ep_pair.first;
  Entity &parent = ep_pair.second;

  auto max_height = config.size.y_axis;
  config.size.y_axis = children(max_height.value);
  _init_component(ctx, entity, parent, config, "button_group");
  config.size.y_axis = max_height;

  bool clicked = false;
  int value = -1;
  for (int i = 0; i < labels.size(); i++) {
    if (button(ctx, mk(entity, i),
               ComponentConfig{
                   .size = config.size,
                   .label = i < labels.size() ? std::string(labels[i]) : "",
                   // inheritables
                   .label_alignment = config.label_alignment,
                   .skip_when_tabbing = config.skip_when_tabbing,
                   .disabled = config.disabled,
                   .hidden = config.hidden,
                   // debugs
                   .debug_name = std::format("button group {}", i),
                   .render_layer = config.render_layer,
               })) {
      clicked = true;
      value = i;
    }
  }

  return {clicked, entity, value};
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
  checkboxState.on = value;

  config.label = checkboxState.on ? "X" : " ";

  // By default have centered text if user didnt specify anything
  if (config.label_alignment == TextAlignment::None) {
    config.label_alignment = TextAlignment::Center;
  }

  if (config.color_usage == Theme::Usage::Default)
    config.color_usage = Theme::Usage::Primary;

  // Load from theme when not passed in
  if (!config.rounded_corners.has_value()) {
    config.rounded_corners = ctx.theme.rounded_corners;
  }

  _init_component(ctx, entity, parent, config, "checkbox");

  if (config.disabled) {
    entity.removeComponentIfExists<HasClickListener>();
  } else {
    entity.addComponentIfMissing<HasClickListener>([](Entity &checkbox) {
      ui::HasCheckboxState &hcs = checkbox.get<ui::HasCheckboxState>();
      hcs.on = !hcs.on;
      hcs.changed_since = true;
      checkbox.get<ui::HasLabel>().label = hcs.on ? "X" : " ";
    });
  }

  ctx.queue_render(RenderInfo{entity.id, config.render_layer});

  value = checkboxState.on;

  ElementResult result{checkboxState.changed_since, entity, checkboxState.on};
  checkboxState.changed_since = false;
  return result;
}

ElementResult checkbox_group_row(HasUIContext auto &ctx, EntityParent ep_pair,
                                 int index, bool &value,
                                 ComponentConfig config = ComponentConfig()) {

  Entity &entity = ep_pair.first;
  Entity &parent = ep_pair.second;

  auto label = config.label;
  config.label = "";

  _init_component(ctx, entity, parent, config, "checkbox_row");

  config.size = ComponentSize(pixels(default_component_size.x),
                              children(default_component_size.y));
  if (!label.empty()) {
    config.size = config.size._scale_x(0.5f);

    div(ctx, mk(entity),
        ComponentConfig{
            .size = config.size,
            .label = label,
            // inheritables
            .label_alignment = config.label_alignment,
            .skip_when_tabbing = config.skip_when_tabbing,
            .disabled = config.disabled,
            .hidden = config.hidden,
            // debugs
            .debug_name = std::format("checkbox label {}", index),
            .render_layer = config.render_layer,
        });
  }

  bool changed = false;
  if (checkbox(ctx, mk(entity), value,
               ComponentConfig{
                   .size = config.size,
                   // inheritables
                   .label_alignment = config.label_alignment,
                   .skip_when_tabbing = config.skip_when_tabbing,
                   .disabled = config.disabled,
                   .hidden = config.hidden,
                   // debugs
                   .debug_name = std::format("checkbox {}", index),
                   .render_layer = config.render_layer,
               })) {
    changed = true;
  }

  return {changed, entity};
}

// imm::checkbox_group(context, mk(elem.ent()), enabled_weapons);
template <size_t Size>
ElementResult
checkbox_group(HasUIContext auto &ctx, EntityParent ep_pair,
               std::bitset<Size> &values,
               const std::array<std::string_view, Size> &labels = {{}},
               std::pair<int, int> min_max = {-1, -1},
               ComponentConfig config = ComponentConfig()) {

  Entity &entity = ep_pair.first;
  Entity &parent = ep_pair.second;

  auto max_height = config.size.y_axis;
  config.size.y_axis = children();
  _init_component(ctx, entity, parent, config, "checkbox_group");
  config.size.y_axis = max_height;

  int count = (int)values.count();

  const auto should_disable = [min_max, count](bool value) -> bool {
    // we should disable, if not checked and we are at the cap
    bool at_cap = !value && min_max.second != -1 && count >= min_max.second;
    // we should disable, if checked and we are at the min
    bool at_min = value && min_max.first != -1 && count <= min_max.first;
    return at_cap || at_min;
  };

  bool changed = false;
  for (int i = 0; i < values.size(); i++) {
    bool value = values.test(i);

    if (checkbox_group_row(
            ctx, mk(entity, i), i, value,
            ComponentConfig{
                .size = config.size,
                .label = i < labels.size() ? std::string(labels[i]) : "",
                .flex_direction = FlexDirection::Row,
                // inheritables
                .label_alignment = config.label_alignment,
                .skip_when_tabbing = config.skip_when_tabbing,
                .disabled = should_disable(value),
                .hidden = config.hidden,
                // debugs
                .debug_name = std::format("checkbox row {}", i),
                .render_layer = config.render_layer,

            })) {
      changed = true;
      if (value)
        values.set(i);
      else
        values.reset(i);
    }
  }

  return {changed, entity, values};
}

ElementResult slider(HasUIContext auto &ctx, EntityParent ep_pair,
                     float &owned_value,
                     ComponentConfig config = ComponentConfig()) {
  Entity &entity = ep_pair.first;
  Entity &parent = ep_pair.second;

  ElementResult result = {false, entity};

  std::string original_label = config.label;
  config.label = "";

  // Load from theme when not passed in
  if (!config.rounded_corners.has_value()) {
    config.rounded_corners = ctx.theme.rounded_corners;
  }

  _init_component(ctx, entity, parent, config, "slider");

  auto label_corners = std::bitset<4>(config.rounded_corners.value()) //
                           .set(1, false)
                           .set(3, false);

  auto label = div(ctx, mk(entity, entity.id + 0),
                   ComponentConfig{
                       .size = config.size,
                       .label = original_label,
                       .rounded_corners = label_corners,
                       // inheritables
                       .label_alignment = config.label_alignment,
                       .skip_when_tabbing = config.skip_when_tabbing,
                       .disabled = config.disabled,
                       .hidden = config.hidden,
                       // debugs
                       .debug_name = "slider_text",
                       .render_layer = config.render_layer + 0,
                       //
                       .color_usage = Theme::Usage::Primary,
                   });
  label.ent()
      .template get<UIComponent>()
      .set_desired_width(config.size._scale_x(0.5f).x_axis)
      .set_desired_height(config.size.y_axis);

  // TODO we want the left two to not be rounded here,
  // but for some reason it doesnt work?
  auto elem_corners = std::bitset<4>(config.rounded_corners.value())
                          .set(0, false)
                          .set(2, false);

  auto elem = div(ctx, mk(entity, parent.id + entity.id + 0),
                  ComponentConfig{
                      .size = config.size,
                      .rounded_corners = elem_corners,
                      // inheritables
                      .label_alignment = config.label_alignment,
                      .skip_when_tabbing = config.skip_when_tabbing,
                      .disabled = config.disabled,
                      .hidden = config.hidden,
                      // debugs
                      .debug_name = "slider_background",
                      .render_layer = config.render_layer + 1,
                      .color_usage = Theme::Usage::Secondary,
                  });

  // TODO why do we need to do this?
  // why isnt this covered by the pixels(size.x) above?
  elem.ent().template get<UIComponent>().set_desired_width(config.size.x_axis);

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
                        .size = {pixels(0.25f * config.size.x_axis.value),
                                 config.size.y_axis},
                        .padding =
                            Padding{
                                .left = pixels(owned_value * 0.75f *
                                               config.size.x_axis.value),
                            },
                        // inheritables
                        .label_alignment = config.label_alignment,
                        .skip_when_tabbing = config.skip_when_tabbing,
                        .disabled = config.disabled,
                        .hidden = config.hidden,
                        // debugs
                        .debug_name = "slider_handle",
                        .render_layer = config.render_layer + 2,
                        .color_usage = Theme::Usage::Primary,
                    });

  handle.cmp()
      .set_desired_width(pixels(0.25f * config.size.x_axis.value))
      .set_desired_height(config.size.y_axis);

  ctx.queue_render(RenderInfo{entity.id, config.render_layer});

  owned_value = sliderState.value;
  return ElementResult{sliderState.changed_since, entity, sliderState.value};
}

/*
    {
      ComponentConfig resolution_config;
      resolution_config.label = "Resolution";

      if (imm::pagination(context, mk(elem.ent()), {{"test", "test2", "test3"}},
                          win_condition_index, std::move(resolution_config))) {
      }
    }
    */
// TODO option_index being returned isnt the same as the one being shown
// visually
template <typename Container>
ElementResult pagination(HasUIContext auto &ctx, EntityParent ep_pair,
                         const Container &options, size_t &option_index,
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
  dropdownState.last_option_clicked = (size_t)option_index;
  dropdownState.changed_since = false;

  const auto on_option_click = [options, &ctx](Entity &dd, size_t i) {
    size_t index = i % options.size();
    HasDropdownState &ds = dd.get<ui::HasDropdownState>();
    ds.last_option_clicked = index;
    ds.on = !ds.on;
    ds.changed_since = true;

    EntityID id = dd.get<UIComponent>().children[i];
    ctx.set_focus(id);
  };

  config.size = ComponentSize(children(default_component_size.x),
                              pixels(default_component_size.y));
  config.flex_direction = FlexDirection::Row;

  // we have to clear the label otherwise itll render at the full
  // component width..
  // TODO is there a way for us to support doing this without the gotchas?
  std::string label_str = config.label;
  config.label = "";

  bool first_time = _init_component(ctx, entity, parent, config, "pagination");

  int child_index = 0;

  if (button(ctx, mk(entity),
             ComponentConfig{
                 .size = ComponentSize{pixels(default_component_size.x / 4.f),
                                       config.size.y_axis},
                 .label = "<",
                 // inheritables
                 .label_alignment = config.label_alignment,
                 .skip_when_tabbing = config.skip_when_tabbing,
                 .disabled = config.disabled,
                 .hidden = config.hidden,
                 // debugs
                 .debug_name = "left",
                 .render_layer = (config.render_layer),
             })) {
    if (option_index > 1) {
      on_option_click(entity, option_index - 1);
    } else {
      on_option_click(entity, options.size());
    }
  }

  for (size_t i = 0; i < options.size(); i++) {
    if (button(ctx, mk(entity, child_index + i),
               ComponentConfig{
                   .size = ComponentSize{pixels(default_component_size.x / 2.f),
                                         config.size.y_axis},
                   .label = std::string(options[i]),
                   // inheritables
                   .label_alignment = config.label_alignment,
                   .skip_when_tabbing = config.skip_when_tabbing,
                   .disabled = config.disabled,
                   .hidden = config.hidden,
                   // debugs
                   .debug_name = std::format("option {}", i + 1),
                   .render_layer = (config.render_layer + 1),
               })) {
      // we do +1 because the < button
      on_option_click(entity, i + 1);
    }
  }

  if (button(ctx, mk(entity),
             ComponentConfig{
                 .size = ComponentSize{pixels(default_component_size.x / 4.f),
                                       config.size.y_axis},
                 .label = ">",
                 // inheritables
                 .label_alignment = config.label_alignment,
                 .skip_when_tabbing = config.skip_when_tabbing,
                 .disabled = config.disabled,
                 .hidden = config.hidden,
                 // debugs
                 .debug_name = "right",
                 .render_layer = (config.render_layer),
             })) {
    if (option_index < options.size()) {
      on_option_click(entity, option_index + 1);
    } else {
      on_option_click(entity, 1);
    }
  }

  // When its the fist time we load, we want the focus to not be on the
  // < button but to be on the current option with index option_index
  if (first_time) {
    EntityID id = entity.get<UIComponent>()
                      .children[dropdownState.last_option_clicked + 1];
    ctx.set_focus(id);
  }

  ctx.queue_render(RenderInfo{entity.id, config.render_layer});

  option_index = dropdownState.last_option_clicked;
  return ElementResult{dropdownState.changed_since, entity,
                       dropdownState.last_option_clicked};
}

/*
    if (imm::dropdown(context, mk(button_group.ent()), data, option_index)) {
      log_info("dropdown {}", option_index);
    }
    */

// TODO add arrows so its easier to distinguish between dropdown and just a
// normal button

template <typename Container>
ElementResult dropdown(HasUIContext auto &ctx, EntityParent ep_pair,
                       const Container &options, size_t &option_index,
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

  config.size = ComponentSize(children(default_component_size.x),
                              pixels(default_component_size.y));

  // we have to clear the label otherwise itll render at the full
  // component width..
  // TODO is there a way for us to support doing this without the gotchas?
  std::string label_str = config.label;
  config.label = "";

  _init_component(ctx, entity, parent, config, "dropdown");

  if (!label_str.empty()) {
    auto label = div(ctx, mk(entity),
                     ComponentConfig{
                         .size =
                             ComponentSize{
                                 pixels(default_component_size.x / 2.f),
                                 pixels(default_component_size.y),
                             },
                         .label = std::string(label_str),
                         // inheritables
                         .label_alignment = config.label_alignment,
                         .skip_when_tabbing = config.skip_when_tabbing,
                         .disabled = config.disabled,
                         .hidden = config.hidden,
                         // debugs
                         .debug_name = "dropdown_label",
                         .render_layer = (config.render_layer + 1),
                         //
                         .color_usage = Theme::Usage::Primary,
                     });
  }

  const auto toggle_visibility = [&](Entity &) {
    dropdownState.on = !dropdownState.on;
  };

  const auto on_option_click = [&](Entity &dd, size_t option_index) {
    toggle_visibility(dd);
    dropdownState.last_option_clicked = option_index;
    dropdownState.changed_since = true;

    EntityID id = entity.get<UIComponent>().children[label_str.empty() ? 0 : 1];
    Entity &first_child = EntityHelper::getEntityForIDEnforce(id);

    first_child.get<ui::HasLabel>().label = (options[option_index]);
    ctx.set_focus(first_child.id);
  };

  if (button(ctx, mk(entity),
             ComponentConfig{
                 .size =
                     ComponentSize{
                         pixels(default_component_size.x / 2.f),
                         pixels(default_component_size.y),
                     },
                 .label = std::string(
                     options[dropdownState.on
                                 ? 0
                                 : dropdownState.last_option_clicked]),
                 // inheritables
                 .label_alignment = config.label_alignment,
                 .skip_when_tabbing = config.skip_when_tabbing,
                 .disabled = config.disabled,
                 .hidden = config.hidden,
                 // debugs
                 .debug_name = "option 1",
                 .render_layer =
                     (config.render_layer + (dropdownState.on ? 0 : 0)),
             })) {

    dropdownState.on // when closed we dont want to "select" the visible option
        ? on_option_click(entity, 0)
        : toggle_visibility(entity);
  }

  if (auto result =
          button_group(ctx, mk(entity), options,
                       ComponentConfig{
                           // inheritables
                           .label_alignment = config.label_alignment,
                           .skip_when_tabbing = config.skip_when_tabbing,
                           .disabled = config.disabled,
                           .hidden = config.hidden || !dropdownState.on,
                           // debugs
                           .debug_name = std::format("dropdown button group"),
                           .render_layer = config.render_layer + 1,
                       });
      result) {
    on_option_click(entity, result.template as<int>());
  }

  ctx.queue_render(RenderInfo{entity.id, config.render_layer});
  option_index = dropdownState.last_option_clicked;
  return ElementResult{dropdownState.changed_since, entity,
                       dropdownState.last_option_clicked};
}

template <typename Container>
ElementResult dropdown_old(HasUIContext auto &ctx, EntityParent ep_pair,
                           const Container &options, size_t &option_index,
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

  size_t first_hidable = config.label.empty() ? 1 : 2;

  const auto toggle_visibility = [first_hidable, &ctx](Entity &dd) {
    HasDropdownState &ds = dd.get<ui::HasDropdownState>();
    ds.on = !ds.on;
    ds.changed_since = true;

    auto children = dd.get<UIComponent>().children;
    if (ds.on) {
      for (size_t i = first_hidable; i < children.size(); i++) {
        EntityID id = children[i];
        Entity &opt = EntityHelper::getEntityForIDEnforce(id);
        if (opt.has<UIComponent>())
          opt.get<UIComponent>().should_hide = false;

        if (i == ds.last_option_clicked) {
          ctx.set_focus(id);
        }
      }
    } else {
      for (size_t i = first_hidable; i < children.size(); i++) {
        EntityID id = children[i];
        Entity &opt = EntityHelper::getEntityForIDEnforce(id);
        if (opt.has<UIComponent>())
          opt.get<UIComponent>().should_hide = true;
      }
    }
  };

  const auto on_option_click = [toggle_visibility, options, &ctx](Entity &dd,
                                                                  size_t i) {
    toggle_visibility(dd);

    HasDropdownState &ds = dd.get<ui::HasDropdownState>();
    ds.last_option_clicked = i;

    EntityID id = dd.get<UIComponent>().children[0];
    Entity &first_child = EntityHelper::getEntityForIDEnforce(id);

    first_child.get<ui::HasLabel>().label = (options[i]);
    ctx.set_focus(first_child.id);
  };

  config.size = ComponentSize(pixels(default_component_size.x),
                              children(default_component_size.y));

  // we have to clear the label otherwise itll render at the full
  // component width..
  // TODO is there a way for us to support doing this without the gotchas?
  std::string label_str = config.label;
  config.label = "";

  _init_component(ctx, entity, parent, config, "dropdown");

  Size width = config.size._scale_x(0.5f).x_axis;

  int child_index = 0;

  if (!label_str.empty()) {
    auto label = div(ctx, mk(entity, child_index++),
                     ComponentConfig{
                         .size = config.size,
                         .label = std::string(label_str),
                         // inheritables
                         .label_alignment = config.label_alignment,
                         .skip_when_tabbing = config.skip_when_tabbing,
                         .disabled = config.disabled,
                         .hidden = config.hidden,
                         // debugs
                         .debug_name = "dropdown_label",
                         .render_layer = (config.render_layer + 1),
                         //
                         .color_usage = Theme::Usage::Primary,
                     });
    label
        .ent() //
        .template get<UIComponent>()
        .set_desired_width(width)
        .set_desired_height(config.size.y_axis);
  }

  if (button(ctx, mk(entity, child_index++),
             ComponentConfig{
                 .size = ComponentSize{width, config.size.y_axis},
                 .label = std::string(
                     options[dropdownState.on
                                 ? 0
                                 : dropdownState.last_option_clicked]),
                 // inheritables
                 .label_alignment = config.label_alignment,
                 .skip_when_tabbing = config.skip_when_tabbing,
                 .disabled = config.disabled,
                 .hidden = config.hidden,
                 // debugs
                 .debug_name = "option 1",
                 .render_layer =
                     (config.render_layer + (dropdownState.on ? 0 : 0)),
             })) {

    dropdownState.on // when closed we dont want to "select" the visible option
        ? on_option_click(entity, 0)
        : toggle_visibility(entity);
  }

  if (dropdownState.on) {
    for (size_t i = 1; i < options.size(); i++) {
      if (button(ctx, mk(entity, child_index + i),
                 ComponentConfig{
                     .size = ComponentSize{width, config.size.y_axis},
                     .label = std::string(options[i]),
                     // inheritables
                     .label_alignment = config.label_alignment,
                     .skip_when_tabbing = config.skip_when_tabbing,
                     .disabled = config.disabled,
                     .hidden = config.hidden,
                     // debugs
                     .debug_name = std::format("option {}", i + 1),
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
  }
};

static RectangleType position_text(const ui::FontManager &fm,
                                   const std::string &text,
                                   RectangleType container,
                                   TextAlignment alignment,
                                   Vector2Type margin_px) {
  Font font = fm.get_active_font();

  // Calculate the maximum text size based on the container size and margins
  Vector2Type max_text_size = Vector2Type{
      .x = container.width - 2 * margin_px.x,
      .y = container.height - 2 * margin_px.y,
  };

  // TODO add some caching here?

  // Find the largest font size that fits within the maximum text size
  float font_size = 1.f;
  Vector2Type text_size;
  while (true) {
    text_size = measure_text(font, text.c_str(), font_size, 1.f);
    if (text_size.x > max_text_size.x || text_size.y > max_text_size.y) {
      break;
    }
    font_size++;
  }
  font_size--; // Decrease font size by 1 to ensure it fits

  // Calculate the text position based on the alignment and margins
  Vector2Type position;
  switch (alignment) {
  case TextAlignment::Left:
    position = Vector2Type{
        .x = container.x + margin_px.x,
        .y = container.y + margin_px.y +
             (container.height - 2 * margin_px.y - text_size.y) / 2,
    };
    break;
  case TextAlignment::Center:
    position = Vector2Type{
        .x = container.x + margin_px.x +
             (container.width - 2 * margin_px.x - text_size.x) / 2,
        .y = container.y + margin_px.y +
             (container.height - 2 * margin_px.y - text_size.y) / 2,
    };
    break;
  case TextAlignment::Right:
    position = Vector2Type{
        .x = container.x + container.width - margin_px.x - text_size.x,
        .y = container.y + margin_px.y +
             (container.height - 2 * margin_px.y - text_size.y) / 2,
    };
    break;
  default:
    // Handle unknown alignment (shouldn't happen)
    break;
  }

  return RectangleType{
      .x = position.x,
      .y = position.y,
      .width = font_size,
      .height = font_size,
  };
}

static void draw_text_in_rect(const ui::FontManager &fm,
                              const std::string &text, RectangleType rect,
                              TextAlignment alignment, Color color) {
  RectangleType sizing =
      position_text(fm, text, rect, alignment, Vector2Type{5.f, 5.f});
  draw_text_ex(fm.get_active_font(), text.c_str(),
               Vector2Type{sizing.x, sizing.y}, sizing.height, 1.f, color);
}

static Vector2Type
position_texture(texture_manager::Texture, Vector2Type size,
                 RectangleType container,
                 texture_manager::HasTexture::Alignment alignment,
                 Vector2Type margin_px = {0.f, 0.f}) {

  // Calculate the text position based on the alignment and margins
  Vector2Type position;

  switch (alignment) {
  case texture_manager::HasTexture::Alignment::Left:
    position = Vector2Type{
        .x = container.x + margin_px.x,
        .y = container.y + margin_px.y + size.x,
    };
    break;
  case texture_manager::HasTexture::Alignment::Center:
    position = Vector2Type{
        .x = container.x + margin_px.x + (container.width / 2) + (size.x / 2),
        .y = container.y + margin_px.y + (container.height / 2) + (size.y / 2),
    };
    break;
  case texture_manager::HasTexture::Alignment::Right:
    position = Vector2Type{
        .x = container.x + container.width - margin_px.x + size.x,
        .y = container.y + margin_px.y + size.y,
    };
    break;
  default:
    // Handle unknown alignment (shouldn't happen)
    break;
  }

  return Vector2Type{
      .x = position.x,
      .y = position.y,
  };
}

static void
draw_texture_in_rect(texture_manager::Texture texture, RectangleType rect,
                     texture_manager::HasTexture::Alignment alignment) {

  float scale = texture.height / rect.height;
  Vector2Type size = {
      texture.width / scale,
      texture.height / scale,
  };

  Vector2Type location = position_texture(texture, size, rect, alignment);

  texture_manager::draw_texture_pro(texture,
                                    RectangleType{
                                        0,
                                        0,
                                        texture.width,
                                        texture.height,
                                    },
                                    RectangleType{
                                        .x = location.x,
                                        .y = location.y,
                                        .width = size.x,
                                        .height = size.y,
                                    },
                                    size, 0.f, colors::UI_WHITE);
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

  float fontSize = 20;

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

    draw_text(std::format("mouse({}, {})", this->context->mouse_pos.x,
                          this->context->mouse_pos.y)
                  .c_str(),
              0, 0, fontSize,
              this->context->theme.from_usage(Theme::Usage::Font));

    // starting at 1 to avoid the mouse text
    this->level = 1;
    this->indent = 0;
  }

  void render_me(const Entity &entity) const {
    const UIComponent &cmp = entity.get<UIComponent>();

    float x = 10 * indent;
    float y = (fontSize * level) + fontSize / 2.f;

    std::string component_name = "Unknown";
    if (entity.has<UIComponentDebug>()) {
      const auto &cmpdebug = entity.get<UIComponentDebug>();
      component_name = cmpdebug.name();
    }

    std::string widget_str = std::format(
        "{:03} (x{:05.2f} y{:05.2f}) w{:05.2f}xh{:05.2f} {}", (int)entity.id,
        cmp.x(), cmp.y(), cmp.rect().width, cmp.rect().height, component_name);

    float text_width = measure_text_internal(widget_str.c_str(), fontSize);
    Rectangle debug_label_location = Rectangle{x, y, text_width, fontSize};

    if (is_mouse_inside(this->context->mouse_pos, debug_label_location)) {
      draw_rectangle_outline(
          cmp.rect(), this->context->theme.from_usage(Theme::Usage::Error));
      draw_rectangle_outline(cmp.bounds(), colors::UI_BLACK);
      draw_rectangle(debug_label_location, colors::UI_BLUE);
    } else {
      draw_rectangle(debug_label_location, colors::UI_BLACK);
    }

    draw_text(widget_str.c_str(), x, y, fontSize,
              this->context->is_hot(entity.id)
                  ? this->context->theme.from_usage(Theme::Usage::Error)
                  : this->context->theme.from_usage(Theme::Usage::Font));
  }

  void render(const Entity &entity) const {
    const UIComponent &cmp = entity.get<UIComponent>();
    if (cmp.should_hide)
      return;

    if (cmp.was_rendered_to_screen) {
      render_me(entity);
      level++;
    }

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

    if (entity.has<HasColor>()) {
      Color col = entity.template get<HasColor>().color();

      if (context.is_hot(entity.id)) {
        col = context.theme.from_usage(Theme::Usage::Background);
      }

      auto corner_settings = entity.has<HasRoundedCorners>()
                                 ? entity.get<HasRoundedCorners>().get()
                                 : std::bitset<4>().reset();

      // TODO do we need another color for this?
      if (context.has_focus(entity.id)) {
        draw_rectangle_rounded(cmp.focus_rect(),
                               0.5f, // roundness
                               8,    // segments
                               context.theme.from_usage(Theme::Usage::Accent),
                               corner_settings);
      }
      draw_rectangle_rounded(cmp.rect(),
                             0.5f, // roundness
                             8,    // segments
                             col, corner_settings);
    }

    if (entity.has<HasLabel>()) {
      const HasLabel &hasLabel = entity.get<HasLabel>();
      draw_text_in_rect(
          font_manager, hasLabel.label.c_str(), cmp.rect(), hasLabel.alignment,
          context.theme.from_usage(Theme::Usage::Font, hasLabel.is_disabled));
    }

    if (entity.has<texture_manager::HasTexture>()) {
      const texture_manager::HasTexture &texture =
          entity.get<texture_manager::HasTexture>();
      draw_texture_in_rect(texture.texture, cmp.rect(), texture.alignment);
    }
  }

  void render(const UIContext<InputAction> &context,
              const FontManager &font_manager, const Entity &entity) const {
    const UIComponent &cmp = entity.get<UIComponent>();
    if (cmp.should_hide || entity.has<ShouldHide>())
      return;

    if (cmp.font_name != UIComponent::UNSET_FONT)
      const_cast<FontManager &>(font_manager).set_active(cmp.font_name);

    if (entity.has<HasColor>() || entity.has<HasLabel>()) {
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
      if (a.layer == b.layer)
        return a.id < b.id;
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
  dropdown.addComponent<UIComponentDebug>("dropdown");

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

  dropdown.addComponent<HasColor>(colors::UI_BLUE);

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
  print_debug_autolayout_tree(root, root.get<ui::UIComponent>());
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

  entity.addComponent<ui::FontManager>()
      .load_font(UIComponent::DEFAULT_FONT, get_default_font())
      .load_font(UIComponent::UNSET_FONT, get_unset_font());
  EntityHelper::registerSingleton<ui::FontManager>(entity);
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
  sm.register_update_system(
      std::make_unique<developer::EnforceSingleton<ui::FontManager>>());
}

template <typename InputAction>
static void register_before_ui_updates(SystemManager &sm) {
  sm.register_update_system(std::make_unique<ui::ClearUIComponentChildren>());
  sm.register_update_system(
      std::make_unique<ui::BeginUIContextManager<InputAction>>());
}

template <typename InputAction>
static void register_after_ui_updates(SystemManager &sm) {
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

template <typename InputAction>
static void
register_render_systems(SystemManager &sm,
                        InputAction toggle_debug = InputAction::None) {
  sm.register_render_system(std::make_unique<ui::RenderImm<InputAction>>());
  sm.register_render_system(
      std::make_unique<ui::RenderDebugAutoLayoutRoots<InputAction>>(
          toggle_debug));
}

} // namespace ui

} // namespace afterhours
