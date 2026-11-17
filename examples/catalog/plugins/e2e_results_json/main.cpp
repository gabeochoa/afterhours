#include "../../../../ah.h"
#include "../../../../src/plugins/e2e_testing/e2e_testing.h"
#include "../../../../src/plugins/e2e_testing/harness.h"

using namespace afterhours;
using namespace afterhours::testing;

struct ScoreCommands : System<PendingE2ECommand> {
    int &score;
    explicit ScoreCommands(int &s) : score(s) {}

    void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
        if (cmd.is_consumed()) return;
        if (cmd.is("set_score")) {
            score = cmd.arg_as<int>(0);
            cmd.consume();
        } else if (cmd.is("expect_score")) {
            if (score == cmd.arg_as<int>(0))
                cmd.consume();
            else
                cmd.fail(std::format("score is {}", score));
        }
    }
};

int main(int argc, char *argv[]) {
    const E2EArgs args = parse_e2e_args(argc, argv);
    int score = 0;
    SystemManager systems;
    register_builtin_handlers(systems);
    systems.register_update_system(std::make_unique<ScoreCommands>(score));
    register_unknown_handler(systems);
    register_cleanup(systems);

    E2ERunner runner;
    configure_runner(runner, args);
    runner.set_reset_callback([&] { score = 0; });
    if (!args.script_dir.empty())
        runner.load_scripts_from_directory(args.script_dir);
    else
        runner.load_script(args.script_path);
    while (!runner.is_finished()) {
        runner.tick(1.f / 60.f);
        systems.run(1.f / 60.f);
    }
    runner.print_results();
    return runner.has_failed() ? 1 : 0;
}
