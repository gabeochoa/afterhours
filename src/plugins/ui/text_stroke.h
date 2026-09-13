#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <deque>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "../../drawing_helpers.h"

namespace afterhours::ui::text_stroke {

inline std::vector<unsigned char> dilate_alpha(const std::vector<unsigned char> &alpha,
                                               int width, int height, float radius) {
  if (width <= 0 || height <= 0 || alpha.size() != static_cast<size_t>(width) * height)
    return {};
  if (!(radius > 0) || !std::isfinite(radius)) return alpha;
  constexpr float far = 1e20f;
  std::vector<float> distance(alpha.size(), far);
  for (size_t i = 0; i < alpha.size(); ++i) {
    if (alpha[i] == 0) continue;
    const float edge = 1.f - static_cast<float>(alpha[i]) / 255.f;
    distance[i] = edge * edge;
  }
  const int longest = std::max(width, height);
  std::vector<float> input(longest), output(longest), limits(longest + 1);
  std::vector<int> sites(longest);
  const auto transform = [&](int count) {
    int end = 0;
    sites[0] = 0;
    limits[0] = -far;
    limits[1] = far;
    for (int q = 1; q < count; ++q) {
      float crossing = 0;
      for (;;) {
        const int previous = sites[end];
        crossing = ((input[q] + static_cast<float>(q) * q) -
                    (input[previous] + static_cast<float>(previous) * previous)) /
                   (2.f * (q - previous));
        if (crossing > limits[end] || end == 0) break;
        --end;
      }
      ++end;
      sites[end] = q;
      limits[end] = crossing;
      limits[end + 1] = far;
    }
    end = 0;
    for (int q = 0; q < count; ++q) {
      while (limits[end + 1] < q) ++end;
      const float delta = static_cast<float>(q - sites[end]);
      output[q] = delta * delta + input[sites[end]];
    }
  };
  for (int x = 0; x < width; ++x) {
    for (int y = 0; y < height; ++y) input[y] = distance[y * width + x];
    transform(height);
    for (int y = 0; y < height; ++y) distance[y * width + x] = output[y];
  }
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) input[x] = distance[y * width + x];
    transform(width);
    for (int x = 0; x < width; ++x) distance[y * width + x] = output[x];
  }
  std::vector<unsigned char> result(alpha.size());
  for (size_t i = 0; i < result.size(); ++i)
    result[i] = static_cast<unsigned char>(255.f *
        std::clamp(radius + .5f - std::sqrt(distance[i]), 0.f, 1.f));
  return result;
}

#ifdef AFTER_HOURS_USE_RAYLIB
struct OutlineFont {
  raylib::Font font{};
  std::vector<raylib::GlyphInfo> glyphs;
  std::vector<raylib::Rectangle> rectangles;
  const raylib::GlyphInfo *source = nullptr;
  unsigned int texture = 0;
  float radius = 0;
  std::string text;
  size_t bytes = 0;

  ~OutlineFont() {
    if (font.texture.id != 0 && raylib::IsWindowReady()) raylib::UnloadTexture(font.texture);
    for (auto &glyph : glyphs)
      if (glyph.image.data) raylib::UnloadImage(glyph.image);
  }
};

inline auto &cache() {
  static std::deque<std::unique_ptr<OutlineFont>> entries;
  return entries;
}

inline auto &retired() {
  static std::deque<std::unique_ptr<OutlineFont>> entries;
  return entries;
}

inline void begin_frame() { retired().clear(); }
inline void clear_cache() { cache().clear(); retired().clear(); }

