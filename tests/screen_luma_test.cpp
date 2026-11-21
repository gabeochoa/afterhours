// expect_screen_luma: the mean-luma helper and the command handler.
//
// The GPU readback is the consumer's provider, so what is covered here is
// the part that decides yes or no: mean_luma over known colours, and the
// handler's retry/fail edges. An assertion that cannot fail is worse than
// no assertion.
#define FMT_HEADER_ONLY
#if !defined(AFTER_HOURS_USE_RAYLIB) && !defined(AFTER_HOURS_USE_SOKOL)
#define AFTER_HOURS_USE_NONE
#define AFTERHOURS_STB_IMAGE_IMPLEMENTATION
#endif

#include <cmath>
#include <cstdio>

#include "ui_test_harness.h"
#include <afterhours/src/plugins/e2e_testing/screen_commands.h>

using namespace afterhours;
using afterhours::testing::PendingE2ECommand;
using namespace afterhours::testing::screen_commands;

static int checks = 0;
static int failures = 0;

static void check(bool cond, const char *what) {
  checks++;
  if (!cond) {
    failures++;
    std::printf("  FAIL: %s\n", what);
  }
}

static Image solid(int w, int h, std::uint8_t r, std::uint8_t g,
                   std::uint8_t b) {
  Image img;
  img.width = w;
  img.height = h;
  img.rgba.assign(static_cast<std::size_t>(w) * h * 4, 0);
  for (std::size_t i = 0; i + 3 < img.rgba.size(); i += 4) {
    img.rgba[i] = r;
    img.rgba[i + 1] = g;
    img.rgba[i + 2] = b;
    img.rgba[i + 3] = 255;
  }
  return img;
}

static bool near(float a, float b, float eps = 1e-4f) {
  return std::fabs(a - b) <= eps;
}

static PendingE2ECommand run(std::initializer_list<std::string> args,
                             std::optional<float> luma) {
  LumaProvider p;
  p.get_mean_luma = [luma] { return luma; };
  set_provider(std::move(p));
  EntityCollection coll;
  Entity &e = coll.createEntity();
  PendingE2ECommand cmd;
  cmd.name = "expect_screen_luma";
  cmd.args.assign(args.begin(), args.end());
  HandleExpectScreenLumaCommand handler;
  handler.for_each_with(e, cmd, 0.f);
  return cmd;
}

int main() {
  // Known-colour frames produce the expected luma.
  auto black = mean_luma(solid(4, 4, 0, 0, 0));
  check(black && near(*black, 0.f), "black frame is luma 0");
  auto white = mean_luma(solid(4, 4, 255, 255, 255));
  check(white && near(*white, 1.f), "white frame is luma 1");
  auto grey = mean_luma(solid(4, 4, 128, 128, 128));
  check(grey && *grey > 0.15f && *grey < 0.30f,
        "mid grey frame lands in range");
  auto half = solid(2, 1, 0, 0, 0);
  half.rgba[4] = half.rgba[5] = half.rgba[6] = 255;
  auto half_white = mean_luma(half);
  check(half_white && near(*half_white, 0.5f, 1e-3f),
        "half black / half white averages to 0.5");
  check(!mean_luma(Image{}), "empty image has no luma");

  // In range consumes; out of range retries and reports the measured value.
  auto ok = run({"0.0", "1.0"}, 0.5f);
  check(ok.is_consumed() && ok.succeeded(), "in-range luma consumes");
  auto low = run({"0.2", "0.8"}, 0.0f);
  check(!low.is_consumed() && low.is_retry(), "out-of-range luma retries");
  check(low.last_mismatch.find("0.000") != std::string::npos,
        "retry reports the measured value");

  // Malformed assertions fail immediately -- they can never pass.
  check(run({"0.5"}, 0.5f).error_message.find("requires") != std::string::npos,
        "missing arg fails");
  auto inverted = run({"0.8", "0.2"}, 0.5f);
  check(inverted.is_consumed() && !inverted.succeeded(),
        "min > max fails immediately");
  auto big = run({"0.0", "2.0"}, 0.5f);
  check(big.is_consumed() && !big.succeeded(), "max > 1 fails immediately");

  // No provider fails; an unavailable readback retries.
  set_provider(LumaProvider{});
  {
    EntityCollection coll;
    Entity &e = coll.createEntity();
    PendingE2ECommand cmd;
    cmd.name = "expect_screen_luma";
    cmd.args = {"0.0", "1.0"};
    HandleExpectScreenLumaCommand handler;
    handler.for_each_with(e, cmd, 0.f);
    check(!cmd.succeeded(), "no provider fails");
  }
  auto unavailable = run({"0.0", "1.0"}, std::nullopt);
  check(unavailable.is_retry(), "unavailable readback retries");

  if (failures == 0)
    std::printf("All %d checks passed\n", checks);
  return failures == 0 ? 0 : 1;
}
