#pragma once

#include "../core/system.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#if AFTERHOURS_ENABLE_PROFILING
#include <map>
#include "process_metrics.h"
#endif

namespace afterhours::profiling {

inline constexpr bool available = AFTERHOURS_ENABLE_PROFILING != 0;
struct Options {
    std::size_t history = 600;
    std::size_t max_systems = 256;
    std::size_t max_counters = 32;
    bool sample_process = true;
};
struct SystemSample {
    std::string name;
    SystemPhase phase = SystemPhase::Update;
    double total_ms = 0;
    double last_frame_ms = 0;
    double average_frame_ms = 0;
    double recent_average_frame_ms = 0;
    double peak_frame_ms = 0;
    std::uint64_t calls = 0;
    std::vector<double> frame_history;
    double mean_ms() const { return calls ? total_ms / static_cast<double>(calls) : 0; }
};
struct Counter { std::string name, unit; double value = 0; };
struct Snapshot {
    std::vector<double> frame_ms;
    std::vector<SystemSample> systems;
    std::vector<Counter> counters;
    std::uint64_t frames = 0;
    std::uint64_t dropped_system_samples = 0;
    double fps = 0, average_frame_ms = 0, p50_ms = 0, p95_ms = 0, p99_ms = 0;
    std::optional<double> cpu_percent, resident_mb;
};


inline Snapshot select_frames(const Snapshot &source, std::size_t first, std::size_t last) {
    Snapshot result;
    if (source.frame_ms.empty()) return result;
    first = std::min(first, source.frame_ms.size() - 1);
    last = std::min(last, source.frame_ms.size() - 1);
    if (first > last) std::swap(first, last);
    const auto count = last - first + 1;
    result.frames = source.frames - source.frame_ms.size() + last + 1;
    result.frame_ms.assign(source.frame_ms.begin() + first, source.frame_ms.begin() + last + 1);
    double total = 0;
    for (const auto ms : result.frame_ms) total += ms;
    result.average_frame_ms = total / static_cast<double>(count);
    result.fps = total > 0 ? 1000. * static_cast<double>(count) / total : 0;
    auto sorted = result.frame_ms;
    std::sort(sorted.begin(), sorted.end());
    const auto percentile = [&](double p) { return sorted[static_cast<std::size_t>(std::ceil(p * count)) - 1]; };
    result.p50_ms = percentile(.5);
    result.p95_ms = percentile(.95);
    result.p99_ms = percentile(.99);
    for (const auto &sample : source.systems) {
        if (sample.frame_history.size() != source.frame_ms.size()) continue;
        SystemSample selected;
        selected.name = sample.name;
        selected.phase = sample.phase;
        for (auto i = first; i <= last; ++i) {
            selected.total_ms += sample.frame_history[i];
            selected.peak_frame_ms = std::max(selected.peak_frame_ms, sample.frame_history[i]);
        }
        selected.average_frame_ms = selected.recent_average_frame_ms = selected.total_ms / static_cast<double>(count);
        selected.last_frame_ms = sample.frame_history[last];
        result.systems.push_back(std::move(selected));
    }
    return result;
}

class Collector {
 public:
    explicit Collector(Options options = {})
#if AFTERHOURS_ENABLE_PROFILING
        : options_(options), history_(std::max(std::size_t{1}, options.history))
#endif
    {
#if !AFTERHOURS_ENABLE_PROFILING
        (void)options;
#endif
    }
    ~Collector() { stop(); }
    Collector(const Collector &) = delete;
    Collector &operator=(const Collector &) = delete;
    bool start() {
#if AFTERHOURS_ENABLE_PROFILING
        if (token_) return true;
        token_ = detail::subscribe(this, [](void *context, std::string_view name, SystemPhase phase, double ms) {
            static_cast<Collector *>(context)->record(name, phase, ms);
        });
        frame_start_ = detail::Clock::now();
        return token_ != 0;
#else
        return false;
#endif
    }
    void stop() {
#if AFTERHOURS_ENABLE_PROFILING
        if (!token_) return;
        detail::unsubscribe(token_);
        token_ = 0;
        for (auto &[key, value] : systems_) {
            value.frame_ms = 0;
            value.frame_calls = 0;
        }
#endif
    }
    bool recording() const {
#if AFTERHOURS_ENABLE_PROFILING
        return token_ != 0;
#else
        return false;
#endif
    }
    void reset() {
#if AFTERHOURS_ENABLE_PROFILING
        const bool restart = recording();
        stop();
        systems_.clear();
        counters_.clear();
        frames_ = dropped_ = 0;
        last_process_ = {};
        cpu_percent_.reset();
        resident_mb_.reset();
        process_time_ = {};
        if (restart) start();
#endif
    }
    void end_frame() {
#if AFTERHOURS_ENABLE_PROFILING
        if (!recording()) return;
        const auto now = detail::Clock::now();
        end_frame(std::chrono::duration<double, std::milli>(now - frame_start_).count());
        frame_start_ = now;
#endif
    }
    void end_frame(double elapsed_ms) {
#if AFTERHOURS_ENABLE_PROFILING
        if (!recording() || !std::isfinite(elapsed_ms) || elapsed_ms < 0) return;
        const auto frame_index = frames_ % history_.size();
        history_[frame_index] = elapsed_ms;
        ++frames_;
        sample_process();
        for (auto &[key, value] : systems_) {
            value.sample.last_frame_ms = std::exchange(value.frame_ms, 0);
            value.recent_ms += value.sample.last_frame_ms - value.history[frame_index];
            value.history[frame_index] = value.sample.last_frame_ms;
            value.sample.total_ms += value.sample.last_frame_ms;
            value.sample.calls += std::exchange(value.frame_calls, 0);
        }
#else
        (void)elapsed_ms;
#endif
    }
    bool counter(std::string_view name, std::string_view unit, double value) {
#if AFTERHOURS_ENABLE_PROFILING
        if (!recording() || !std::isfinite(value)) return false;
        for (auto &counter : counters_) {
            if (counter.name != name) continue;
            counter.unit = unit;
            counter.value = value;
            return true;
        }
        if (counters_.size() >= options_.max_counters) return false;
        counters_.push_back({std::string(name), std::string(unit), value});
        return true;
#else
        (void)name; (void)unit; (void)value;
        return false;
#endif
    }
    Snapshot snapshot(bool include_system_history = false) const {
        Snapshot result;
#if AFTERHOURS_ENABLE_PROFILING
        result.cpu_percent = cpu_percent_;
        result.resident_mb = resident_mb_;
        result.frames = frames_;
        result.dropped_system_samples = dropped_;
        result.counters = counters_;
        const auto count = std::min<std::uint64_t>(frames_, history_.size());
        result.systems.reserve(systems_.size());
        for (const auto &[key, value] : systems_) {
            auto sample = value.sample;
            sample.recent_average_frame_ms = count ? std::max(0., value.recent_ms) / static_cast<double>(count) : 0;
            sample.average_frame_ms = frames_ ? sample.total_ms / static_cast<double>(frames_) : 0;
            if (include_system_history) {
                sample.frame_history.reserve(count);
                for (auto frame = frames_ - count; frame < frames_; ++frame)
                    sample.frame_history.push_back(value.history[frame % history_.size()]);
            }
            result.systems.push_back(std::move(sample));
        }
        std::sort(result.systems.begin(), result.systems.end(), [](const auto &a, const auto &b) {
            return a.recent_average_frame_ms > b.recent_average_frame_ms;
        });
        result.frame_ms.reserve(count);
        double total = 0;
        for (auto i = frames_ - count; i < frames_; ++i) {
            const double ms = history_[i % history_.size()];
            result.frame_ms.push_back(ms);
            total += ms;
        }
        if (count == 0) return result;
        result.average_frame_ms = total / static_cast<double>(count);
        result.fps = total > 0 ? 1000. * static_cast<double>(count) / total : 0;
        auto sorted = result.frame_ms;
        std::sort(sorted.begin(), sorted.end());
        auto percentile = [&](double p) { return sorted[static_cast<std::size_t>(std::ceil(p * count)) - 1]; };
        result.p50_ms = percentile(.50);
        result.p95_ms = percentile(.95);
        result.p99_ms = percentile(.99);
#endif
#if !AFTERHOURS_ENABLE_PROFILING
        (void)include_system_history;
#endif
        return result;
    }

