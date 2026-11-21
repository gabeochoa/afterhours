#pragma once

// Test stand-in for the Emscripten sysroot header. Real emcc provides this;
// this shim exists so the web branch of crash_handler.h can be compiled and
// linked on a native box. __has_include(<emscripten.h>) finds it because the
// test target adds -Iweb_sim before the system include dirs.
extern "C" char *emscripten_get_callstack(void);
