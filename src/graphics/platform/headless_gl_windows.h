#pragma once
#ifdef _WIN32

namespace afterhours::graphics {

struct HeadlessGLWindows {
  bool init(int width, int height) {
    (void)width;
    (void)height;
    return false;
  }
  void *get_proc_address() { return nullptr; }
  void make_current() {}
  void shutdown() {}
};

} // namespace afterhours::graphics

#endif
