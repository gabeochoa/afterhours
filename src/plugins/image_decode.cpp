#include "image_decode.h"

#include <climits>
#include <cstdlib>

// A private, internal-linkage copy of stb_image: no symbol clash with the
// sokol backend's copy or any the application links.
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include <stb/stb_image.h>
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace afterhours::image_decode {

struct Reservation {
  static bool take(Budget *budget, std::size_t bytes) {
    if (!budget) return true;
    auto reserved = budget->reserved_.load();
    do {
      if (bytes > budget->limit_ || reserved > budget->limit_ - bytes) return false;
    } while (!budget->reserved_.compare_exchange_weak(reserved, reserved + bytes));
    return true;
  }
  static void give_back(Budget *budget, std::size_t bytes) {
    if (budget) budget->reserved_.fetch_sub(bytes);
  }
};

void PixelRelease::operator()(unsigned char *pixels) const {
  stbi_image_free(pixels);
  Reservation::give_back(budget, bytes);
}

struct Decoder {
  static DecodedImage make(unsigned char *pixels, int width, int height,
                           Budget *budget, std::size_t bytes) {
    DecodedImage image;
    image.pixels_ = {pixels, PixelRelease{budget, bytes}};
    image.width_ = width;
    image.height_ = height;
    return image;
  }
};

static tl::unexpected<Error> fail(ErrorCode code, std::string message) {
  return tl::unexpected(Error{code, std::move(message)});
}

tl::expected<DecodedImage, Error> decode(std::span<const unsigned char> encoded,
                                         const Limits &limits,
                                         std::stop_token stop, Budget *budget) {
  if (stop.stop_requested()) return fail(ErrorCode::Cancelled, "Image decode cancelled");
  if (encoded.size() > limits.max_encoded_bytes || encoded.size() > INT_MAX)
    return fail(ErrorCode::EncodedTooLarge, "Encoded image exceeds the size limit");
  const auto length = static_cast<int>(encoded.size());
  int width = 0, height = 0, channels = 0;
  if (!stbi_info_from_memory(encoded.data(), length, &width, &height, &channels) ||
      width <= 0 || height <= 0)
    return fail(ErrorCode::InvalidImage, "Unsupported or invalid image");
  if (width > limits.max_width || height > limits.max_height)
    return fail(ErrorCode::DimensionsTooLarge, "Image dimensions exceed the limit");
  const std::size_t bytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4;
  if (bytes > limits.max_decoded_bytes)
    return fail(ErrorCode::DecodedTooLarge, "Decoded image exceeds the memory limit");
  if (!Reservation::take(budget, bytes))
    return fail(ErrorCode::BudgetExhausted, "Shared decode budget exhausted");
  int decoded_width = 0, decoded_height = 0;
  unsigned char *pixels = stbi_load_from_memory(encoded.data(), length, &decoded_width,
                                                &decoded_height, &channels, 4);
  if (!pixels || decoded_width != width || decoded_height != height) {
    if (pixels) stbi_image_free(pixels);
    Reservation::give_back(budget, bytes);
    return fail(ErrorCode::DecodeFailed,
                std::string("Unable to decode image: ") +
                    (stbi_failure_reason() ? stbi_failure_reason() : "size mismatch"));
  }
  auto image = Decoder::make(pixels, width, height, budget, bytes);
  if (stop.stop_requested()) return fail(ErrorCode::Cancelled, "Image decode cancelled");
  return image;
}

} // namespace afterhours::image_decode
