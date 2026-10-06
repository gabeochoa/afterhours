#pragma once

// CPU image decoding with limits, safe to call from worker threads. Compile
// image_decode.cpp. Upload the result on the render thread with
// load_texture_from_rgba (drawing_helpers.h). See docs/plugins.md
// "Image decoding and texture upload".

#include <atomic>
#include <cstddef>
#include <memory>
#include <span>
#include <stop_token>
#include <string>

#include "../../expected.hpp"

namespace afterhours::image_decode {

struct Limits {
  std::size_t max_encoded_bytes = std::size_t{64} << 20;
  int max_width = 16384;
  int max_height = 16384;
  // width * height * 4 for the RGBA8 output.
  std::size_t max_decoded_bytes = std::size_t{256} << 20;
};

// Decoded bytes shared by several decoders, e.g. all thumbnail workers. Each
// DecodedImage reserves its pixel bytes before decoding and returns them when
// destroyed. Decoder scratch memory and GPU textures are not counted. Must
// outlive every image decoded against it.
class Budget {
public:
  explicit Budget(std::size_t limit) : limit_(limit) {}
  Budget(const Budget &) = delete;
  Budget &operator=(const Budget &) = delete;
  std::size_t reserved() const { return reserved_.load(); }
  std::size_t limit() const { return limit_; }

private:
  friend struct PixelRelease;
  friend struct Reservation;
  std::atomic<std::size_t> reserved_{0};
  std::size_t limit_;
};

struct PixelRelease {
  Budget *budget = nullptr;
  std::size_t bytes = 0;
  void operator()(unsigned char *pixels) const;
};

// Owned pixels: RGBA8, straight (unpremultiplied) alpha, rows top to bottom,
// tightly packed (stride = width * 4). Move-only.
class DecodedImage {
public:
  int width() const { return width_; }
  int height() const { return height_; }
  const unsigned char *pixels() const { return pixels_.get(); }
  std::size_t size_bytes() const { return pixels_.get_deleter().bytes; }

private:
  friend struct Decoder;
  std::unique_ptr<unsigned char, PixelRelease> pixels_;
  int width_ = 0;
  int height_ = 0;
};

enum class ErrorCode {
  Cancelled,
  EncodedTooLarge,
  InvalidImage,       // unrecognised format or unreadable header
  DimensionsTooLarge,
  DecodedTooLarge,
  BudgetExhausted,
  DecodeFailed,       // header was fine, pixel data was not
};

struct Error {
  ErrorCode code;
  std::string message;
};

// Formats: whatever the bundled stb_image accepts (PNG, JPEG, BMP, TGA, GIF
// first frame, PSD, HDR, PIC, PNM). Limits are checked from the header
// before any pixel allocation. Cancellation is checked before decoding and
// after it; stb cannot be interrupted mid-decode. Every failure, including a
// late cancellation, frees the pixels and returns the budget reservation.
tl::expected<DecodedImage, Error> decode(std::span<const unsigned char> encoded,
                                         const Limits &limits = {},
                                         std::stop_token stop = {},
                                         Budget *budget = nullptr);

} // namespace afterhours::image_decode
