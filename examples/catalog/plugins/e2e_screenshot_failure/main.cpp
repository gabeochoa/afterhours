#define AFTER_HOURS_EXAMPLE_E2E
#include "../../../example_helper.h"

using namespace afterhours;
using namespace afterhours::examples;
using namespace afterhours::testing;

int main(int argc, char *argv[]) {
    E2EHarness harness(argc, argv);

    graphics::RenderTextureType canvas{};
    harness.runner.set_screenshot_callback([&](const std::string &name) {
        auto png = capture_render_texture_png(canvas);
        if (png) log_info("captured {}", name);
        return png.has_value();
    });
    const bool failed = harness.run() != 0;
    log_info("the none backend cannot capture, so the run {}",
             failed ? "fails, as it should" : "passed, which is wrong");
    return failed ? 0 : 1;
}
