// R-137: start-hidden plumbing at config level (no real window opened).
#include <afterhours/src/graphics_common.h>
#include <cstdio>
namespace g = afterhours::graphics;
static int checks = 0, failures = 0;
static void check(bool c, const char *m) { ++checks; if (!c) { ++failures; std::fprintf(stderr, "FAIL: %s\n", m); } }
int main() {
  check(!g::Config{}.start_hidden, "Config defaults visible");
  check(!g::RunConfig{}.start_hidden, "RunConfig defaults visible");
  g::Config cfg; cfg.start_hidden = true;
  check(cfg.start_hidden, "Config carries start_hidden");
  g::RunConfig run; run.start_hidden = true;
  check(run.start_hidden, "RunConfig carries start_hidden");
  g::note_window_hidden(true);
  check(g::window_is_hidden(), "hidden state records true");
  g::note_window_hidden(false);
  check(!g::window_is_hidden(), "show state records false");
  std::printf("%d/%d checks passed\n", checks - failures, checks);
  return failures ? 1 : 0;
}
