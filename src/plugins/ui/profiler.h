#pragma once

#include "../profiling.h"
#if AFTERHOURS_ENABLE_PROFILING
#include "line_chart.h"
#endif

namespace afterhours::ui::imm {

struct ProfilerOptions {
    bool show_chart = true;
    bool show_systems = true;
    bool show_counters = true;
    std::size_t max_rows = 6;
    double refresh_hz = 0;
    float font_size = 18;
};
struct ProfilerState {
#if AFTERHOURS_ENABLE_PROFILING
    bool paused = false;
    bool sort_by_overall = false;
    profiling::Snapshot snapshot;
    profiling::Snapshot selection_snapshot;
    std::optional<std::pair<std::size_t, std::size_t>> selection;
    RectangleType chart_rect{};
    EntityID chart_id = -1;
    std::size_t anchor = 0;
    bool selecting = false;
    bool mouse_was_down = false;
    profiling::detail::Clock::time_point refreshed{};
#endif
};

#if AFTERHOURS_ENABLE_PROFILING
inline void profiler_view(HasUIContext auto &ctx, EntityParent parent,
                          const profiling::Snapshot &snapshot,
                          ProfilerOptions options = {}, ComponentConfig config = {},
                          ProfilerState *interaction = nullptr) {
    if (config.size.is_default) config.with_size({percent(1), pixels(520)});
    auto root = vstack(ctx, parent, config.with_no_wrap().with_gap(pixels(6)));
    const float row_height = options.font_size + 8;
    auto text = [&](int id, std::string value) {
        div(ctx, mk(root.ent(), id), ComponentConfig{}.with_label(value)
            .with_size({percent(1), pixels(row_height)}).with_font_size(options.font_size)
            .with_background(Theme::Usage::None));
    };
    text(0, (interaction && interaction->selection ? fmt::format("Frames {}-{} | ",
        interaction->snapshot.frames - interaction->snapshot.frame_ms.size() + interaction->selection->first + 1,
        interaction->snapshot.frames - interaction->snapshot.frame_ms.size() + interaction->selection->second + 1) : std::string{}) + fmt::format("{:.1f} FPS | Avg {:.2f} ms/frame | p50 {:.2f}  p95 {:.2f}  p99 {:.2f} ms | {}",
                      snapshot.fps, snapshot.average_frame_ms, snapshot.p50_ms, snapshot.p95_ms, snapshot.p99_ms,
                      interaction && interaction->selection
                          ? fmt::format("{} selected frames", snapshot.frame_ms.size())
                          : fmt::format("Last {} frames", snapshot.frame_ms.size())));
    text(1, fmt::format("Process CPU: {}   |   Resident memory: {}",
        snapshot.cpu_percent ? fmt::format("{:.1f}%", *snapshot.cpu_percent) : "unavailable",
        snapshot.resident_mb ? fmt::format("{:.1f} MB", *snapshot.resident_mb) : "unavailable"));
    if (options.show_chart) {
        const auto &timeline = interaction ? interaction->snapshot : snapshot;
        ChartSeries series{"Frame ms", {}, ctx.theme.from_usage(Theme::Usage::Font)};
        series.points.reserve(timeline.frame_ms.size());
        const auto start = timeline.frames - timeline.frame_ms.size();
        for (std::size_t i = 0; i < timeline.frame_ms.size(); ++i)
            series.points.push_back({static_cast<double>(start + i + 1), timeline.frame_ms[i]});
        auto chart = line_chart(ctx, mk(root.ent(), 2), {std::move(series)},
            {"ms", {}, options.font_size - 2, interaction ? interaction->selection : std::nullopt},
            ComponentConfig{}.with_size({percent(1), pixels(190)})
                .with_background(Theme::Usage::Surface).with_debug_name("profiler_chart"));
        if (interaction) {
            interaction->chart_rect = chart_plot_bounds(chart.cmp().rect(), options.font_size - 2);
            interaction->chart_id = chart.ent().id;
        }
    }
    if (options.show_systems) {
        auto row = [&](int id, const std::array<std::string, 4> &cells) {
            auto line = hstack(ctx, mk(root.ent(), id), ComponentConfig{}
                .with_size({percent(1), pixels(row_height)}).with_no_wrap());
            for (std::size_t column = 0; column < cells.size(); ++column)
                div(ctx, mk(line.ent(), static_cast<int>(column)), ComponentConfig{}
                    .with_label(cells[column]).with_size({percent(column == 0 ? .46f : .18f), pixels(row_height)})
                    .with_font_size(options.font_size).with_background(Theme::Usage::None));
        };
        row(3, {"System / phase", interaction && interaction->selection ? "Selected avg (ms)" : "Recent avg (ms)",
                interaction && interaction->selection ? "Peak frame (ms)" : "Overall avg (ms)", "Last frame ms"});
        const auto count = std::min(options.max_rows, snapshot.systems.size());
        for (std::size_t i = 0; i < count; ++i) {
            const auto &sample = snapshot.systems[i];
            const char *phase = sample.phase == SystemPhase::Update ? "update" :
                                sample.phase == SystemPhase::Render ? "render" : "fixed";
            std::string name = sample.name;
            if (name.starts_with("afterhours::")) name.erase(0, 12);
            if (name.size() > 42) name = name.substr(0, 39) + "...";
            row(10 + static_cast<int>(i), {name + " / " + phase, fmt::format("{:.3f}", sample.recent_average_frame_ms),
                fmt::format("{:.3f}", interaction && interaction->selection ? sample.peak_frame_ms : sample.average_frame_ms), fmt::format("{:.3f}", sample.last_frame_ms)});
        }
        if (snapshot.systems.empty()) text(4, "No system samples yet");
    }
    if (options.show_counters) {
        std::string values;
        for (const auto &counter : snapshot.counters)
            values += fmt::format("{}: {:.2f} {}   ", counter.name, counter.value, counter.unit);
        if (!values.empty()) text(1000, values);
    }
    if (snapshot.dropped_system_samples)
        text(1001, fmt::format("System capacity reached: {} samples dropped", snapshot.dropped_system_samples));
}

inline void profiler_panel(HasUIContext auto &ctx, EntityParent parent,
                           profiling::Collector &collector, ProfilerState &state,
                           ProfilerOptions options = {}, ComponentConfig config = {}) {
    if (config.size.is_default) config.with_size({percent(1), pixels(600)});
    auto root = vstack(ctx, parent, config.with_no_wrap().with_gap(pixels(8)));
    auto controls = hstack(ctx, mk(root.ent(), 0), ComponentConfig{}
        .with_size({percent(1), pixels(36)}).with_gap(pixels(8)).with_no_wrap());
    auto control = [&](int id, const char *label, const char *debug) {
        return button(ctx, mk(controls.ent(), id), ComponentConfig{}
            .with_label(label).with_size({pixels(165), pixels(34)})
            .with_font_size(options.font_size).with_debug_name(debug));
    };
    bool changed = false;
    const auto clear_selection = [&] {
        state.selection.reset();
        state.selection_snapshot = {};
        state.selecting = false;
    };
    if (control(0, collector.recording() ? "Stop recording" : "Start recording", "profiler_record")) {
        if (collector.recording()) collector.stop();
        else collector.start();
        changed = !state.paused;
    }
    if (control(1, state.paused ? "Resume view" : "Pause view", "profiler_pause")) {
        state.paused = !state.paused;
        clear_selection();
        if (state.paused) state.snapshot = collector.snapshot(true);
        changed = !state.paused;
    }
    if (control(2, "Reset", "profiler_reset")) {
        collector.reset(); clear_selection(); changed = true;
    }
    const char *sort_label = state.selection
        ? (state.sort_by_overall ? "Sort: peak" : "Sort: selected")
        : (state.sort_by_overall ? "Sort: overall" : "Sort: recent");
    if (control(3, sort_label, "profiler_sort")) {
        state.sort_by_overall = !state.sort_by_overall;
    }
    if (control(4, "Clear selection", "profiler_clear_selection")) {
        clear_selection(); state.paused = false; changed = true;
    }
    const bool mouse_down = ctx.mouse.left_down;
    const auto plot = state.chart_rect;
    const auto index_at = [&](float x) {
        const float fraction = std::clamp((x - plot.x) / std::max(1.f, plot.width), 0.f, 1.f);
        return static_cast<std::size_t>(std::round(fraction * static_cast<float>(state.snapshot.frame_ms.size() - 1)));
    };
    if (options.show_chart && mouse_down && !state.mouse_was_down && plot.width > 0 &&
        is_mouse_inside(ctx.mouse.pos, plot) && ctx.is_input_allowed(state.chart_id)) {
        if (!state.paused) state.snapshot = collector.snapshot(true);
        if (!state.snapshot.frame_ms.empty()) {
            state.paused = true;
            state.selecting = true;
            state.anchor = index_at(ctx.mouse.pos.x);
        }
    }
    if (state.selecting && !state.snapshot.frame_ms.empty()) {
        const auto end = index_at(ctx.mouse.pos.x);
        const std::pair range{std::min(state.anchor, end), std::max(state.anchor, end)};
        if (state.selection != range) {
            state.selection = range;
            state.selection_snapshot = profiling::select_frames(state.snapshot, range.first, range.second);
        }
        if (!mouse_down) state.selecting = false;
    }
    state.mouse_was_down = mouse_down;
    const auto now = profiling::detail::Clock::now();
    if (changed || (!state.paused && (options.refresh_hz <= 0 || state.refreshed == profiling::detail::Clock::time_point{} ||
         std::chrono::duration<double>(now - state.refreshed).count() >= 1. / options.refresh_hz))) {
        state.snapshot = collector.snapshot(state.paused);
        state.refreshed = now;
    }
    auto &display = state.selection ? state.selection_snapshot : state.snapshot;
    std::sort(display.systems.begin(), display.systems.end(), [&](const auto &a, const auto &b) {
        if (!state.sort_by_overall) return a.recent_average_frame_ms > b.recent_average_frame_ms;
        if (state.selection) return a.peak_frame_ms > b.peak_frame_ms;
        return a.average_frame_ms > b.average_frame_ms;
    });
    profiler_view(ctx, mk(root.ent(), 1), display, options,
                  ComponentConfig{}.with_size({percent(1), expand()}), &state);
}
#else
inline void profiler_view(auto &, auto, const profiling::Snapshot &, ProfilerOptions = {}, auto... ) {}
inline void profiler_panel(auto &, auto, profiling::Collector &, ProfilerState &, ProfilerOptions = {}, auto... ) {}
#endif

}
