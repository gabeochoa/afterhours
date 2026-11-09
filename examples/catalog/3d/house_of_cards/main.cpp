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

#include <cmath>

namespace raylib {
#include <raylib.h>
}  // namespace raylib

#include "../../../../ah.h"
#include "../../../../src/plugins/house_of_cards.h"

using namespace afterhours;
namespace hoc = afterhours::house_of_cards;

struct Entry {
    const char *file;
    raylib::Model model{};
    hoc::Cards cards{};
    bool ready = false;
};

static bool prepare(Entry &e) {
    if (e.ready) return true;
    e.model = raylib::LoadModel((std::string(MODELS_DIR) + "/" + e.file).c_str());
    if (e.model.meshCount == 0) return false;
    hoc::Baked baked = hoc::bake(e.model);
    e.cards = hoc::upload(baked);
    hoc::unload(baked);
    e.ready = true;
    return true;
}

int main(int, char **) {
    raylib::SetConfigFlags(raylib::FLAG_WINDOW_RESIZABLE);
    raylib::InitWindow(1200, 640, "house_of_cards");
    raylib::SetTargetFPS(60);

    Entry entries[] = {{"tree-large.glb"},
                       {"kaykit/Tree_3_C_Color1.gltf"},
                       {"kaykit/arch_gate.gltf"},
                       {"kaykit/lantern.gltf"},
                       {"kaykit/drafting_compass.gltf"}};
    constexpr int N = sizeof(entries) / sizeof(entries[0]);
    int current = 0;
    if (!prepare(entries[current])) {
        raylib::CloseWindow();
        return 1;
    }

    raylib::Shader shader = hoc::load_shader();
    const raylib::Matrix identity = {1, 0, 0, 0, 0, 1, 0, 0,
                                     0, 0, 1, 0, 0, 0, 0, 1};
    const char *labels[2] = {"real model", "cards"};
    raylib::RenderTexture2D panes[2] = {};
    int pane_w = 0, pane_h = 0;
    float yaw = 0.f, distance = 3.f;
    bool paused = false, parallax = true;

    while (!raylib::WindowShouldClose()) {
        if (raylib::IsKeyPressed(raylib::KEY_RIGHT) ||
            raylib::IsKeyPressed(raylib::KEY_LEFT)) {
            int step = raylib::IsKeyPressed(raylib::KEY_RIGHT) ? 1 : N - 1;
            int next = (current + step) % N;
            if (prepare(entries[next])) current = next;
        }
        if (raylib::IsKeyDown(raylib::KEY_UP)) distance -= 2.f * raylib::GetFrameTime();
        if (raylib::IsKeyDown(raylib::KEY_DOWN)) distance += 2.f * raylib::GetFrameTime();
        distance = distance < 1.5f ? 1.5f : (distance > 24.f ? 24.f : distance);
        if (raylib::IsKeyPressed(raylib::KEY_SPACE)) paused = !paused;
        if (raylib::IsKeyPressed(raylib::KEY_P)) parallax = !parallax;
        if (!paused) yaw += 30.f * raylib::GetFrameTime();

        // Panes follow the window size.
        int w = raylib::GetScreenWidth() / 2, h = raylib::GetScreenHeight() - 40;
        if (w != pane_w || h != pane_h) {
            for (auto &p : panes) {
                if (p.id) raylib::UnloadRenderTexture(p);
                p = raylib::LoadRenderTexture(w, h);
            }
            pane_w = w, pane_h = h;
        }

        Entry &e = entries[current];
        const hoc::Meta &m = e.cards.meta;
        float el = 20.f * DEG2RAD, yr = yaw * DEG2RAD;
        float dist = m.radius * distance;
        raylib::Camera3D cam{};
        cam.position = {m.center.x + dist * std::cos(el) * std::cos(yr),
                        m.center.y + dist * std::sin(el),
                        m.center.z + dist * std::cos(el) * std::sin(yr)};
        cam.target = m.center;
        cam.up = {0.f, 1.f, 0.f};
        cam.fovy = 45.f;
        cam.projection = raylib::CAMERA_PERSPECTIVE;
        int steps = parallax ? 2 : 0;

        for (int i = 0; i < 2; i++) {
            raylib::BeginTextureMode(panes[i]);
            raylib::ClearBackground({135, 190, 230, 255});
            raylib::BeginMode3D(cam);
            if (i == 0) raylib::DrawModel(e.model, {0, 0, 0}, 1.f, raylib::WHITE);
            else hoc::draw_instanced(e.cards, shader, &identity, 1, cam, steps);
            raylib::EndMode3D();
            raylib::EndTextureMode();
        }

        raylib::BeginDrawing();
        raylib::ClearBackground({30, 30, 30, 255});
        for (int i = 0; i < 2; i++) {
            float x = static_cast<float>(i * pane_w);
            // Negative source height: render textures are bottom-up.
            raylib::DrawTextureRec(panes[i].texture,
                                   {0, 0, static_cast<float>(pane_w),
                                    -static_cast<float>(pane_h)},
                                   {x, 0}, raylib::WHITE);
            raylib::DrawText(labels[i], static_cast<int>(x) + 12, 10, 24,
                             raylib::BLACK);
            if (i > 0)
                raylib::DrawLine(static_cast<int>(x), 0, static_cast<int>(x),
                                 pane_h, {30, 30, 30, 255});
        }
        raylib::DrawText(
            raylib::TextFormat("%s   distance %.1f radii   parallax %s   "
                               "[LEFT/RIGHT model  UP/DOWN zoom  SPACE pause  "
                               "P parallax]",
                               e.file, static_cast<double>(distance),
                               parallax ? "on" : "off"),
            12, pane_h + 10, 20, raylib::RAYWHITE);
        raylib::EndDrawing();
    }

    for (auto &p : panes) raylib::UnloadRenderTexture(p);
    for (Entry &e : entries) {
        if (!e.ready) continue;
        hoc::unload(e.cards);
        raylib::UnloadModel(e.model);
    }
    raylib::UnloadShader(shader);
    raylib::CloseWindow();
    return 0;
}
