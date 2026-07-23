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
