// R-013: Cmd chords on macOS never became key events because sokol's view
// answered performKeyEquivalent: only for Tab. The live AppKit path needs a
// human at a keyboard, so this tests the exact policy function the view
// calls, plus the RunConfig plumbing that feeds it. Red on revert: with the
// policy forced off-equivalent (no SUPER forwarding), the opt-in checks fail.

#if !defined(AFTER_HOURS_USE_RAYLIB) && !defined(AFTER_HOURS_USE_VULKAN) && !defined(AFTER_HOURS_USE_SOKOL)
#define AFTER_HOURS_USE_SOKOL
#endif

#include <afterhours/src/graphics.h>
#include <afterhours/src/backends/sokol/drawing_helpers.h>

#include <cstdio>
#include <string>

static int checks_run = 0;
static int checks_passed = 0;

static void check(bool cond, const std::string &what) {
  checks_run++;
  if (cond) {
    checks_passed++;
  } else {
    fprintf(stderr, "  FAIL: %s\n", what.c_str());
  }
}

int main() {
  printf("=== sokol cmd chords ===\n\n");

  using P = decltype(&sapp_macos_should_forward_key_equivalent);
  const P policy = &sapp_macos_should_forward_key_equivalent;

  check(!policy(SAPP_KEYCODE_W, SAPP_MODIFIER_SUPER, false),
        "default off: Cmd+W is left for menu equivalents");
  check(policy(SAPP_KEYCODE_W, SAPP_MODIFIER_SUPER, true),
        "opt-in: Cmd+W forwards as a key event");
  check(policy(SAPP_KEYCODE_1, SAPP_MODIFIER_SUPER, true),
        "opt-in: Cmd+1 forwards as a key event");
  check(policy(SAPP_KEYCODE_W, SAPP_MODIFIER_SUPER | SAPP_MODIFIER_SHIFT, true),
        "opt-in: Cmd+Shift+W forwards");
  check(!policy(SAPP_KEYCODE_W, 0, true), "plain W does not forward");
  check(!policy(SAPP_KEYCODE_W, SAPP_MODIFIER_CTRL, true),
        "Ctrl+W does not forward (not a Cmd chord)");
  check(!policy(SAPP_KEYCODE_TAB, SAPP_MODIFIER_SUPER, true),
        "Tab stays on its existing unconditional path");
  check(!policy(SAPP_KEYCODE_INVALID, SAPP_MODIFIER_SUPER, true),
        "invalid key never forwards");

  afterhours::graphics::RunConfig cfg;
  check(!cfg.macos_forward_cmd_chords, "RunConfig defaults to menu behavior");
  cfg.macos_forward_cmd_chords = true;
  check(cfg.macos_forward_cmd_chords, "RunConfig opt-in sticks");

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
