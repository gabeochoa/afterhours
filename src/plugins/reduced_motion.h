#pragma once

// The OS reduced-motion accessibility setting, for seeding an app's own
// reduced_motion option on first run. Read-only and safe to call before any
// window exists.
//
// macOS reads NSWorkspace.accessibilityDisplayShouldReduceMotion through the
// Objective-C runtime C API, following gestures_macos.h: afterhours is
// header-only, so no .mm translation unit is forced on consumers. Linking
// needs the Objective-C runtime, which macOS apps already pull in with
// AppKit. Other platforms return false until their backend lands; callers
// must only seed when the app has no stored value yet, so a false here never
// overwrites a user choice.

#if defined(__APPLE__)
#include <objc/message.h>
#include <objc/runtime.h>
#endif

namespace afterhours {
namespace os {

inline bool reduced_motion_enabled() {
#if defined(__APPLE__)
  using IdSend = id (*)(id, SEL);
  using BoolSend = bool (*)(id, SEL);
  const id workspace_class = (id)objc_getClass("NSWorkspace");
  if (!workspace_class)
    return false;
  const id workspace =
      ((IdSend)objc_msgSend)(workspace_class, sel_registerName("sharedWorkspace"));
  if (!workspace)
    return false;
  return ((BoolSend)objc_msgSend)(
      workspace, sel_registerName("accessibilityDisplayShouldReduceMotion"));
#else
  return false;
#endif
}

} // namespace os
} // namespace afterhours
