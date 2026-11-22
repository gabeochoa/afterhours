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

extern "C" char *emscripten_get_callstack(void);
