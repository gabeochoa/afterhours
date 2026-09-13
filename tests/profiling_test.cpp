#define AFTERHOURS_ENABLE_PROFILING 1
#include "ui_test_harness.h"
#include <afterhours/src/plugins/profiling.h>
#include <afterhours/src/plugins/process_metrics.h>
#include <afterhours/src/plugins/ui/profiler.h>
#include <thread>

using namespace afterhours;

TEST(collector_coexists_with_hook_and_separates_phases) {
    profiling::Collector collector({3, 4, 1});
    int begins = 0, ends = 0;
    SystemManager::set_profile_hook({[&](auto, auto) { ++begins; }, [&](auto, auto) { ++ends; }});
    SystemManager manager;
    manager.register_update_system([](float) { std::this_thread::sleep_for(std::chrono::milliseconds(2)); });
    manager.register_render_system([](float) {});
    Entities entities;
    CHECK(collector.start());
    for (int i = 0; i < 5; ++i) {
        manager.tick(entities, .016f);
        manager.render(entities, .016f);
        collector.end_frame(10. + i);
    }
    const auto snapshot = collector.snapshot();
    CHECK(snapshot.frames == 5);
    CHECK(snapshot.frame_ms == std::vector<double>({12, 13, 14}));
    CHECK(snapshot.p50_ms == 13 && snapshot.p99_ms == 14);
    CHECK(snapshot.systems.size() == 2);
    CHECK(snapshot.systems[0].phase == SystemPhase::Update);
    CHECK(snapshot.systems[0].calls == 5);
    CHECK(snapshot.systems[0].mean_ms() >= 2);
    CHECK(snapshot.systems[0].last_frame_ms >= 2);
    CHECK(collector.counter("load", "%", 40));
    CHECK(!collector.counter("second", "", 1));
    CHECK(collector.snapshot().counters[0].value == 40);
    collector.stop();
    manager.tick(entities, .016f);
    collector.end_frame(100);
    CHECK(collector.snapshot().frames == 5);
    CHECK(begins == ends && begins == 11);
    collector.reset();
    CHECK(collector.snapshot().systems.empty());
    CHECK(collector.snapshot().frame_ms.empty());
    SystemManager::set_profile_hook({});
}

TEST(frame_averages_include_idle_frames_and_expire_old_work) {
    profiling::Collector collector({3, 4, 1, false});
    CHECK(collector.start());
    const auto record = [&](std::string_view name, double ms) {
        for (const auto &observer : profiling::detail::observers) {
            if (observer.context != &collector) continue;
            observer.callback(observer.context, name, SystemPhase::Update, ms);
        }
    };
    record("work", 2);
    record("work", 4);
    record("work", 3);
    CHECK(collector.snapshot().systems[0].total_ms == 0);
    collector.end_frame(10);
    collector.end_frame(20);
    record("work", 12);
    collector.end_frame(30);
    auto snapshot = collector.snapshot();
    CHECK(snapshot.average_frame_ms == 20);
    CHECK(snapshot.systems[0].mean_ms() == 5.25);
    CHECK(snapshot.systems[0].average_frame_ms == 7);
    CHECK(snapshot.systems[0].recent_average_frame_ms == 7);
    CHECK(snapshot.systems[0].last_frame_ms == 12);
    collector.end_frame(40);
    collector.end_frame(50);
    snapshot = collector.snapshot();
    CHECK(snapshot.frame_ms == std::vector<double>({30, 40, 50}));
    CHECK(snapshot.average_frame_ms == 40);
    CHECK(snapshot.systems[0].average_frame_ms == 4.2);
    CHECK(snapshot.systems[0].recent_average_frame_ms == 4);
    CHECK(snapshot.systems[0].last_frame_ms == 0);
    collector.end_frame(60);
    CHECK(collector.snapshot().systems[0].recent_average_frame_ms == 0);
    record("late", 12);
    collector.end_frame(70);
    snapshot = collector.snapshot();
    CHECK(snapshot.systems[0].name == "late");
    CHECK(snapshot.systems[0].recent_average_frame_ms == 4);
    CHECK(snapshot.systems[0].average_frame_ms == 12. / 7.);
    record("late", 99);
    collector.stop();
    collector.start();
    collector.end_frame(80);
    snapshot = collector.snapshot();
    CHECK(snapshot.systems[0].total_ms == 12);
    CHECK(snapshot.systems[0].average_frame_ms == 1.5);
    CHECK(snapshot.systems[0].recent_average_frame_ms == 4);
    collector.reset();
    CHECK(collector.snapshot().frame_ms.empty());
    CHECK(collector.snapshot().average_frame_ms == 0);
    record("fresh", 6);
    collector.end_frame(10);
    snapshot = collector.snapshot();
    CHECK(snapshot.systems.size() == 1);
    CHECK(snapshot.systems[0].recent_average_frame_ms == 6);
    CHECK(snapshot.systems[0].average_frame_ms == 6);
}

