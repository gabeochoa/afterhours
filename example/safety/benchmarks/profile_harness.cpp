
// Standalone profiling harness — runs the expensive operations in tight loops
// so `sample` / `instruments` / `perf` can capture meaningful stacks.

#include <algorithm>
#include <chrono>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <unistd.h>
#include <vector>

#define AFTER_HOURS_ENTITY_HELPER
#define AFTER_HOURS_ENTITY_QUERY
#define AFTER_HOURS_SYSTEM
#include "../../../ah.h"
#include "../../../src/core/opt_entity_handle.h"

#include "../../core/tag_filter_regression/demo_tags.h"

using namespace afterhours;

struct Position : BaseComponent {
    float x = 0.f, y = 0.f;
};
struct Velocity : BaseComponent {
    float vx = 1.f, vy = 0.5f;
};

static void cleanup_all() {
    EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();
}

using Clock = std::chrono::high_resolution_clock;

// Phase 1: Entity creation & merge (heavy allocation)
static void __attribute__((noinline)) phase_entity_creation() {
    for (int it = 0; it < 200; ++it) {
        for (int i = 0; i < 10000; ++i) {
            EntityHelper::createEntity();
        }
        EntityHelper::merge_entity_arrays();
        cleanup_all();
    }
}

// Phase 2: Query with component filters
static void __attribute__((noinline)) phase_query() {
    cleanup_all();
    for (int i = 0; i < 10000; ++i) {
        auto &e = EntityHelper::createEntity();
        e.addComponent<Position>().x = static_cast<float>(i);
        if (i % 2 == 0) e.addComponent<Velocity>();
        if (i % 3 == 0) e.enableTag(DemoTag::Runner);
    }
    EntityHelper::merge_entity_arrays();

    volatile size_t sink = 0;
    for (int it = 0; it < 2000; ++it) {
        auto results = EntityQuery<>({.ignore_temp_warning = true})
            .whereHasComponent<Position>()
            .whereHasComponent<Velocity>()
            .gen();
        sink = results.size();
    }
    (void)sink;
    cleanup_all();
}

// Phase 3: Handle resolution
static void __attribute__((noinline)) phase_handle_resolve() {
    cleanup_all();
    for (int i = 0; i < 10000; ++i) {
        EntityHelper::createEntity();
    }
    EntityHelper::merge_entity_arrays();

    std::vector<EntityHandle> handles;
    handles.reserve(10000);
    auto ents = EntityQuery<>({.ignore_temp_warning = true}).gen();
    for (Entity &e : ents) {
        handles.push_back(EntityHelper::handle_for(e));
    }

    volatile int sink = 0;
    for (int it = 0; it < 2000; ++it) {
        int valid = 0;
        for (const auto &h : handles) {
            if (EntityHelper::resolve(h).valid())
                valid++;
        }
        sink = valid;
    }
    (void)sink;
    cleanup_all();
}

// Phase 4: Game tick (query + component access + update)
static void __attribute__((noinline)) phase_game_tick() {
    cleanup_all();
    for (int i = 0; i < 5000; ++i) {
        auto &e = EntityHelper::createEntity();
        e.addComponent<Position>().x = static_cast<float>(i);
        e.addComponent<Velocity>();
    }
    EntityHelper::merge_entity_arrays();

    for (int it = 0; it < 2000; ++it) {
        auto movers = EntityQuery<>({.ignore_temp_warning = true})
            .whereHasComponent<Position>()
            .whereHasComponent<Velocity>()
            .gen();
        for (Entity &e : movers) {
            auto &pos = e.get<Position>();
            const auto &vel = e.get<Velocity>();
            pos.x += vel.vx;
            pos.y += vel.vy;
        }
    }
    cleanup_all();
}

// Phase 5: Churn (create + destroy cycles)
static void __attribute__((noinline)) phase_churn() {
    cleanup_all();
    for (int it = 0; it < 300; ++it) {
        std::vector<int> ids;
        ids.reserve(1000);
        for (int i = 0; i < 1000; ++i) {
            auto &e = EntityHelper::createEntity();
            e.addComponent<Position>();
            ids.push_back(e.id);
        }
        EntityHelper::merge_entity_arrays();
        for (int id : ids) {
            EntityHelper::markIDForCleanup(id);
        }
        EntityHelper::cleanup();
    }
    cleanup_all();
}

int main() {
    std::cout << "Profiling harness starting (PID " << getpid() << ")" << std::endl;
    std::cout << "Run: sample " << getpid() << " 10 -file /tmp/ah_profile.txt" << std::endl;
    std::cout.flush();

    auto t0 = Clock::now();
    phase_entity_creation();
    auto t1 = Clock::now();
    std::cout << "phase_entity_creation: " << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count() << " ms" << std::endl;

    t0 = Clock::now();
    phase_query();
    t1 = Clock::now();
    std::cout << "phase_query: " << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count() << " ms" << std::endl;

    t0 = Clock::now();
    phase_handle_resolve();
    t1 = Clock::now();
    std::cout << "phase_handle_resolve: " << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count() << " ms" << std::endl;

    t0 = Clock::now();
    phase_game_tick();
    t1 = Clock::now();
    std::cout << "phase_game_tick: " << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count() << " ms" << std::endl;

    t0 = Clock::now();
    phase_churn();
    t1 = Clock::now();
    std::cout << "phase_churn: " << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count() << " ms" << std::endl;

    std::cout << "Done." << std::endl;
    return 0;
}
