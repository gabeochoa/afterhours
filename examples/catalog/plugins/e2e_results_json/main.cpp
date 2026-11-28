#define AFTER_HOURS_EXAMPLE_E2E
#include "../../../example_helper.h"

using namespace afterhours;
using namespace afterhours::examples;
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
    E2EHarness harness(argc, argv);
    int score = 0;
    harness.runner.set_reset_callback([&] { score = 0; });
    harness.add_systems(std::make_unique<ScoreCommands>(score));
    return harness.run();
}
