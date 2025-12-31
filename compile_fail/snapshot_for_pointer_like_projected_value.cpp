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
// NOTE: snapshot_for<T>(...) requires T to be a component type (inherits
// BaseComponent), so we can't literally do snapshot_for<Bad>(...) where Bad is a
// raw pointer. Instead, this checks the intended guarantee: that snapshot_for<T>
// rejects a pointer-like projected value type.
void compile_fail_snapshot_for_pointer_like_projected_value() {
  (void)afterhours::snapshot_for<SnapshotCompileFailCmp>(
      [](const SnapshotCompileFailCmp &) -> Bad { return nullptr; });
}

} // namespace afterhours