inline raylib::Font outline_font(raylib::Font source, const std::string &text,
                                  float font_size, float thickness) {
  if (!source.glyphs || !source.recs || source.baseSize <= 0 || font_size <= 0)
    return {};
  const float radius = thickness * static_cast<float>(source.baseSize) / font_size;
  if (!std::isfinite(radius) || radius <= 0 || radius > 512) return {};
  auto &entries = cache();
  for (size_t i = 0; i < entries.size(); ++i) {
    auto &entry = entries[i];
    if (entry->source != source.glyphs || entry->texture != source.texture.id ||
        entry->radius != radius || entry->text != text) continue;
    auto found = std::move(entry);
    entries.erase(entries.begin() + static_cast<std::ptrdiff_t>(i));
    entries.push_back(std::move(found));
    return entries.back()->font;
  }
  auto entry = std::make_unique<OutlineFont>();
  entry->source = source.glyphs;
  entry->texture = source.texture.id;
  entry->radius = radius;
  entry->text = text;
  std::vector<int> indices;
  for (size_t i = 0; i < text.size();) {
    int consumed = 0;
    const int codepoint = raylib::GetCodepointNext(text.c_str() + i, &consumed);
    i += static_cast<size_t>(std::max(1, consumed));
    if (codepoint == '\n' || codepoint == '\r') continue;
    const int index = raylib::GetGlyphIndex(source, codepoint);
    if (std::find(indices.begin(), indices.end(), index) == indices.end()) indices.push_back(index);
  }
  if (indices.empty()) return {};
  const int padding = static_cast<int>(std::ceil(radius)) + 2;
  for (int index : indices) {
    auto glyph = source.glyphs[index];
    const auto image = glyph.image;
    const int width = image.width + 2 * padding;
    const int height = image.height + 2 * padding;
    std::vector<unsigned char> alpha(static_cast<size_t>(width) * height, 0);
    if (image.data) {
      auto *colors = raylib::LoadImageColors(image);
      if (colors) {
        for (int y = 0; y < image.height; ++y)
          for (int x = 0; x < image.width; ++x)
            alpha[(y + padding) * width + x + padding] =
                image.format == raylib::PIXELFORMAT_UNCOMPRESSED_GRAYSCALE
                    ? colors[y * image.width + x].r : colors[y * image.width + x].a;
        raylib::UnloadImageColors(colors);
      }
    }
    auto expanded = dilate_alpha(alpha, width, height, radius);
    glyph.image = raylib::GenImageColor(width, height, raylib::BLANK);
    if (!glyph.image.data) return {};
    auto *pixels = static_cast<raylib::Color *>(glyph.image.data);
    for (size_t i = 0; i < expanded.size(); ++i) pixels[i] = {255, 255, 255, expanded[i]};
    glyph.offsetX -= padding;
    glyph.offsetY -= padding;
    if (glyph.advanceX == 0) glyph.advanceX = static_cast<int>(source.recs[index].width);
    entry->glyphs.push_back(glyph);
  }
  entry->font.baseSize = source.baseSize;
  entry->font.glyphCount = static_cast<int>(entry->glyphs.size());
  entry->font.glyphPadding = 2;
  entry->font.glyphs = entry->glyphs.data();
  size_t area = 0;
  int largest = 0;
  for (const auto &glyph : entry->glyphs) {
    area += static_cast<size_t>(glyph.image.width + 4) * (glyph.image.height + 4);
    largest = std::max({largest, glyph.image.width + 4, glyph.image.height + 4});
  }
  int side = 32;
  while (side < largest || static_cast<size_t>(side) * side < area) side *= 2;
  entry->rectangles.resize(entry->glyphs.size());
  for (;;) {
    if (side > 8192) return {};
    int x = 0, y = 0, row_height = 0;
    bool fits = true;
    for (size_t i = 0; i < entry->glyphs.size(); ++i) {
      const auto &image = entry->glyphs[i].image;
      if (x + image.width + 4 > side) {
        x = 0;
        y += row_height;
        row_height = 0;
      }
      if (y + image.height + 4 > side) { fits = false; break; }
      entry->rectangles[i] = {static_cast<float>(x + 2), static_cast<float>(y + 2),
                             static_cast<float>(image.width), static_cast<float>(image.height)};
      x += image.width + 4;
      row_height = std::max(row_height, image.height + 4);
    }
    if (fits) break;
    side *= 2;
  }
  auto atlas = raylib::GenImageColor(side, side, raylib::BLANK);
  if (!atlas.data) return {};
  auto *pixels = static_cast<raylib::Color *>(atlas.data);
  for (size_t i = 0; i < entry->glyphs.size(); ++i) {
    const auto &image = entry->glyphs[i].image;
    const auto &rect = entry->rectangles[i];
    const auto *source_pixels = static_cast<const raylib::Color *>(image.data);
    for (int y = 0; y < image.height; ++y)
      std::copy_n(source_pixels + y * image.width, image.width,
          pixels + (static_cast<int>(rect.y) + y) * side + static_cast<int>(rect.x));
  }
  entry->font.recs = entry->rectangles.data();
  entry->font.texture = raylib::LoadTextureFromImage(atlas);
  entry->bytes = static_cast<size_t>(atlas.width) * atlas.height * 4;
  raylib::UnloadImage(atlas);
  for (auto &glyph : entry->glyphs) {
    raylib::UnloadImage(glyph.image);
    glyph.image = {};
  }
  if (entry->font.texture.id == 0) return {};
  raylib::SetTextureFilter(entry->font.texture, raylib::TEXTURE_FILTER_BILINEAR);
  size_t bytes = entry->bytes;
  for (const auto &existing : entries) bytes += existing->bytes;
  while (!entries.empty() && (bytes > 16 * 1024 * 1024 || entries.size() >= 32)) {
    bytes -= entries.front()->bytes;
    retired().push_back(std::move(entries.front()));
    entries.pop_front();
  }
  entries.push_back(std::move(entry));
  return entries.back()->font;
}
#elif defined(AFTER_HOURS_USE_METAL)
struct OutlineText {
  TextureType texture{};
  FONScontext *context = nullptr;
  int font = FONS_INVALID;
  float size = 0, thickness = 0, dpi = 1;
  int left = 0, top = 0;
  std::string text;
  size_t bytes = 0;

