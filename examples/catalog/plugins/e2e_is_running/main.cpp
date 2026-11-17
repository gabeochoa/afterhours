#include "../../../../ah.h"
#include "../../../../src/plugins/e2e_testing/e2e_testing.h"
#include "../../../../src/plugins/e2e_testing/harness.h"

using namespace afterhours;
using namespace afterhours::testing;

static std::string hud_text(int score, int fps) {
    std::string hud = std::format("SCORE {}", score);
    if (!is_e2e_running()) hud += std::format("  FPS {}", fps);
    return hud;
}

struct ExpectHud : System<PendingE2ECommand> {
    const std::string &hud;
    explicit ExpectHud(const std::string &h) : hud(h) {}
    void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
        if (cmd.is_consumed() || !cmd.is("expect_hud")) return;
        if (hud == cmd.arg(0))
            cmd.consume();
        else
            cmd.fail(std::format("hud is '{}'", hud));
    }
};

int main(int argc, char *argv[]) {
    const E2EArgs args = parse_e2e_args(argc, argv);
    std::string hud = hud_text(0, 144);
    if (args.script_path.empty()) {
        log_info("player sees: {}", hud);
        return 0;
    }

    SystemManager systems;
    register_builtin_handlers(systems);
    systems.register_update_system(std::make_unique<ExpectHud>(hud));
    register_unknown_handler(systems);
    register_cleanup(systems);

    E2ERunner runner;
    configure_runner(runner, args);
    runner.load_script(args.script_path);
    while (!runner.is_finished()) {
        hud = hud_text(0, 144);
        runner.tick(1.f / 60.f);
        systems.run(1.f / 60.f);
    }
    runner.print_results();
    log_info("under e2e the hud is: {}", hud);
    return runner.has_failed() ? 1 : 0;
}
