#pragma once

#include <bitset>
#include <variant>

#include "../../../ecs.h"
#include "../components.h"
#include "../context.h"

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

  template <typename T> T as() const { return std::get<T>(data); }

private:
  bool result = false;
  Entity &element;
  std::variant<float, int, bool, unsigned long> data = 0.f;
};

} // namespace imm

} // namespace ui

} // namespace afterhours
