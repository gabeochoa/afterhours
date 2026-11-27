#pragma once

#if defined(__APPLE__) && defined(AFTER_HOURS_USE_METAL)

#import <AppKit/AppKit.h>
#import <objc/runtime.h>

namespace afterhours::graphics::visibility_detail {
inline bool &armed() { static bool v = false; return v; }
inline bool &installed() { static bool v = false; return v; }
using LaunchFn = void (*)(id, SEL, NSNotification *);
inline LaunchFn &orig_launch() { static LaunchFn v = nullptr; return v; }

inline void launch_and_hide(id self, SEL sel, NSNotification *note) {
  orig_launch()(self, sel, note);
  if (!armed())
    return;
  armed() = false;
  NSWindow *window = (__bridge NSWindow *)sapp_macos_get_window();
  if (window)
    [window orderOut:nil];
}
}

extern "C" void ah_sokol_install_hidden_start(void) {
  using namespace afterhours::graphics::visibility_detail;
  armed() = true;
  if (installed())
    return;
  installed() = true;
  Method m = class_getInstanceMethod(
      [_sapp_macos_app_delegate class],
      @selector(applicationDidFinishLaunching:));
  if (!m)
    return;
  orig_launch() = reinterpret_cast<LaunchFn>(method_getImplementation(m));
  method_setImplementation(m, reinterpret_cast<IMP>(launch_and_hide));
}

extern "C" void ah_sokol_show_window(void) {
  NSWindow *window = (__bridge NSWindow *)sapp_macos_get_window();
  if (!window)
    return;
  [NSApp activateIgnoringOtherApps:YES];
  [window makeKeyAndOrderFront:nil];
}

extern "C" void ah_sokol_hide_window(void) {
  NSWindow *window = (__bridge NSWindow *)sapp_macos_get_window();
  if (window)
    [window orderOut:nil];
}

#endif
