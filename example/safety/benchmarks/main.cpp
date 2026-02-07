
// ECS Performance Benchmarks
//
// Measures throughput of core ECS operations:
// - Entity creation and destruction
// - Component add/get
// - EntityQuery with various filters
// - EntityHandle resolution
// - Tag operations
// - Singleton access
//
// To run: make && ./ecs_benchmarks.exe
//
// Methodology:
// - Compiled with -O3 -DNDEBUG for meaningful results
// - Each BENCHMARK_ADVANCED sets up state, then measures a read-only or
//   self-contained workload so repeated iterations don't corrupt state.

#include <algorithm>
#include <array>
#include <functional>
#include <map>
#include <memory>
#include <numeric>
#include <vector>

#define AFTER_HOURS_ENTITY_HELPER
#define AFTER_HOURS_ENTITY_QUERY
#define AFTER_HOURS_SYSTEM
#include "../../../ah.h"
#include "../../../src/core/opt_entity_handle.h"
#include "../../../src/core/snapshot.h"

#define CATCH_CONFIG_ENABLE_BENCHMARKING
#define CATCH_CONFIG_MAIN
#include "../../../vendor/catch2/catch.hpp"

#include "../../core/tag_filter_regression/demo_tags.h"

using namespace afterhours;

// ============================================================================
// Test components
// ============================================================================

struct Position : BaseComponent {
    float x = 0.f, y = 0.f, z = 0.f;
};

struct Velocity : BaseComponent {
    float vx = 0.f, vy = 0.f, vz = 0.f;
};

struct Health : BaseComponent {
    int hp = 100;
    int max_hp = 100;
};

struct Marker : BaseComponent {};

// ============================================================================
// Helpers
// ============================================================================

static void cleanup_all() {
    EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();
}

// ============================================================================
// ENTITY CREATION BENCHMARKS
// Each iteration does full create + merge + cleanup cycle.
// ============================================================================

TEST_CASE("entity_creation", "[benchmark][creation]") {
    BENCHMARK_ADVANCED("create+merge+cleanup 1000 entities")
    (Catch::Benchmark::Chronometer meter) {
        meter.measure([] {
            for (int i = 0; i < 1000; ++i) {
                EntityHelper::createEntity();
            }
            EntityHelper::merge_entity_arrays();
            auto sz = EntityHelper::get_entities().size();
            cleanup_all();
            return sz;
        });
    };

    BENCHMARK_ADVANCED("create+merge+cleanup 10000 entities")
    (Catch::Benchmark::Chronometer meter) {
        meter.measure([] {
            for (int i = 0; i < 10000; ++i) {
                EntityHelper::createEntity();
            }
            EntityHelper::merge_entity_arrays();
            auto sz = EntityHelper::get_entities().size();
            cleanup_all();
            return sz;
        });
    };
}

// ============================================================================
// ENTITY CHURN BENCHMARK
// ============================================================================

TEST_CASE("entity_churn", "[benchmark][churn]") {
    BENCHMARK_ADVANCED("create 100 + destroy 100, 10 rounds")
    (Catch::Benchmark::Chronometer meter) {
        meter.measure([] {
            for (int round = 0; round < 10; ++round) {
                std::vector<int> ids;
                ids.reserve(100);
                for (int i = 0; i < 100; ++i) {
                    ids.push_back(EntityHelper::createEntity().id);
                }
                EntityHelper::merge_entity_arrays();
                for (int id : ids) {
                    EntityHelper::markIDForCleanup(id);
                }
                EntityHelper::cleanup();
            }
            cleanup_all();
            return 0;
        });
    };
}

// ============================================================================
// COMPONENT OPERATION BENCHMARKS
// Read-only benchmarks against pre-built entity sets.
// ============================================================================

