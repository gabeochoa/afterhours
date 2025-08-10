#pragma once

#include "../components.h"
#include "component_config.h"

namespace afterhours {
namespace ui {
namespace imm {

inline void ensure_modifiers(Entity &entity) {
  entity.addComponentIfMissing<HasUIModifiers>();
}

inline void set_scale(Entity &entity, float scale) {
  ensure_modifiers(entity);
  entity.get<HasUIModifiers>().scale = scale;
}

inline void set_translate(Entity &entity, float x, float y) {
  ensure_modifiers(entity);
  auto &m = entity.get<HasUIModifiers>();
  m.translate_x = x;
  m.translate_y = y;
}

// TODO
// inline void set_rotation(Entity &, float) {}

} // namespace imm
} // namespace ui
} // namespace afterhours
