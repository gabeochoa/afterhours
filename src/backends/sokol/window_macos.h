// window_macos.h
// macOS window resizing for the sokol/Metal backend: programmatic resize
// requests and painting during a live (drag) resize.
//
// Include this file from your sokol_impl.mm AFTER the sokol headers, next to
// capture_impl.h:
//
//   #include <afterhours/src/backends/sokol/window_macos.h>
//
// (capture_impl.h already includes it, so the canonical impl TU gets these
// definitions for free.)
//
// Provides the extern "C" implementations called by backend.h /
// window_manager.h:
//
//   metal_set_window_size(w, h)     -- queue a content-area resize
//   metal_set_window_min_size(w, h) -- queue a minimum content size
//   metal_window_resize_pending()   -- a queued resize has not applied yet
//   metal_install_live_resize_draw()-- paint each live-resize step in-step
//   metal_live_resize_draw_count()  -- sync draws performed (test bookkeeping)
//
// Programmatic resize contract (ported from floatinghotel, verified there
// 2026-09-12):
//   - Requests made during update/draw apply between draw callbacks: the
//     actual setFrame is dispatched to the main queue, so the Metal drawable
//     dimensions never change inside a draw callback.
//   - Several pending requests coalesce to the latest size.
//   - Units are content-area points, not framebuffer pixels.
//   - The top-left corner stays anchored (macOS frames are bottom-left).
//   - metal_window_resize_pending() is true from request until the queued
//     block has applied the size, so callers do not draw a mismatched layout
//     or present a first frame at the wrong size.
//
// Live-resize contract (ported from hanabi/floatinghotel):
//   sokol's windowDidResize: updates the drawable size and returns; the frame
//   for the new size used to wait for the next display-link tick, so a resize
//   step could be applied that no frame ever painted. The hook installed by
//   metal_install_live_resize_draw() calls the original delegate method first
//   (dimensions updated), then -- during a live resize -- draws one frame
//   synchronously through the normal frame callback before returning to
//   AppKit. Guards: never re-entrant, never during the first frame, and
//   never while a programmatic resize is pending. Every live-resize step
//   draws exactly once -- a throttled/skipped step would be a size no frame
//   ever painted, which is the bug.

#pragma once

#if defined(__APPLE__) && defined(AFTER_HOURS_USE_METAL)

#import <AppKit/AppKit.h>
#import <MetalKit/MetalKit.h>
#import <objc/runtime.h>

#include <algorithm>
#include <optional>
#include <utility>

// sokol_app (with SOKOL_IMPL) must already be included: this file reads
// _sapp.valid / _sapp.first_frame and the window-delegate class, exactly
// like the app-side helpers it replaces did.

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

  // Honour the minimum content size, including one set in the same batch.
  const NSSize min_size = window.contentMinSize;
  if (min_size.width > 0)
    target_w = std::max(target_w, static_cast<int>(min_size.width));
  if (min_size.height > 0)
    target_h = std::max(target_h, static_cast<int>(min_size.height));

  // Keep the top-left corner anchored (macOS uses a bottom-left origin):
  // grow/shrink the height from the top edge, not the bottom.
  NSRect frame = [window frame];
  const CGFloat title_bar =
      frame.size.height - [[window contentView] frame].size.height;
  const NSRect new_frame =
      NSMakeRect(frame.origin.x,
                 frame.origin.y - ((target_h + title_bar) - frame.size.height),
                 target_w, target_h + title_bar);

  // Pin the Metal view's layer to the top-left for this resize. Sokol renders
  // into an MTKView whose layerContentsPlacement defaults to a scaling value,
  // so on a programmatic setFrame AppKit stretches the last-drawn frame to
  // fill the new bounds until the next frame is drawn. Anchoring top-left
  // keeps the old frame 1:1; the newly exposed strip shows the window
  // background until the next redraw. (Same workaround sokol applies for
  // user drags; see sokol #700/#727.)
  NSView *content_view = [window contentView];
  if (content_view.layer)
    content_view.layerContentsPlacement =
        NSViewLayerContentsPlacementTopLeft;

  // Instant (animate:NO): the animated variant desyncs from the Metal
  // drawable. display:NO: no synchronous AppKit display inside the resize;
  // the next frame callback paints the new size.
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
  // Only paint in-step during a live resize. inLiveResize is true while the
  // user drags; a programmatic setContentSize driven from the tracking run
  // loop (the native test's journey) runs in NSEventTrackingRunLoopMode.
  const bool live = [window inLiveResize] ||
                    [NSRunLoop.currentRunLoop.currentMode
                        isEqualToString:NSEventTrackingRunLoopMode];
  if (!live)
    return;
  if (!_sapp.valid || _sapp.first_frame)
    return;
  // A queued programmatic resize is about to change the size again; drawing
  // now would present a layout for a size that is already stale.
  if (pending_size().has_value())
    return;

  if (drawing_for_resize())
    return;
  // The strip exposed by the resize shows the window background until this
  // draw lands; keep it black so it cannot flash white.
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
  pending_size() = {width, height}; // coalesce to the latest request
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

// Replace the sokol window delegate's windowDidResize: with a version that
// paints the new size before returning to AppKit during a live resize.
// Idempotent; called by graphics::run() before sapp_run().
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