TEST_CASE("component_get", "[benchmark][component]") {
    cleanup_all();
    for (int i = 0; i < 1000; ++i) {
        auto &e = EntityHelper::createEntity();
        e.addComponent<Position>().x = static_cast<float>(i);
    }
    EntityHelper::merge_entity_arrays();
    auto all_ents = EntityQuery<>({.ignore_temp_warning = true}).gen();

    BENCHMARK_ADVANCED("get<Position> from 1000 entities")
    (Catch::Benchmark::Chronometer meter) {
        meter.measure([&all_ents] {
            float sum = 0.f;
            for (Entity &e : all_ents) {
                sum += e.get<Position>().x;
            }
            return sum;
        });
    };

    BENCHMARK_ADVANCED("has<Position>() on 1000 entities")
    (Catch::Benchmark::Chronometer meter) {
        meter.measure([&all_ents] {
            int count = 0;
            for (Entity &e : all_ents) {
                if (e.has<Position>())
                    count++;
            }
            return count;
        });
    };

    cleanup_all();
}

TEST_CASE("component_add_remove_cycle", "[benchmark][component]") {
    // Each iteration adds + removes so state is restored for next iteration.
    BENCHMARK_ADVANCED("add+remove Position on 1000 entities")
    (Catch::Benchmark::Chronometer meter) {
        cleanup_all();
        std::vector<Entity *> ents;
        ents.reserve(1000);
        for (int i = 0; i < 1000; ++i) {
            ents.push_back(&EntityHelper::createEntity());
        }
        EntityHelper::merge_entity_arrays();

        meter.measure([&ents] {
            for (auto *e : ents) {
                e->addComponent<Position>();
            }
            for (auto *e : ents) {
                e->removeComponent<Position>();
            }
            return ents.size();
        });
        cleanup_all();
    };
}

// ============================================================================
// ENTITY QUERY BENCHMARKS
// Read-only queries against pre-built entity sets.
// ============================================================================

TEST_CASE("query_benchmarks", "[benchmark][query]") {
    cleanup_all();
    for (int i = 0; i < 10000; ++i) {
        auto &e = EntityHelper::createEntity();
        e.addComponent<Position>().x = static_cast<float>(i);
        if (i % 2 == 0)
            e.addComponent<Velocity>();
        if (i % 3 == 0)
            e.enableTag(DemoTag::Runner);
        if (i % 5 == 0)
            e.enableTag(DemoTag::Store);
    }
    EntityHelper::merge_entity_arrays();

    BENCHMARK_ADVANCED("query all 10000 (no filter)")
    (Catch::Benchmark::Chronometer meter) {
        meter.measure([] {
            return EntityQuery<>({.ignore_temp_warning = true})
                .gen()
                .size();
        });
    };

    BENCHMARK_ADVANCED("whereHasComponent on 10000 (100% match)")
    (Catch::Benchmark::Chronometer meter) {
        meter.measure([] {
            return EntityQuery<>({.ignore_temp_warning = true})
                .whereHasComponent<Position>()
                .gen()
                .size();
        });
    };

    BENCHMARK_ADVANCED("whereHasComponent on 10000 (50% match)")
    (Catch::Benchmark::Chronometer meter) {
        meter.measure([] {
            return EntityQuery<>({.ignore_temp_warning = true})
                .whereHasComponent<Velocity>()
                .gen()
                .size();
        });
    };

    BENCHMARK_ADVANCED("2 component filters on 10000 (50% match)")
    (Catch::Benchmark::Chronometer meter) {
        meter.measure([] {
            return EntityQuery<>({.ignore_temp_warning = true})
                .whereHasComponent<Position>()
                .whereHasComponent<Velocity>()
                .gen()
                .size();
        });
    };

    BENCHMARK_ADVANCED("tag filter on 10000 (33% match)")
    (Catch::Benchmark::Chronometer meter) {
        meter.measure([] {
            return EntityQuery<>({.ignore_temp_warning = true})
                .whereHasAnyTag(DemoTag::Runner)
                .gen()
                .size();
        });
    };

    BENCHMARK_ADVANCED("component+tag+not on 10000")
    (Catch::Benchmark::Chronometer meter) {
        meter.measure([] {
            return EntityQuery<>({.ignore_temp_warning = true})
                .whereHasComponent<Position>()
                .whereHasAnyTag(DemoTag::Runner)
                .whereHasNoTags(DemoTag::Store)
                .gen()
                .size();
        });
    };

    BENCHMARK_ADVANCED("gen_first on 10000 (early match)")
    (Catch::Benchmark::Chronometer meter) {
        meter.measure([] {
            auto r = EntityQuery<>({.ignore_temp_warning = true})
                         .whereHasComponent<Position>()
                         .gen_first();
            return r.valid() ? 1 : 0;
        });
    };

    BENCHMARK_ADVANCED("has_values on 10000 (early match)")
    (Catch::Benchmark::Chronometer meter) {
        meter.measure([] {
            return EntityQuery<>({.ignore_temp_warning = true})
                       .whereHasComponent<Position>()
                       .has_values()
                       ? 1
                       : 0;
        });
    };

    BENCHMARK_ADVANCED("gen_count on 10000")
    (Catch::Benchmark::Chronometer meter) {
        meter.measure([] {
            return EntityQuery<>({.ignore_temp_warning = true}).gen_count();
        });
    };

    cleanup_all();
}

