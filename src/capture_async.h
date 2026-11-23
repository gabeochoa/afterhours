#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <future>
#include <optional>
#include <utility>
#include <vector>

#include "render_capture.h"

#if defined(AFTERHOURS_STB_IMAGE_WRITE_IMPLEMENTATION) && \
    !defined(STB_IMAGE_WRITE_IMPLEMENTATION)
#define STB_IMAGE_WRITE_IMPLEMENTATION
#endif
#include <stb/stb_image_write.h>

namespace afterhours::capture_async {


inline std::optional<PNGCapture> encode_png(const RGBACapture &capture) {
  if (capture.width <= 0 || capture.height <= 0)
    return std::nullopt;
  const auto expected = static_cast<std::size_t>(capture.width) *
                        static_cast<std::size_t>(capture.height) * 4;
  if (capture.pixels.size() != expected)
    return std::nullopt;
  PNGCapture out;
  auto append = [](void *ctx, void *data, int size) {
    auto *bytes = static_cast<std::vector<std::uint8_t> *>(ctx);
    const auto *p = static_cast<const std::uint8_t *>(data);
    bytes->insert(bytes->end(), p, p + size);
  };
  if (!stbi_write_png_to_func(append, &out.bytes, capture.width,
                              capture.height, 4, capture.pixels.data(),
                              capture.width * 4))
    return std::nullopt;
  if (out.bytes.empty())
    return std::nullopt;
  return out;
}

inline bool save_png(const RGBACapture &capture,
                     const std::filesystem::path &path) {
  auto png = encode_png(capture);
  if (!png || path.empty())
    return false;
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out)
    return false;
  out.write(reinterpret_cast<const char *>(png->bytes.data()),
            static_cast<std::streamsize>(png->bytes.size()));
  return static_cast<bool>(out);
}

struct SaveJob {
  std::future<bool> future;
  bool valid() const { return future.valid(); }
  bool done() {
    return future.valid() &&
           future.wait_for(std::chrono::seconds(0)) ==
               std::future_status::ready;
  }

  bool wait() { return future.valid() ? future.get() : false; }
};

inline SaveJob save_async(RGBACapture capture, std::filesystem::path path) {
  SaveJob job;
  job.future = std::async(std::launch::async,
                          [capture = std::move(capture),
                           path = std::move(path)]() mutable {
                            return save_png(capture, path);
                          });
  return job;
}

inline std::future<std::optional<PNGCapture>>
encode_async(RGBACapture capture) {
  return std::async(std::launch::async, [capture = std::move(capture)]() {
    return encode_png(capture);
  });
}

} // namespace afterhours::capture_async
