#include "../../../../ah.h"
#include "../../../../src/plugins/e2e_testing/e2e_testing.h"
#include "../../../../src/plugins/e2e_testing/harness.h"

using namespace afterhours;
using namespace afterhours::testing;

struct Switcher {
    bool open = false;
    int selected = 0;
};

struct UpdateSwitcher : System<> {
    Switcher &sw;
    explicit UpdateSwitcher(Switcher &s) : sw(s) {}
    void once(float) override {
        const bool ctrl = input_injector::is_key_down(keys::LEFT_CONTROL);
        if (ctrl && input_injector::consume_press(keys::TAB)) {
            if (!sw.open) sw.selected = 0;
            sw.open = true;
            ++sw.selected;
        }
        if (!ctrl) sw.open = false;
    }
};

struct ExpectSwitcher : System<PendingE2ECommand> {
    Switcher &sw;
    explicit ExpectSwitcher(Switcher &s) : sw(s) {}
    void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
        if (cmd.is_consumed() || !cmd.is("expect_switcher")) return;
        const bool want_open = cmd.arg(0) == "open";
        const int want_selected = cmd.arg_as<int>(1);
        if (sw.open == want_open && sw.selected == want_selected)
            cmd.consume();
        else
            cmd.fail(std::format("switcher is {} on {}",
                                 sw.open ? "open" : "closed", sw.selected));
    }
};

int main(int argc, char *argv[]) {
    const E2EArgs args = parse_e2e_args(argc, argv);
    Switcher sw;
    SystemManager systems;
    register_builtin_handlers(systems);
    systems.register_update_system(std::make_unique<UpdateSwitcher>(sw));
    systems.register_update_system(std::make_unique<ExpectSwitcher>(sw));
    register_unknown_handler(systems);
    register_cleanup(systems);

    E2ERunner runner;
    configure_runner(runner, args);
    runner.load_script(args.script_path);
    while (!runner.is_finished()) {
        runner.tick(1.f / 60.f);
        systems.run(1.f / 60.f);
        input_injector::reset_frame();
    }
    runner.print_results();
    return runner.has_failed() ? 1 : 0;
}
