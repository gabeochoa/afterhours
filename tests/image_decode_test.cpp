// image_decode_test.cpp
// image_decode::decode: owned RGBA8 output, header limits checked before
// allocation, structured errors, shared budget, and cancellation releasing
// memory.

#include <afterhours/src/plugins/image_decode.h>

#include <atomic>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

using namespace afterhours::image_decode;

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

using Bytes = std::vector<unsigned char>;

// Uncompressed 32-bit TGA, top-left origin; rgba is row-major from the top.
static Bytes tga(int width, int height, const Bytes &rgba) {
  Bytes out(18, 0);
  out[2] = 2;
  out[12] = static_cast<unsigned char>(width);
  out[13] = static_cast<unsigned char>(width >> 8);
  out[14] = static_cast<unsigned char>(height);
  out[15] = static_cast<unsigned char>(height >> 8);
  out[16] = 32;
  out[17] = 0x28; // 8 alpha bits, top-left origin
  for (size_t i = 0; i < rgba.size(); i += 4)
    out.insert(out.end(), {rgba[i + 2], rgba[i + 1], rgba[i], rgba[i + 3]});
  return out;
}

static Bytes ppm(int width, int height, unsigned char value = 0x7f) {
  const auto header = "P6\n" + std::to_string(width) + " " + std::to_string(height) + "\n255\n";
  Bytes out(header.begin(), header.end());
  out.resize(out.size() + static_cast<size_t>(width) * height * 3, value);
  return out;
}

static bool is(const tl::expected<DecodedImage, Error> &result, ErrorCode code) {
  return !result && result.error().code == code && !result.error().message.empty();
}

static void decodes_owned_rgba_rows_top_down() {
  const Bytes rgba = {255, 0, 0, 255,   0, 255, 0, 128,   0, 0, 255, 0,
                      10, 20, 30, 40,   50, 60, 70, 80,   90, 100, 110, 120};
  auto image = decode(tga(3, 2, rgba));
  check(image.has_value(), "valid TGA decodes");
  if (!image) return;
  check(image->width() == 3 && image->height() == 2, "dimensions reported");
  check(image->size_bytes() == 24, "size is width * height * 4");
  check(Bytes(image->pixels(), image->pixels() + 24) == rgba,
        "pixels are RGBA8, straight alpha, top row first");
  auto rgb = decode(ppm(2, 2, 9));
  check(rgb && rgb->pixels()[0] == 9 && rgb->pixels()[3] == 255, "RGB input expands to opaque RGBA");
}

static void limits_are_checked_before_allocation() {
  Budget budget(1 << 20);
  check(is(decode(Bytes{}, {}, {}, &budget), ErrorCode::InvalidImage), "empty input is invalid");
  check(is(decode(Bytes{'n', 'o', 'p', 'e', 0, 1, 2, 3}, {}, {}, &budget), ErrorCode::InvalidImage),
        "unknown bytes are invalid");
  const auto image = ppm(64, 32);
  check(is(decode(image, {.max_encoded_bytes = image.size() - 1}, {}, &budget), ErrorCode::EncodedTooLarge),
        "encoded size limit");
  check(is(decode(image, {.max_width = 63}, {}, &budget), ErrorCode::DimensionsTooLarge), "width limit");
  check(is(decode(image, {.max_height = 31}, {}, &budget), ErrorCode::DimensionsTooLarge), "height limit");
  check(is(decode(image, {.max_decoded_bytes = 64 * 32 * 4 - 1}, {}, &budget), ErrorCode::DecodedTooLarge),
        "decoded byte limit");
  check(decode(image, {.max_width = 64, .max_height = 32, .max_decoded_bytes = 64 * 32 * 4}).has_value(),
        "limits are inclusive");
  // A declared-huge image is refused from its header alone.
  Bytes huge = {'P', '6', '\n', '1', '0', '0', '0', '0', '0', ' ', '1', '0', '0', '0', '0', '0', '\n', '2', '5', '5', '\n'};
  check(is(decode(huge, {}, {}, &budget), ErrorCode::DimensionsTooLarge),
        "100000x100000 header rejected without decoding");
  auto truncated = image;
  truncated.resize(truncated.size() / 2);
  check(is(decode(truncated, {}, {}, &budget), ErrorCode::DecodeFailed), "truncated pixel data fails");
  check(budget.reserved() == 0, "no failure leaves a reservation behind");
}

static void shared_budget() {
  Budget budget(2 * 16 * 16 * 4);
  auto a = decode(ppm(16, 16), {}, {}, &budget);
  auto b = decode(ppm(16, 16), {}, {}, &budget);
  check(a && b && budget.reserved() == budget.limit(), "two images fill the budget");
  check(is(decode(ppm(1, 1), {}, {}, &budget), ErrorCode::BudgetExhausted), "a third is refused");
  DecodedImage moved = std::move(*a);
  check(budget.reserved() == budget.limit(), "moving an image does not double-release");
  { DecodedImage gone = std::move(moved); }
  check(budget.reserved() == 16 * 16 * 4, "destroying an image returns its bytes");
  check(decode(ppm(16, 16), {}, {}, &budget).has_value(), "returned bytes are reusable");
  check(budget.reserved() == 16 * 16 * 4, "the temporary result released on scope exit");
}

static void cancellation_releases_memory() {
  Budget budget(std::size_t{1} << 30);
  std::stop_source cancelled;
  cancelled.request_stop();
  check(is(decode(ppm(8, 8), {}, cancelled.get_token(), &budget), ErrorCode::Cancelled),
        "cancelled before decoding");
  check(budget.reserved() == 0, "nothing reserved for an early cancel");

  // Cancel while stb is decoding: the reservation is taken just before the
  // pixel decode, so stop as soon as it appears. Scheduling can let a decode
  // finish first, so retry; every attempt must leave the budget empty.
  const auto big = ppm(4096, 2048);
  bool saw_late_cancel = false;
  bool always_released = true;
  for (int attempt = 0; attempt < 20 && !saw_late_cancel; ++attempt) {
    std::stop_source stop;
    std::atomic<bool> done{false};
    std::thread canceller([&] {
      while (!done && budget.reserved() == 0) std::this_thread::yield();
      stop.request_stop();
    });
    {
      auto result = decode(big, {}, stop.get_token(), &budget);
      done = true;
      canceller.join();
      if (is(result, ErrorCode::Cancelled)) {
        saw_late_cancel = true;
        always_released = always_released && budget.reserved() == 0;
      }
    }
    always_released = always_released && budget.reserved() == 0;
  }
  check(saw_late_cancel, "a cancel during decode reports Cancelled");
  check(always_released, "late cancellation frees the decoded pixels and reservation");
}

int main() {
  printf("=== image decode tests ===\n\n");
  decodes_owned_rgba_rows_top_down();
  limits_are_checked_before_allocation();
  shared_budget();
  cancellation_releases_memory();
  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
