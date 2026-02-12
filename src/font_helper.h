
#pragma once

#ifdef AFTER_HOURS_USE_RAYLIB
#include "backends/raylib/font_helper.h"
#elif defined(AFTER_HOURS_USE_METAL)
#include "backends/sokol/font_helper.h"
#else
#include "backends/none/font_helper.h"
#endif
