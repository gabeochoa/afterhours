#include "../../../../ah.h"
#include "../../../../src/plugins/e2e_testing/e2e_testing.h"
#include "../../../../src/plugins/e2e_testing/harness.h"

using namespace afterhours;
using namespace afterhours::testing;

struct Match {
    std::string mode = "menu";
    int left = 0;
    int right = 0;
    float ball_x = 0.f;
    float ball_y = 0.f;
};

void register_match_commands(SystemManager &systems, Match &match) {
    register_command<std::string>(
        systems, "start_match", [&](PendingE2ECommand &cmd, std::string mode) {
            match = Match{.mode = mode};
            cmd.consume();
        });
    register_command<int, int>(
        systems, "set_score", [&](PendingE2ECommand &cmd, int left, int right) {
            match.left = left;
            match.right = right;
            cmd.consume();
        });
    register_command<int, int>(
        systems, "expect_score", [&](PendingE2ECommand &cmd, int left, int right) {
            cmd.expect(match.left == left && match.right == right,
                       std::format("score is {}:{}", match.left, match.right));
        });
    register_command<float, float>(
        systems, "teleport_ball", [&](PendingE2ECommand &cmd, float x, float y) {
            match.ball_x = x;
            match.ball_y = y;
            cmd.consume();
        });
    register_command(systems, "expect_ball", [&](PendingE2ECommand &cmd) {
        auto x = cmd.maybe_arg_as<float>(0), y = cmd.maybe_arg_as<float>(1);
        if (!x || !y) {
            cmd.fail("expect_ball needs x y");
            return;
        }
        cmd.expect(match.ball_x == *x && match.ball_y == *y,
                   std::format("ball is at {} {}", match.ball_x, match.ball_y));
    });
}

int main(int argc, char *argv[]) {
    const E2EArgs args = parse_e2e_args(argc, argv);
    if (args.script_path.empty()) {
        log_error("usage: --test-script match.e2e");
        return 2;
    }

    Match match;
    SystemManager systems;
    register_builtin_handlers(systems);
    register_match_commands(systems, match);
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
    log_info("mode {} score {}:{}", match.mode, match.left, match.right);
    return runner.has_failed() ? 1 : 0;
}
