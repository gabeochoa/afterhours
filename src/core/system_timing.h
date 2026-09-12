#pragma once

#ifndef AFTERHOURS_ENABLE_PROFILING
#define AFTERHOURS_ENABLE_PROFILING 0
#endif

#if AFTERHOURS_ENABLE_PROFILING
#include <array>
#include <chrono>
#include <cstdint>
#include <string_view>

namespace afterhours {
enum struct SystemPhase;
namespace profiling::detail {
using Clock = std::chrono::steady_clock;
using Callback = void (*)(void *, std::string_view, SystemPhase, double);
struct Observer {
    std::uint64_t token = 0;
    void *context = nullptr;
    Callback callback = nullptr;
};
inline thread_local std::array<Observer, 8> observers{};
inline thread_local std::uint64_t next_token = 1;
inline thread_local std::size_t observer_count = 0;
inline std::uint64_t subscribe(void *context, Callback callback) {
    for (auto &observer : observers) {
        if (observer.token) continue;
        observer = {next_token++, context, callback};
        ++observer_count;
        return observer.token;
    }
    return 0;
}
inline void unsubscribe(std::uint64_t token) {
    if (!token) return;
    for (auto &observer : observers) {
        if (observer.token != token) continue;
        observer = {};
        --observer_count;
        return;
    }
}
struct TimingScope {
    std::array<std::uint64_t, 8> tokens{};
    Clock::time_point start{};
    std::string_view name;
    SystemPhase phase;
    bool active = false;
    TimingScope(std::string_view label, SystemPhase p) : name(label), phase(p) {
        if (!observer_count) return;
        for (std::size_t i = 0; i < observers.size(); ++i) {
            tokens[i] = observers[i].token;
            active |= tokens[i] != 0;
        }
        if (active) start = Clock::now();
    }
    ~TimingScope() {
        if (!active) return;
        const double elapsed = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
        for (std::size_t i = 0; i < observers.size(); ++i) {
            const auto observer = observers[i];
            if (!tokens[i] || observer.token != tokens[i]) continue;
            observer.callback(observer.context, name, phase, elapsed);
        }
    }
    TimingScope(const TimingScope &) = delete;
    TimingScope &operator=(const TimingScope &) = delete;
};
}
}
#endif
