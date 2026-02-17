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
#else
#error "Headless GL not supported on this platform"
#endif

static_assert(
    afterhours::graphics::HeadlessGLImpl<afterhours::graphics::HeadlessGL>,
    "HeadlessGL must satisfy HeadlessGLImpl concept");