TEST_CASE("query_ordered", "[benchmark][query]") {
    cleanup_all();
    for (int i = 0; i < 1000; ++i) {
        auto &e = EntityHelper::createEntity();
        e.addComponent<Position>().x = static_cast<float>(1000 - i);
    }
    EntityHelper::merge_entity_arrays();

    BENCHMARK_ADVANCED("query + orderByLambda on 1000")
    (Catch::Benchmark::Chronometer meter) {
        meter.measure([] {
            return EntityQuery<>({.ignore_temp_warning = true})
                .whereHasComponent<Position>()
                .orderByLambda([](const Entity &a, const Entity &b) {
                    return a.get<Position>().x < b.get<Position>().x;
                })
                .gen()
                .size();
        });
    };

    cleanup_all();
}

// ============================================================================
// ENTITY HANDLE BENCHMARKS
// ============================================================================

TEST_CASE("handle_benchmarks", "[benchmark][handle]") {
    cleanup_all();
    std::vector<EntityHandle> handles;
    handles.reserve(10000);
    for (int i = 0; i < 10000; ++i) {
        EntityHelper::createEntity();
    }
    EntityHelper::merge_entity_arrays();

    auto ents = EntityQuery<>({.ignore_temp_warning = true}).gen();
    for (Entity &e : ents) {
        handles.push_back(EntityHelper::handle_for(e));
    }

    BENCHMARK_ADVANCED("handle_for on 10000 entities")
    (Catch::Benchmark::Chronometer meter) {
        meter.measure([&ents] {
            int valid = 0;
            for (Entity &e : ents) {
                if (EntityHelper::handle_for(e).valid())
                    valid++;
            }
            return valid;
        });
    };

    BENCHMARK_ADVANCED("resolve 10000 valid handles")
    (Catch::Benchmark::Chronometer meter) {
        meter.measure([&handles] {
            int valid = 0;
            for (const auto &h : handles) {
                if (EntityHelper::resolve(h).valid())
                    valid++;
            }
            return valid;
        });
    };

    cleanup_all();

    // Now handles are stale — measure stale resolution cost.
    BENCHMARK_ADVANCED("resolve 10000 stale handles")
    (Catch::Benchmark::Chronometer meter) {
        meter.measure([&handles] {
            int stale = 0;
            for (const auto &h : handles) {
                if (!EntityHelper::resolve(h).valid())
                    stale++;
            }
            return stale;
        });
    };
}

