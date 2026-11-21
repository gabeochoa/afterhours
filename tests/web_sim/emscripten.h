// Minimal emscripten.h shim for the simulated-web (web_sim) compile probe.
// src/graphics/web.h (included from graphics.h under __EMSCRIPTEN__) uses
// only the EM_ASM* macros. This shim is for syntax/symbol auditing on a
// non-Emscripten host; it is never linked into a real build.
#pragma once
#ifndef EM_ASM
#define EM_ASM(...) ((void)0)
#endif
#ifndef EM_ASM_INT
#define EM_ASM_INT(...) 0
#endif
#ifndef EM_ASM_DOUBLE
#define EM_ASM_DOUBLE(...) 0.0
#endif
#ifndef EM_ASM_PTR
#define EM_ASM_PTR(...) 0
#endif
