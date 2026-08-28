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
  // Which item a recycled row is currently showing. 0 means not recycled.
  size_t key = 0;
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
   const std::source_location location = std::source_location::current());

namespace detail {
// A recycled list row reuses one entity for a position in the visible window
// rather than creating one per item. `otherID` is that position and `key` is
// the item currently in it, so *key_changed_out reports the position has moved
// to a different item and whatever the old one was doing has to be undone.
// Internal: virtual_list owns both halves, so no caller has to know.
inline EntityParent
mk_keyed(Entity &parent, EntityID otherID, size_t key,
         bool *key_changed_out = nullptr,
         const std::source_location location = std::source_location::current());
} // namespace detail

// The same bytes the old stringstream spelled out, hashed in place. mk() runs
// once per widget per frame, and building a stringstream and materialising the
// string it holds cost about five allocations and 1.5 KB every single time,
// for a question that is pure lookup once the widget exists.
inline UI_UUID hash_call_site(EntityID parent_id, EntityID otherID,
                              const std::source_location &loc) {
  UI_UUID h = 1469598103934665603ull; // FNV-1a offset basis
  const auto mix = [&h](unsigned char c) {
    h ^= c;
    h *= 1099511628211ull;
  };
  const auto mix_int = [&mix](long long v) {
    for (int i = 0; i < 8; i++)
      mix(static_cast<unsigned char>((v >> (i * 8)) & 0xff));
  };
  // The characters, not the pointer: the same call site in a header gets a
  // different literal address per translation unit, and must still hash alike.
  const auto mix_str = [&mix](const char *s) {
    if (!s)
      return;
    while (*s)
      mix(static_cast<unsigned char>(*s++));
  };
  mix_int(parent_id);
  mix_int(otherID);
  mix_str(loc.file_name());
  mix_int(loc.line());
  mix_int(loc.column());
  mix_str(loc.function_name());
  return h;
}

// Shared by mk() and mk_keyed(): resolve the call site to its entity, stamp it
// built, and report whether the slot changed item.
inline EntityParent mk_impl(Entity &parent, EntityID otherID, size_t key,
                            bool has_key, bool *key_changed_out,
                            const std::source_location &location) {
  UI_UUID hash = hash_call_site(parent.id, otherID, location);

  if (key_changed_out)
    *key_changed_out = false;

  if (existing_ui_elements.contains(hash)) {
    auto &record = existing_ui_elements.at(hash);
    record.last_built_frame = ui_build_frame;
    if (has_key) {
      if (key_changed_out && record.key != key)
        *key_changed_out = true;
      record.key = key;
    }
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
  existing_ui_elements[hash] = {entity.id, ui_build_frame, has_key ? key : 0};
  // A slot seen for the first time has not changed item; it has no history.
  log_trace("Creating element {} for {}", hash, entity.id);
  return {entity, parent};
}

inline EntityParent
mk(Entity &parent, EntityID otherID,
   const std::source_location location) {
  return mk_impl(parent, otherID, 0, false, nullptr, location);
}

namespace detail {
inline EntityParent mk_keyed(Entity &parent, EntityID otherID, size_t key,
                             bool *key_changed_out,
                             const std::source_location location) {
  return mk_impl(parent, otherID, key, true, key_changed_out, location);
}
} // namespace detail

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
