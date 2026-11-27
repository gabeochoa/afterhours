#define AFTER_HOURS_IMM_UI
#define AFTER_HOURS_ENABLE_E2E_TESTING
#include "../../../../src/plugins/ui.h"

#include "../../../../ah.h"
#include "../../../../src/plugins/autolayout.h"
#include "../../../../src/plugins/e2e_testing/harness.h"
#include "../../../../src/plugins/e2e_testing/ui_commands.h"

using namespace afterhours;
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
        scrub.held = inj::is_mouse_button_down();
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
    const E2EArgs args = parse_e2e_args(argc, argv);
    test_input::detail::test_mode = true;
    Scrub scrub;
    SystemManager systems;
    ui::setup_with_resolution<>(systems, {800, 600},
                                std::make_unique<BuildRail>(scrub));
    register_builtin_handlers(systems);
    ui_commands::register_ui_commands<DefaultAction>(systems);
    systems.register_update_system(std::make_unique<ScrubCommands>(scrub));
    register_unknown_handler(systems);
    register_cleanup(systems);

    E2ERunner runner;
    configure_runner(runner, args);
    runner.load_script(args.script_path);
    while (!runner.is_finished()) {
        runner.tick(1.f / 60.f);
        systems.run(1.f / 60.f);
        test_input::reset_frame();
    }
    runner.print_results();
    return runner.has_failed() ? 1 : 0;
}
