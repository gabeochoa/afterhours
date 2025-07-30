#pragma once

#include <map>
#include <source_location>
#include <sstream>

#include "../../../entity.h"
#include "../../../entity_helper.h"
#include "../../../logging.h"

namespace afterhours {

namespace ui {

namespace imm {

using UI_UUID = size_t;
static std::map<UI_UUID, EntityID> existing_ui_elements;

using EntityParent = std::pair<RefEntity, RefEntity>;

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
    return {EntityHelper::getEntityForIDEnforce(entityID), parent};
  }

  Entity &entity = EntityHelper::createEntity();
  existing_ui_elements[hash] = entity.id;
  log_info("Creating element {} for {}", hash, entity.id);
  return {entity, parent};
}

} // namespace imm

} // namespace ui

} // namespace afterhours