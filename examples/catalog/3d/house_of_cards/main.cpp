// house_of_cards — a model and its impostor cards, side by side, rotating.
//
// Left: the real model. Right: its house_of_cards impostor, based on Thomas R
// Murphy's Imposter Cards (links in src/plugins/house_of_cards.h). One
// orbiting camera drives both panes. Cards are baked when a model is first
// shown.
//
//   LEFT / RIGHT  previous / next model
//   UP / DOWN     camera closer / further
//   SPACE         pause the rotation
//   P             depth parallax on / off

#include <algorithm>
#include <cmath>
#include <format>
#include <string>

#include "../../../../ah.h"
#include "../../../../src/core/key_codes.h"
#include "../../../../src/drawing_helpers.h"
#include "../../../../src/drawing_helpers_3d.h"
#include "../../../../src/graphics.h"
#include "../../../../src/plugins/house_of_cards.h"
#include "../../../../src/plugins/models.h"

using namespace afterhours;
namespace hoc = afterhours::house_of_cards;

struct Entry {
    const char *file;
    hoc::Cards cards{};
    bool ready = false;
};

// Loads the model into ModelLibrary under its file name and bakes its cards.
static bool prepare(Entry &e) {
    if (e.ready) return true;
    auto &models = ModelLibrary::get();
    if (!models.load((std::string(MODELS_DIR) + "/" + e.file).c_str(), e.file))
        return false;
    hoc::Baked baked = hoc::bake(models.get(e.file));
    e.cards = hoc::upload(baked);
    hoc::unload(baked);
    e.ready = true;
    return true;
}

int main(int, char **) {
    graphics::set_config_flags(graphics::FLAG_WINDOW_RESIZABLE);
    graphics::init_window(1200, 640, "house_of_cards");
    graphics::set_target_fps(60);

    Entry entries[] = {{"tree-large.glb"},
                       {"kaykit/Tree_3_C_Color1.gltf"},
                       {"kaykit/arch_gate.gltf"},
                       {"kaykit/lantern.gltf"},
                       {"kaykit/drafting_compass.gltf"}};
    constexpr int N = sizeof(entries) / sizeof(entries[0]);
    int current = 0;
    if (!prepare(entries[current])) {
        graphics::close_window();
        return 1;
    }

    ShaderType shader = hoc::load_shader();
    const MatrixType identity = {1, 0, 0, 0, 0, 1, 0, 0,
                                 0, 0, 1, 0, 0, 0, 0, 1};
    const Color sky{135, 190, 230, 255}, backdrop{30, 30, 30, 255};
    const char *labels[2] = {"real model", "cards"};
    graphics::RenderTextureType panes[2] = {};
    int pane_w = 0, pane_h = 0;
    float yaw = 0.f, distance = 3.f;
    bool paused = false, parallax = true;

    while (!graphics::window_should_close()) {
        float dt = graphics::get_frame_time();
        bool next = graphics::is_key_pressed(keys::RIGHT);
        if (next || graphics::is_key_pressed(keys::LEFT)) {
            int i = (current + (next ? 1 : N - 1)) % N;
            if (prepare(entries[i])) current = i;
        }
        if (graphics::is_key_down(keys::UP)) distance -= 2.f * dt;
        if (graphics::is_key_down(keys::DOWN)) distance += 2.f * dt;
        distance = std::clamp(distance, 1.5f, 24.f);
        if (graphics::is_key_pressed(keys::SPACE)) paused = !paused;
        if (graphics::is_key_pressed(keys::P)) parallax = !parallax;
        if (!paused) yaw += 30.f * dt;

        // Panes follow the window size.
        int w = graphics::get_screen_width() / 2;
        int h = graphics::get_screen_height() - 40;
        if (w != pane_w || h != pane_h) {
            for (auto &p : panes) {
                if (pane_w > 0) unload_render_texture(p);
                p = load_render_texture(w, h);
            }
            pane_w = w, pane_h = h;
        }

        Entry &e = entries[current];
        const ModelType &model = ModelLibrary::get().get(e.file);
        const hoc::Meta &m = e.cards.meta;
        float el = 20.f * DEG2RAD, yr = yaw * DEG2RAD;
        float dist = m.radius * distance;
        Camera3D cam{};
        cam.position = {m.center.x + dist * std::cos(el) * std::cos(yr),
                        m.center.y + dist * std::sin(el),
                        m.center.z + dist * std::cos(el) * std::sin(yr)};
        cam.target = m.center;
        cam.up = {0.f, 1.f, 0.f};
        cam.fovy = 45.f;
        cam.projection = camera::PERSPECTIVE;
        int steps = parallax ? 2 : 0;

        for (int i = 0; i < 2; i++) {
            begin_texture_mode(panes[i]);
            graphics::clear_background(sky);
            begin_3d(cam);
            if (i == 0) draw_model(model, {0, 0, 0}, 0.f, {1, 1, 1});
            else hoc::draw_instanced(e.cards, shader, &identity, 1, cam, steps);
            end_3d();
            end_texture_mode();
        }

        graphics::begin_drawing();
        graphics::clear_background(backdrop);
        for (int i = 0; i < 2; i++) {
            float x = static_cast<float>(i * pane_w);
            // Negative source height: render textures are bottom-up.
            draw_texture_rec(panes[i].texture,
                             RectangleType{0, 0, static_cast<float>(pane_w),
                                           -static_cast<float>(pane_h)},
                             Vector2Type{x, 0}, Color{255, 255, 255, 255});
            draw_text(labels[i], x + 12, 10, 24, Color{0, 0, 0, 255});
            if (i > 0)
                draw_line(static_cast<int>(x), 0, static_cast<int>(x), pane_h,
                          backdrop);
        }
        draw_text(std::format("{}   distance {:.1f} radii   parallax {}   "
                              "[LEFT/RIGHT model  UP/DOWN zoom  SPACE pause  "
                              "P parallax]",
                              e.file, distance, parallax ? "on" : "off")
                      .c_str(),
                  12, static_cast<float>(pane_h + 10), 20,
                  Color{245, 245, 245, 255});
        graphics::end_drawing();
    }

    for (auto &p : panes) unload_render_texture(p);
    for (Entry &e : entries)
        if (e.ready) hoc::unload(e.cards);
    ModelLibrary::get().unload_all();
    graphics::unload_shader(shader);
    graphics::close_window();
    return 0;
}
