#pragma once

#include "display_mode.h"

// All raylib-specific types are defined here behind this single guard.
// Other files in the graphics module should include this file instead of
// scattering #ifdef AFTER_HOURS_USE_RAYLIB throughout the codebase.

#ifdef AFTER_HOURS_USE_RAYLIB

// Include raylib headers
#if defined(__has_include)
#if __has_include(<raylib.h>)
namespace raylib {
#include <raylib.h>
#include <rlgl.h>
}  // namespace raylib
#elif __has_include("raylib/raylib.h")
namespace raylib {
#include "raylib/raylib.h"
#include "raylib/rlgl.h"
}  // namespace raylib
#else
#error "raylib headers not found"
#endif
#else
namespace raylib {
#include <raylib.h>
#include <rlgl.h>
}  // namespace raylib
#endif

namespace afterhours::graphics {

// Type alias for render texture (raylib-specific)
using RenderTextureType = raylib::RenderTexture2D;

}  // namespace afterhours::graphics

#else  // !AFTER_HOURS_USE_RAYLIB

namespace afterhours::graphics {

// Placeholder type when raylib is not available
struct RenderTextureType {};

}  // namespace afterhours::graphics

#endif  // AFTER_HOURS_USE_RAYLIB
