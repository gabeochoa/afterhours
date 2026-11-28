#define AFTER_HOURS_EXAMPLE_E2E
#include "../../../example_helper.h"

using namespace afterhours;
using namespace afterhours::examples;
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
    E2EHarness harness(argc, argv);
    Switcher sw;
    harness.add_systems(std::make_unique<UpdateSwitcher>(sw),
                        std::make_unique<ExpectSwitcher>(sw));
    return harness.run([&] { input_injector::reset_frame(); });
}
