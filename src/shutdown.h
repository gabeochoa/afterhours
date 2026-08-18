#pragma once

// Ordered teardown.
//
// Both the entity collection and the graphics backend are function-local
// statics, and destruction order between them is unspecified. Left to exit,
// the backend often dies first, and entity destructors that still call into
// it hit a variant that no longer holds an alternative -- reported downstream
// as std::bad_variant_access on the way out of main(). Entities have to go
// first, and only the caller knows when that is.

#include "core/entity_helper.h"
#include "graphics_common.h"

namespace afterhours {

/// Call at the end of main(), before any static destructor runs. Idempotent.
inline void shutdown() {
  static bool done = false;
  if (done) return;
  done = true;

  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();
  graphics::shutdown();
}

} // namespace afterhours
