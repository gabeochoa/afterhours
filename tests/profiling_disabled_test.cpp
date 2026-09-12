#include "ui_test_harness.h"
#include <afterhours/src/plugins/ui/profiler.h>

int main() {
    static_assert(!afterhours::profiling::available);
    static_assert(sizeof(afterhours::profiling::Collector) == 1);
    afterhours::profiling::Collector collector;
    int evaluated = 0;
    AFTERHOURS_PROFILE_COUNTER(collector, "counter", "", ++evaluated);
    if (collector.start() || evaluated != 0) return 1;
    int calls = 0;
    afterhours::SystemManager::set_profile_hook({[&](auto, auto) { ++calls; }, [](auto, auto) {}});
    afterhours::SystemManager manager;
    manager.register_update_system([](float) {});
    afterhours::Entities entities;
    manager.tick(entities, .016f);
    afterhours::SystemManager::set_profile_hook({});
    return calls == 1 && collector.snapshot().systems.empty() ? 0 : 1;
}
