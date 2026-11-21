#define AFTERHOURS_STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "ui_test_harness.h"
#include <afterhours/src/plugins/e2e_testing/screenshot_baseline.h>
#include <stb/stb_image_write.h>

#include <unistd.h>

#include <filesystem>
#include <string>
#include <vector>

using namespace afterhours;
using namespace afterhours::testing;

static std::filesystem::path dir_for(const std::string &what) {
  auto dir = std::filesystem::temp_directory_path() /
             ("afterhours-expect-" + what + "-" + std::to_string(::getpid()));
  std::filesystem::create_directories(dir);
  return dir;
}

static std::vector<std::uint8_t> pixels(int w, int h, std::uint8_t r) {
  std::vector<std::uint8_t> p(static_cast<std::size_t>(w) * h * 4, 255);
  for (std::size_t i = 0; i < p.size(); i += 4) p[i] = r;
  return p;
}

static bool write_png(const std::filesystem::path &path,
                      const std::vector<std::uint8_t> &p, int w, int h) {
  return stbi_write_png(path.string().c_str(), w, h, 4, p.data(), w * 4) != 0;
}

static Image image_of(const std::vector<std::uint8_t> &p, int w, int h) {
  return Image{w, h, p};
}

static PendingE2ECommand run_expect(HandleExpectScreenshotCommand handler,
                                    const std::string &name) {
  EntityCollection coll;
  Entity &e = coll.createEntity();
  PendingE2ECommand cmd;
  cmd.name = "expect_screenshot";
  cmd.args = {name};
  handler.for_each_with(e, cmd, 0.f);
  return cmd;
}

static ScreenshotCaptureFn capture_of(std::vector<std::uint8_t> p, int w,
                                      int h) {
  return [p = std::move(p), w, h](const std::string &path) {
    return write_png(path, p, w, h);
  };
}

TEST(identical_images_pass) {
  auto p = pixels(4, 4, 10);
  auto d = diff_images(image_of(p, 4, 4), image_of(p, 4, 4));
  CHECK(d.size_match && d.mismatched_pixels == 0);
  CHECK(d.within({}));
}

TEST(one_channel_off_by_one_is_within_tolerance) {
  auto a = pixels(4, 4, 10);
  auto b = a;
  b[0] = 11;
  auto d = diff_images(image_of(a, 4, 4), image_of(b, 4, 4));
  CHECK(d.mismatched_pixels == 0);
  CHECK(d.max_channel_delta == 1);
  CHECK(d.within({}));
}

TEST(real_diff_fails_and_reports_counts) {
  auto dir = dir_for("diff");
  auto base = pixels(10, 10, 10);
  auto cur = base;
  cur[0] = 200;
  CHECK(write_png(dir / "menu.png", base, 10, 10));
  ScreenshotBaselineOptions opts;
  opts.baseline_dir = dir.string();
  auto cmd = run_expect(
      HandleExpectScreenshotCommand(capture_of(cur, 10, 10), opts), "menu");
  CHECK(cmd.is_consumed() && !cmd.succeeded());
  CHECK(cmd.error_message.find("1/100 pixels differ") != std::string::npos);
  std::filesystem::remove_all(dir);
}

TEST(missing_baseline_fails) {
  auto dir = dir_for("missing");
  ScreenshotBaselineOptions opts;
  opts.baseline_dir = dir.string();
  auto cmd = run_expect(HandleExpectScreenshotCommand(
                            capture_of(pixels(2, 2, 10), 2, 2), opts),
                        "nope");
  CHECK(cmd.is_consumed() && !cmd.succeeded());
  CHECK(cmd.error_message.find("missing baseline") != std::string::npos);
  CHECK(cmd.error_message.find("--update-baselines") != std::string::npos);
  std::filesystem::remove_all(dir);
}

TEST(update_mode_writes_then_check_passes) {
  auto dir = dir_for("update");
  ScreenshotBaselineOptions opts;
  opts.baseline_dir = dir.string();
  opts.update_baselines = true;
  auto p = pixels(4, 4, 42);
  auto cmd = run_expect(
      HandleExpectScreenshotCommand(capture_of(p, 4, 4), opts), "menu");
  CHECK(cmd.is_consumed() && cmd.succeeded());
  CHECK(std::filesystem::exists(dir / "menu.png"));
  opts.update_baselines = false;
  auto again = run_expect(
      HandleExpectScreenshotCommand(capture_of(p, 4, 4), opts), "menu");
  CHECK(again.is_consumed() && again.succeeded());
  std::filesystem::remove_all(dir);
}

TEST(failed_capture_fails) {
  ScreenshotBaselineOptions opts;
  opts.baseline_dir = dir_for("capfail").string();
  auto cmd = run_expect(HandleExpectScreenshotCommand(
                            [](const std::string &) { return false; }, opts),
                        "menu");
  CHECK(cmd.is_consumed() && !cmd.succeeded());
  CHECK(cmd.error_message.find("failed to capture") != std::string::npos);
}

int main() { return ui_test::run_registered_tests("E2E expect_screenshot"); }
