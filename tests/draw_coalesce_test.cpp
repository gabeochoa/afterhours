#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace ui_test;

TEST(representative_scene_draws_unchanged_state_ops_coalesce) {
  Arena arena(Arena::DEFAULT_CAPACITY);
  RenderCommandBuffer buffer(arena);
  BatchedRenderer renderer;
  FontManager fonts;
  graphics::ShaderType shader{};
  shader.id = 7;
  for (int i = 0; i < 30; ++i) {
    buffer.add_scissor_start(0, 0, 800, 600, 0, i);
    buffer.add_rectangle({10.f + i, 10, 20, 20}, Color{255, 0, 0, 255}, 0, i);
    buffer.add_scissor_start(0, 0, 800, 600, 0, i);
    buffer.add_rectangle({30.f + i, 10, 20, 20}, Color{0, 255, 0, 255}, 0, i);
    buffer.add_scissor_start(0, 0, 800, 600, 0, i);
    buffer.add_shader_start(&shader, 0, i);
    buffer.add_rectangle({50.f + i, 10, 20, 20}, Color{0, 0, 255, 255}, 0, i);
    buffer.add_shader_end(0, i);
    buffer.add_scissor_end(0, i);
  }
  capture::clear();
  capture::enabled() = true;
  renderer.render(buffer, fonts);
  capture::enabled() = false;
  CHECK(renderer.stats().total_commands == 270);
  CHECK(capture::calls().size() == 90);
  CHECK(renderer.stats().scissor_operations == 2);
  CHECK(renderer.stats().shader_operations == 60);
  bool order_ok = capture::calls().size() == 90;
  for (size_t i = 0; order_ok && i < capture::calls().size(); ++i) {
    const auto &c = capture::calls()[i];
    const auto want = i % 3 == 0   ? Color{255, 0, 0, 255}
                      : i % 3 == 1 ? Color{0, 255, 0, 255}
                                   : Color{0, 0, 255, 255};
    order_ok = c.op == "rectangle_rounded" && c.color.r == want.r &&
               c.color.g == want.g && c.color.b == want.b;
  }
  CHECK(order_ok);
}

TEST(duplicate_scissor_start_and_unmatched_end_submit_nothing) {
  Arena arena(Arena::DEFAULT_CAPACITY);
  RenderCommandBuffer buffer(arena);
  BatchedRenderer renderer;
  FontManager fonts;
  buffer.add_scissor_end(0);
  buffer.add_scissor_start(10, 20, 100, 50, 0);
  buffer.add_scissor_start(10, 20, 100, 50, 0);
  buffer.add_rectangle({0, 0, 4, 4}, Color{255, 255, 255, 255}, 0);
  buffer.add_scissor_start(10, 20, 101, 50, 0);
  buffer.add_scissor_end(0);
  buffer.add_scissor_end(0);
  renderer.render(buffer, fonts);
  CHECK(renderer.stats().scissor_operations == 3);
}

TEST(shader_state_coalesces_only_when_identical) {
  Arena arena(Arena::DEFAULT_CAPACITY);
  RenderCommandBuffer buffer(arena);
  BatchedRenderer renderer;
  FontManager fonts;
  graphics::ShaderType a{}, b{};
  a.id = 1;
  b.id = 2;
  buffer.add_shader_end(0);
  buffer.add_shader_start(&a, 0);
  buffer.add_shader_start(&a, 0);
  buffer.add_rectangle({0, 0, 4, 4}, Color{255, 255, 255, 255}, 0);
  buffer.add_shader_end(0);
  buffer.add_shader_start(&a, 0);
  buffer.add_rectangle({4, 0, 4, 4}, Color{255, 255, 255, 255}, 0);
  buffer.add_shader_end(0);
  buffer.add_shader_start(&b, 0);
  buffer.add_shader_end(0);
  buffer.add_shader_start(nullptr, 0);
  buffer.add_shader_end(0);
  renderer.render(buffer, fonts);
  CHECK(renderer.stats().shader_operations == 4);
}

int main() { return run_registered_tests("draw coalesce"); }