// ============================================================================
// TAG OPERATION BENCHMARKS
// ============================================================================

TEST_CASE("tag_benchmarks", "[benchmark][tags]") {
    cleanup_all();
    for (int i = 0; i < 1000; ++i) {
        auto &e = EntityHelper::createEntity();
        e.enableTag(DemoTag::Runner);
        if (i % 2 == 0)
            e.enableTag(DemoTag::Chaser);
        if (i % 3 == 0)
            e.enableTag(DemoTag::Store);
    }
    EntityHelper::merge_entity_arrays();

    auto ents = EntityQuery<>({.ignore_temp_warning = true}).gen();

    TagBitset runner_chaser;
    runner_chaser.set(static_cast<TagId>(DemoTag::Runner));
    runner_chaser.set(static_cast<TagId>(DemoTag::Chaser));

    TagBitset store_mask;
    store_mask.set(static_cast<TagId>(DemoTag::Store));

    BENCHMARK_ADVANCED("hasTag check on 1000 entities")
    (Catch::Benchmark::Chronometer meter) {
        meter.measure([&ents] {
            int count = 0;
            for (Entity &e : ents) {
                if (e.hasTag(DemoTag::Runner))
                    count++;
                if (e.hasTag(DemoTag::Chaser))
                    count++;
                if (e.hasTag(DemoTag::Store))
                    count++;
            }
            return count;
        });
    };

    BENCHMARK_ADVANCED("hasAllTags/hasAnyTag/hasNoTags on 1000")
    (Catch::Benchmark::Chronometer meter) {
        meter.measure([&ents, &runner_chaser, &store_mask] {
            int count = 0;
            for (Entity &e : ents) {
                if (e.hasAllTags(runner_chaser))
                    count++;
                if (e.hasAnyTag(runner_chaser))
                    count++;
                if (e.hasNoTags(store_mask))
                    count++;
            }
            return count;
        });
    };

    cleanup_all();
}

// ============================================================================
// SINGLETON & LOOKUP BENCHMARKS
// ============================================================================

struct BenchConfig : BaseComponent {
    int value = 42;
};

TEST_CASE("singleton_and_lookup", "[benchmark][singleton]") {
    cleanup_all();

    std::vector<int> ids;
    ids.reserve(10000);
    for (int i = 0; i < 10000; ++i) {
        ids.push_back(EntityHelper::createEntity().id);
    }
    // Add singleton on the first entity
    auto &first =
        EntityHelper::get_temp().front();
    first->addComponent<BenchConfig>().value = 99;
    EntityHelper::merge_entity_arrays();
    EntityHelper::registerSingleton<BenchConfig>(*first);

    BENCHMARK_ADVANCED("get_singleton_cmp 10000 calls")
    (Catch::Benchmark::Chronometer meter) {
        meter.measure([] {
            int sum = 0;
            for (int i = 0; i < 10000; ++i) {
                sum += EntityHelper::get_singleton_cmp<BenchConfig>()->value;
            }
            return sum;
        });
    };

    BENCHMARK_ADVANCED("has_singleton 10000 calls")
    (Catch::Benchmark::Chronometer meter) {
        meter.measure([] {
            int count = 0;
            for (int i = 0; i < 10000; ++i) {
                if (EntityHelper::has_singleton<BenchConfig>())
                    count++;
            }
            return count;
        });
    };

    BENCHMARK_ADVANCED("getEntityForID 10000 lookups")
    (Catch::Benchmark::Chronometer meter) {
        meter.measure([&ids] {
            int found = 0;
            for (int id : ids) {
                if (EntityHelper::getEntityForID(id).valid())
                    found++;
            }
            return found;
        });
    };

    cleanup_all();
}

// ============================================================================
// SNAPSHOT BENCHMARKS
// ============================================================================

struct SnapPosition : BaseComponent {
    float x = 0.f, y = 0.f;
};

