#define AFTERHOURS_ENABLE_PROFILING 1
#include "ui_test_harness.h"
#include <afterhours/src/plugins/profiling.h>

using namespace afterhours;

namespace {
auto record_fn(profiling::Collector &collector) {
    return [&collector](std::string_view name, double ms) {
        for (const auto &observer : profiling::detail::observers) {
            if (observer.context != &collector) continue;
            observer.callback(observer.context, name, SystemPhase::Update, ms, 0);
        }
    };
}
}

TEST(slow_frame_is_captured_with_its_samples) {
    profiling::Collector collector({600, 8, 1, false, 20, 4});
    CHECK(collector.start());
    const auto record = record_fn(collector);
    record("work", 11);
    record("render", 31);
    collector.end_frame(42);
    const auto spikes = collector.spikes();
    CHECK(spikes.size() == 1);
    CHECK(spikes[0].frame == 1);
    CHECK(spikes[0].frame_ms == 42);
    CHECK(spikes[0].systems.size() == 2);
    for (const auto &sample : spikes[0].systems) {
        if (sample.name == "work") CHECK(sample.last_frame_ms == 11);
        if (sample.name == "render") CHECK(sample.last_frame_ms == 31);
    }
    CHECK(collector.snapshot().spikes.size() == 1);
    CHECK(collector.snapshot().spikes[0].frame_ms == 42);
}

TEST(fast_frames_are_not_retained) {
    profiling::Collector collector({600, 8, 1, false, 20, 4});
    CHECK(collector.start());
    const auto record = record_fn(collector);
    record("work", 5);
    collector.end_frame(10);
    collector.end_frame(20);
    collector.end_frame(19.999);
    CHECK(collector.spikes().empty());
    CHECK(collector.snapshot().spikes.empty());
    CHECK(collector.snapshot().frames == 3);
}

TEST(ring_evicts_oldest_spike) {
    profiling::Collector collector({600, 8, 1, false, 20, 2});
    CHECK(collector.start());
    const auto record = record_fn(collector);
    record("work", 25);
    collector.end_frame(25);
    collector.end_frame(30);
    collector.end_frame(35);
    const auto spikes = collector.spikes();
    CHECK(spikes.size() == 2);
    CHECK(spikes[0].frame == 2);
    CHECK(spikes[0].frame_ms == 30);
    CHECK(spikes[1].frame == 3);
    CHECK(spikes[1].frame_ms == 35);
}

TEST(disabled_threshold_and_zero_capacity_retain_nothing) {
    profiling::Collector off({600, 8, 1, false, 0, 4});
    CHECK(off.start());
    off.end_frame(500);
    CHECK(off.spikes().empty());

    profiling::Collector zero({600, 8, 1, false, 20, 0});
    CHECK(zero.start());
    zero.end_frame(500);
    CHECK(zero.spikes().empty());

    profiling::Collector setter;
    CHECK(setter.start());
    setter.set_spike_threshold_ms(10);
    setter.end_frame(11);
    CHECK(setter.spikes().size() == 1);
    setter.set_spike_threshold_ms(-1);
    setter.end_frame(100);
    CHECK(setter.spikes().size() == 1);
    setter.set_spike_threshold_ms(NAN);
    setter.end_frame(100);
    CHECK(setter.spikes().size() == 1);
}

TEST(reset_clears_retained_spikes) {
    profiling::Collector collector({600, 8, 1, false, 20, 4});
    CHECK(collector.start());
    const auto record = record_fn(collector);
    record("work", 25);
    collector.end_frame(25);
    CHECK(collector.spikes().size() == 1);
    collector.reset();
    CHECK(collector.spikes().empty());
    CHECK(collector.snapshot().spikes.empty());
    record("work", 25);
    collector.end_frame(25);
    CHECK(collector.spikes().size() == 1);
    CHECK(collector.spikes()[0].frame == 1);
}

int main() { return ui_test::run_registered_tests("spike capture"); }
