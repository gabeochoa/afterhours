#define AFTER_HOURS_EXAMPLE_E2E
#include "../../../example_helper.h"

using namespace afterhours;
using namespace afterhours::examples;
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
    E2EHarness harness(argc, argv);
    std::string hud = hud_text(0, 144);
    if (!harness.has_script()) {
        log_info("player sees: {}", hud);
        return 0;
    }

    harness.add_systems(std::make_unique<ExpectHud>(hud));
    const int result = harness.run([&] { hud = hud_text(0, 144); });
    log_info("under e2e the hud is: {}", hud);
    return result;
}