 private:
#if AFTERHOURS_ENABLE_PROFILING
    struct Key {
        std::string name;
        SystemPhase phase;
    };
    struct KeyView { std::string_view name; SystemPhase phase; };
    struct Less {
        using is_transparent = void;
        bool operator()(const auto &a, const auto &b) const {
            if (a.name != b.name) return a.name < b.name;
            return a.phase < b.phase;
        }
    };
    struct Accum {
        SystemSample sample;
        std::vector<double> history;
        double frame_ms = 0;
        double recent_ms = 0;
        std::uint64_t frame_calls = 0;
    };
    Options options_;
    std::vector<double> history_;
    std::map<Key, Accum, Less> systems_;
    std::vector<Counter> counters_;
    std::uint64_t token_ = 0, frames_ = 0, dropped_ = 0;
    detail::Clock::time_point frame_start_{};
    detail::Clock::time_point process_time_{};
    ProcessMetrics last_process_{};
    std::optional<double> cpu_percent_, resident_mb_;
    void sample_process() {
        if (!options_.sample_process) return;
        const auto now = detail::Clock::now();
        const double seconds = std::chrono::duration<double>(now - process_time_).count();
        if (seconds < .5) return;
        const auto current = process_metrics();
        resident_mb_ = current.resident_mb;
        if (current.cpu_seconds && last_process_.cpu_seconds)
            cpu_percent_ = 100 * (*current.cpu_seconds - *last_process_.cpu_seconds) / seconds;
        last_process_ = current;
        process_time_ = now;
    }
    void record(std::string_view name, SystemPhase phase, double ms) {
        auto it = systems_.find(KeyView{name, phase});
        if (it == systems_.end()) {
            if (systems_.size() >= options_.max_systems) { ++dropped_; return; }
            it = systems_.emplace(Key{std::string(name), phase}, Accum{{std::string(name), phase}, std::vector<double>(history_.size())}).first;
        }
        auto &value = it->second;
        ++value.frame_calls;
        value.frame_ms += ms;
    }
#endif
};

inline Collector &default_collector() {
    static thread_local Collector collector;
    return collector;
}

}

#if AFTERHOURS_ENABLE_PROFILING
#define AFTERHOURS_PROFILE_COUNTER(collector, name, unit, value) \
    do { if ((collector).recording()) (collector).counter(name, unit, value); } while (false)
#else
#define AFTERHOURS_PROFILE_COUNTER(collector, name, unit, value) do {} while (false)
#endif
