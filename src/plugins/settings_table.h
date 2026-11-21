#pragma once

// One field table for a settings struct: JSON (de)serialization plus the
// generic `set_setting` / `expect_setting` E2E commands, from one list.
//
//   struct GameSettings {
//     bool fullscreen = false;
//     int width = 1280;
//     float volume = 0.5f;
//     std::string player_name = "player";
//   };
//   static const auto kGameSettingsFields =
//       afterhours::settings_fields<GameSettings>(
//           afterhours::setting_field("fullscreen",
//           &GameSettings::fullscreen),
//           afterhours::setting_field("width", &GameSettings::width),
//           afterhours::setting_field("volume", &GameSettings::volume),
//           afterhours::setting_field("player_name",
//           &GameSettings::player_name));
//
//   json j = afterhours::settings_to_json(settings, kGameSettingsFields);
//   std::string err;
//   if (!afterhours::settings_from_json(settings, kGameSettingsFields, j,
//   &err))
//     log_warn("settings: {}", err);
//
// Supported member types: bool, int, long, float, double, std::string.
// Nested structs, containers and enums-by-name are not in this table; keep
// hand-writing to_json/from_json for those (nlohmann ADL already covers it).

#include <format>
#include <functional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "settings.h"

#if defined(AFTER_HOURS_ENABLE_E2E_TESTING)
#include "e2e_testing/command_handlers.h"
#endif