TEST(stopping_and_resetting_inside_a_system_is_safe) {
    profiling::Collector collector;
    SystemManager manager;
    manager.register_update_system([&](float) { collector.stop(); });
    Entities entities;
    collector.start();
    manager.tick(entities, .016f);
    CHECK(collector.snapshot().systems.empty());
    manager.update_systems_.clear();
    manager.register_update_system([&](float) { collector.reset(); });
    collector.start();
    manager.tick(entities, .016f);
    CHECK(collector.recording());
    CHECK(collector.snapshot().systems.empty());
}

TEST(bounded_systems_and_subscription_capacity) {
    profiling::Collector collector({2, 0, 0});
    collector.start();
    Entities entities;
    SystemManager manager;
    manager.register_update_system([](float) {});
    manager.tick(entities, .016f);
    CHECK(collector.snapshot().dropped_system_samples == 1);
    CHECK(collector.snapshot().systems.empty());
    std::vector<std::unique_ptr<profiling::Collector>> others;
    for (int i = 0; i < 7; ++i) {
        others.push_back(std::make_unique<profiling::Collector>());
        CHECK(others.back()->start());
    }
    profiling::Collector overflow;
    CHECK(!overflow.start());
    others.pop_back();
    CHECK(overflow.start());
    collector.end_frame(NAN);
    CHECK(collector.snapshot().frames == 0);
}

TEST(custom_expressions_are_not_evaluated_when_stopped) {
    profiling::Collector collector;
    int evaluated = 0;
    AFTERHOURS_PROFILE_COUNTER(collector, "value", "", ++evaluated);
    CHECK(evaluated == 0);
    collector.start();
    AFTERHOURS_PROFILE_COUNTER(collector, "value", "", ++evaluated);
    CHECK(evaluated == 1);
#if defined(__APPLE__)
    const auto metrics = profiling::process_metrics();
    CHECK(metrics.cpu_seconds.has_value());
    CHECK(metrics.resident_mb.value_or(0) > 0);
#endif
}

TEST(panel_refreshes_each_frame_or_at_the_configured_rate) {
    ui_test::ImmTestHarness ui;
    profiling::Collector collector;
    collector.start();
    ui::imm::ProfilerState state;
    ui::imm::ProfilerOptions options;
    const auto draw = [&] {
        ui.begin_frame();
        ui::imm::profiler_panel(ui.context(), ui::imm::mk(ui.root()), collector, state, options);
        ui.layout_only();
    };
    collector.end_frame(16);
    draw();
    CHECK(state.snapshot.frames == 1);
    collector.end_frame(20);
    draw();
    CHECK(state.snapshot.frames == 2);
    options.refresh_hz = 120;
    state.refreshed = profiling::detail::Clock::now() + std::chrono::hours(1);
    collector.end_frame(24);
    draw();
    CHECK(state.snapshot.frames == 2);
    state.refreshed = profiling::detail::Clock::now() - std::chrono::milliseconds(10);
    draw();
    CHECK(state.snapshot.frames == 3);
    state.paused = true;
    options.refresh_hz = 0;
    collector.end_frame(28);
    draw();
    CHECK(state.snapshot.frames == 3);
    state.paused = false;
    draw();
    CHECK(state.snapshot.frames == 4);
}

TEST(paused_view_retains_a_snapshot_while_collection_continues) {
    ui_test::ImmTestHarness ui;
    profiling::Collector collector;
    collector.start();
    collector.end_frame(16);
    ui::imm::ProfilerState state;
    state.paused = true;
    state.snapshot = collector.snapshot();
    collector.end_frame(40);
    ui.begin_frame();
    ui::imm::profiler_panel(ui.context(), ui::imm::mk(ui.root()), collector, state);
    ui.layout_only();
    CHECK(state.snapshot.frames == 1);
    CHECK(collector.snapshot().frames == 2);
}

