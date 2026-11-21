// Optional perf-oriented E2E commands.
// This module is intentionally opt-in: projects must explicitly register it.
#pragma once
#include <utility>
#include <map>
#include <chrono>
#include <format>
#include <sstream>

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "../../logging.h"
#include "../../warn_once.h"
#include "../profiling.h"
#include "pending_command.h"

namespace afterhours {
namespace testing {
namespace perf_commands {

struct PerfEntry {
    std::string name;
    float ms = 0.f;
    std::optional<int> entity_count;
    std::optional<std::uint64_t> calls;
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

inline void set_provider(PerfProvider p) {
    if (provider().get_fps || provider().get_p99_ms || provider().top_entries)
        warn_once(0, "perf_commands::set_provider: overwriting an existing perf provider, the previous provider will be discarded");
    provider() = std::move(p);
}

// A profile the library fills in itself, so dump_profile works without the
// consumer writing one.
namespace builtin_profile {

inline void reset() { profiling::default_collector().reset(); }

inline void enable() {
    if (!profiling::default_collector().start())
        warn_once(0, "builtin_profile::enable: profiling is unavailable (AFTERHOURS_ENABLE_PROFILING=0), so the builtin profile will report no data; rebuild with AFTERHOURS_ENABLE_PROFILING=1 to collect it");
    PerfProvider p = provider();
    p.get_fps = []() -> std::optional<float> {
        const auto snapshot = profiling::default_collector().snapshot();
        if (!snapshot.frames) return std::nullopt;
        return static_cast<float>(snapshot.fps);
    };
    p.get_p99_ms = []() -> std::optional<float> {
        const auto snapshot = profiling::default_collector().snapshot();
        if (!snapshot.frames) return std::nullopt;
        return static_cast<float>(snapshot.p99_ms);
    };
    p.top_entries = [](int count) {
        std::vector<PerfEntry> out;
        for (const auto &sample : profiling::default_collector().snapshot().systems) {
            out.push_back({sample.name, static_cast<float>(sample.mean_ms()), std::nullopt, sample.calls});
        }
        std::sort(out.begin(), out.end(), [](const auto &a, const auto &b) { return a.ms > b.ms; });
        if (count > 0 && out.size() > static_cast<std::size_t>(count)) out.resize(count);
        return out;
    };
    set_provider(std::move(p));
}

inline void disable() { profiling::default_collector().stop(); }

} // namespace builtin_profile

inline std::string escape_json_string(std::string_view in) {
    std::string out;
    out.reserve(in.size());
    for (const char c : in) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out += c; break;
        }
    }
    return out;
}

inline const char *phase_name(SystemPhase phase) {
    switch (phase) {
    case SystemPhase::Render: return "render";
    case SystemPhase::FixedUpdate: return "fixed";
    default: return "update";
    }
}

inline std::string snapshot_json(const profiling::Snapshot &snapshot) {
    std::ostringstream out;
    out << std::format(
        "{{\"frames\":{},\"fps\":{:.3f},\"average_frame_ms\":{:.3f},"
        "\"p50_ms\":{:.3f},\"p95_ms\":{:.3f},\"p99_ms\":{:.3f},"
        "\"dropped_system_samples\":{},\"systems\":[",
        snapshot.frames, snapshot.fps, snapshot.average_frame_ms,
        snapshot.p50_ms, snapshot.p95_ms, snapshot.p99_ms,
        snapshot.dropped_system_samples);
    bool first = true;
    for (const auto &sample : snapshot.systems) {
        if (!first) out << ',';
        first = false;
        out << std::format(
            "{{\"name\":\"{}\",\"phase\":\"{}\",\"mean_ms\":{:.3f},"
            "\"average_frame_ms\":{:.3f},\"recent_average_frame_ms\":{:.3f},"
            "\"peak_frame_ms\":{:.3f},\"last_frame_ms\":{:.3f},\"calls\":{}}}",
            escape_json_string(sample.name), phase_name(sample.phase),
            sample.mean_ms(), sample.average_frame_ms,
            sample.recent_average_frame_ms, sample.peak_frame_ms,
            sample.last_frame_ms, sample.calls);
    }
    out << "],\"counters\":[";
    bool first_counter = true;
    for (const auto &counter : snapshot.counters) {
        if (!first_counter) out << ',';
        first_counter = false;
        out << std::format("{{\"name\":\"{}\",\"unit\":\"{}\",\"value\":{:.3f}}}",
                           escape_json_string(counter.name),
                           escape_json_string(counter.unit), counter.value);
    }
    out << "]}";
    return out.str();
}

struct HandleDumpProfileCommand : System<PendingE2ECommand> {
    void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
        if (cmd.is_consumed() || !cmd.is("dump_profile")) return;

        bool want_json = false;
        int count = 20;
        bool count_seen = false;
        for (std::size_t i = 0; i < cmd.args.size(); ++i) {
            const std::string &arg = cmd.args[i];
            if (arg == "--json") { want_json = true; continue; }
            if (arg.size() > 2 && arg[0] == '-' && arg[1] == '-') {
                cmd.fail("dump_profile: unknown flag '" + arg + "'");
                return;
            }
            if (!count_seen) {
                count = cmd.arg_as<int>(i, 0);
                count_seen = true;
            }
        }
        if (want_json) {
            if (!profiling::available) {
                cmd.fail("dump_profile --json unavailable: rebuild with "
                         "AFTERHOURS_ENABLE_PROFILING=1 to collect it");
                return;
            }
            const auto snapshot =
                profiling::default_collector().snapshot();
            if (!snapshot.frames) {
                cmd.fail("dump_profile --json unavailable: no frames recorded "
                         "(enable the builtin profile first)");
                return;
            }
            log_info("{}", snapshot_json(snapshot));
            cmd.consume();
            return;
        }

        PerfProvider &p = provider();
        if (!p.top_entries) {
            cmd.fail("dump_profile unavailable: no perf provider configured");
            return;
        }

        std::vector<PerfEntry> entries = p.top_entries(count);
        std::sort(entries.begin(), entries.end(),
                  [](const PerfEntry &a, const PerfEntry &b) {
                      return a.ms > b.ms;
                  });
        if (count > 0 && static_cast<int>(entries.size()) > count) entries.resize(count);

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
            if (e.calls) {
                log_info("  {:.2f}ms mean  {} ({} calls)", e.ms, e.name, *e.calls);
            } else if (e.entity_count.has_value()) {
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

struct HandleResetProfileCommand : System<PendingE2ECommand> {
    void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
        if (cmd.is_consumed() || !cmd.is("reset_profile")) return;
        builtin_profile::reset();
        cmd.consume();
    }
};

inline void register_perf_commands(SystemManager &sm) {
    sm.register_update_system(std::make_unique<HandleDumpProfileCommand>());
    sm.register_update_system(std::make_unique<HandleResetProfileCommand>());
    sm.register_update_system(std::make_unique<HandleExpectFpsAboveCommand>());
    sm.register_update_system(std::make_unique<HandleExpectP99BelowCommand>());
}

}  // namespace perf_commands
}  // namespace testing
}  // namespace afterhours
