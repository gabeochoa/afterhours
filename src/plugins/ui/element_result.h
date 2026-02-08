#pragma once

#include <bitset>
#include <variant>

#include "../../ecs.h"
#include "components.h"
#include "context.h"

namespace afterhours {

namespace ui {

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

  /// Apply a decorator to this element. The decorator is any callable
  /// that takes an Entity& and adds visual decorations as children.
  ///
  /// Plugins can define factory functions that return lambdas:
  /// ```cpp
  /// auto with_brackets(auto& ctx, Color c) {
  ///     return [&ctx, c](Entity& e) { /* add bracket divs */ };
  /// }
  /// ```
  ///
  /// Usage:
  /// ```cpp
  /// button(ctx, mk(parent, 1), config)
  ///     .decorate(with_brackets(ctx, teal))
  ///     .decorate(with_grid_bg(ctx, 32.0f, gray));
  /// ```
  template <typename Fn> ElementResult &decorate(Fn &&fn) {
    std::forward<Fn>(fn)(element);
    return *this;
  }

  template <typename T> T as() const { return std::get<T>(data); }

private:
  bool result = false;
  Entity &element;
  std::variant<float, int, bool, unsigned long> data = 0.f;
};

} // namespace imm

} // namespace ui

} // namespace afterhours
