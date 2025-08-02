#pragma once

#include <functional>
#include <magic_enum/magic_enum.hpp>
#include <string>
#include <vector>

#include "../../entity.h"
#include "../../entity_helper.h"
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

} // namespace ui

} // namespace afterhours