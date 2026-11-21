#define AFTERHOURS_ENABLE_PROFILING 1
#include "ui_test_harness.h"
#include <afterhours/src/debug_allocator.h>
#include <afterhours/src/plugins/profiling.h>

using namespace afterhours;

TEST(allocations_are_counted_exactly) {
    developer::reset_allocation_counters();
    developer::DebugAllocator<int> alloc;
    std::vector<int *> blocks;
    for (int i = 0; i < 7; ++i) blocks.push_back(alloc.allocate(4));
    const auto counted = developer::allocation_counters();
    CHECK(counted.allocations == 7);
    CHECK(counted.live_allocations == 7);
    CHECK(counted.live_bytes == 7 * 4 * sizeof(int));
    CHECK(counted.total_bytes == 7 * 4 * sizeof(int));
    // A second element type feeds the same process-wide tally.
    developer::DebugAllocator<double> doubles;
    auto *extra = doubles.allocate(3);
    CHECK(developer::allocation_counters().allocations == 8);
    CHECK(developer::allocation_counters().live_bytes ==
          7 * 4 * sizeof(int) + 3 * sizeof(double));
    doubles.deallocate(extra, 3);
    for (auto *block : blocks) alloc.deallocate(block, 4);
    const auto drained = developer::allocation_counters();
    CHECK(drained.allocations == 8);
    CHECK(drained.live_allocations == 0);
    CHECK(drained.live_bytes == 0);
    CHECK(drained.total_bytes == 7 * 4 * sizeof(int) + 3 * sizeof(double));
}

TEST(deallocate_decrements_live_bytes) {
    developer::reset_allocation_counters();
    developer::DebugAllocator<char> alloc;
    auto *a = alloc.allocate(100);
    auto *b = alloc.allocate(50);
    alloc.deallocate(a, 100);
    const auto counted = developer::allocation_counters();
    CHECK(counted.live_bytes == 50);
    CHECK(counted.live_allocations == 1);
    CHECK(counted.total_bytes == 150);
    alloc.deallocate(b, 50);
    CHECK(developer::allocation_counters().live_bytes == 0);
}

TEST(feed_pushes_counters_into_profiler) {
    developer::reset_allocation_counters();
    profiling::Collector collector({1, 1, 4, false});
    CHECK(collector.start());
    developer::DebugAllocator<int> alloc;
    auto *block = alloc.allocate(8);
    CHECK(developer::feed_alloc_counters(collector));
    const auto snapshot = collector.snapshot();
    CHECK(snapshot.counters.size() == 3);
    CHECK(snapshot.counters[0].name == "alloc.count");
    CHECK(snapshot.counters[0].value == 1);
    CHECK(snapshot.counters[1].name == "alloc.live_bytes");
    CHECK(snapshot.counters[1].value == 8 * sizeof(int));
    CHECK(snapshot.counters[2].value == 8 * sizeof(int));
    alloc.deallocate(block, 8);
    CHECK(developer::feed_alloc_counters(collector));
    CHECK(collector.snapshot().counters[1].value == 0);
    collector.stop();
}

int main() { return ui_test::run_registered_tests("allocation_counters_test"); }
