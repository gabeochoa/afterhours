#pragma once

#include <map>
#include <source_location>
#include <sstream>

#include "../../../ecs.h"
#include "../../../logging.h"

namespace afterhours {

namespace ui {

namespace imm {

using UI_UUID = size_t;
static std::map<UI_UUID, EntityID> existing_ui_elements;

using EntityParent = std::pair<RefEntity, RefEntity>;

inline std::pair<Entity &, Entity &> deref(EntityParent p) {
  return {p.first.get(), p.second.get()};
}

static EntityParent
mk(Entity &parent, EntityID otherID = -1,
   const std::source_location location = std::source_location::current()) {

  std::stringstream pre_hash;
  pre_hash << parent.id << otherID << "file: " << location.file_name() << '('
           << location.line() << ':' << location.column() << ") `"
           << location.function_name() << "`: " << '\n';

  UI_UUID hash = std::hash<std::string>{}(pre_hash.str());

  if (existing_ui_elements.contains(hash)) {
    auto entityID = existing_ui_elements.at(hash);
    log_trace("Reusing element {} for {}", hash, entityID);

    // Add better error handling for entity ID conflicts
    try {
      return {EntityHelper::getEntityForIDEnforce(entityID), parent};
    } catch (const std::bad_optional_access &e) {
      log_error(
          "Entity ID conflict detected! This usually happens when mk() is "
          "called multiple times "
          "from the same source location without proper index management. "
          "Location: {}:{}:{}, Function: {}. "
          "Consider using mk(parent, index) with unique indices or "
          "mk_next(parent) for auto-incrementing.",
          location.file_name(), location.line(), location.column(),
          location.function_name());
      throw;
    }
  }

  Entity &entity = EntityHelper::createEntity();
  existing_ui_elements[hash] = entity.id;
  // TODO - add a count of how many elements are created
  // so we can track if its growing every frame
  log_trace("Creating element {} for {}", hash, entity.id);
  return {entity, parent};
}

} // namespace imm

} // namespace ui

} // namespace afterhours
