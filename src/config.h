#pragma once

// Central afterhours build configuration.
//
// afterhours core headers include this FIRST, so feature macros resolve
// consistently regardless of user include order. Turn features on either way:
//   - Build flag (robust, order-independent): -DAFTER_HOURS_ENABLE_RANDOM
//   - Matching plugin header (convenience): #include plugins/random.h before
//     core -- it defines the same flag. Prefer the build flag for multi-TU
//     builds without a shared PCH (see the ODR note in plugins/random.h).
//
// Each feature exposes a 0/1 macro (AFTER_HOURS_*_ENABLED) that core code tests
// with `#if`, so there is one place that maps build flags -> features.

// Seedable random: RandomEngine + Library::get_random_match /
// EntityQuery::gen_random. Off by default (no <random> cost).
#if defined(AFTER_HOURS_ENABLE_RANDOM)
#define AFTER_HOURS_RANDOM_ENABLED 1
#else
#define AFTER_HOURS_RANDOM_ENABLED 0
#endif
