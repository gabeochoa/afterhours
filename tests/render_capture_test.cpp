#define FMT_HEADER_ONLY
#ifndef AFTER_HOURS_USE_RAYLIB
#define AFTER_HOURS_USE_METAL
#endif
#include <afterhours/src/graphics.h>
#include <afterhours/src/drawing_helpers.h>
#include <cassert>
#include <cstring>

int main() {
    using namespace afterhours;
    graphics::Config config;
    config.display = graphics::DisplayMode::Headless;
    config.width = 64;
    config.height = 32;
    assert(graphics::init(config));
    auto &rt = graphics::get_render_texture();
#ifdef AFTER_HOURS_USE_METAL
    sg_pass clear{};
    clear.attachments.colors[0] = {rt.color_view_id};
    clear.attachments.depth_stencil = {rt.depth_view_id};
    clear.action.colors[0].load_action = SG_LOADACTION_CLEAR;
    clear.action.colors[0].clear_value = {1, 0, 0, 0.5f};
    sg_begin_pass(&clear);
    sg_end_pass();
    sg_commit();
#endif
    graphics::begin_frame();
#ifdef AFTER_HOURS_USE_RAYLIB
    graphics::clear_background(Color{255, 0, 0, 128});
#endif
    draw_rectangle(RectangleType{0, 16, 64, 16}, Color{0, 0, 255, 255});
    graphics::end_frame();
    auto rgba = capture_render_texture_rgba(rt);
    assert(rgba && rgba->width == 64 && rgba->height == 32);
    assert(rgba->pixels.size() == 64 * 32 * 4);
    const auto top = (4 * 64 + 4) * 4;
    const auto bottom = (24 * 64 + 4) * 4;
    assert(rgba->pixels[top] > 100 && rgba->pixels[top + 2] == 0);
    assert(rgba->pixels[top + 3] > 0 && rgba->pixels[top + 3] < 255);
    assert(rgba->pixels[bottom] == 0 && rgba->pixels[bottom + 2] == 255);
    auto png = capture_render_texture_png(rt);
    assert(png && png->bytes.size() > 8);
    assert(std::memcmp(png->bytes.data(), "\x89PNG\r\n\x1a\n", 8) == 0);
    auto legacy = capture_render_texture_to_memory(rt);
    assert(legacy.size() > 8 && std::memcmp(legacy.data(), "\x89PNG\r\n\x1a\n", 8) == 0);
#ifdef AFTER_HOURS_USE_RAYLIB
    auto decoded = raylib::LoadImageFromMemory(".png", png->bytes.data(), static_cast<int>(png->bytes.size()));
    assert(decoded.data && decoded.width == 64 && decoded.height == 32);
    raylib::ImageFormat(&decoded, raylib::PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    assert(std::memcmp(decoded.data, rgba->pixels.data(), rgba->pixels.size()) == 0);
    raylib::UnloadImage(decoded);
#else
    int width = 0, height = 0, channels = 0;
    auto decoded = stbi_load_from_memory(png->bytes.data(), static_cast<int>(png->bytes.size()),
                                        &width, &height, &channels, 4);
    assert(decoded && width == 64 && height == 32);
    assert(std::memcmp(decoded, rgba->pixels.data(), rgba->pixels.size()) == 0);
    stbi_image_free(decoded);
#endif
    assert(!capture_render_texture_rgba(graphics::RenderTextureType{}));
    assert(!capture_render_texture_png(graphics::RenderTextureType{}));
    graphics::shutdown();
}
