#include "../../../../ah.h"
#include "../../../../src/plugins/e2e_testing/e2e_testing.h"
#include "../../../../src/plugins/e2e_testing/harness.h"

using namespace afterhours;
using namespace afterhours::testing;

int main(int argc, char *argv[]) {
    const E2EArgs args = parse_e2e_args(argc, argv);

    // The resolution the stepped resize starts from.
    Entity &singleton = EntityHelper::createEntity();
    window_manager::add_singleton_components(
        singleton, window_manager::Resolution{800, 600}, 60);

    SystemManager systems;
    register_all_handlers(systems);

    E2ERunner runner;
    configure_runner(runner, args);
    if (!args.script_path.empty()) runner.load_script(args.script_path);
    while (!args.script_path.empty() && !runner.is_finished()) {
        runner.tick(1.f / 60.f);
        systems.run(1.f / 60.f);
        // This example has no renderer; its frame loop is the present, which
        // is what the UI render pass signals in a real app (one registry
        // clear per rendered frame). wait_resized waits for that signal.
        VisibleTextRegistry::instance().clear();
    }
    if (!args.script_path.empty()) runner.print_results();

    auto *pcr = EntityHelper::get_singleton_cmp<
        window_manager::ProvidesCurrentResolution>();
    log_info("final resolution: {}x{}", pcr ? pcr->width() : 0,
             pcr ? pcr->height() : 0);
    return runner.has_failed() ? 1 : 0;
}
