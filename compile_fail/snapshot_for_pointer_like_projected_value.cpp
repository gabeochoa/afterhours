#include "../src/ecs.h"

namespace afterhours {

struct SnapshotCompileFailCmp : BaseComponent {
  int x = 0;
};

// Intentionally pointer-like: this must be rejected by the snapshot API.
using Bad = int *;

// This translation unit is a "compile-fail" regression:
// it must FAIL to compile due to the pointer-free policy static_assert.
//
// We force template instantiation by actually calling snapshot_for<>.
void compile_fail_snapshot_for_pointer_like_projected_value() {
  (void)afterhours::snapshot_for<SnapshotCompileFailCmp>(
      [](const SnapshotCompileFailCmp &) -> Bad { return nullptr; });
}

} // namespace afterhours

