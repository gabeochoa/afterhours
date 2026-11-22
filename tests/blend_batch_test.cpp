// blend_batch_test.cpp
// The batched renderer had scissor/shader commands but no blend command, so a
// widget could not ask for a blend mode without leaving the batch -- and
// per-blit BlendStart/End pairs are what cost puzzle 3.8ms/frame in flushes.
// These pin: adjacent pairs for the active mode submit nothing apart, a
// redundant start/end submits nothing, and draws keep their order.
#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace ui_test;

static void add_blit(RenderCommandBuffer &b, RenderPrimitive::CustomDrawFn *fn,
                     int i) {
  b.add_blend_start(blend::Mode::AlphaPremultiply, 0, i);
  b.add_custom({10.f + i, 20, 4, 4}, fn, 0, i);
  b.add_blend_end(0, i);
}

TEST(eight_per_blit_pairs_submit_two_transitions) {
  Arena arena(Arena::DEFAULT_CAPACITY);
  RenderCommandBuffer buffer(arena);
  BatchedRenderer renderer;
  FontManager fonts;
  std::vector<int> drawn;
  RenderPrimitive::CustomDrawFn draw = [&](RectangleType rect) {
    drawn.push_back(static_cast<int>(rect.x));
  };
  // Representative scene: 8 blits, each wrapped in its own pair (puzzle shape).
  for (int i = 0; i < 8; ++i) add_blit(buffer, &draw, i);
  blend::reset_transition_count();
  CHECK(blend::current() == blend::Mode::Alpha);
  renderer.render(buffer, fonts);
  printf("  8 per-blit pairs = %zu submitted transitions\n",
         renderer.stats().blend_operations);
  CHECK(renderer.stats().blend_operations == 2);
  CHECK(blend::transitions_this_frame() == 2);
  CHECK(blend::current() == blend::Mode::Alpha);
  CHECK(drawn.size() == 8);
  for (int i = 0; i < 8; ++i) CHECK(drawn[i] == 10 + i);
}

TEST(redundant_start_and_unmatched_end_submit_nothing) {
  Arena arena(Arena::DEFAULT_CAPACITY);
  RenderCommandBuffer buffer(arena);
  BatchedRenderer renderer;
  FontManager fonts;
  buffer.add_blend_end(0); // nothing active to end
  buffer.add_blend_start(blend::Mode::Alpha, 0); // already active
  buffer.add_blend_start(blend::Mode::Additive, 0);
  buffer.add_blend_start(blend::Mode::Additive, 0); // redundant
  buffer.add_rectangle({0, 0, 4, 4}, Color{255, 255, 255, 255}, 0);
  buffer.add_blend_end(0);
  buffer.add_blend_end(0); // unmatched
  blend::reset_transition_count();
  renderer.render(buffer, fonts);
  CHECK(renderer.stats().blend_operations == 2);
  CHECK(blend::transitions_this_frame() == 2);
  CHECK(blend::current() == blend::Mode::Alpha);
}

TEST(a_draw_between_pairs_forces_a_real_transition) {
  Arena arena(Arena::DEFAULT_CAPACITY);
  RenderCommandBuffer buffer(arena);
  BatchedRenderer renderer;
  FontManager fonts;
  RenderPrimitive::CustomDrawFn noop = [](RectangleType) {};
  add_blit(buffer, &noop, 0);
  buffer.add_custom({0, 0, 4, 4}, &noop, 0); // unblended draw between
  add_blit(buffer, &noop, 1);
  blend::reset_transition_count();
  renderer.render(buffer, fonts);
  CHECK(renderer.stats().blend_operations == 4);
  CHECK(blend::current() == blend::Mode::Alpha);
}

int main() { return run_registered_tests("blend batch"); }
