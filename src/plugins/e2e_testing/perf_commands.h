// Optional perf-oriented E2E commands.
// This module is intentionally opt-in: projects must explicitly register it.
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "../../logging.h"
#include "pending_command.h"

namespace afterhours {
namespace testing {
namespace perf_commands {

struct PerfEntry {
    std::string name;
    float ms = 0.f;
    std::optional<int> entity_count;
};

struct PerfProvider {
    // Return current averaged FPS if available.
    std::function<std::optional<float>()> get_fps;
    // Return current p99 frame time in ms if available.
    std::function<std::optional<float>()> get_p99_ms;
    // Return top profile entries (already sorted or unsorted; handler sorts).
    std::function<std::vector<PerfEntry>(int)> top_entries;
};

inline PerfProvider &provider() {
    static PerfProvider p;
    return p;
}

inline void set_provider(PerfProvider p) { provider() = std::move(p); }

struct HandleDumpProfileCommand : System<PendingE2ECommand> {
    void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
        if (cmd.is_consumed() || !cmd.is("dump_profile")) return;

        PerfProvider &p = provider();
        if (!p.top_entries) {
            cmd.fail("dump_profile unavailable: no perf provider configured");
            return;
        }

        int count = cmd.has_args(1) ? cmd.arg_as<int>(0) : 20;
        std::vector<PerfEntry> entries = p.top_entries(count);
        std::sort(entries.begin(), entries.end(),
                  [](const PerfEntry &a, const PerfEntry &b) {
                      return a.ms > b.ms;
                  });
        if (static_cast<int>(entries.size()) > count) entries.resize(count);

        log_info("=== PERFORMANCE PROFILE ===");
        if (p.get_fps) {
            std::optional<float> fps = p.get_fps();
            if (fps) log_info("Avg FPS: {:.1f}", *fps);
        }
        if (p.get_p99_ms) {
            std::optional<float> p99 = p.get_p99_ms();
            if (p99) log_info("P99 frame time: {:.2f}ms", *p99);
        }
        log_info("--- Top {} systems by avg time ---", entries.size());
        for (const PerfEntry &e : entries) {
            if (e.entity_count.has_value()) {
                log_info("  {:.2f}ms  {} ({} entities)", e.ms, e.name,
                         *e.entity_count);
            } else {
                log_info("  {:.2f}ms  {}", e.ms, e.name);
            }
        }
        log_info("=== END PROFILE ===");
        cmd.consume();
    }
};

struct HandleExpectFpsAboveCommand : System<PendingE2ECommand> {
    void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
        if (cmd.is_consumed() || !cmd.is("expect_fps_above")) return;
        if (!cmd.has_args(1)) {
            cmd.fail("expect_fps_above requires: min_fps");
            return;
        }

        PerfProvider &p = provider();
        if (!p.get_fps) {
            cmd.fail("expect_fps_above unavailable: no fps provider configured");
            return;
        }
        std::optional<float> fps = p.get_fps();
        if (!fps) {
            cmd.fail("expect_fps_above unavailable: fps value not available");
            return;
        }

        float min_fps = cmd.arg_as<float>(0);
        if (*fps < min_fps) {
            cmd.fail(std::format("FPS {:.1f} is below minimum {:.1f}", *fps,
                                 min_fps));
            return;
        }
        log_info("expect_fps_above: {:.1f} fps >= {:.1f} (PASS)", *fps,
                 min_fps);
        cmd.consume();
    }
};

struct HandleExpectP99BelowCommand : System<PendingE2ECommand> {
    void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
        if (cmd.is_consumed() || !cmd.is("expect_p99_below")) return;
        if (!cmd.has_args(1)) {
            cmd.fail("expect_p99_below requires: max_ms");
            return;
        }

        PerfProvider &p = provider();
        if (!p.get_p99_ms) {
            cmd.fail("expect_p99_below unavailable: no p99 provider configured");
            return;
        }
        std::optional<float> p99 = p.get_p99_ms();
        if (!p99) {
            cmd.fail(
                "expect_p99_below unavailable: p99 value not available");
            return;
        }

        float max_ms = cmd.arg_as<float>(0);
        if (*p99 > max_ms) {
            cmd.fail(std::format("P99 {:.2f}ms is above maximum {:.2f}ms", *p99,
                                 max_ms));
            return;
        }
        log_info("expect_p99_below: {:.2f}ms <= {:.2f}ms (PASS)", *p99,
                 max_ms);
        cmd.consume();
    }
};

inline void register_perf_commands(SystemManager &sm) {
    sm.register_update_system(std::make_unique<HandleDumpProfileCommand>());
    sm.register_update_system(std::make_unique<HandleExpectFpsAboveCommand>());
    sm.register_update_system(std::make_unique<HandleExpectP99BelowCommand>());
}

}  // namespace perf_commands
}  // namespace testing
}  // namespace afterhours
