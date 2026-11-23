#pragma once

#include <string>
#include <string_view>
#include <utility>
#if __has_include(<magic_enum/magic_enum.hpp>)
#include <magic_enum/magic_enum.hpp>
#else
#include "../../../vendor/magic_enum/magic_enum.hpp"
#endif

#include "../../ecs.h"

namespace afterhours {

namespace ui {

enum struct AccessibleRole {
  None,
  Button,
  Checkbox,
  Radio,
  Slider,
  Switch,
  Stepper,
  Dropdown,
  TextInput,
  Text,
  Group,
  Image,
  ProgressBar,
  Tab,
  Link,
};
// Every role, for name round-trips.
inline constexpr AccessibleRole accessible_roles[] = {
    AccessibleRole::Button,   AccessibleRole::Checkbox,
    AccessibleRole::Radio,    AccessibleRole::Slider,
    AccessibleRole::Switch,   AccessibleRole::Stepper,
    AccessibleRole::Dropdown, AccessibleRole::TextInput,
    AccessibleRole::Text,     AccessibleRole::Group,
    AccessibleRole::Image,    AccessibleRole::ProgressBar,
    AccessibleRole::Tab,      AccessibleRole::Link,
    AccessibleRole::None};
inline std::string accessible_role_name(AccessibleRole role) {
  std::string out;
  std::string_view name = magic_enum::enum_name(role);
  for (size_t i = 0; i < name.size(); i++) {
    char c = name[i];
    if (c >= 'A' && c <= 'Z') {
      if (i > 0) out.push_back('_');
      out.push_back(static_cast<char>(c - 'A' + 'a'));
    } else {
      out.push_back(c);
    }
  }
  return out;
}

inline bool accessible_role_from_name(std::string_view s,
                                      AccessibleRole &out) {
  for (auto r : accessible_roles) {
    if (s == accessible_role_name(r)) { out = r; return true; }
  }
  return false;
}

inline bool accessible_is_blank(std::string_view s) {
  for (char c : s)
    if (c != ' ' && c != '\t' && c != '\n' && c != '\r') return false;
  return true;
}
struct AccessibleConfig {
  AccessibleRole role = AccessibleRole::None;
  std::string name;
  std::string value;
  std::string description;
  bool empty() const {
    return role == AccessibleRole::None && name.empty() && value.empty() &&
           description.empty();
  }
};
struct HasAccessibility : BaseComponent {
  AccessibleRole role = AccessibleRole::None;
  std::string name;
  std::string value;
  std::string description;
  HasAccessibility() = default;
  HasAccessibility(AccessibleRole r, std::string n, std::string v = "",
                   std::string d = "")
      : role(r), name(std::move(n)), value(std::move(v)),
        description(std::move(d)) {}
  bool empty() const {
    return role == AccessibleRole::None && name.empty() && value.empty() &&
           description.empty();
  }
};
inline std::string resolve_accessible_name(std::string_view explicit_name,
                                           std::string_view label,
                                           std::string_view tooltip) {
  if (!accessible_is_blank(explicit_name)) return std::string(explicit_name);
  if (!accessible_is_blank(label)) return std::string(label);
  if (!accessible_is_blank(tooltip)) return std::string(tooltip);
  return "";
}
} // namespace ui

} // namespace afterhours