TEST(selected_frames_use_matching_system_history_after_ring_wrap) {
    profiling::Collector collector({3, 4, 1, false});
    collector.start();
    for (int i = 1; i <= 5; ++i) {
        for (const auto &observer : profiling::detail::observers)
            if (observer.context == &collector)
                observer.callback(observer.context, "work", SystemPhase::Update, i * 2.);
        collector.end_frame(i * 10.);
    }
    CHECK(collector.snapshot().systems[0].frame_history.empty());
    const auto frozen = collector.snapshot(true);
    CHECK(frozen.systems[0].frame_history == std::vector<double>({6, 8, 10}));
    collector.end_frame(100);
    const auto selected = profiling::select_frames(frozen, 1, 0);
    CHECK(selected.frame_ms == std::vector<double>({30, 40}));
    CHECK(selected.frames == 4);
    CHECK(selected.average_frame_ms == 35);
    CHECK(selected.p95_ms == 40);
    CHECK(selected.systems[0].recent_average_frame_ms == 7);
    CHECK(selected.systems[0].last_frame_ms == 8);
    CHECK(selected.systems[0].peak_frame_ms == 8);
    CHECK(!selected.cpu_percent);
    CHECK(selected.counters.empty());
    CHECK(profiling::select_frames({}, 0, 100).frame_ms.empty());
    const auto single = profiling::select_frames(frozen, 99, 99);
    CHECK(single.frame_ms == std::vector<double>({50}));
    CHECK(single.systems[0].recent_average_frame_ms == 10);
}

TEST(recording_controls_do_not_replace_the_selected_timeline) {
    ui_test::ImmTestHarness harness;
    profiling::Collector collector;
    collector.start();
    collector.end_frame(10);
    collector.end_frame(20);
    ui::imm::ProfilerState state;
    state.paused = true;
    state.snapshot = collector.snapshot(true);
    state.selection = std::pair<std::size_t, std::size_t>{0, 1};
    state.selection_snapshot = profiling::select_frames(state.snapshot, 0, 1);
    const auto draw = [&] {
        harness.begin_frame();
        ui::imm::profiler_panel(harness.context(), ui::imm::mk(harness.root()), collector, state);
        harness.layout_only();
    };
    draw();
    collector.end_frame(90);
    auto *record = harness.find("profiler_record");
    CHECK(record != nullptr);
    if (!record) return;
    auto &control = ui::AutoLayout::to_ent_static(record->id);
    control.get<ui::HasClickListener>().down = true;
    draw();
    CHECK(!collector.recording());
    CHECK(state.snapshot.frames == 2);
    CHECK(state.snapshot.frame_ms == std::vector<double>({10, 20}));
    CHECK(state.selection_snapshot.frame_ms == std::vector<double>({10, 20}));
    draw();
    CHECK(collector.recording());
    CHECK(state.paused);
    CHECK(state.selection.has_value());
    CHECK(state.snapshot.frames == 2);
    CHECK(state.selection_snapshot.average_frame_ms == 15);
    control.get<ui::HasClickListener>().down = false;
}

TEST(selected_system_rows_can_sort_by_peak_instead_of_average) {
    ui_test::ImmTestHarness harness;
    profiling::Collector collector;
    ui::imm::ProfilerState state;
    state.paused = true;
    state.snapshot.frames = 2;
    state.snapshot.frame_ms = {10, 20};
    profiling::SystemSample spike;
    spike.name = "spike";
    spike.frame_history = {0, 9};
    profiling::SystemSample steady;
    steady.name = "steady";
    steady.frame_history = {5, 5};
    state.snapshot.systems = {spike, steady};
    state.selection = std::pair<std::size_t, std::size_t>{0, 1};
    state.selection_snapshot = profiling::select_frames(state.snapshot, 0, 1);
    const auto draw = [&] {
        harness.begin_frame();
        ui::imm::profiler_panel(harness.context(), ui::imm::mk(harness.root()), collector, state);
        harness.layout_only();
    };
    draw();
    CHECK(state.selection_snapshot.systems.front().name == "steady");
    auto *sort = harness.find("profiler_sort");
    CHECK(sort != nullptr);
    if (!sort) return;
    auto &control = ui::AutoLayout::to_ent_static(sort->id);
    control.get<ui::HasClickListener>().down = true;
    draw();
    control.get<ui::HasClickListener>().down = false;
    draw();
    CHECK(state.sort_by_overall);
    CHECK(state.selection_snapshot.systems.front().name == "spike");
    CHECK(control.get<ui::HasLabel>().label == "Sort: peak");
}

int main() { return ui_test::run_registered_tests("profiling collector"); }
