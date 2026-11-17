#include <string>

#include "../../../../ah.h"
#include "../../../../src/plugins/e2e_testing/e2e_testing.h"
#include "../../../../src/plugins/e2e_testing/harness.h"
#include "../../../../src/plugins/ui/text_input/utils.h"

using namespace afterhours;
using namespace afterhours::testing;

struct ExpectTypedCommand : System<PendingE2ECommand> {
    const std::string &typed;
    explicit ExpectTypedCommand(const std::string &t) : typed(t) {}

    void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
        if (cmd.is_consumed() || !cmd.is("expect_typed")) return;
        if (typed == cmd.arg(0))
            cmd.consume();
        else
            cmd.fail(std::format("typed '{}', expected '{}'", typed, cmd.arg(0)));
    }
};

int main(int argc, char *argv[]) {
    const E2EArgs args = parse_e2e_args(argc, argv);
    if (args.script_path.empty()) {
        log_error("usage: --test-script unicode.e2e");
        return 2;
    }
    test_input::detail::test_mode = true;

    std::string typed;
    SystemManager systems;
    register_builtin_handlers(systems);
    systems.register_update_system(std::make_unique<ExpectTypedCommand>(typed));
    register_unknown_handler(systems);
    register_cleanup(systems);

    E2ERunner runner;
    configure_runner(runner, args);
    runner.load_script(args.script_path);

    const float dt = 1.f / 60.f;
    while (!runner.is_finished()) {
        runner.tick(dt);
        systems.run(dt);
        while (int cp = test_input::get_char_pressed([] { return 0; }))
            typed += text_input::codepoint_to_utf8(cp);
    }
    runner.print_results();
    return runner.has_failed() ? 1 : 0;
}
