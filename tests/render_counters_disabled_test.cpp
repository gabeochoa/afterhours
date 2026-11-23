#include "ui_test_harness.h"
#include <afterhours/src/render_counters.h>

using namespace afterhours;
using namespace afterhours::ui;

int main() {
  static_assert(!afterhours::profiling::available);
  Arena arena(Arena::DEFAULT_CAPACITY);
  RenderCommandBuffer buffer(arena);
  const Color c{255, 255, 255, 255};
  buffer.add_rectangle({0, 0, 10, 10}, c, 0);
  render_counters::note_create();
  BatchedRenderer renderer;
  FontManager fonts;
  renderer.render(buffer, fonts);
  // Counting is compiled out with profiling off: local stats stay zero and
  // nothing reaches a collector.
  if (renderer.stats().draws != 0) return 1;
  if (renderer.stats().creates != 0) return 1;
  if (!profiling::default_collector().snapshot().counters.empty()) return 1;
  return 0;
}
