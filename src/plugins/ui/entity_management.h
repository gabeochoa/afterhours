#pragma once

#include <map>
#include <source_location>
#include <sstream>

#include "../../ecs.h"
#include "../../logging.h"
#include "ui_collection.h"

namespace afterhours {

namespace ui {

namespace imm {

using UI_UUID = size_t;

struct UIElementRecord {
  EntityID id;
  size_t last_built_frame;
};
inline std::map<UI_UUID, UIElementRecord> existing_ui_elements;

// Bumped once per frame by retire_unbuilt_ui_elements().
inline size_t ui_build_frame = 0;

// Frames a widget may go unbuilt before it is destroyed. 0 disables the sweep,
// for an app that holds EntityIDs across screens and does its own lifetime.
inline size_t ui_retire_grace_frames = 90;

using EntityParent = std::pair<RefEntity, RefEntity>;

inline std::pair<Entity &, Entity &> deref(EntityParent p) {
  return {p.first.get(), p.second.get()};
}

inline EntityParent
mk(Entity &parent, EntityID otherID = -1,
   const std::source_location location = std::source_location::current()) {
  std::stringstream pre_hash;
  pre_hash << parent.id << otherID << "file: " << location.file_name() << '('
           << location.line() << ':' << location.column() << ") `"
           << location.function_name() << "`: " << '\n';

  UI_UUID hash = std::hash<std::string>{}(pre_hash.str());

  if (existing_ui_elements.contains(hash)) {
    auto &record = existing_ui_elements.at(hash);
    record.last_built_frame = ui_build_frame;
    auto entityID = record.id;
    log_trace("Reusing element {} for {}", hash, entityID);

    // Look up via UICollectionHolder (checks UI collection first, then default)
    try {
      return {UICollectionHolder::getEntityForIDEnforce(entityID), parent};
    } catch (const std::bad_optional_access &e) {
      log_error("Entity ID conflict detected! This usually happens when mk() "
                "is "
                "called multiple times "
                "from the same source location without proper index "
                "management. "
                "Location: {}:{}:{}, Function: {}. "
                "Consider using mk(parent, index) with unique indices or "
                "mk_next(parent) for auto-incrementing.",
                location.file_name(), location.line(), location.column(),
                location.function_name());
      throw;
    }
  }

  Entity &entity = UICollectionHolder::get().collection.createEntity();
  existing_ui_elements[hash] = {entity.id, ui_build_frame};
  log_trace("Creating element {} for {}", hash, entity.id);
  return {entity, parent};
}

// Mark for destruction; the collection's own cleanup() does the freeing.
inline void mark_ui_element_for_cleanup(EntityID id) {
  OptEntity opt = UICollectionHolder::getEntityForID(id);
  if (opt.valid())
    opt.asE().cleanup = true;
}

inline void clear_existing_ui_elements() {
  for (const auto &[hash, record] : existing_ui_elements)
    mark_ui_element_for_cleanup(record.id);
  existing_ui_elements.clear();
}

// Destroy widgets nothing has built for ui_retire_grace_frames. Retaining an
// entity per call site with nothing to retire one is not a cache, it is a leak
// with a bounded key space: every system then walks the union of every screen
// the app has ever shown.
inline void retire_unbuilt_ui_elements() {
  if (ui_retire_grace_frames > 0) {
    for (auto it = existing_ui_elements.begin();
         it != existing_ui_elements.end();) {
      if (ui_build_frame - it->second.last_built_frame <=
          ui_retire_grace_frames) {
        ++it;
        continue;
      }
      mark_ui_element_for_cleanup(it->second.id);
      it = existing_ui_elements.erase(it);
    }
  }
  ui_build_frame++;
}

} // namespace imm

} // namespace ui

} // namespace afterhours
