#pragma once

// Image decoding for the sokol/Metal backend.
//
// stb_image is a single-header library: exactly one translation unit must
// define STB_IMAGE_IMPLEMENTATION before including it. The sokol backend
// already forces the consumer to compile SOKOL_IMPL in exactly one obj-c++ TU
// (that is inherent to sokol/fontstash on Metal, not something afterhours
// adds), so we piggyback stb onto that same TU: whenever SOKOL_IMPL is being
// compiled, we define STB_IMAGE_IMPLEMENTATION here automatically.
//
// Net effect: stb adds ZERO extra burden on the consumer. There is no extra
// define to remember and no extra .cc/.mm file to add -- stb rides along with
// the one SOKOL_IMPL TU the sokol backend already required. Every other TU
// that includes this header (via drawing_helpers.h) only pulls in the stb
// *declarations* used by load_texture.
//
// If a project wants stb folded into a different TU than its SOKOL_IMPL one, it
// can define AFTERHOURS_STB_IMAGE_IMPLEMENTATION there instead.
#if defined(SOKOL_IMPL) || defined(AFTERHOURS_STB_IMAGE_IMPLEMENTATION)
#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#endif
#endif

#include <stb/stb_image.h>

// ── Impl-TU sentinel ─────────────────────────────────────────────────────────
// The sokol/Metal backend requires exactly ONE Objective-C++ translation unit
// that defines SOKOL_IMPL and includes this header (see backend.h). If a project
// forgets that TU, the raw failure is a wall of cryptic "Undefined symbols:
// _sg_setup, _stbi_load, _sfons_create ..." link errors.
//
// To make that failure self-explanatory, the impl TU DEFINES the sentinel below
// (compiled only when SOKOL_IMPL is set), and graphics::run() REFERENCES it. If
// the impl TU is missing, the linker fails on this one symbol whose name spells
// out the fix, e.g.:
//
//   Undefined symbols for architecture arm64:
//     "afterhours::graphics::
//        AFTERHOURS_MISSING_sokol_impl_TU__add_one_objcpp_file_that_defines_SOKOL_IMPL_and_includes_afterhours_src_backends_sokol_image_decode_h()"
//
// See examples/web/sokol_impl.cc for the ~10-line file to add.
namespace afterhours::graphics {
// clang-format off
int AFTERHOURS_MISSING_sokol_impl_TU__add_one_objcpp_file_that_defines_SOKOL_IMPL_and_includes_afterhours_src_backends_sokol_image_decode_h();
// clang-format on
#if defined(SOKOL_IMPL) || defined(AFTERHOURS_SOKOL_IMPL_SENTINEL)
// Real (non-inline) definition emitted ONLY by the impl TU, so exactly one
// definition exists across the program. Referenced by graphics::run() below.
int
AFTERHOURS_MISSING_sokol_impl_TU__add_one_objcpp_file_that_defines_SOKOL_IMPL_and_includes_afterhours_src_backends_sokol_image_decode_h() {
  return 0;
}
#endif

// Platform-neutral twin of the sentinel above. The "objcpp file" wording is
// only literally correct on macOS/Metal (where the impl TU must be compiled as
// Objective-C++); on Linux (SOKOL_GLCORE), Windows (SOKOL_D3D11), and web
// (SOKOL_GLES3) the impl TU is a plain C++ translation unit. This twin uses
// OS-agnostic wording so the link error reads correctly everywhere. Both are
// referenced by graphics::run(); the impl TU defines both.
// clang-format off
int AFTERHOURS_MISSING_sokol_impl_TU__add_one_TU_that_defines_SOKOL_IMPL_and_includes_afterhours_src_backends_sokol_image_decode_h();
// clang-format on
#if defined(SOKOL_IMPL) || defined(AFTERHOURS_SOKOL_IMPL_SENTINEL)
int
AFTERHOURS_MISSING_sokol_impl_TU__add_one_TU_that_defines_SOKOL_IMPL_and_includes_afterhours_src_backends_sokol_image_decode_h() {
  return 0;
}
#endif
} // namespace afterhours::graphics
