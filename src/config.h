#pragma once

// Central afterhours build configuration.
//
// afterhours core headers include this FIRST, so config resolves consistently
// regardless of user include order. Override any value below with a build flag,
// e.g. -DAFTER_HOURS_MAX_COMPONENTS=256; the #ifndef guards let -D win.

// ---------------------------------------------------------------------------
// Tunable values (override via -D<name>=<value>)
// ---------------------------------------------------------------------------

// Max distinct tag ids an entity can carry.
#ifndef AFTER_HOURS_MAX_ENTITY_TAGS
#define AFTER_HOURS_MAX_ENTITY_TAGS 64
#endif

// Max distinct component types.
#ifndef AFTER_HOURS_MAX_COMPONENTS
#define AFTER_HOURS_MAX_COMPONENTS 128
#endif

// Sprite atlas geometry (texture_manager).
#ifndef AFTERHOURS_SPRITE_SIZE_PX
#define AFTERHOURS_SPRITE_SIZE_PX 32
#endif
#ifndef AFTERHOURS_SPRITE_SHEET_NUM_SPRITES_WIDE
#define AFTERHOURS_SPRITE_SHEET_NUM_SPRITES_WIDE 32
#endif

// ---------------------------------------------------------------------------
// Feature toggles (define via -D<name> to enable; checked in place across the
// codebase). Listed here as the central reference:
//   AFTER_HOURS_USE_RAYLIB            select raylib backend (the default)
//   AFTER_HOURS_USE_SOKOL            select sokol backend, API picked by platform
//   AFTER_HOURS_USE_METAL            legacy alias for USE_SOKOL (sokol family)
//   AFTER_HOURS_USE_VULKAN           sokol family + force the Vulkan API
//   AFTER_HOURS_USE_NONE             select none backend (records draw calls)
//   AFTER_HOURS_ENABLE_E2E_TESTING   e2e input injection + commands
//   AFTER_HOURS_UI_SINGLE_COLLECTION single UI entity collection
//   AFTERHOURS_SINGLE_RENDER_PASS    one render pass
//   AFTER_HOURS_DEBUG                debug tooling
//   AFTER_HOURS_ENABLE_RANDOM        seedable RandomEngine (see below)
// ---------------------------------------------------------------------------

// Derived: backend selection. Define your backend macro before including
// any afterhours header (or via -D); a late define is silently ignored.
// Default: raylib. To use sokol, define AFTER_HOURS_USE_SOKOL and the API
// is picked by platform (Metal on Apple, Vulkan on Windows, GLES3 on
// Emscripten). AFTER_HOURS_USE_METAL is a legacy alias for USE_SOKOL;
// AFTER_HOURS_USE_VULKAN forces the Vulkan API. AFTER_HOURS_USE_NONE
// selects the none backend. AFTER_HOURS_USE_D3D11 is not implemented yet.
#if defined(AFTER_HOURS_USE_D3D11)
#error "AFTER_HOURS_USE_D3D11 is not implemented yet"
#endif
#if defined(AFTER_HOURS_USE_RAYLIB) &&                                          \
    (defined(AFTER_HOURS_USE_SOKOL) || defined(AFTER_HOURS_USE_METAL) ||       \
     defined(AFTER_HOURS_USE_VULKAN) || defined(AFTER_HOURS_USE_NONE))
#error "AFTER_HOURS_USE_RAYLIB is mutually exclusive with the sokol / none backends"
#endif
#if defined(AFTER_HOURS_USE_NONE) &&                                           \
    (defined(AFTER_HOURS_USE_SOKOL) || defined(AFTER_HOURS_USE_METAL) ||       \
     defined(AFTER_HOURS_USE_VULKAN))
#error "AFTER_HOURS_USE_NONE is mutually exclusive with the sokol backends"
#endif
#if defined(AFTER_HOURS_USE_VULKAN) && (defined(__APPLE__) || defined(__EMSCRIPTEN__))
#error "AFTER_HOURS_USE_VULKAN is not available on Apple / Emscripten"
#endif
#if !defined(AFTER_HOURS_USE_RAYLIB) && !defined(AFTER_HOURS_USE_SOKOL) &&     \
    !defined(AFTER_HOURS_USE_METAL) && !defined(AFTER_HOURS_USE_VULKAN) &&     \
    !defined(AFTER_HOURS_USE_NONE)
#define AFTER_HOURS_USE_RAYLIB 1
#endif
#if defined(AFTER_HOURS_USE_SOKOL) || defined(AFTER_HOURS_USE_METAL) ||       \
    defined(AFTER_HOURS_USE_VULKAN)
#define AFTER_HOURS_SOKOL 1
#endif

// Derived: sokol graphics API. An explicit SOKOL_* (-D) wins; otherwise it
// follows the platform. Generic sokol code checks AFTER_HOURS_SOKOL;
// API-specific code checks SOKOL_METAL / SOKOL_VULKAN / SOKOL_GLES3.
// SOKOL is defined only when a sokol backend is selected: sites check
// defined(), so never default it to 0.
#if defined(AFTER_HOURS_SOKOL) && !defined(SOKOL_METAL) &&                     \
    !defined(SOKOL_VULKAN) && !defined(SOKOL_GLES3) && !defined(SOKOL_D3D11) && \
    !defined(SOKOL_GLCORE)
#if defined(AFTER_HOURS_USE_VULKAN)
#define SOKOL_VULKAN 1
#elif defined(__APPLE__)
#define SOKOL_METAL 1
#elif defined(_WIN32)
#define SOKOL_VULKAN 1
#elif defined(__EMSCRIPTEN__)
#define SOKOL_GLES3 1
#else
#error "Sokol backend on this platform needs an explicit SOKOL_* API define (SOKOL_VULKAN / SOKOL_METAL / SOKOL_GLES3)"
#endif
#endif

// Derived: seedable random. RandomEngine + Library::get_random_match /
// EntityQuery::gen_random. Off by default (no <random> cost).
#if defined(AFTER_HOURS_ENABLE_RANDOM)
#define AFTER_HOURS_RANDOM_ENABLED 1
#else
#define AFTER_HOURS_RANDOM_ENABLED 0
#endif
