// sokol_impl.cc
// The single translation unit that compiles the sokol + fontstash
// implementations. Everything else only includes the sokol headers in
// declaration mode.
//
// The GPU backend is chosen by the compiler/build:
//   - web:   emcc + -DSOKOL_GLES3 (WebGL2)
//   - macOS: compiled as Objective-C++, sokol auto-selects Metal
//
// SOKOL_NO_ENTRY: afterhours calls sapp_run() itself from graphics::run().

#define SOKOL_IMPL
#define SOKOL_NO_ENTRY

#include <sokol/sokol_app.h>
#include <sokol/sokol_gfx.h>
#include <sokol/sokol_glue.h>
#include <sokol/sokol_log.h>
#include <sokol/sokol_time.h>
#include <sokol/sokol_gl.h>

// Text rendering: fontstash implementation, then the sokol integration.
#define FONTSTASH_IMPLEMENTATION
#include <fontstash/fontstash.h>
#include <sokol/sokol_fontstash.h>

// Image decoding for texture loading: the sokol backend's image_decode.h
// automatically defines STB_IMAGE_IMPLEMENTATION when SOKOL_IMPL is defined
// (see src/backends/sokol/image_decode.h), so stb rides along with this TU --
// no manual STB define and no extra .cc file for the consumer to manage.
#include <afterhours/src/backends/sokol/image_decode.h>

// Metal device creation + texture readback. These are extern "C" and called
// from the header-only backend, so they need an Objective-C++ TU to live in;
// this is it. Without them the native build compiles and fails at link on
// metal_create_system_device / metal_capture_render_texture.
//
// capture_impl.h is guarded on AFTER_HOURS_USE_METAL, which main.cpp defines
// for itself -- this TU has to say so too. Only on the native Metal build:
// the emscripten/WebGL2 build has no Metal.
#if defined(SOKOL_METAL)
#define AFTER_HOURS_USE_METAL
#include <afterhours/src/backends/sokol/capture_impl.h>
#endif
