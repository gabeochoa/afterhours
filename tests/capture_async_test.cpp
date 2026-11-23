
#define AFTERHOURS_STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#include <afterhours/src/capture_async.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>

static int checks = 0;
static int failures = 0;
static void check(bool cond, const char *what) {
  checks++;
  if (!cond) {
    failures++;
    std::printf("  FAIL: %s\n", what);
  }
}

using namespace afterhours;

static RGBACapture make_capture(int w, int h, bool noise = false) {
  RGBACapture c;
  c.width = w;
  c.height = h;
  c.pixels.resize(static_cast<std::size_t>(w) * h * 4);
  if (noise) {
    std::mt19937 rng(42);
    for (auto &b : c.pixels) b = static_cast<std::uint8_t>(rng());
  } else {
    for (int y = 0; y < h; y++)
      for (int x = 0; x < w; x++) {
        auto *p = &c.pixels[(static_cast<std::size_t>(y) * w + x) * 4];
        p[0] = static_cast<std::uint8_t>(x * 255 / (w - 1));
        p[1] = static_cast<std::uint8_t>(y * 255 / (h - 1));
        p[2] = 128;
        p[3] = 255;
      }
  }
  return c;
}

static std::vector<std::uint8_t> read_file(const std::filesystem::path &path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), {}};
}

int main() {
  const auto dir = std::filesystem::temp_directory_path() / "ah_capture_async";
  std::filesystem::create_directories(dir);

  {
    auto capture = make_capture(16, 8);
    auto png = capture_async::encode_png(capture);
    check(png.has_value(), "encode_png succeeds");
    check(png && png->bytes.size() > 8 &&
              std::memcmp(png->bytes.data(), "\x89PNG\r\n\x1a\n", 8) == 0,
          "encode_png emits PNG magic");
    int w = 0, h = 0, comp = 0;
    auto *decoded = stbi_load_from_memory(
        png->bytes.data(), static_cast<int>(png->bytes.size()), &w, &h, &comp, 4);
    check(decoded && w == 16 && h == 8, "encoded PNG decodes at same size");
    check(decoded &&
              std::memcmp(decoded, capture.pixels.data(),
                          capture.pixels.size()) == 0,
          "encoded PNG round-trips pixels");
    stbi_image_free(decoded);
  }

  {
    auto capture = make_capture(32, 24);
    auto expected = capture_async::encode_png(capture);
    const auto path = dir / "async.png";
    auto job = capture_async::save_async(capture, path);
    check(job.valid(), "save_async returns a valid job");
    check(job.wait(), "async save reports success");
    check(read_file(path) == expected->bytes,
          "async save writes correct PNG bytes");
  }

  {
    auto job = capture_async::save_async(make_capture(1024, 1024, true),
                                         dir / "big.png");
    check(!job.done(), "job not done immediately after launch (ordering)");
    check(job.wait(), "big async save completes");
  }

  {
    auto capture = make_capture(8, 8);
    auto fut = capture_async::encode_async(capture);
    auto png = fut.get();
    check(png && png->bytes == capture_async::encode_png(capture)->bytes,
          "encode_async matches sync encode");
  }

  {
    RGBACapture bad;
    bad.width = 4;
    bad.height = 4;
    check(!capture_async::encode_png(bad), "encode_png rejects bad capture");
    const auto path = dir / "bad.png";
    std::filesystem::remove(path);
    auto job = capture_async::save_async(bad, path);
    check(!job.wait(), "async save of bad capture fails");
    check(!std::filesystem::exists(path), "failed save writes no file");

    auto job2 = capture_async::save_async(make_capture(8, 8),
                                          dir / "no_such_dir" / "x.png");
    check(!job2.wait(), "async save to missing directory fails");
  }

  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
  std::printf("%d/%d checks passed\n", checks - failures, checks);
  if (failures == 0) std::printf("All checks passed!\n");
  return failures == 0 ? 0 : 1;
}
