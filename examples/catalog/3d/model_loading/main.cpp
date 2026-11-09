// model_loading — load a glTF model and look at it.
//
// ModelLibrary owns the model; draw_model, the camera, 3D mode and the ground
// plane are afterhours helpers. Bounds still come straight from raylib.
// The tree is from Kenney's Survival Kit (CC0, see ../KENNEY_LICENSE.txt).

namespace raylib {
#include <raylib.h>
}  // namespace raylib

#include "../../../../ah.h"
#include "../../../../src/drawing_helpers_3d.h"
#include "../../../../src/plugins/models.h"

using namespace afterhours;

int main(int, char **) {
    raylib::SetConfigFlags(raylib::FLAG_MSAA_4X_HINT |
                           raylib::FLAG_WINDOW_RESIZABLE);
    raylib::InitWindow(1280, 720, "model_loading");
    raylib::SetTargetFPS(60);

    auto &models = ModelLibrary::get();
    if (!models.load(MODEL_PATH, "tree")) {
        raylib::CloseWindow();
        return 1;
    }
    const ModelType &model = models.get("tree");

    // Frame the model from its bounds instead of hardcoding its size.
    raylib::BoundingBox box = raylib::GetModelBoundingBox(model);
    float height = box.max.y - box.min.y;
    raylib::Vector3 center = {(box.min.x + box.max.x) / 2.f, height / 2.f,
                              (box.min.z + box.max.z) / 2.f};

    Camera3D cam{};
    cam.position = {height * 1.5f, height * 1.2f, height * 1.5f};
    cam.target = center;
    cam.up = {0.f, 1.f, 0.f};
    cam.fovy = 45.f;
    cam.projection = camera::PERSPECTIVE;

    while (!raylib::WindowShouldClose()) {
        raylib::UpdateCamera(&cam, raylib::CAMERA_ORBITAL);

        raylib::BeginDrawing();
        raylib::ClearBackground({135, 190, 230, 255});

        begin_3d(cam);
        draw_plane({0.f, box.min.y, 0.f}, {height * 3.f, height * 3.f},
                   {110, 160, 90, 255});
        draw_model(model, {0.f, 0.f, 0.f}, 0.f, {1.f, 1.f, 1.f});
        draw_cube_wires(center, box.max.x - box.min.x, height,
                        box.max.z - box.min.z, {255, 255, 255, 80});
        end_3d();

        raylib::DrawText(raylib::TextFormat("%s  meshes: %d  materials: %d",
                                    raylib::GetFileName(MODEL_PATH),
                                    model.meshCount, model.materialCount),
                         10, 10, 20, raylib::BLACK);
        raylib::EndDrawing();
    }

    models.unload_all();
    raylib::CloseWindow();
    return 0;
}
