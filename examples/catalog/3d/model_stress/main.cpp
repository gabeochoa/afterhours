// model_stress — how many copies of one model can we draw?
//
// Every tree is an entity with a Tree component. An update system spins them
// and a render system draws them, so this measures the ECS walk plus one
// DrawModelEx per entity. Frame rate is uncapped so the number means something.
//
//   UP / DOWN   double / halve the tree count
//   --count N   start with N trees (default 256)
//   --bench     no interaction: double the count until a frame takes longer
//               than 16.7ms, print a table, exit
//
// ponytail: one draw call per tree. Instancing (DrawMeshInstanced + an
// instancing shader) is the next step once draw calls are the ceiling.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>

namespace raylib {
#include <raylib.h>
}  // namespace raylib

#define AFTER_HOURS_ENTITY_HELPER
#define AFTER_HOURS_ENTITY_QUERY
#define AFTER_HOURS_SYSTEM
#include "../../../../ah.h"
#include "../../../../src/drawing_helpers_3d.h"

using namespace afterhours;

constexpr float SPACING = 2.f;

struct Tree : BaseComponent {
    raylib::Vector3 pos;
    float yaw;
    float spin;
    float scale;
    Tree(raylib::Vector3 p, float y, float s, float sc)
        : pos(p), yaw(y), spin(s), scale(sc) {}
};

struct Spin : System<Tree> {
    void for_each_with(Entity &, Tree &t, float dt) override {
        t.yaw += t.spin * dt;
    }
};

struct DrawTrees : System<Tree> {
    raylib::Model model;
    explicit DrawTrees(raylib::Model m) : model(m) {}
    // Render systems get both the mutable and the const pass; drawing only in
    // the const one keeps it to one draw per tree.
    void for_each_with(const Entity &, const Tree &t, float) const override {
        raylib::DrawModelEx(model, t.pos, {0.f, 1.f, 0.f}, t.yaw,
                            {t.scale, t.scale, t.scale}, raylib::WHITE);
    }
};

// Field radius for n trees at roughly SPACING apart.
static float field_radius(int n) {
    return SPACING * std::sqrt(static_cast<float>(n)) * 0.6f;
}

static void spawn(int n, int total_after) {
    static std::mt19937 rng(1234);  // fixed seed: --bench runs are comparable
    std::uniform_real_distribution<float> unit(0.f, 1.f);
    float r_max = field_radius(total_after);
    for (int i = 0; i < n; i++) {
        float r = r_max * std::sqrt(unit(rng));
        float a = unit(rng) * 2.f * PI;
        EntityHelper::createEntity().addComponent<Tree>(
            raylib::Vector3{r * std::cos(a), 0.f, r * std::sin(a)},
            unit(rng) * 360.f, 20.f + unit(rng) * 60.f, 0.8f + unit(rng) * 0.4f);
    }
}

// Entities created this frame sit in a temp array until a system tick merges
// them, so get_entities() lags; drop from what is already merged.
static void despawn_half() {
    const auto &ents = EntityHelper::get_entities();
    for (size_t i = ents.size() - ents.size() / 2; i < ents.size(); i++)
        EntityHelper::markIDForCleanup(ents[i]->id);
}

static void frame(SystemManager &systems, Camera3D &cam, int count,
                  int tris_per_tree) {
    float r = field_radius(count) + 4.f;
    float a = static_cast<float>(raylib::GetTime()) * 0.2f;
    cam.position = {r * std::cos(a), r * 0.6f, r * std::sin(a)};

    raylib::BeginDrawing();
    raylib::ClearBackground({135, 190, 230, 255});
    begin_3d(cam);
    draw_plane({0.f, 0.f, 0.f}, {r * 2.f, r * 2.f}, {110, 160, 90, 255});
    systems.run(raylib::GetFrameTime());
    end_3d();
    raylib::DrawRectangle(0, 0, 420, 70, {0, 0, 0, 140});
    raylib::DrawText(raylib::TextFormat("trees: %d   tris: %d", count,
                                        count * tris_per_tree),
                     10, 10, 20, raylib::WHITE);
    raylib::DrawText(raylib::TextFormat("%d fps  %.2f ms   [UP/DOWN]",
                                        raylib::GetFPS(),
                                        raylib::GetFrameTime() * 1000.f),
                     10, 40, 20, raylib::WHITE);
    raylib::EndDrawing();
}

int main(int argc, char **argv) {
    int count = 256;
    bool bench = false;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--bench")) bench = true;
        else if (!strcmp(argv[i], "--count") && i + 1 < argc)
            count = std::max(1, atoi(argv[++i]));
    }

    raylib::SetConfigFlags(raylib::FLAG_WINDOW_RESIZABLE);
    raylib::InitWindow(1280, 720, "model_stress");
    raylib::SetTargetFPS(0);

    raylib::Model model = raylib::LoadModel(MODEL_PATH);
    // raylib logs a warning and hands back an empty model rather than failing.
    if (model.meshCount == 0) {
        fprintf(stderr, "failed to load %s\n", MODEL_PATH);
        raylib::CloseWindow();
        return 1;
    }
    int tris_per_tree = 0;
    for (int i = 0; i < model.meshCount; i++)
        tris_per_tree += model.meshes[i].triangleCount;

    SystemManager systems;
    systems.register_update_system(std::make_unique<Spin>());
    systems.register_render_system(std::make_unique<DrawTrees>(model));

    Camera3D cam{};
    cam.target = {0.f, 0.f, 0.f};
    cam.up = {0.f, 1.f, 0.f};
    cam.fovy = 45.f;
    cam.projection = camera::PERSPECTIVE;

    spawn(count, count);

    if (bench) {
        constexpr int WARMUP = 30, FRAMES = 120;
        printf("%10s %12s %10s %8s\n", "trees", "tris", "avg_ms", "fps");
        while (!raylib::WindowShouldClose()) {
            for (int i = 0; i < WARMUP; i++)
                frame(systems, cam, count, tris_per_tree);
            double start = raylib::GetTime();
            for (int i = 0; i < FRAMES; i++)
                frame(systems, cam, count, tris_per_tree);
            double ms = (raylib::GetTime() - start) * 1000.0 / FRAMES;
            printf("%10d %12d %10.2f %8.0f\n", count, count * tris_per_tree,
                   ms, 1000.0 / ms);
            fflush(stdout);
            if (ms > 1000.0 / 60.0 || count >= (1 << 20)) break;
            spawn(count, count * 2);
            count *= 2;
        }
    } else {
        while (!raylib::WindowShouldClose()) {
            if (raylib::IsKeyPressed(raylib::KEY_UP)) {
                spawn(count, count * 2);
                count *= 2;
            }
            if (raylib::IsKeyPressed(raylib::KEY_DOWN) && count > 1) {
                despawn_half();
                count -= count / 2;
            }
            frame(systems, cam, count, tris_per_tree);
        }
    }

    raylib::UnloadModel(model);
    raylib::CloseWindow();
    return 0;
}
