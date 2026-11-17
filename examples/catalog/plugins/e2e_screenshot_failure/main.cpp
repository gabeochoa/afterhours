#include "../../../../ah.h"
#include "../../../../src/plugins/e2e_testing/e2e_testing.h"
#include "../../../../src/plugins/e2e_testing/harness.h"

using namespace afterhours;
using namespace afterhours::testing;

int main(int argc, char *argv[]) {
    const E2EArgs args = parse_e2e_args(argc, argv);
    SystemManager systems;
    register_all_handlers(systems);

    graphics::RenderTextureType canvas{};
    E2ERunner runner;
    configure_runner(runner, args);
    runner.set_screenshot_callback([&](const std::string &name) {
        auto png = capture_render_texture_png(canvas);
        if (png) log_info("captured {}", name);
        return png.has_value();
    });
    runner.load_script(args.script_path);
    while (!runner.is_finished()) {
        runner.tick(1.f / 60.f);
        systems.run(1.f / 60.f);
    }
    runner.print_results();
    const bool failed = runner.has_failed();
    log_info("the none backend cannot capture, so the run {}",
             failed ? "fails, as it should" : "passed, which is wrong");
    return failed ? 0 : 1;
}