namespace afterhours {

enum class SettingKind { Bool, Int, Float, String };

template <typename T> struct SettingField {
  std::string name;
  SettingKind kind;
  std::function<json(const T &)> to_json;
  // Each setter validates before assigning, so a bad value leaves obj alone.
  std::function<bool(const json &, std::string &)> check_json;
  std::function<void(T &, const json &)> set_json;
  std::function<bool(T &, std::string_view, std::string &)> set_string;
  std::function<std::string(const T &)> to_string;
};

namespace detail {

template <typename M> constexpr SettingKind setting_kind_for() {
  if constexpr (std::is_same_v<M, bool>)
    return SettingKind::Bool;
  else if constexpr (std::is_integral_v<M>)
    return SettingKind::Int;
  else if constexpr (std::is_floating_point_v<M>)
    return SettingKind::Float;
  else
    return SettingKind::String;
}

inline bool parse_setting_bool(std::string_view s, bool &out) {
  if (s == "true" || s == "1" || s == "yes" || s == "on") out = true;
  else if (s == "false" || s == "0" || s == "no" || s == "off") out = false;
  else return false;
  return true;
}

template <typename M> bool parse_setting_number(std::string_view s, M &out) {
  try {
    size_t used = 0;
    if constexpr (std::is_floating_point_v<M>) {
      out = static_cast<M>(std::stod(std::string(s), &used));
    } else {
      out = static_cast<M>(std::stoll(std::string(s), &used));
    }
    return used == s.size();
  } catch (...) {
    return false;
  }
}

template <typename M> std::string format_setting_value(const M &value) {
  if constexpr (std::is_same_v<M, bool>)
    return value ? "true" : "false";
  else if constexpr (std::is_same_v<M, std::string>)
    return value;
  else
    return std::format("{}", value);
}

} // namespace detail

template <typename T, typename M>
SettingField<T> setting_field(std::string name, M T::*member) {
  static_assert(std::is_same_v<M, bool> || std::is_integral_v<M> ||
                    std::is_floating_point_v<M> ||
                    std::is_same_v<M, std::string>,
                "setting_field supports bool, int, long, float, double, "
                "std::string members");
  SettingField<T> field;
  field.name = std::move(name);
  field.kind = detail::setting_kind_for<M>();
  field.to_json = [member](const T &obj) { return json(obj.*member); };
  field.check_json = [name = field.name, kind = field.kind](
                         const json &j, std::string &err) {
    const bool ok = (kind == SettingKind::Bool && j.is_boolean()) ||
                    (kind == SettingKind::Int && j.is_number_integer()) ||
                    (kind == SettingKind::Float && j.is_number()) ||
                    (kind == SettingKind::String && j.is_string());
    if (!ok)
      err = std::format("setting '{}': expected {}, got {}", name,
                        kind == SettingKind::Bool    ? "boolean"
                        : kind == SettingKind::Int   ? "integer"
                        : kind == SettingKind::Float ? "number"
                                                     : "string",
                        j.type_name());
    return ok;
  };
  field.set_json = [member](T &obj, const json &j) {
    obj.*member = j.get<M>();
  };
  field.set_string = [member, name = field.name](T &obj, std::string_view s,
                                                 std::string &err) {
    M value{};
    bool ok = true;
    if constexpr (std::is_same_v<M, bool>)
      ok = detail::parse_setting_bool(s, value);
    else if constexpr (std::is_same_v<M, std::string>)
      value = std::string(s);
    else
      ok = detail::parse_setting_number(s, value);
    if (!ok) {
      err = std::format("setting '{}': cannot parse '{}'", name, s);
      return false;
    }
    obj.*member = value;
    return true;
  };
  field.to_string = [member](const T &obj) {
    return detail::format_setting_value(obj.*member);
  };
  return field;
}

template <typename T, typename... Fields>
std::vector<SettingField<T>> settings_fields(Fields... fields) {
  return {std::move(fields)...};
}

template <typename T>
const SettingField<T> *find_setting(const std::vector<SettingField<T>> &fields,
                                    std::string_view name) {
  for (const auto &field : fields)
    if (field.name == name) return &field;
  return nullptr;
}

template <typename T>
json settings_to_json(const T &obj,
                      const std::vector<SettingField<T>> &fields) {
  json j = json::object();
  for (const auto &field : fields) j[field.name] = field.to_json(obj);
  return j;
}

// Missing fields keep their current value and unknown fields are ignored,
// matching the settings plugin's hand-written from_json functions. A type
// mismatch fails the whole load before anything is assigned.
template <typename T>
bool settings_from_json(T &obj, const std::vector<SettingField<T>> &fields,
                        const json &j, std::string *error = nullptr) {
  if (!j.is_object()) {
    if (error) *error = std::format("settings: expected object, got {}",
                                    j.type_name());
    return false;
  }
  std::string err;
  for (const auto &field : fields) {
    if (j.contains(field.name) && !field.check_json(j.at(field.name), err)) {
      if (error) *error = err;
      return false;
    }
  }
  for (const auto &field : fields)
    if (j.contains(field.name)) field.set_json(obj, j.at(field.name));
  return true;
}

#if defined(AFTER_HOURS_ENABLE_E2E_TESTING)
// Registers two commands over obj, which must outlive the SystemManager:
//   set_setting <name> <value>      fail on unknown field / bad value
//   expect_setting <name> <value>   retrying expect, like other expect_*
// String values may span several script words; they are joined with spaces.
template <typename T>
void register_setting_commands(SystemManager &sm,
                               const std::vector<SettingField<T>> &fields,
                               T &obj) {
  auto join_value = [](const testing::PendingE2ECommand &cmd) {
    std::string value = cmd.arg(1);
    for (size_t i = 2; i < cmd.args.size(); ++i) value += " " + cmd.arg(i);
    return value;
  };
  testing::register_command(
      sm, "set_setting",
      [&fields, &obj, join_value](testing::PendingE2ECommand &cmd) {
        if (!cmd.has_args(2)) {
          cmd.fail("set_setting requires: <name> <value>");
          return;
        }
        const auto *field = find_setting(fields, cmd.arg(0));
        if (!field) {
          cmd.fail(std::format("set_setting: unknown setting '{}'",
                               cmd.arg(0)));
          return;
        }
        std::string err;
        if (!field->set_string(obj, join_value(cmd), err)) {
          cmd.fail(err);
          return;
        }
        cmd.consume();
      });
  testing::register_command(
      sm, "expect_setting",
      [&fields, &obj, join_value](testing::PendingE2ECommand &cmd) {
        if (!cmd.has_args(2)) {
          cmd.fail("expect_setting requires: <name> <value>");
          return;
        }
        const auto *field = find_setting(fields, cmd.arg(0));
        if (!field) {
          cmd.fail(std::format("expect_setting: unknown setting '{}'",
                               cmd.arg(0)));
          return;
        }
        // Compare canonical spellings so 0.50 and 0.5 expect the same float.
        T probe = obj;
        std::string err;
        if (!field->set_string(probe, join_value(cmd), err)) {
          cmd.fail(err);
          return;
        }
        const std::string want = field->to_string(probe);
        const std::string got = field->to_string(obj);
        cmd.expect(got == want, std::format("setting '{}' is {}, expected {}",
                                            field->name, got, want));
      });
}
#endif

} // namespace afterhours
