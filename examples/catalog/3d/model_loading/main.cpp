// model_loading — load a glTF model and look at it.
//
// ModelLibrary owns the model; the window, camera, 3D mode, ground plane and
// text are afterhours helpers. The tree is from Kenney's Survival Kit (CC0,
// see ../KENNEY_LICENSE.txt).

#include <cmath>
#include <filesystem>
#include <format>

#include "../../../../ah.h"
#include "../../../../src/drawing_helpers.h"
#include "../../../../src/drawing_helpers_3d.h"
#include "../../../../src/graphics.h"
#include "../../../../src/plugins/models.h"

using namespace afterhours;

int main(int, char **) {
    graphics::set_config_flags(graphics::FLAG_WINDOW_RESIZABLE);
    graphics::init_window(1280, 720, "model_loading");
    graphics::set_target_fps(60);

    auto &models = ModelLibrary::get();
    if (!models.load(MODEL_PATH, "tree")) {
        graphics::close_window();
        return 1;
    }
    const ModelType &model = models.get("tree");

    // Frame the model from its bounds instead of hardcoding its size.
    Bounds box = model_bounds(model);
    Vector3Type size = box.size();
    Vector3Type center = box.center();
    std::string label =
        std::format("{}  triangles: {}",
                    std::filesystem::path(MODEL_PATH).filename().string(),
                    model_triangle_count(model));

    Camera3D cam{};
    cam.target = center;
    cam.up = {0.f, 1.f, 0.f};
    cam.fovy = 45.f;
    cam.projection = camera::PERSPECTIVE;

    while (!graphics::window_should_close()) {
        float a = static_cast<float>(graphics::get_time()) * 0.5f;
        float dist = size.y * 2.1f;
        cam.position = {center.x + dist * std::cos(a), size.y * 1.2f,
                        center.z + dist * std::sin(a)};

        graphics::begin_drawing();
        graphics::clear_background(Color{135, 190, 230, 255});

        begin_3d(cam);
        draw_plane({center.x, box.min.y, center.z}, {size.y * 3.f, size.y * 3.f},
                   Color{110, 160, 90, 255});
        draw_model(model, {0.f, 0.f, 0.f}, 0.f, {1.f, 1.f, 1.f});
        draw_cube_wires(center, size.x, size.y, size.z,
                        Color{255, 255, 255, 80});
        end_3d();

        draw_text(label.c_str(), 10, 10, 20, Color{0, 0, 0, 255});
        graphics::end_drawing();
    }

    models.unload_all();
    graphics::close_window();
    return 0;
}
