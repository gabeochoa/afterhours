
#if !defined(AFTER_HOURS_USE_RAYLIB) && !defined(AFTER_HOURS_USE_VULKAN) &&     \
    !defined(AFTER_HOURS_USE_SOKOL)
#define AFTER_HOURS_USE_SOKOL
#endif

#include <afterhours/src/graphics.h>
#include <afterhours/src/backends/sokol/drawing_helpers.h>

#import <AppKit/AppKit.h>

#include <cstdio>

static int checks = 0;
static int failures = 0;
static void check(bool condition, const char *message) {
  ++checks;
  if (condition)
    return;
  ++failures;
  std::fprintf(stderr, "FAIL: %s\n", message);
}

static int g_probe_draws = 0;
static bool g_probe_drawn_before_vsync = false;
static bool g_vsync_simulated = false;

@interface AHStartupProbeView : NSView
@end
@implementation AHStartupProbeView
- (void)drawRect:(NSRect)rect {
  (void)rect;
  ++g_probe_draws;
  if (!g_vsync_simulated)
    g_probe_drawn_before_vsync = true;
}
@end

int main() {
  using namespace afterhours::graphics;

  check(metal_should_draw_first_frame_early(true, false),
        "draws early when a view exists and nothing drew yet");
  check(!metal_should_draw_first_frame_early(false, false),
        "no early draw without a view");
  check(!metal_should_draw_first_frame_early(true, true),
        "no second early draw");

  metal_reset_first_frame_early_draw();
  metal_install_first_frame_early_draw();
  metal_install_first_frame_early_draw();

  [NSApplication sharedApplication];
  NSWindow *window =
      [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 64, 48)
                                  styleMask:NSWindowStyleMaskTitled
                                    backing:NSBackingStoreBuffered
                                      defer:NO];
  AHStartupProbeView *view = [[AHStartupProbeView alloc] init];
  [window setContentView:view];

  NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
  [center postNotificationName:NSApplicationWillFinishLaunchingNotification
                        object:nil];
  check(g_probe_draws == 0, "nothing draws at WillFinishLaunching");
  check(metal_first_frame_early_attempt_count() == 0,
        "no early-draw attempt before DidFinishLaunching");

  [center postNotificationName:NSApplicationDidFinishLaunchingNotification
                        object:nil];
  check(g_probe_draws == 1, "probe view drawn once at DidFinishLaunching");
  check(g_probe_drawn_before_vsync,
        "early draw happened before the first vsync");
  check(metal_first_frame_early_draw_count() == 1, "one early draw counted");
  check(metal_first_frame_early_attempt_count() == 1,
        "double install still registers one observer");

  [center postNotificationName:NSApplicationDidFinishLaunchingNotification
                        object:nil];
  check(g_probe_draws == 1, "second DidFinish does not redraw");
  check(metal_first_frame_early_draw_count() == 1, "still one early draw");

  g_vsync_simulated = true;
  [view drawRect:[view bounds]];
  check(g_probe_draws == 2, "the display-link draw still follows");

  std::printf("%d/%d checks passed\n", checks - failures, checks);
  return failures == 0 ? 0 : 1;
}
