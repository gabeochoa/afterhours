#include <afterhours/src/frame_rate_stats.h>

#include <cmath>
#include <cstdio>

using afterhours::FrameRateStats;

static int checks_run = 0;
static int checks_passed = 0;
static void check(bool cond, const char *expr, int line) {
  checks_run++;
  if (cond)
    checks_passed++;
  else
    fprintf(stderr, "  FAIL: %s  (line %d)\n", expr, line);
}
#define CHECK(expr) check((expr), #expr, __LINE__)
#define NEAR(a, b) (std::fabs((a) - (b)) < 0.01f)

int main() {
  {
    FrameRateStats stats(2.f);
    int windows = 0;
    for (int i = 0; i < 119; i++)
      windows += stats.add(1.f / 60.f);
    CHECK(windows == 0);
    CHECK(!stats.ready());
    windows += stats.add(1.f / 60.f);
    CHECK(windows == 1);
    CHECK(stats.ready());
    CHECK(NEAR(stats.fps(), 60.f));
    CHECK(NEAR(stats.avg_frame_ms(), 1000.f / 60.f));
  }

  {
    FrameRateStats stats(1.f);
    for (int i = 0; i < 9; i++)
      stats.add(0.1f);
    stats.add(0.1f);
    CHECK(NEAR(stats.fps(), 10.f));
    for (int i = 0; i < 8; i++)
      stats.add(0.05f);
    stats.add(0.6f);
    CHECK(NEAR(stats.fps(), 9.f));
    CHECK(NEAR(stats.max_frame_ms(), 600.f));
  }

  printf("%d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
