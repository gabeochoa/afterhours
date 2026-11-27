#pragma once

// Unused-backend raylib stubs for the scripted-input tests.
//
// input_device_activity_test and input_enum_actions_test drive the input
// system through their own raylib::IsKeyPressed / IsGamepad* doubles (in each
// test file). Including the raylib-backend headers also compiles window /
// audio / texture helpers those tests never run, and those helpers reference
// the raylib entry points below. If the tests link the real libraylib, the
// linker must pull rcore.c.o to satisfy them -- and in raylib 6.x rcore.c.o
// (which now also carries the rlgl entry points) defines the very input
// functions the tests double, so the link fails with duplicate symbols
// against the static archive. Earlier raylib installs did not collide
// because -lraylib resolved to a shared library there, whose symbols an
// executable definition simply preempts.
//
// So these tests link no raylib at all: every raylib symbol their
// translation unit references is defined here or in the test file. The
// functions below are never called by the tests (no window is opened); they
// exist only so the unused backend code has something to link against.
// Include this after a header that has already pulled <raylib.h> into
// namespace raylib (e.g. input_system.h); signatures mirror raylib 5.5/6.x.

namespace raylib {

inline void InitWindow(int, int, const char *) {}
inline void CloseWindow() {}
inline bool IsWindowReady() { return false; }
inline bool IsWindowHidden() { return false; }
inline bool IsWindowMinimized() { return false; }
inline bool IsWindowResized() { return false; }
inline int GetScreenWidth() { return 0; }
inline int GetScreenHeight() { return 0; }
inline Vector2 GetWindowScaleDPI() { return {1.f, 1.f}; }
inline void BeginTextureMode(RenderTexture2D) {}
inline void EndTextureMode() {}
inline void SetTargetFPS(int) {}
inline float GetFrameTime() { return 0.f; }
inline void SetConfigFlags(unsigned int) {}

inline void InitAudioDevice() {}
inline void CloseAudioDevice() {}
inline bool IsAudioDeviceReady() { return false; }

inline Image LoadImageFromTexture(Texture2D) { return {}; }
inline void UnloadImage(Image) {}
inline bool ExportImage(Image, const char *) { return false; }
inline void ImageFlipVertical(Image *) {}
inline Image GenImageColor(int, int, Color) { return {}; }
inline Image ImageCopy(Image) { return {}; }
#if RAYLIB_VERSION_MAJOR >= 6
inline void ImageDrawImage(Image *, Image, int, int, Color) {}
#else
inline void ImageDraw(Image *, Image, Rectangle, Rectangle, Color) {}
#endif
inline void ImageResize(Image *, int, int) {}
inline RenderTexture2D LoadRenderTexture(int, int) { return {}; }
inline void UnloadRenderTexture(RenderTexture2D) {}

inline bool FileExists(const char *) { return false; }
inline int SetGamepadMappings(const char *) { return 0; }
inline char *LoadFileText(const char *) { return nullptr; }
inline void UnloadFileText(char *) {}

inline void rlMatrixMode(int) {}
inline void rlLoadIdentity() {}
inline void rlOrtho(double, double, double, double, double, double) {}
inline void rlSetBlendMode(int) {}
inline void rlglInit(int, int) {}
inline void rlglClose() {}
inline void rlLoadExtensions(void *) {}
inline void rlDrawRenderBatchActive() {}

} // namespace raylib
