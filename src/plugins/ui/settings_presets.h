#pragma once
#include <string>
#include <string_view>
#include <vector>
#include "component_config.h"
#include "element_result.h"
#include "entity_management.h"
#include "imm_value.h"
namespace afterhours {
namespace ui {
namespace imm {
inline constexpr std::string_view kCustomPresetName = "Custom";
template <typename Values>
struct SettingsPreset {
  std::string name;
  Values values;
};
template <typename Values>
size_t active_preset_index(const Values &values,
                           const std::vector<SettingsPreset<Values>> &presets) {
  for (size_t i = 0; i < presets.size(); ++i) {
    if (values == presets[i].values) {
      return i;
    }
  }
  return presets.size();
}

template <typename Values>
std::string_view
active_preset_name(const Values &values,
                   const std::vector<SettingsPreset<Values>> &presets) {
  const size_t idx = active_preset_index(values, presets);
  return idx < presets.size() ? std::string_view(presets[idx].name)
                              : kCustomPresetName;
}

template <typename Values>
std::vector<std::string>
preset_option_names(const std::vector<SettingsPreset<Values>> &presets) {
  std::vector<std::string> names;
  names.reserve(presets.size() + 1);
  for (const auto &preset : presets) {
    names.push_back(preset.name);
  }
  names.emplace_back(kCustomPresetName);
  return names;
}

template <typename Values>
bool apply_preset(Values &values,
                  const std::vector<SettingsPreset<Values>> &presets,
                  size_t index) {
  if (index >= presets.size()) {
    return false;
  }
  values = presets[index].values;
  return true;
}

template <typename Values>
ElementResult
settings_preset_stepper(HasUIContext auto &ctx, EntityParent ep_pair,
                        Values &values,
                        const std::vector<SettingsPreset<Values>> &presets,
                        ComponentConfig config = ComponentConfig()) {
  const auto options = preset_option_names(presets);
  size_t index = active_preset_index(values, presets);
  auto result = stepper(ctx, ep_pair, options, index, config);
  if (result && apply_preset(values, presets, index)) {
    return {true, result.ent()};
  }
  return {false, result.ent()};
}
}
}
}
