#pragma once


#include <algorithm>
#include <string>
#include <vector>

#include "element_result.h"
#include "entity_management.h"
#include "setting_row.h"
#include "text_input/text_input.h"

namespace afterhours {
namespace ui {
namespace form {
using namespace imm;

inline ElementResult slider(HasUIContext auto &ctx, EntityParent ep_pair,
                            const std::string &label, float &value,
                            ComponentConfig config = ComponentConfig()) {
  return imm::setting_row_slider(ctx, ep_pair, label, value, "", colors::soft_red,
                                 config);
}

inline ElementResult checkbox(HasUIContext auto &ctx, EntityParent ep_pair,
                              const std::string &label, bool &value,
                              ComponentConfig config = ComponentConfig()) {
  if (config.size.is_default)
    config.with_size(ComponentSize{pixels(300), pixels(44)});
  return imm::checkbox(ctx, ep_pair, value, config.with_label(label));
}

inline ElementResult toggle(HasUIContext auto &ctx, EntityParent ep_pair,
                            const std::string &label, bool &value,
                            ComponentConfig config = ComponentConfig()) {
  return imm::setting_row_toggle(ctx, ep_pair, label, value, "", colors::soft_green,
                                 config);
}

inline ElementResult text(HasUIContext auto &ctx, EntityParent ep_pair,
                          const std::string &label, std::string &value,
                          ComponentConfig config = ComponentConfig()) {
  if (config.size.is_default)
    config.with_size(ComponentSize{pixels(300), pixels(44)});
  return imm::text_input(ctx, ep_pair, value, config.with_label(label));
}

inline ElementResult integer(HasUIContext auto &ctx, EntityParent ep_pair,
                             const std::string &label, int &value,
                             int min_value = 0, int max_value = 10,
                             ComponentConfig config = ComponentConfig()) {
  if (max_value < min_value) std::swap(min_value, max_value);
  value = std::clamp(value, min_value, max_value);
  std::vector<std::string> options;
  for (int i = min_value; i <= max_value; ++i) options.push_back(std::to_string(i));
  size_t idx = static_cast<size_t>(value - min_value);
  auto result = imm::setting_row_stepper(ctx, ep_pair, label, idx, options, "",
                                         colors::soft_blue, config);
  value = min_value + static_cast<int>(idx);
  return result;
}

}
}
}
