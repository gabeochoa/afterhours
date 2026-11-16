#pragma once

#include <climits>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <span>
#include <vector>

#include "drawing_helpers.h"
#include "logging.h"

#ifndef AFTER_HOURS_SOKOL
#if defined(AFTERHOURS_STB_IMAGE_IMPLEMENTATION) && !defined(STB_IMAGE_IMPLEMENTATION)
#define STB_IMAGE_IMPLEMENTATION
#endif
#include <stb/stb_image.h>
#endif

namespace afterhours {

struct ImageSize {
  int width = 0;
  int height = 0;
};

struct ImageLimits {
  int max_dimension = 16384;
  std::size_t max_bytes = std::size_t{256} << 20;
};

struct Image {
  int width = 0;
  int height = 0;
  std::vector<std::uint8_t> rgba;

  Color pixel(int x, int y) const {
    if (x < 0 || y < 0 || x >= width || y >= height) {
      log_error("Image::pixel({}, {}) is outside {}x{}", x, y, width, height);
      std::abort();
    }
    const std::uint8_t *p =
        rgba.data() + (static_cast<std::size_t>(y) * width + x) * 4;
    return Color{p[0], p[1], p[2], p[3]};
  }
};

inline std::optional<ImageSize>
image_size(std::span<const std::uint8_t> encoded) {
  int w = 0, h = 0, comp = 0;
  if (encoded.size() > INT_MAX ||
      !stbi_info_from_memory(encoded.data(), static_cast<int>(encoded.size()),
                             &w, &h, &comp)) {
    log_warn("image_size: unreadable image header");
    return std::nullopt;
  }
  return ImageSize{w, h};
}

inline std::optional<Image> decode_image(std::span<const std::uint8_t> encoded,
                                         ImageLimits limits = {}) {
  auto size = image_size(encoded);
  if (!size)
    return std::nullopt;
  const std::size_t bytes =
      static_cast<std::size_t>(size->width) * size->height * 4;
  if (size->width > limits.max_dimension ||
      size->height > limits.max_dimension || bytes > limits.max_bytes) {
    log_warn("decode_image: {}x{} exceeds limits ({} px, {} bytes)",
             size->width, size->height, limits.max_dimension,
             limits.max_bytes);
    return std::nullopt;
  }
  int w = 0, h = 0, comp = 0;
  stbi_uc *pixels =
      stbi_load_from_memory(encoded.data(), static_cast<int>(encoded.size()),
                            &w, &h, &comp, 4);
  if (pixels == nullptr) {
    log_warn("decode_image: {}", stbi_failure_reason());
    return std::nullopt;
  }
  Image image{w, h,
              std::vector<std::uint8_t>(
                  pixels, pixels + static_cast<std::size_t>(w) * h * 4)};
  stbi_image_free(pixels);
  return image;
}

inline std::optional<ImageSize> image_size(const std::filesystem::path &path) {
  int w = 0, h = 0, comp = 0;
  if (!stbi_info(path.string().c_str(), &w, &h, &comp)) {
    log_warn("image_size: cannot read '{}'", path.string());
    return std::nullopt;
  }
  return ImageSize{w, h};
}

inline std::optional<Image> load_image(const std::filesystem::path &path,
                                       ImageLimits limits = {}) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    log_warn("load_image: cannot open '{}'", path.string());
    return std::nullopt;
  }
  const std::vector<std::uint8_t> bytes(std::istreambuf_iterator<char>(file),
                                        {});
  return decode_image(bytes, limits);
}

} // namespace afterhours
