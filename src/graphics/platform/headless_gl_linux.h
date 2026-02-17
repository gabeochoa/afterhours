#pragma once
#ifdef __linux__

#include <cassert>

namespace afterhours::graphics {

struct HeadlessGLLinux {
  bool init(int width, int height) {
    (void)width;
    (void)height;
    assert(
        false &&
        "Headless GL not yet implemented for Linux - use Xvfb as workaround");
    return false;
  }
  void *get_proc_address() {
    assert(false && "Headless GL not yet implemented for Linux");
    return nullptr;
  }
  void make_current() {
    assert(false && "Headless GL not yet implemented for Linux");
  }
  void shutdown() {}
};

} // namespace afterhours::graphics

#endif