  ~OutlineText() {
    if (texture.img_id && sg_isvalid()) unload_texture(texture);
  }
};

inline auto &cache() {
  static std::deque<std::unique_ptr<OutlineText>> entries;
  return entries;
}

inline auto &retired() {
  static std::deque<std::unique_ptr<OutlineText>> entries;
  return entries;
}

inline void begin_frame() { retired().clear(); }
inline void clear_cache() { cache().clear(); retired().clear(); }

inline const OutlineText *outline_text(Font font, const std::string &text,
                                       float size, float thickness) {
  auto *context = graphics::metal_detail::g_fons_ctx;
  const int id = font.id == FONS_INVALID ? graphics::metal_detail::g_active_font : font.id;
  if (!context || id == FONS_INVALID || !(size > 0)) return nullptr;
  const float dpi = std::max(1.f, graphics::metal_detail::dpi_scale());
  auto &entries = cache();
  for (size_t i = 0; i < entries.size(); ++i) {
    auto &entry = entries[i];
    if (entry->context != context || entry->font != id || entry->size != size ||
        entry->thickness != thickness || entry->dpi != dpi || entry->text != text) continue;
    auto found = std::move(entry);
    entries.erase(entries.begin() + static_cast<std::ptrdiff_t>(i));
    entries.push_back(std::move(found));
    return entries.back().get();
  }
  struct Glyph {
    int x, y, width, height;
    std::vector<unsigned char> alpha;
  };
  std::vector<Glyph> glyphs;
  int left = 0, top = 0, right = 0, bottom = 0;
  fonsPushState(context);
  fonsSetFont(context, id);
  fonsSetAlign(context, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);
  fonsSetSize(context, size * dpi);
  FONStextIter iter{};
  if (!fonsTextIterInit(context, &iter, 0, 0, text.c_str(), nullptr)) {
    fonsPopState(context);
    return nullptr;
  }
  FONSquad quad{};
  bool complete = true;
  while (fonsTextIterNext(context, &iter, &quad)) {
    if (iter.prevGlyphIndex < 0) { complete = false; break; }
    int atlas_width = 0, atlas_height = 0;
    const auto *atlas = fonsGetTextureData(context, &atlas_width, &atlas_height);
    const int sx = static_cast<int>(std::lround(quad.s0 * atlas_width));
    const int sy = static_cast<int>(std::lround(quad.t0 * atlas_height));
    const int width = static_cast<int>(std::lround(quad.x1 - quad.x0));
    const int height = static_cast<int>(std::lround(quad.y1 - quad.y0));
    if (!atlas || width <= 0 || height <= 0) continue;
    if (sx < 0 || sy < 0 || sx + width > atlas_width || sy + height > atlas_height) {
      complete = false;
      break;
    }
    Glyph glyph{static_cast<int>(std::lround(quad.x0)), static_cast<int>(std::lround(quad.y0)),
                width, height, std::vector<unsigned char>(static_cast<size_t>(width) * height)};
    for (int y = 0; y < height; ++y)
      std::copy_n(atlas + (sy + y) * atlas_width + sx, width, glyph.alpha.data() + y * width);
    left = std::min(left, glyph.x);
    top = std::min(top, glyph.y);
    right = std::max(right, glyph.x + width);
    bottom = std::max(bottom, glyph.y + height);
    glyphs.push_back(std::move(glyph));
  }
  fonsPopState(context);
  if (!complete || glyphs.empty()) return nullptr;
  const float radius = thickness * dpi;
  if (!std::isfinite(radius) || radius > 512) return nullptr;
  const int padding = static_cast<int>(std::ceil(radius)) + 2;
  left -= padding;
  top -= padding;
  const int width = right - left + padding;
  const int height = bottom - top + padding;
  if (width <= 0 || height <= 0 || static_cast<size_t>(width) * height > 4 * 1024 * 1024)
    return nullptr;
  std::vector<unsigned char> alpha(static_cast<size_t>(width) * height, 0);
  for (const auto &glyph : glyphs)
    for (int y = 0; y < glyph.height; ++y)
      for (int x = 0; x < glyph.width; ++x) {
        auto &target = alpha[(glyph.y - top + y) * width + glyph.x - left + x];
        target = std::max(target, glyph.alpha[y * glyph.width + x]);
      }
  auto expanded = dilate_alpha(alpha, width, height, radius);
  std::vector<unsigned char> rgba(expanded.size() * 4, 255);
  for (size_t i = 0; i < expanded.size(); ++i) rgba[i * 4 + 3] = expanded[i];
  auto entry = std::make_unique<OutlineText>();
  entry->texture = metal_texture_detail::load_texture_from_pixels(rgba.data(), width, height);
  if (!entry->texture.img_id) return nullptr;
  entry->context = context;
  entry->font = id;
  entry->size = size;
  entry->thickness = thickness;
  entry->dpi = dpi;
  entry->left = left;
  entry->top = top;
  entry->text = text;
  entry->bytes = rgba.size();
  size_t bytes = entry->bytes;
  for (const auto &existing : entries) bytes += existing->bytes;
  while (!entries.empty() && (bytes > 16 * 1024 * 1024 || entries.size() >= 32)) {
    bytes -= entries.front()->bytes;
    retired().push_back(std::move(entries.front()));
    entries.pop_front();
  }
  entries.push_back(std::move(entry));
  return entries.back().get();
}
#else
inline void begin_frame() {}
inline void clear_cache() {}
#endif

