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
//   AFTER_HOURS_USE_RAYLIB            select raylib backend
//   AFTER_HOURS_USE_METAL            select metal/sokol backend
//   AFTER_HOURS_ENABLE_E2E_TESTING   e2e input injection + commands
//   AFTER_HOURS_UI_SINGLE_COLLECTION single UI entity collection
//   AFTERHOURS_SINGLE_RENDER_PASS    one render pass
//   AFTER_HOURS_DEBUG                debug tooling
//   AFTER_HOURS_ENABLE_RANDOM        seedable RandomEngine (see below)
// ---------------------------------------------------------------------------

// Derived: seedable random. RandomEngine + Library::get_random_match /
// EntityQuery::gen_random. Off by default (no <random> cost).
#if defined(AFTER_HOURS_ENABLE_RANDOM)
#define AFTER_HOURS_RANDOM_ENABLED 1
#else
#define AFTER_HOURS_RANDOM_ENABLED 0
#endif
