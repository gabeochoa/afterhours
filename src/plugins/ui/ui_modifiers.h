#pragma once

#include <vector>

#include "../../ecs.h"
#include "components.h"
#include "ui_core_components.h"

namespace afterhours {

namespace ui {

static inline RectangleType apply_ui_modifiers_recursive(EntityID entity_id,
                                                         RectangleType rect) {
  std::vector<EntityID> chain;
  EntityID current = entity_id;
  int guard = 0;
  while (current >= 0 && guard++ < 512) {
    OptEntity opt = EntityHelper::getEntityForID(current);
    if (!opt.has_value())
      break;
    Entity &ent = opt.asE();
    chain.push_back(current);
    if (!ent.has<UIComponent>())
      break;
    EntityID parent = ent.get<UIComponent>().parent;
    if (parent < 0 || parent == current)
      break;
    current = parent;
  }

  for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
    OptEntity opt = EntityHelper::getEntityForID(*it);
    if (!opt.has_value())
      continue;
    Entity &ent = opt.asE();
    if (ent.has<HasUIModifiers>()) {
      rect = ent.get<HasUIModifiers>().apply_modifier(rect);
    }
  }
  return rect;
}

} // namespace ui

} // namespace afterhours

