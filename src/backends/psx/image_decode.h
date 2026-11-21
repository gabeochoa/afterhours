#pragma once

// Image *header* decoding for the PSX backend.
//
// The budget only needs a texture's dimensions, never its pixels (there is
// no GPU on this side of the simulation), so load_texture reads sizes with
// stbi_info from stb_image -- the same library image.h wraps.
//
// stb is a single-header library: exactly one translation unit must define
// STB_IMAGE_IMPLEMENTATION. Same rule as image.h: define
// AFTERHOURS_STB_IMAGE_IMPLEMENTATION in one of your .cpp files (or include
// image.h there, which does it for you) and every other TU sees only the
// declarations pulled in here. If the link fails with an undefined
// stbi_info, that define is what is missing.
#if defined(AFTERHOURS_STB_IMAGE_IMPLEMENTATION)
#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#endif
#endif

#include <stb/stb_image.h>
