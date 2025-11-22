#pragma once

#include "../texture_manager.h"
#if __has_include(<magic_enum/magic_enum.hpp>)
#include <magic_enum/magic_enum.hpp>
#else
#include "../../../vendor/magic_enum/magic_enum.hpp"
#endif
#include <functional>
#include <magic_enum/magic_enum.hpp>
#include <optional>
#include <string>
#include <vector>

#include "../../drawing_helpers.h"
#include "../../ecs.h"
#include "../../logging.h"

namespace afterhours {

namespace ui {

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

struct HasLeftRightListener : BaseComponent {
  std::function<void(Entity &, int)> cb;
  HasLeftRightListener(const std::function<void(Entity &, int)> &callback)
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
struct SelectOnFocus : BaseComponent {};

struct FocusClusterRoot : BaseComponent {};
struct InFocusCluster : BaseComponent {};

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
  std::function<Options(HasDropdownState &)> fetch_options = nullptr;
  std::function<void(size_t)> on_option_changed = nullptr;
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

  size_t current_index() const { return last_option_clicked; }
  void set_current_index(size_t index) { last_option_clicked = index; }
};

struct HasNavigationBarState : ui::HasDropdownState {
  HasNavigationBarState(const Options &opts,
                        const std::function<void(size_t)> opt_changed = nullptr)
      : HasDropdownState(opts, nullptr, opt_changed) {}

  template <size_t N>
  HasNavigationBarState(const std::array<std::string_view, N> &opts,
                        const std::function<void(size_t)> opt_changed = nullptr)
      : HasNavigationBarState(Options(opts.begin(), opts.end()), opt_changed) {}

  size_t current_index() const { return last_option_clicked; }
  void set_current_index(size_t index) { last_option_clicked = index; }
};

struct HasRoundedCorners : BaseComponent {
  std::bitset<4> rounded_corners = std::bitset<4>().reset();
  auto &set(std::bitset<4> input) {
    rounded_corners = input;
    return *this;
  }
  auto &get() const { return rounded_corners; }
};

struct HasImage : BaseComponent {
  afterhours::texture_manager::Texture texture;
  std::optional<afterhours::texture_manager::Rectangle> source_rect;
  afterhours::texture_manager::HasTexture::Alignment alignment =
      afterhours::texture_manager::HasTexture::Alignment::Center;

  HasImage(
      afterhours::texture_manager::Texture tex,
      std::optional<afterhours::texture_manager::Rectangle> src = std::nullopt,
      afterhours::texture_manager::HasTexture::Alignment align =
          afterhours::texture_manager::HasTexture::Alignment::Center)
      : texture(tex), source_rect(src), alignment(align) {}
};

struct HasOpacity : BaseComponent {
  float value = 1.0f;
  HasOpacity() = default;
  explicit HasOpacity(float v) : value(v) {}
};

struct HasButtonAnimState : BaseComponent {
  bool hovered = false;
};

struct HasUIModifiers : BaseComponent {
  float scale = 1.0f;
  float translate_x = 0.f;
  float translate_y = 0.f;

  RectangleType apply_modifier(RectangleType rect) const {
    rect = scale_rect(rect);
    if (translate_x != 0.f || translate_y != 0.f) {
      rect.x += translate_x;
      rect.y += translate_y;
    }
    return rect;
  }

private:
  RectangleType scale_rect(RectangleType rect) const {
    float s = scale;
    if (s != 1.0f) {
      float cx = rect.x + rect.width / 2.0f;
      float cy = rect.y + rect.height / 2.0f;
      float new_w = rect.width * s;
      float new_h = rect.height * s;
      rect.x = cx - new_w / 2.0f;
      rect.y = cy - new_h / 2.0f;
      rect.width = new_w;
      rect.height = new_h;
    }
    if (translate_x != 0.f || translate_y != 0.f) {
      rect.x += translate_x;
      rect.y += translate_y;
    }
    return rect;
  }
};

} // namespace ui

} // namespace afterhours
