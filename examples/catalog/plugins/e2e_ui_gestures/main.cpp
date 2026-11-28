#define AFTER_HOURS_IMM_UI
#define AFTER_HOURS_ENABLE_E2E_TESTING
#define AFTER_HOURS_EXAMPLE_E2E
#include "../../../example_helper.h"

#include "../../../../src/plugins/ui.h"
#include "../../../../src/plugins/autolayout.h"
#include "../../../../src/plugins/e2e_testing/ui_commands.h"

using namespace afterhours;
using namespace afterhours::examples;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using namespace afterhours::testing;
namespace inj = afterhours::testing::input_injector;

struct Scrub {
    float offset = 0.f;
    bool held = false;
    float wheel = 0.f;
};

struct BuildRail : System<DefaultUIContext> {
    Scrub &scrub;
    explicit BuildRail(Scrub &s) : scrub(s) {}
    void for_each_with(Entity &entity, DefaultUIContext &ctx, float) override {
        auto rail = div(ctx, mk(entity),
                        ComponentConfig{}
                            .with_size(ComponentSize{pixels(200), pixels(40)})
                            .with_absolute_position(100, 50)
                            .with_debug_name("rail"));
        const Rectangle r = rail.ent().get<UIComponent>().rect();
        scrub.held = inj::detail::mouse.left_down;
        if (scrub.held) scrub.offset = inj::detail::mouse.pos.x - r.x;
        if (const float dy = inj::consume_wheel().y; dy != 0.f) scrub.wheel = dy;
    }
};

struct ScrubCommands : System<PendingE2ECommand> {
    Scrub &scrub;
    explicit ScrubCommands(Scrub &s) : scrub(s) {}
    void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
        if (cmd.is_consumed()) return;
        if (cmd.is("expect_scrub")) {
            const bool ok = scrub.offset == cmd.arg_as<float>(0) &&
                            scrub.held == (cmd.arg(1) == "true");
            if (ok) cmd.consume();
            else cmd.fail(std::format("offset {} held {}", scrub.offset, scrub.held));
        } else if (cmd.is("expect_wheel")) {
            if (scrub.wheel == cmd.arg_as<float>(0)) cmd.consume();
            else cmd.fail(std::format("wheel {}", scrub.wheel));
        }
    }
};

int main(int argc, char *argv[]) {
    E2EHarness harness(argc, argv);
    test_input::detail::test_mode = true;
    Scrub scrub;
    ui::setup_with_resolution<DefaultAction>(
        harness.systems, {800, 600}, std::make_unique<BuildRail>(scrub));
    ui_commands::register_ui_commands<DefaultAction>(harness.systems);
    harness.add_systems(std::make_unique<ScrubCommands>(scrub));
    return harness.run([&] { test_input::reset_frame(); });
}
