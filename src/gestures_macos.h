#pragma once

// Trackpad pinch (NSEventTypeMagnify) on macOS.
//
// Nothing below the app can see this gesture otherwise: raylib's gesture module
// drives off one synthetic touch point so PINCH_IN/OUT never fire on desktop,
// GLFW's content view does not implement -magnifyWithEvent: so the event dies
// in the responder chain, and a native pinch is not a scroll event (the
// ctrl+wheel convention is browsers synthesising one for web content).
//
// Implemented with a local NSEvent monitor rather than an
// NSMagnificationGestureRecognizer, because the monitor needs no window, no
// content view and no GLFW native headers -- which means it also does not care
// whether raylib's GetWindowHandle() hands back an NSWindow* on this platform.
// A monitor's event.magnification is already per-event incremental, so there is
// nothing to difference either.
//
// Objective-C runtime C API rather than an .mm translation unit: afterhours is
// header-only and 20 projects vendor it. Forcing every one of them to add
// Objective-C++ to their build so that one can pinch is the wrong trade.
//
// OPT-IN. Define AFTER_HOURS_ENABLE_MACOS_GESTURES and build with
// `-fblocks -framework AppKit`. Without it every accessor reads zero, which is
// the correct answer on a machine with no trackpad anyway.

#include <atomic>

namespace afterhours {
namespace gestures {

// Accumulated magnification since the last drain, and whether a gesture is
// mid-flight. Written from the AppKit event thread, read from the game loop.
inline std::atomic<float> pinch_accum{0.f};
inline std::atomic<bool> pinch_active{false};

/// Magnification since the previous call, then zeroed. +0.01 = grow 1%.
inline float consume_pinch_delta() {
  return pinch_accum.exchange(0.f, std::memory_order_relaxed);
}
inline bool is_pinching() {
  return pinch_active.load(std::memory_order_relaxed);
}

#if defined(__APPLE__) && defined(AFTER_HOURS_ENABLE_MACOS_GESTURES)

} // namespace gestures
} // namespace afterhours

#include <objc/message.h>
#include <objc/objc.h>
#include <objc/runtime.h>

namespace afterhours {
namespace gestures {

namespace detail {
// NSEventTypeMagnify == 30, so its mask is 1 << 30. Spelled out rather than
// included, to keep AppKit headers out of a C++ TU.
inline constexpr unsigned long long kEventMaskMagnify = 1ULL << 30;
inline bool monitor_installed = false;
} // namespace detail

/// Call once after the window exists. Idempotent.
///
/// A *local* monitor sees events headed for this app and hands them back
/// untouched, so nothing downstream loses the event -- unlike swallowing it or
/// subclassing the view.
inline void install_pinch_monitor() {
  if (detail::monitor_installed)
    return;

  Class ns_event = objc_getClass("NSEvent");
  if (!ns_event)
    return; // no AppKit (headless, or a non-Cocoa build): stay at zero

  using AddMonitorFn = id (*)(id, SEL, unsigned long long, id (^)(id));
  const SEL add = sel_registerName(
      "addLocalMonitorForEventsMatchingMask:handler:");
  if (!class_respondsToSelector(object_getClass((id)ns_event), add))
    return;

  const SEL magnification = sel_registerName("magnification");
  const SEL phase = sel_registerName("phase");

  reinterpret_cast<AddMonitorFn>(objc_msgSend)(
      (id)ns_event, add, detail::kEventMaskMagnify, ^id(id event) {
        using GetFloatFn = double (*)(id, SEL);
        using GetUIntFn = unsigned long (*)(id, SEL);

        const double delta =
            reinterpret_cast<GetFloatFn>(objc_msgSend)(event, magnification);
        pinch_accum.store(pinch_accum.load(std::memory_order_relaxed) +
                              static_cast<float>(delta),
                          std::memory_order_relaxed);

        // NSEventPhaseEnded (1<<3) and NSEventPhaseCancelled (1<<4) close the
        // gesture; anything else while magnifying means it is still live.
        const unsigned long p =
            reinterpret_cast<GetUIntFn>(objc_msgSend)(event, phase);
        constexpr unsigned long kEnded = 1UL << 3;
        constexpr unsigned long kCancelled = 1UL << 4;
        pinch_active.store(!(p & (kEnded | kCancelled)),
                           std::memory_order_relaxed);

        return event; // pass it on
      });

  detail::monitor_installed = true;
}

#else

/// No-op off macOS, or when the opt-in macro is not set.
inline void install_pinch_monitor() {}

#endif

} // namespace gestures
} // namespace afterhours
