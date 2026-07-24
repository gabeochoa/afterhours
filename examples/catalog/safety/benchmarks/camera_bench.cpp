// camera_bench.cpp
// Benchmark for the ONLY measurable perf claim in docs/speed/camera.md:
//   item #1 — "cache the HasCamera singleton pointer instead of querying every
//   frame". BeginCameraMode::once() and EndCameraMode::once() each call
//   EntityHelper::get_singleton_cmp<HasCamera>() once per frame (2 lookups per
//   rendered frame total).
//
// This bench isolates that lookup cost (backend-independent — it is pure ECS,
// no raylib/GL needed) and compares it to the proposed optimization of holding
// a cached raw pointer. It answers: how many nanoseconds/frame does the current
// code spend on the singleton lookups, and how much could caching save?
//
// Build (from this dir): see makefile target `camera_bench`. -O3 -DNDEBUG.
//
// ⚠️ Santa lockdown on this machine SIGKILLs freshly-compiled binaries
// (exit 137), so this may compile-clean but refuse to run. The numbers must
// come from an actual run; do not fabricate. Compile-clean is the floor.

#include <memory>
#include <vector>

#define AFTER_HOURS_ENTITY_HELPER
#define AFTER_HOURS_ENTITY_QUERY
#define AFTER_HOURS_SYSTEM
#include "../../../../ah.h"

#define CATCH_CONFIG_ENABLE_BENCHMARKING
#define CATCH_CONFIG_MAIN
#include "../../../../vendor/catch2/catch.hpp"

using namespace afterhours;

// Mirror of camera::HasCamera's storage footprint for the non-raylib bench.
// The real component (behind AFTER_HOURS_USE_RAYLIB) holds a raylib::Camera2D
// (6 floats). We use an equivalent POD so the singleton map / get<> path is
// exercised with a representative component size. (The lookup cost is dominated
// by the unordered_map hash+probe + Entity::get<>, not by the payload size.)
struct HasCameraLike : BaseComponent {
    float off_x = 0.f, off_y = 0.f;
    float tgt_x = 0.f, tgt_y = 0.f;
    float rotation = 0.f;
    float zoom = 0.75f;
};

static void cleanup_all() {
    EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();
}

TEST_CASE("camera_singleton_lookup", "[benchmark][camera]") {
    cleanup_all();

    // Populate a realistic entity population so the singleton map is not the
    // only thing in the collection (matches an in-game frame).
    for (int i = 0; i < 5000; ++i)
        EntityHelper::createEntity();
    auto &cam_entity = EntityHelper::createEntity();
    cam_entity.addComponent<HasCameraLike>().zoom = 0.75f;
    EntityHelper::merge_entity_arrays();
    EntityHelper::registerSingleton<HasCameraLike>(cam_entity);

    // CURRENT CODE: begin + end each do one get_singleton_cmp per frame.
    // Measure 2 lookups == the per-frame cost the plugin pays today.
    BENCHMARK_ADVANCED("current: 2x get_singleton_cmp per frame (x100000 frames)")
    (Catch::Benchmark::Chronometer meter) {
        meter.measure([] {
            float acc = 0.f;
            for (int f = 0; f < 100000; ++f) {
                // BeginCameraMode::once()
                acc += EntityHelper::get_singleton_cmp<HasCameraLike>()->zoom;
                // EndCameraMode::once()
                acc += EntityHelper::get_singleton_cmp<HasCameraLike>()->zoom;
            }
            return acc;
        });
    };

    // PROPOSED OPT (#1): cache the pointer once, reuse every frame.
    BENCHMARK_ADVANCED("proposed: cached pointer, 2x deref per frame (x100000 frames)")
    (Catch::Benchmark::Chronometer meter) {
        HasCameraLike *cached = EntityHelper::get_singleton_cmp<HasCameraLike>();
        meter.measure([cached] {
            float acc = 0.f;
            for (int f = 0; f < 100000; ++f) {
                acc += cached->zoom; // begin
                acc += cached->zoom; // end
            }
            return acc;
        });
    };

    cleanup_all();
}
