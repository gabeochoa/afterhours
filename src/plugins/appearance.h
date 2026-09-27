#pragma once

// The OS appearance (light/dark), for seeding an app's own theme choice on
// first run. Read-only and safe to call before any window exists.
//
// macOS reads the AppleInterfaceStyle user default through the Objective-C
// runtime C API, following reduced_motion.h: afterhours is header-only, so
// no .mm translation unit is forced on consumers. The default is absent in
// light mode and "Dark" in dark mode, and the OS keeps it current when the
// appearance follows the Auto schedule. Linking needs the Objective-C
// runtime, which macOS apps already pull in with AppKit. Other platforms
// return Appearance::Unknown until their backend lands; callers must only
// seed when the app has no stored value yet, so Unknown never overwrites a
// user choice.

#if defined(__APPLE__)
#include <objc/message.h>
#include <objc/runtime.h>
#endif

namespace afterhours {
namespace os {

enum struct Appearance { Light, Dark, Unknown };

inline Appearance appearance() {
#if defined(__APPLE__)
  using IdSend = id (*)(id, SEL);
  using IdSendArg = id (*)(id, SEL, id);
  using CStringSend = id (*)(id, SEL, const char *);
  using BoolSendArg = bool (*)(id, SEL, id);
  const id defaults_class = (id)objc_getClass("NSUserDefaults");
  const id string_class = (id)objc_getClass("NSString");
  if (!defaults_class || !string_class) return Appearance::Unknown;
  const id defaults = ((IdSend)objc_msgSend)(
      defaults_class, sel_registerName("standardUserDefaults"));
  if (!defaults) return Appearance::Unknown;
  const auto ns_string = [&](const char *text) {
    return ((CStringSend)objc_msgSend)(
        string_class, sel_registerName("stringWithUTF8String:"), text);
  };
  const id style = ((IdSendArg)objc_msgSend)(
      defaults, sel_registerName("stringForKey:"), ns_string("AppleInterfaceStyle"));
  if (!style) return Appearance::Light;
  const bool dark = ((BoolSendArg)objc_msgSend)(
      style, sel_registerName("isEqualToString:"), ns_string("Dark"));
  return dark ? Appearance::Dark : Appearance::Light;
#else
  return Appearance::Unknown;
#endif
}

inline bool dark_mode_enabled() { return appearance() == Appearance::Dark; }

} // namespace os
} // namespace afterhours
