// model_stress — how many copies of one model can we draw?
//
// Every tree is an entity with a Tree component. An update system spins them
// and a render system draws them, in one of three modes:
//
//   per-draw   draw_model once per tree
//   instanced  one matrix per tree, all drawn with draw_model_instanced
//   cards      the same matrices, drawn as house_of_cards impostors baked
//              from the model at startup: two triangles per tree
//
// Frame rate is uncapped so the number means something.
//
//   UP / DOWN   double / halve the tree count
//   M           next mode
//   --count N   start with N trees (default 4096)
//   --mode M    per-draw, instanced (default) or cards
//   --bench     no interaction: double the count until a frame takes longer
//               than 100ms (10fps), print a table, exit

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>

namespace raylib {
#include <raylib.h>
#include <raymath.h>
}  // namespace raylib

#define AFTER_HOURS_ENTITY_HELPER
#define AFTER_HOURS_ENTITY_QUERY
#define AFTER_HOURS_SYSTEM
#include "../../../../ah.h"
#include "../../../../src/drawing_helpers_3d.h"
#include "../../../../src/plugins/house_of_cards.h"
#include "../../../../src/plugins/models.h"

using namespace afterhours;

// Trees sit about this far apart; set from the model's size so a big model
// does not bury the field in overlapping copies.
static float spacing = 2.f;

enum class Mode { PerDraw, Instanced, Cards };
constexpr const char *MODE_NAMES[] = {"per-draw", "instanced", "cards"};

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
    // ModelLibrary is a std::map, so the reference survives later loads.
    const ModelType &model;
    raylib::Shader shader;
    house_of_cards::Cards &cards;
    raylib::Shader card_shader;
    const Camera3D &cam;
    const Mode &mode;
    mutable std::vector<raylib::Matrix> transforms;

    DrawTrees(const ModelType &m, raylib::Shader s, house_of_cards::Cards &c,
              raylib::Shader cs, const Camera3D &camera, const Mode &md)
        : model(m), shader(s), cards(c), card_shader(cs), cam(camera),
          mode(md) {}

    // Render systems get both the mutable and the const pass; working only in
    // the const one keeps it to one draw per tree.
    void once(float) const override { transforms.clear(); }

    void for_each_with(const Entity &, const Tree &t, float) const override {
        if (mode == Mode::PerDraw) {
            draw_model(model, t.pos, t.yaw, {t.scale, t.scale, t.scale});
            return;
        }
        // Same order DrawModelEx uses: scale, then rotate, then translate.
        transforms.push_back(raylib::MatrixMultiply(
            raylib::MatrixMultiply(
                raylib::MatrixScale(t.scale, t.scale, t.scale),
                raylib::MatrixRotateY(t.yaw * DEG2RAD)),
            raylib::MatrixTranslate(t.pos.x, t.pos.y, t.pos.z)));
    }

    void after(float) const override {
        int n = static_cast<int>(transforms.size());
        if (mode == Mode::Instanced)
            draw_model_instanced(model, shader, transforms.data(), n);
        else if (mode == Mode::Cards)
            house_of_cards::draw_instanced(cards, card_shader,
                                           transforms.data(), n, cam);
    }
};

// Field radius for n trees at roughly `spacing` apart.
static float field_radius(int n) {
    return spacing * std::sqrt(static_cast<float>(n)) * 0.6f;
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
                  int tris_per_tree, Mode mode) {
    float r = field_radius(count) + spacing * 2.f;
    float a = static_cast<float>(raylib::GetTime()) * 0.2f;
    cam.position = {r * std::cos(a), r * 0.6f, r * std::sin(a)};

    raylib::BeginDrawing();
    raylib::ClearBackground({135, 190, 230, 255});
    begin_3d(cam);
    draw_plane({0.f, 0.f, 0.f}, {r * 2.f, r * 2.f}, {110, 160, 90, 255});
    systems.run(raylib::GetFrameTime());
    end_3d();
    raylib::DrawRectangle(0, 0, 520, 70, {0, 0, 0, 140});
    if (mode == Mode::Cards) tris_per_tree = 2;
    raylib::DrawText(raylib::TextFormat("trees: %d   tris: %d", count,
                                        count * tris_per_tree),
                     10, 10, 20, raylib::WHITE);
    raylib::DrawText(raylib::TextFormat("%d fps  %.2f ms  %s  [UP/DOWN/M]",
                                        raylib::GetFPS(),
                                        raylib::GetFrameTime() * 1000.f,
                                        MODE_NAMES[static_cast<int>(mode)]),
                     10, 40, 20, raylib::WHITE);
    raylib::EndDrawing();
}

