// macOS sokol resizing. Include after sokol headers (capture_impl.h does).
// Sizes are content-area points; requests coalesce to the latest and apply
// on the main queue between draws, so the drawable never changes mid-draw.
// metal_window_resize_pending() holds callers off a mismatched layout.
// Live resize: draw synchronously in windowDidResize: or a step can be
// applied that no frame ever paints.

#pragma once

#if defined(__APPLE__) && defined(AFTER_HOURS_USE_METAL)

#import <AppKit/AppKit.h>
#import <MetalKit/MetalKit.h>
#import <objc/runtime.h>

#include <algorithm>
#include <optional>
#include <utility>

// Requires SOKOL_IMPL already included: reads _sapp and the delegate class.

namespace afterhours::graphics::window_macos_detail {

inline std::optional<std::pair<int, int>> &pending_size() {
  static std::optional<std::pair<int, int>> s;
  return s;
}
inline std::optional<std::pair<int, int>> &pending_min_size() {
  static std::optional<std::pair<int, int>> s;
  return s;
}
inline bool &apply_scheduled() {
  static bool s = false;
  return s;
}
inline bool &drawing_for_resize() {
  static bool s = false;
  return s;
}
inline int &live_resize_draw_count() {
  static int s = 0;
  return s;
}

inline void apply_pending_window_size() {
  apply_scheduled() = false;
  NSWindow *window = (__bridge NSWindow *)sapp_macos_get_window();
  if (!window) {
    pending_size().reset();
    pending_min_size().reset();
    return;
  }
  if (pending_min_size()) {
    const auto [min_w, min_h] = *pending_min_size();
    pending_min_size().reset();
    window.contentMinSize = NSMakeSize(min_w, min_h);
  }
  if (!pending_size())
    return;
  auto [target_w, target_h] = *pending_size();
  pending_size().reset();

  const NSSize min_size = window.contentMinSize;
  if (min_size.width > 0)
    target_w = std::max(target_w, static_cast<int>(min_size.width));
  if (min_size.height > 0)
    target_h = std::max(target_h, static_cast<int>(min_size.height));

  // Top-left anchored: macOS frames are bottom-left.
  NSRect frame = [window frame];
  const CGFloat title_bar =
      frame.size.height - [[window contentView] frame].size.height;
  const NSRect new_frame =
      NSMakeRect(frame.origin.x,
                 frame.origin.y - ((target_h + title_bar) - frame.size.height),
                 target_w, target_h + title_bar);

  // TopLeft placement or AppKit stretches the last frame to the new bounds
  // until the next draw (sokol #700/#727).
  NSView *content_view = [window contentView];
  if (content_view.layer)
    content_view.layerContentsPlacement =
        NSViewLayerContentsPlacementTopLeft;

  // animate:NO would desync the drawable; display:NO defers paint to frame cb.
  [window setFrame:new_frame display:NO animate:NO];
  [content_view setNeedsDisplay:YES];
}

inline void schedule_apply() {
  if (apply_scheduled())
    return;
  apply_scheduled() = true;
  dispatch_async(dispatch_get_main_queue(), ^{
    apply_pending_window_size();
  });
}

using WindowDidResizeFn = void (*)(id, SEL, NSNotification *);
inline WindowDidResizeFn &orig_window_did_resize() {
  static WindowDidResizeFn s = nullptr;
  return s;
}
inline bool &live_resize_installed() {
  static bool s = false;
  return s;
}

inline void window_did_resize_and_draw(id delegate, SEL selector,
                                       NSNotification *notification) {
  // Original first: sokol updates the drawable dimensions here.
  orig_window_did_resize()(delegate, selector, notification);

  NSWindow *window = (__bridge NSWindow *)sapp_macos_get_window();
  if (!window)
    return;
  // Tracking-run-loop mode covers programmatic resizes driven like a drag.
  const bool live = [window inLiveResize] ||
                    [NSRunLoop.currentRunLoop.currentMode
                        isEqualToString:NSEventTrackingRunLoopMode];
  if (!live)
    return;
  if (!_sapp.valid || _sapp.first_frame)
    return;
  // Drawing now would present a size a queued resize already superseded.
  if (pending_size().has_value())
    return;

  if (drawing_for_resize())
    return;
  // Black so the newly exposed strip cannot flash white before the draw.
  window.backgroundColor =
      [NSColor colorWithSRGBRed:0 green:0 blue:0 alpha:1];
  MTKView *view = (MTKView *)window.contentView;
  if (!view)
    return;
  drawing_for_resize() = true;
  [view draw];
  drawing_for_resize() = false;
  ++live_resize_draw_count();
}

} // namespace afterhours::graphics::window_macos_detail

extern "C" void metal_set_window_size(int width, int height) {
  using namespace afterhours::graphics::window_macos_detail;
  if (width <= 0 || height <= 0)
    return;
  pending_size() = {width, height};
  schedule_apply();
}

extern "C" void metal_set_window_min_size(int width, int height) {
  using namespace afterhours::graphics::window_macos_detail;
  if (width <= 0 || height <= 0)
    return;
  pending_min_size() = {width, height};
  schedule_apply();
}

extern "C" bool metal_window_resize_pending(void) {
  using namespace afterhours::graphics::window_macos_detail;
  return pending_size().has_value() || pending_min_size().has_value();
}

extern "C" int metal_live_resize_draw_count(void) {
  return afterhours::graphics::window_macos_detail::live_resize_draw_count();
}

extern "C" void metal_install_live_resize_draw(void) {
  using namespace afterhours::graphics::window_macos_detail;
  if (live_resize_installed())
    return;
  live_resize_installed() = true;
  Method resize = class_getInstanceMethod([_sapp_macos_window_delegate class],
                                          @selector(windowDidResize:));
  if (!resize)
    return;
  orig_window_did_resize() = reinterpret_cast<WindowDidResizeFn>(
      method_getImplementation(resize));
  method_setImplementation(
      resize, reinterpret_cast<IMP>(window_did_resize_and_draw));
}

#endif // __APPLE__ && AFTER_HOURS_USE_METAL
