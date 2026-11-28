#define AFTER_HOURS_EXAMPLE_E2E
#include "../../../example_helper.h"

#include "../../../../src/plugins/ui/text_input/utils.h"

using namespace afterhours;
using namespace afterhours::examples;
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
    E2EHarness harness(argc, argv);
    if (!harness.has_script()) {
        log_error("usage: --test-script unicode.e2e");
        return 2;
    }
    test_input::detail::test_mode = true;

    std::string typed;
    harness.add_systems(std::make_unique<ExpectTypedCommand>(typed));
    return harness.run([&] {
        while (int cp = test_input::get_char_pressed([] { return 0; }))
            typed += text_input::codepoint_to_utf8(cp);
    });
}
