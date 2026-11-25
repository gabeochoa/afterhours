#define AFTERHOURS_ENABLE_PROFILING 1
#include "ui_test_harness.h"
#include <afterhours/src/render_counters.h>

using namespace afterhours;
using namespace afterhours::ui;

static double counter_value(const profiling::Snapshot &snap,
                            const char *name) {
  for (const auto &counter : snap.counters)
    if (counter.name == name) return counter.value;
  return -1;
}

TEST(known_scene_publishes_exact_draw_bind_create_counts) {
  Arena arena(Arena::DEFAULT_CAPACITY);
  RenderCommandBuffer buffer(arena);
  const Color c{255, 255, 255, 255};
  const RectangleType rect{0, 0, 10, 10};
  for (int i = 0; i < 3; ++i) buffer.add_rectangle(rect, c, 0);
  buffer.add_line({0, 0}, {5, 5}, 1.f, c, 0, -1);
  buffer.add_image(rect, rect, TextureType{}, c, 0);
  buffer.add_text(rect, "x", "", 10.f, c, TextAlignment::Left, 0);
  buffer.add_rectangle_outline(rect, c, 0);

  auto &collector = profiling::default_collector();
  collector.reset();
  CHECK(collector.start());
  render_counters::note_create();
  render_counters::note_create();

  BatchedRenderer renderer;
  FontManager fonts;
  renderer.render(buffer, fonts);
  CHECK(renderer.stats().draws == 7);
  CHECK(renderer.stats().binds == 2);
  CHECK(renderer.stats().creates == 2);
  auto snap = collector.snapshot();
  CHECK(counter_value(snap, "render.draws") == 7);
  CHECK(counter_value(snap, "render.binds") == 2);
  CHECK(counter_value(snap, "render.creates") == 2);

  // Per-frame, not cumulative: the same scene republishes the same numbers.
  renderer.render(buffer, fonts);
  CHECK(renderer.stats().draws == 7);
  CHECK(renderer.stats().creates == 0);
  snap = collector.snapshot();
  CHECK(counter_value(snap, "render.draws") == 7);
  CHECK(counter_value(snap, "render.creates") == 0);
  collector.stop();
  collector.reset();
}

TEST(empty_scene_publishes_zero) {
  Arena arena(Arena::DEFAULT_CAPACITY);
  RenderCommandBuffer buffer(arena);
  auto &collector = profiling::default_collector();
  collector.reset();
  CHECK(collector.start());
  BatchedRenderer renderer;
  FontManager fonts;
  renderer.render(buffer, fonts);
  CHECK(renderer.stats().draws == 0);
  CHECK(counter_value(collector.snapshot(), "render.draws") == 0);
  collector.stop();
  collector.reset();
}

int main() { return ui_test::run_registered_tests("render counters"); }
