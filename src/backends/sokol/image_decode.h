#pragma once

// Image decoding for the sokol/Metal backend.
//
// afterhours is header-only, so the STB_IMAGE_IMPLEMENTATION must live in the
// same single translation unit that compiles the sokol implementation. This
// mirrors how fontstash / sokol_fontstash are compiled: the *declarations* are
// pulled in here (used by load_texture in drawing_helpers.h), and exactly ONE
// TU defines the implementation.
//
// The demo's sokol_impl.cc defines STB_IMAGE_IMPLEMENTATION before including
// <stb/stb_image.h> (alongside SOKOL_IMPL / FONTSTASH_IMPLEMENTATION). Any
// other project using the sokol backend must do the same in one of its TUs
// (define STB_IMAGE_IMPLEMENTATION, then #include <stb/stb_image.h>), otherwise
// stbi_load* will fail to link.

#include <stb/stb_image.h>
