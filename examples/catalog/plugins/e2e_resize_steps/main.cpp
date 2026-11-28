#define AFTER_HOURS_EXAMPLE_E2E
#include "../../../example_helper.h"

using namespace afterhours;
using namespace afterhours::examples;
using namespace afterhours::testing;

int main(int argc, char *argv[]) {
    E2EHarness harness(argc, argv);

    Entity &singleton = EntityHelper::createEntity();
    window_manager::add_singleton_components(
        singleton, window_manager::Resolution{800, 600}, 60);

    const auto log_resolution = [] {
        auto *pcr = EntityHelper::get_singleton_cmp<
            window_manager::ProvidesCurrentResolution>();
        log_info("final resolution: {}x{}", pcr ? pcr->width() : 0,
                 pcr ? pcr->height() : 0);
    };

    if (!harness.has_script()) {
        log_resolution();
        return 0;
    }
    const int result = harness.run(
        [&] { VisibleTextRegistry::instance().clear(); });
    log_resolution();
    return result;
}
