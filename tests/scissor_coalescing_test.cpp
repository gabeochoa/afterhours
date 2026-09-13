#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace ui_test;

TEST(equal_adjacent_clips_share_one_backend_scope) {
  Arena arena(Arena::DEFAULT_CAPACITY);
  RenderCommandBuffer buffer(arena);
  BatchedRenderer renderer;
  FontManager fonts;
  std::vector<size_t> observed;
  RenderPrimitive::CustomDrawFn draw = [&](RectangleType rect) {
    observed.push_back(renderer.stats().scissor_operations);
    draw_rectangle(rect, Color{255, 255, 255, 255});
  };
  for (int i = 0; i < 3; ++i) {
    buffer.add_scissor_start(10, 20, 100, 50, 0, i);
    buffer.add_custom({10.f + i, 20, 4, 4}, &draw, 0, i);
    buffer.add_scissor_end(0, i);
  }
  capture::clear();
  capture::enabled() = true;
  renderer.render(buffer, fonts);
  CHECK(renderer.stats().scissor_operations == 2);
  CHECK(observed == std::vector<size_t>({1, 1, 1}));
  CHECK(capture::calls().size() == 3);
  CHECK(capture::calls()[0].entity_id == 0);
  CHECK(capture::calls()[2].entity_id == 2);
  capture::enabled() = false;
}

TEST(changing_any_clip_dimension_keeps_separate_scopes) {
  for (size_t axis = 0; axis < 4; ++axis) {
    Arena arena(Arena::DEFAULT_CAPACITY);
    RenderCommandBuffer buffer(arena);
    BatchedRenderer renderer;
    FontManager fonts;
    std::vector<size_t> observed;
    RenderPrimitive::CustomDrawFn draw = [&](RectangleType) {
      observed.push_back(renderer.stats().scissor_operations);
    };
    std::array<int, 4> clip{10, 20, 100, 50};
    buffer.add_scissor_start(clip[0], clip[1], clip[2], clip[3], 0);
    buffer.add_custom({}, &draw, 0);
    buffer.add_scissor_end(0);
    ++clip[axis];
    buffer.add_scissor_start(clip[0], clip[1], clip[2], clip[3], 0);
    buffer.add_custom({}, &draw, 0);
    buffer.add_scissor_end(0);
    renderer.render(buffer, fonts);
    CHECK(renderer.stats().scissor_operations == 4);
    CHECK(observed == std::vector<size_t>({1, 3}));
  }
}

TEST(unclipped_drawing_between_equal_clips_ends_the_first_scope) {
  Arena arena(Arena::DEFAULT_CAPACITY);
  RenderCommandBuffer buffer(arena);
  BatchedRenderer renderer;
  FontManager fonts;
  std::vector<size_t> observed;
  RenderPrimitive::CustomDrawFn draw = [&](RectangleType) {
    observed.push_back(renderer.stats().scissor_operations);
  };
  buffer.add_scissor_start(10, 20, 100, 50, 0);
  buffer.add_custom({}, &draw, 0);
  buffer.add_scissor_end(0);
  buffer.add_custom({}, &draw, 0);
  buffer.add_scissor_start(10, 20, 100, 50, 0);
  buffer.add_custom({}, &draw, 0);
  buffer.add_scissor_end(0);
  renderer.render(buffer, fonts);
  CHECK(renderer.stats().scissor_operations == 4);
  CHECK(observed == std::vector<size_t>({1, 2, 3}));
}

TEST(final_scissor_end_closes_the_scope_and_next_render_starts_fresh) {
  Arena arena(Arena::DEFAULT_CAPACITY);
  RenderCommandBuffer clipped(arena);
  BatchedRenderer renderer;
  FontManager fonts;
  clipped.add_scissor_start(0, 0, 100, 50, 0);
  clipped.add_scissor_end(0);
  renderer.render(clipped, fonts);
  CHECK(renderer.stats().scissor_operations == 2);
  RenderCommandBuffer outside(arena);
  outside.add_rectangle({0, 0, 200, 100}, Color{255, 255, 255, 255}, 0);
  renderer.render(outside, fonts);
  CHECK(renderer.stats().scissor_operations == 0);
  CHECK(renderer.stats().total_commands == 1);
}

int main() { return run_registered_tests("scissor coalescing"); }
