#pragma once

#include <string>
#include <utility>

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
inline const char *accessible_role_name(AccessibleRole role) {
  switch (role) {
  case AccessibleRole::Button: return "button";
  case AccessibleRole::Checkbox: return "checkbox";
  case AccessibleRole::Radio: return "radio";
  case AccessibleRole::Slider: return "slider";
  case AccessibleRole::Switch: return "switch";
  case AccessibleRole::Stepper: return "stepper";
  case AccessibleRole::Dropdown: return "dropdown";
  case AccessibleRole::TextInput: return "text_input";
  case AccessibleRole::Text: return "text";
  case AccessibleRole::Group: return "group";
  case AccessibleRole::Image: return "image";
  case AccessibleRole::ProgressBar: return "progress_bar";
  case AccessibleRole::Tab: return "tab";
  case AccessibleRole::Link: return "link";
  case AccessibleRole::None: break;
  }
  return "none";
}
inline bool accessible_role_from_name(const std::string &s,
                                      AccessibleRole &out) {
  for (auto r : {AccessibleRole::Button, AccessibleRole::Checkbox,
                 AccessibleRole::Radio, AccessibleRole::Slider,
                 AccessibleRole::Switch, AccessibleRole::Stepper,
                 AccessibleRole::Dropdown, AccessibleRole::TextInput,
                 AccessibleRole::Text, AccessibleRole::Group,
                 AccessibleRole::Image, AccessibleRole::ProgressBar,
                 AccessibleRole::Tab, AccessibleRole::Link,
                 AccessibleRole::None}) {
    if (s == accessible_role_name(r)) { out = r; return true; }
  }
  return false;
}
inline bool accessible_is_blank(const std::string &s) {
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
inline std::string resolve_accessible_name(const std::string &explicit_name,
                                           const std::string &label,
                                           const std::string &tooltip) {
  if (!accessible_is_blank(explicit_name)) return explicit_name;
  if (!accessible_is_blank(label)) return label;
  if (!accessible_is_blank(tooltip)) return tooltip;
  return "";
}
} // namespace ui

} // namespace afterhours
