#include "../../../../ah.h"
#include "../../../../src/plugins/e2e_testing/e2e_testing.h"
#include "../../../../src/plugins/e2e_testing/harness.h"

using namespace afterhours;
using namespace afterhours::testing;

int main(int argc, char *argv[]) {
    const E2EArgs args = parse_e2e_args(argc, argv);
    int freezes = 0;

    SystemManager systems;
    register_command(systems, "disable_animations", [&](PendingE2ECommand &cmd) {
        ++freezes;
        cmd.consume();
    });
    register_builtin_handlers(systems);
    register_command(systems, "disable_animations", [&](PendingE2ECommand &cmd) {
        freezes += 100;
        cmd.consume();
    });
    register_command<int>(systems, "expect_freeze_count",
                          [&](PendingE2ECommand &cmd, int want) {
                              cmd.expect(freezes == want,
                                         std::format("froze {} times", freezes));
                          });
    register_unknown_handler(systems);
    register_cleanup(systems);

    E2ERunner runner;
    configure_runner(runner, args);
    runner.load_script(args.script_path);
    while (!runner.is_finished()) {
        runner.tick(1.f / 60.f);
        systems.run(1.f / 60.f);
    }
    runner.print_results();
    return runner.has_failed() ? 1 : 0;
}
