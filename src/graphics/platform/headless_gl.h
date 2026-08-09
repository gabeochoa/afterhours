#pragma once

#include <concepts>

namespace afterhours::graphics {

template <typename T>
concept HeadlessGLImpl = requires(T t) {
  { t.init(int{}, int{}) } -> std::same_as<bool>;
  { t.get_proc_address() } -> std::same_as<void *>;
  { t.make_current() } -> std::same_as<void>;
  { t.shutdown() } -> std::same_as<void>;
};

} // namespace afterhours::graphics

#if defined(__APPLE__)
#include "headless_gl_macos.h"
namespace afterhours::graphics {
using HeadlessGL = HeadlessGLMacOS;
}
#elif defined(__linux__)
#include "headless_gl_linux.h"
namespace afterhours::graphics {
using HeadlessGL = HeadlessGLLinux;
}
#elif defined(_WIN32)
#include "headless_gl_windows.h"
namespace afterhours::graphics {
using HeadlessGL = HeadlessGLWindows;
}
#elif defined(__EMSCRIPTEN__)
// Web builds only use the canvas/windowed backend. A stub keeps RaylibHeadless
// in the backend variant compiling; init() always fails if somehow selected.
namespace afterhours::graphics {
struct HeadlessGLEmscripten {
  bool init(int, int) { return false; }
  void *get_proc_address() { return nullptr; }
  void make_current() {}
  void shutdown() {}
};
using HeadlessGL = HeadlessGLEmscripten;
}
#else
#error "Headless GL not supported on this platform"
#endif

static_assert(
    afterhours::graphics::HeadlessGLImpl<afterhours::graphics::HeadlessGL>,
    "HeadlessGL must satisfy HeadlessGLImpl concept");