inline void draw(Font font, const std::string &text, Vector2Type position,
                 float size, float spacing, float thickness, Color color,
                 float rotation, float center_x, float center_y) {
  if (!(thickness > 0) || !std::isfinite(thickness) || !(size > 0) ||
      !std::isfinite(size) || text.empty()) return;
#ifdef AFTER_HOURS_USE_RAYLIB
  const auto outline = outline_font(font, text, size, thickness);
  if (outline.texture.id == 0) return;
  draw_text_ex(outline, text.c_str(), position, size, spacing, color,
               rotation, center_x, center_y);
#elif defined(AFTER_HOURS_USE_METAL)
  (void)spacing;
  (void)rotation;
  (void)center_x;
  (void)center_y;
  const auto *outline = outline_text(font, text, size, thickness);
  if (!outline) return;
  const auto &texture = outline->texture;
  draw_texture_pro(texture, {0, 0, texture.width, texture.height},
      {position.x + static_cast<float>(outline->left) / outline->dpi,
       position.y + static_cast<float>(outline->top) / outline->dpi,
       texture.width / outline->dpi, texture.height / outline->dpi}, {0, 0}, 0, color);
#else
  draw_text_ex(font, text.c_str(), position, size, spacing, color,
               rotation, center_x, center_y);

#endif
}

}
