#include "ui_test_harness.h"
#include <afterhours/src/plugins/ui/profiler.h>
#include <afterhours/src/plugins/process_metrics.h>
#include <chrono>
#include <cstdlib>
#include <new>
#ifdef AFTER_HOURS_USE_RAYLIB
#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif
#endif

static std::size_t allocations = 0;
void *operator new(std::size_t size) {
    ++allocations;
    if (void *p = std::malloc(size ? size : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void *p) noexcept { std::free(p); }
void *operator new[](std::size_t size) { return ::operator new(size); }
void operator delete[](void *p) noexcept { ::operator delete(p); }

struct Work : afterhours::System<> {
    std::string label;
    double value = 0;
    explicit Work(int id) : label("System " + std::to_string(id)) {}
    std::string_view name() const override { return label; }
    bool should_iterate() const override { return false; }
    void once(float) override {
        for (int i = 0; i < 100; ++i) value = std::sin(value + i * .01);
    }
};

int main(int argc, char **argv) {
    using namespace afterhours;
    const std::string mode = argc > 1 ? argv[1] : "stopped";
    const int frame_count = argc > 2 ? std::stoi(argv[2]) : 3000;
    if (frame_count <= 0) return 1;
#ifdef AFTER_HOURS_USE_RAYLIB
    if (argc < 4) return 1;
    graphics::Config config;
    config.display = graphics::DisplayMode::Headless;
    config.width = 1280;
    config.height = 720;
    if (!graphics::init(config)) return 1;
    const auto font = load_font_from_file(argv[3]);
    if (!font.texture.id) return 1;
    auto &font_entity = EntityHelper::createPermanentEntity();
    auto &fonts = font_entity.addComponent<ui::FontManager>();
    fonts.load_font(ui::UIComponent::DEFAULT_FONT, font);
    fonts.load_font(ui::UIComponent::UNSET_FONT, font);
    EntityHelper::registerSingleton<ui::FontManager>(font_entity);
    ui::RenderBatched<ui_test::TestInputAction> renderer;
#endif
    ui_test::ImmTestHarness ui;
#ifdef AFTER_HOURS_USE_RAYLIB
    ui.context().screen_width = 1280;
    ui.context().screen_height = 720;
    ui.root().get<ui::UIComponent>().set_desired_width(ui::pixels(1280));
    ui.root().get<ui::UIComponent>().set_desired_height(ui::pixels(720));
#endif
    profiling::Collector collector;
    ui::imm::ProfilerState state;
    ui::imm::ProfilerOptions options;
    options.refresh_hz = argc > 4 ? std::stod(argv[4]) : 0;
    options.font_size = 20;
    options.max_rows = 5;
    SystemManager manager;
    for (int i = 0; i < 32; ++i) manager.register_update_system(std::make_unique<Work>(i));
    Entities entities;
    if (mode != "stopped") collector.start();
    auto frame = [&] {
        manager.tick(entities, .016f);
#ifdef AFTER_HOURS_USE_RAYLIB
        graphics::begin_frame();
        graphics::clear_background(Color{16, 18, 24, 255});
#endif
        if (mode == "panel") {
            ui.begin_frame();
            ui::imm::profiler_panel(ui.context(), ui::imm::mk(ui.root()), collector, state, options);
#ifdef AFTER_HOURS_USE_RAYLIB
            ui.layout_only(false, {1280, 720});
            renderer.for_each_with_derived(ui.root(), ui.context(), fonts, 0);
#else
            ui.layout_only();
            ui.context().render_cmds.clear();
#endif
        }
#ifdef AFTER_HOURS_USE_RAYLIB
        graphics::end_frame();
        glFinish();
#endif
        collector.end_frame();
    };
    for (int i = 0; i < 200; ++i) frame();
    std::vector<double> times(static_cast<std::size_t>(frame_count), 1.0);
    const auto before = profiling::process_metrics();
    const auto start_allocations = allocations;
    const auto start = std::chrono::steady_clock::now();
    for (auto &ms : times) {
        const auto tick = std::chrono::steady_clock::now();
        frame();
        ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tick).count();
    }
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    const auto used_allocations = allocations - start_allocations;
    const auto after = profiling::process_metrics();
    std::sort(times.begin(), times.end());
    std::printf("enabled=%d mode=%s refresh_hz=%.1f seconds=%.4f p50_ms=%.6f p99_ms=%.6f allocations_per_frame=%.2f cpu_seconds=%.4f rss_mb=%.2f rss_growth_mb=%.2f\n",
        profiling::available, mode.c_str(), options.refresh_hz, seconds, times[times.size() / 2], times[times.size() * 99 / 100], static_cast<double>(used_allocations) / frame_count,
        after.cpu_seconds.value_or(0) - before.cpu_seconds.value_or(0), after.resident_mb.value_or(0),
        after.resident_mb.value_or(0) - before.resident_mb.value_or(0));
#ifdef AFTER_HOURS_USE_RAYLIB
    auto png = capture_render_texture_png(graphics::get_render_texture());
    if (mode == "panel" && png) {
        std::FILE *file = std::fopen("/tmp/afterhours-profiler-benchmark.png", "wb");
        if (file) { std::fwrite(png->bytes.data(), 1, png->bytes.size(), file); std::fclose(file); }
    }
    graphics::shutdown();
#endif
}
