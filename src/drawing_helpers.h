
#pragma once

#ifdef AFTER_HOURS_USE_RAYLIB
#include "backends/raylib/drawing_helpers.h"
#elif defined(AFTER_HOURS_USE_METAL)
#include "backends/sokol/drawing_helpers.h"
#else
// The `none` backend records draw calls rather than issuing them (see its
// drawing_helpers.h). Callers that want to assert on what was drawn -- tests,
// mostly -- can key off this instead of re-deriving the backend condition.
#define AFTER_HOURS_BACKEND_NONE
#include "backends/none/drawing_helpers.h"
#endif