int main(int argc, char **argv) {
    int count = 4096;
    bool bench = false;
    Mode mode = Mode::Instanced;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--bench")) bench = true;
        else if (!strcmp(argv[i], "--mode") && i + 1 < argc) {
            ++i;
            for (int m = 0; m < 3; m++)
                if (!strcmp(argv[i], MODE_NAMES[m])) mode = static_cast<Mode>(m);
        }
        else if (!strcmp(argv[i], "--count") && i + 1 < argc)
            count = std::max(1, atoi(argv[++i]));
    }

    raylib::SetConfigFlags(raylib::FLAG_WINDOW_RESIZABLE);
    raylib::InitWindow(1280, 720, "model_stress");
    raylib::SetTargetFPS(0);

    auto &models = ModelLibrary::get();
    if (!models.load(MODEL_PATH, "tree")) {
        raylib::CloseWindow();
        return 1;
    }
    const ModelType &model = models.get("tree");
    int tris_per_tree = 0;
    for (int i = 0; i < model.meshCount; i++)
        tris_per_tree += model.meshes[i].triangleCount;
    raylib::BoundingBox box = raylib::GetModelBoundingBox(model);
    spacing = 0.85f * raylib::Vector3Distance(box.min, box.max);

    raylib::Shader shader = load_instancing_shader();

    house_of_cards::Baked baked = house_of_cards::bake(model);
    house_of_cards::Cards cards = house_of_cards::upload(baked);
    house_of_cards::unload(baked);
    raylib::Shader card_shader = house_of_cards::load_shader();

    Camera3D cam{};
    cam.target = {0.f, 0.f, 0.f};
    cam.up = {0.f, 1.f, 0.f};
    cam.fovy = 45.f;
    cam.projection = camera::PERSPECTIVE;

    SystemManager systems;
    systems.register_update_system(std::make_unique<Spin>());
    systems.register_render_system(std::make_unique<DrawTrees>(
        model, shader, cards, card_shader, cam, mode));

    spawn(count, count);

    if (bench) {
        constexpr int WARMUP = 30, FRAMES = 120;
        printf("%s\n", MODE_NAMES[static_cast<int>(mode)]);
        printf("%10s %12s %10s %8s\n", "trees", "tris", "avg_ms", "fps");
        while (!raylib::WindowShouldClose()) {
            for (int i = 0; i < WARMUP; i++)
                frame(systems, cam, count, tris_per_tree, mode);
            double start = raylib::GetTime();
            for (int i = 0; i < FRAMES; i++)
                frame(systems, cam, count, tris_per_tree, mode);
            double ms = (raylib::GetTime() - start) * 1000.0 / FRAMES;
            printf("%10d %12d %10.2f %8.0f\n", count,
                   count * (mode == Mode::Cards ? 2 : tris_per_tree), ms,
                   1000.0 / ms);
            fflush(stdout);
            if (ms > 1000.0 / 10.0 || count >= (1 << 20)) break;
            spawn(count, count * 2);
            count *= 2;
        }
    } else {
        while (!raylib::WindowShouldClose()) {
            if (raylib::IsKeyPressed(raylib::KEY_UP)) {
                spawn(count, count * 2);
                count *= 2;
            }
            if (raylib::IsKeyPressed(raylib::KEY_M))
                mode = static_cast<Mode>((static_cast<int>(mode) + 1) % 3);
            if (raylib::IsKeyPressed(raylib::KEY_DOWN) && count > 1) {
                despawn_half();
                count -= count / 2;
            }
            frame(systems, cam, count, tris_per_tree, mode);
        }
    }

    raylib::UnloadShader(card_shader);
    house_of_cards::unload(cards);
    raylib::UnloadShader(shader);
    models.unload_all();
    raylib::CloseWindow();
    return 0;
}
