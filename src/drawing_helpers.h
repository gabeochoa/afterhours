
#pragma once

#ifdef AFTER_HOURS_USE_RAYLIB
#include "backends/raylib/drawing_helpers.h"
#elif defined(AFTER_HOURS_USE_METAL)
#include "backends/sokol/drawing_helpers.h"
#else
#include "backends/none/drawing_helpers.h"
#endif
