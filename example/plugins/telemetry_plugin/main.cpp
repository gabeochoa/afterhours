#include <iostream>
#include <memory>
#include <string>
#include <cstdlib>

#define AFTER_HOURS_ENTITY_HELPER
#define AFTER_HOURS_SYSTEM
#include "../../../ah.h"
#include "../../../src/plugins/telemetry.h"

using namespace afterhours;

// Typed payload for a "session has started" event.
struct SessionStartEvent {
    bool headless = false;
    bool test_mode = false;
};

// Typed payload for entering a level.
struct LevelEnterEvent {
    std::string level_id;
};

namespace {
}  // namespace

// telemetry_traits maps C++ event types to wire event names + field encoding.
template<>
struct afterhours::telemetry_traits<SessionStartEvent> {
    static constexpr std::string_view name = "session_start";
    static TelemetryFields to_fields(const SessionStartEvent &e) {
        return {{"headless", e.headless ? "1" : "0"},
                {"test_mode", e.test_mode ? "1" : "0"}};
    }
};

template<>
struct afterhours::telemetry_traits<LevelEnterEvent> {
    static constexpr std::string_view name = "level_enter";
    static TelemetryFields to_fields(const LevelEnterEvent &e) {
        return {{"level_id", e.level_id}};
    }
};

int main(int, char **) {
    // afterhours systems drive telemetry flushing each update tick.
    SystemManager systems;

    const bool use_localhost = std::getenv("AH_TELEMETRY_USE_LOCALHOST") != nullptr;

    // Minimal plugin configuration shared on every event request.
    telemetry::Config cfg;
    cfg.enabled = true;
    cfg.endpoint = "http://localhost:8787/v1/ingest";
    cfg.game_version = "example-1.0.0";
    std::unique_ptr<TelemetryProvider> provider;
    if (use_localhost) {
        // Use afterhours' current HTTP provider hook. Today this is a placeholder
        // until shared transport lands, so localhost validation may show no
        // ingest events yet.
        provider = std::make_unique<HttpGetTelemetryProvider>();
        std::cout << "Localhost validation mode enabled (placeholder provider)."
                  << std::endl;
    } else {
        // Default mode stays offline and deterministic.
        provider = std::make_unique<NoopTelemetryProvider>();
        std::cout << "Offline mode (Noop provider). Set AH_TELEMETRY_USE_LOCALHOST=1 "
                     "to send real GETs."
                  << std::endl;
    }
    telemetry::init(cfg, std::move(provider));

    // Register singleton guards + the update system that drains the queue.
    telemetry::enforce_singletons(systems);
    telemetry::register_update_systems(systems);

    // Queue typed events (compile-time checked via telemetry_traits).
    telemetry::track(SessionStartEvent{.headless = false, .test_mode = false});
    telemetry::track(LevelEnterEvent{.level_id = "test_level"});

    // Run one update tick to flush queued events through the provider.
    systems.run(1.f / 60.f);
    // Flush provider state and clear plugin internals before exit.
    telemetry::shutdown();

    std::cout << "Telemetry plugin example completed successfully." << std::endl;
    return 0;
}
