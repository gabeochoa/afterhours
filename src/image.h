#pragma once

#include <algorithm>
#include <climits>
#include <cmath>
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
#if defined(AFTERHOURS_STB_IMAGE_IMPLEMENTATION) && !defined(STB_IMAGE_WRITE_IMPLEMENTATION)
#define STB_IMAGE_WRITE_IMPLEMENTATION
#endif
#include <stb/stb_image.h>
#include <stb/stb_image_write.h>
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
        rgba.data() + (static_cast<std::size_t>(y) *
                           static_cast<std::size_t>(width) +
                       static_cast<std::size_t>(x)) *
                          4;
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
      static_cast<std::size_t>(size->width) *
      static_cast<std::size_t>(size->height) * 4;
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
                  pixels, pixels + static_cast<std::size_t>(w) *
                                   static_cast<std::size_t>(h) * 4)};
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

inline std::optional<Image> make_image(int width, int height, Color color,
                                       ImageLimits limits = {}) {
  const std::size_t bytes =
      static_cast<std::size_t>(std::max(0, width)) *
      static_cast<std::size_t>(std::max(0, height)) * 4;
  if (width <= 0 || height <= 0 || width > limits.max_dimension ||
      height > limits.max_dimension || bytes > limits.max_bytes) {
    log_warn("make_image: {}x{} exceeds limits ({} px, {} bytes)", width,
             height, limits.max_dimension, limits.max_bytes);
    return std::nullopt;
  }
  Image image{width, height, std::vector<std::uint8_t>(bytes)};
  for (std::size_t i = 0; i + 3 < image.rgba.size(); i += 4) {
    image.rgba[i] = color.r;
    image.rgba[i + 1] = color.g;
    image.rgba[i + 2] = color.b;
    image.rgba[i + 3] = color.a;
  }
  return image;
}

// CPU-side filled rectangle for procedural test cards and baked masks.
// Clipped to the image; partially or fully off-image rects are safe.
inline void draw_rectangle(Image &image, RectangleType rect, Color color) {
  const std::size_t expected = static_cast<std::size_t>(std::max(0, image.width)) *
                               static_cast<std::size_t>(std::max(0, image.height)) * 4;
  if (image.width <= 0 || image.height <= 0 || image.rgba.size() < expected) {
    log_warn("draw_rectangle: image is empty or truncated");
    return;
  }
  const int x0 = std::max(0, static_cast<int>(std::floor(rect.x)));
  const int y0 = std::max(0, static_cast<int>(std::floor(rect.y)));
  const int x1 = std::min(image.width,
                          static_cast<int>(std::ceil(rect.x + rect.width)));
  const int y1 = std::min(image.height,
                          static_cast<int>(std::ceil(rect.y + rect.height)));
  for (int y = y0; y < y1; ++y) {
    std::uint8_t *row = image.rgba.data() +
                        (static_cast<std::size_t>(y) *
                             static_cast<std::size_t>(image.width) +
                         static_cast<std::size_t>(x0)) * 4;
    for (int x = x0; x < x1; ++x) {
      row[0] = color.r;
      row[1] = color.g;
      row[2] = color.b;
      row[3] = color.a;
      row += 4;
    }
  }
}

inline void draw_rectangle(Image &image, int x, int y, int width, int height,
                           Color color) {
  draw_rectangle(image,
                 RectangleType{static_cast<float>(x), static_cast<float>(y),
                               static_cast<float>(width),
                               static_cast<float>(height)},
                 color);
}

inline std::optional<std::vector<std::uint8_t>>
encode_image(const Image &image) {
  if (image.width <= 0 || image.height <= 0 ||
      image.rgba.size() < static_cast<std::size_t>(image.width) *
                              static_cast<std::size_t>(image.height) * 4) {
    log_warn("encode_image: image is empty or truncated");
    return std::nullopt;
  }
  std::vector<std::uint8_t> bytes;
  // to_mem needs the impl TU; raylib's lib already ships it.
  const int ok = stbi_write_png_to_func(
      [](void *context, void *data, int size) {
        auto &out = *static_cast<std::vector<std::uint8_t> *>(context);
        const auto *p = static_cast<const std::uint8_t *>(data);
        out.insert(out.end(), p, p + size);
      },
      &bytes, image.width, image.height, 4, image.rgba.data(),
      image.width * 4);
  if (ok == 0 || bytes.empty()) {
    log_warn("encode_image: PNG encode failed");
    return std::nullopt;
  }
  return bytes;
}

inline bool export_image(const Image &image,
                         const std::filesystem::path &path) {
  auto bytes = encode_image(image);
  if (!bytes)
    return false;
  std::ofstream file(path, std::ios::binary);
  if (!file) {
    log_warn("export_image: cannot open '{}'", path.string());
    return false;
  }
  file.write(reinterpret_cast<const char *>(bytes->data()),
             static_cast<std::streamsize>(bytes->size()));
  return static_cast<bool>(file);
}

} // namespace afterhours