struct SnapPositionDTO {
    float x = 0.f, y = 0.f;
};

TEST_CASE("snapshot_benchmarks", "[benchmark][snapshot]") {
    cleanup_all();
    for (int i = 0; i < 1000; ++i) {
        auto &e = EntityHelper::createEntity();
        auto &pos = e.addComponent<SnapPosition>();
        pos.x = static_cast<float>(i);
        pos.y = static_cast<float>(i * 2);
        e.enableTag(DemoTag::Runner);
        e.entity_type = i % 5;
    }
    EntityHelper::merge_entity_arrays();

    BENCHMARK_ADVANCED("take_entities snapshot of 1000")
    (Catch::Benchmark::Chronometer meter) {
        meter.measure([] {
            return snapshot::take_entities({.force_merge = false}).size();
        });
    };

    BENCHMARK_ADVANCED("take_components snapshot of 1000")
    (Catch::Benchmark::Chronometer meter) {
        meter.measure([] {
            return snapshot::take_components<SnapPosition, SnapPositionDTO>(
                       [](const SnapPosition &p) {
                           return SnapPositionDTO{.x = p.x, .y = p.y};
                       },
                       {.force_merge = false})
                .size();
        });
    };

    cleanup_all();
}

// ============================================================================
// MIXED WORKLOAD BENCHMARKS
// ============================================================================

TEST_CASE("mixed_workload", "[benchmark][mixed]") {
    cleanup_all();
    for (int i = 0; i < 5000; ++i) {
        auto &e = EntityHelper::createEntity();
        auto &pos = e.addComponent<Position>();
        pos.x = static_cast<float>(i);
        pos.y = static_cast<float>(i);
        auto &vel = e.addComponent<Velocity>();
        vel.vx = 1.f;
        vel.vy = 0.5f;
        if (i % 3 == 0)
            e.enableTag(DemoTag::Runner);
    }
    EntityHelper::merge_entity_arrays();

    BENCHMARK_ADVANCED("game tick: query+update 5000 entities")
    (Catch::Benchmark::Chronometer meter) {
        meter.measure([] {
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
            return movers.size();
        });
    };

    BENCHMARK_ADVANCED("selective tick: tagged subset of 5000")
    (Catch::Benchmark::Chronometer meter) {
        meter.measure([] {
            auto runners = EntityQuery<>({.ignore_temp_warning = true})
                               .whereHasComponent<Position>()
                               .whereHasComponent<Velocity>()
                               .whereHasAnyTag(DemoTag::Runner)
                               .gen();
            for (Entity &e : runners) {
                auto &pos = e.get<Position>();
                const auto &vel = e.get<Velocity>();
                pos.x += vel.vx;
            }
            return runners.size();
        });
    };

    cleanup_all();
}

// ============================================================================
// REBUILD HANDLE STORE & ITERATION
// ============================================================================

TEST_CASE("iteration_benchmarks", "[benchmark][iteration]") {
    cleanup_all();
    for (int i = 0; i < 5000; ++i) {
        auto &e = EntityHelper::createEntity();
        e.addComponent<Position>().x = static_cast<float>(i);
    }
    EntityHelper::merge_entity_arrays();

    BENCHMARK_ADVANCED("forEachEntity iterate 5000")
    (Catch::Benchmark::Chronometer meter) {
        meter.measure([] {
            float sum = 0.f;
            EntityHelper::forEachEntity([&](Entity &e) {
                if (e.has<Position>()) {
                    sum += e.get<Position>().x;
                }
                return EntityHelper::NormalFlow;
            });
            return sum;
        });
    };

    auto &collection = EntityHelper::get_default_collection();

    BENCHMARK_ADVANCED("rebuild_handle_store from 5000")
    (Catch::Benchmark::Chronometer meter) {
        meter.measure([&collection] {
            collection.rebuild_handle_store_from_entities();
            return 0;
        });
    };

    cleanup_all();
}
