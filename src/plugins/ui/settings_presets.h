// settings_presets.h
// Named settings presets that flip to Custom when the values diverge.
//
// Cartographer asked for this: a preset selector (e.g. quality Low/Medium/
// High) that reads "Custom" once the user edits any value off the preset,
// and reads the preset name again when the values match it exactly. The
// library had no preset-table concept at all -- only colour/animation
// presets -- so this is the whole slice: a preset table over a plain
// comparable values struct, an index/name lookup, and a stepper selector
// whose index is derived from the values every frame (a stepper is
// stateless, so the flip needs no stored state).
//
// Usage:
//   struct Quality { int resolution_scale; bool shadows; float fov; };
//   std::vector<SettingsPreset<Quality>> presets = {
//       {"Low", {1, false, 70.f}}, {"High", {2, true, 90.f}}};
//   Quality q = presets[0].values;
//   // Each frame: settings_preset_stepper(ctx, mk(parent), q, presets);
//   // ... and the other setting_rows edit q directly; the selector reads
//   // "Custom" until q matches a preset again.

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

/// Name shown when the current values match no preset.
inline constexpr std::string_view kCustomPresetName = "Custom";

/// One named row of a preset table: a name plus the values it sets.
template <typename Values>
struct SettingsPreset {
  std::string name;
  Values values;
};

/// Index of the preset the values match, or presets.size() (the Custom
/// slot) when none matches. Exact == on purpose: a preset is applied by
/// assignment, so a match after applying is exact, and anything edited --
/// even by one float ulp -- is genuinely off-preset.
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

/// Name of the active preset, or "Custom" when the values diverge.
template <typename Values>
std::string_view
active_preset_name(const Values &values,
                   const std::vector<SettingsPreset<Values>> &presets) {
  const size_t idx = active_preset_index(values, presets);
  return idx < presets.size() ? std::string_view(presets[idx].name)
                              : kCustomPresetName;
}

/// Selector options: the preset names with "Custom" last. The Custom slot
/// is read-only -- selecting it changes nothing.
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

/// Copy a preset's values into `values`. False (and no write) when the
/// index is out of range -- in particular for the Custom slot.
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

/// Preset selector: a stepper over the preset names plus Custom, with the
/// index derived from `values` each frame so it flips to Custom the moment
/// a value is edited elsewhere and back when the values match again.
/// Stepping onto a named preset applies it; stepping onto Custom keeps
/// the current (edited) values.
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
  // Stepped onto Custom: values stay as edited, and the selector re-reads
  // the derived index next frame.
  return {false, result.ent()};
}

} // namespace imm

} // namespace ui

} // namespace afterhours
