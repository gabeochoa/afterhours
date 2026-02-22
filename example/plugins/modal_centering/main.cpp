#include <cassert>
#include <cmath>
#include <iostream>

// Tests the modal centering calculation and absolute positioning logic.
// The real modal plugin sets absolute_pos_x/y (not computed_rel) so that
// AutoLayout honors the intended position. This test validates that math.

namespace test {

enum class Dim { Pixels, ScreenPercent, Percent };

struct Size {
  Dim dim = Dim::Pixels;
  float value = 0.f;
};

Size pixels(float v) { return Size{Dim::Pixels, v}; }
Size screen_pct(float v) { return Size{Dim::ScreenPercent, v}; }
Size pct(float v) { return Size{Dim::Percent, v}; }

float resolve_size(const Size &size, int screen_w, int screen_h) {
  switch (size.dim) {
  case Dim::Pixels:
    return size.value;
  case Dim::ScreenPercent:
    return size.value * static_cast<float>(screen_h);
  case Dim::Percent:
    return size.value * static_cast<float>(screen_w);
  default:
    return size.value;
  }
}

struct ModalPosition {
  float x, y;
  float width, height;
};

ModalPosition calculate_centered_position(const Size &width_size,
                                          const Size &height_size,
                                          int screen_w, int screen_h) {
  float width_px = resolve_size(width_size, screen_w, screen_h);
  float height_px = resolve_size(height_size, screen_w, screen_h);
  float x_pos = (static_cast<float>(screen_w) - width_px) / 2.0f;
  float y_pos = (static_cast<float>(screen_h) - height_px) / 2.0f;
  return {x_pos, y_pos, width_px, height_px};
}

// Simulates what AutoLayout does for absolute children:
// computed_rel = absolute_pos + parent_offset
struct SimulatedLayoutResult {
  float final_x, final_y;
};

SimulatedLayoutResult simulate_autolayout(float absolute_pos_x,
                                          float absolute_pos_y,
                                          float parent_offset_x,
                                          float parent_offset_y) {
  return {absolute_pos_x + parent_offset_x, absolute_pos_y + parent_offset_y};
}

} // namespace test

using namespace test;

int main() {
  std::cout << "=== Modal Centering Example ===" << std::endl;

  // Test 1: 400x200 modal on 1280x720 screen (pixel sizes)
  std::cout << "\n1. Pixel-sized modal (400x200 on 1280x720):" << std::endl;
  {
    auto pos = calculate_centered_position(pixels(400), pixels(200), 1280, 720);
    std::cout << "  - Position: (" << pos.x << ", " << pos.y << ")" << std::endl;
    std::cout << "  - Size: " << pos.width << "x" << pos.height << std::endl;
    assert(pos.x == 440.f);
    assert(pos.y == 260.f);
    assert(pos.width == 400.f);
    assert(pos.height == 200.f);
  }

  // Test 2: 50% width modal on 1280x720
  std::cout << "\n2. Percent-width modal (50% width, 300px height):" << std::endl;
  {
    auto pos = calculate_centered_position(pct(0.5f), pixels(300), 1280, 720);
    std::cout << "  - Resolved width: " << pos.width << "px" << std::endl;
    std::cout << "  - Position: (" << pos.x << ", " << pos.y << ")" << std::endl;
    assert(pos.width == 640.f);
    assert(pos.x == 320.f);
    assert(pos.y == 210.f);
  }

  // Test 3: Screen-percent height on 1280x720
  std::cout << "\n3. Screen-percent height (600px width, 50% screen height):" << std::endl;
  {
    auto pos = calculate_centered_position(pixels(600), screen_pct(0.5f), 1280, 720);
    std::cout << "  - Resolved height: " << pos.height << "px" << std::endl;
    std::cout << "  - Position: (" << pos.x << ", " << pos.y << ")" << std::endl;
    assert(pos.height == 360.f);
    assert(pos.x == 340.f);
    assert(pos.y == 180.f);
  }

  // Test 4: Full screen modal is at (0, 0)
  std::cout << "\n4. Full-screen modal:" << std::endl;
  {
    auto pos = calculate_centered_position(pixels(1280), pixels(720), 1280, 720);
    std::cout << "  - Position: (" << pos.x << ", " << pos.y << ")" << std::endl;
    assert(pos.x == 0.f);
    assert(pos.y == 0.f);
  }

  // Test 5: Verify absolute_pos is used (not computed_rel directly)
  // The bug was: setting computed_rel was overwritten by AutoLayout which
  // reads absolute_pos_x/y for absolute children. Now we set absolute_pos.
  std::cout << "\n5. AutoLayout simulation with absolute_pos:" << std::endl;
  {
    auto pos = calculate_centered_position(pixels(400), pixels(200), 1280, 720);

    // Modal's parent is the UI root which has offset (0,0)
    auto result = simulate_autolayout(pos.x, pos.y, 0.f, 0.f);
    std::cout << "  - absolute_pos: (" << pos.x << ", " << pos.y << ")" << std::endl;
    std::cout << "  - parent offset: (0, 0)" << std::endl;
    std::cout << "  - final position: (" << result.final_x << ", " << result.final_y << ")" << std::endl;
    assert(result.final_x == 440.f);
    assert(result.final_y == 260.f);
  }

  // Test 6: The OLD bug - if computed_rel was set instead of absolute_pos,
  // AutoLayout would overwrite it with absolute_pos (which defaults to 0,0)
  std::cout << "\n6. Old bug reproduction (computed_rel overwritten):" << std::endl;
  {
    float old_absolute_pos_x = 0.f;  // default before our fix
    float old_absolute_pos_y = 0.f;
    auto result = simulate_autolayout(old_absolute_pos_x, old_absolute_pos_y, 0.f, 0.f);
    std::cout << "  - Old behavior: modal at (" << result.final_x << ", " << result.final_y << ") = top-left corner" << std::endl;
    assert(result.final_x == 0.f);
    assert(result.final_y == 0.f);

    // The fix: set absolute_pos to the centered position
    auto pos = calculate_centered_position(pixels(400), pixels(200), 1280, 720);
    auto fixed = simulate_autolayout(pos.x, pos.y, 0.f, 0.f);
    std::cout << "  - Fixed behavior: modal at (" << fixed.final_x << ", " << fixed.final_y << ") = centered" << std::endl;
    assert(fixed.final_x == 440.f);
    assert(fixed.final_y == 260.f);
  }

  // Test 7: Backdrop should be at (0, 0) with full screen size
  std::cout << "\n7. Backdrop positioning:" << std::endl;
  {
    float backdrop_abs_x = 0.f;
    float backdrop_abs_y = 0.f;
    auto result = simulate_autolayout(backdrop_abs_x, backdrop_abs_y, 0.f, 0.f);
    std::cout << "  - Backdrop at (" << result.final_x << ", " << result.final_y << ")" << std::endl;
    assert(result.final_x == 0.f);
    assert(result.final_y == 0.f);
  }

  // Test 8: Different screen resolutions
  std::cout << "\n8. Various screen resolutions:" << std::endl;
  struct ResTest { int w, h; };
  ResTest resolutions[] = {
    {1920, 1080}, {1280, 720}, {800, 600}, {640, 480}
  };
  for (auto &r : resolutions) {
    auto pos = calculate_centered_position(pixels(400), pixels(200), r.w, r.h);
    float expected_x = (static_cast<float>(r.w) - 400.f) / 2.f;
    float expected_y = (static_cast<float>(r.h) - 200.f) / 2.f;
    std::cout << "  - " << r.w << "x" << r.h << ": (" << pos.x << ", " << pos.y << ")" << std::endl;
    assert(std::abs(pos.x - expected_x) < 0.001f);
    assert(std::abs(pos.y - expected_y) < 0.001f);
  }

  // Test 9: Modal larger than screen (negative position = clamping scenario)
  std::cout << "\n9. Oversized modal (negative centering):" << std::endl;
  {
    auto pos = calculate_centered_position(pixels(1500), pixels(800), 1280, 720);
    std::cout << "  - Position: (" << pos.x << ", " << pos.y << ")" << std::endl;
    assert(pos.x < 0.f);
    assert(pos.y < 0.f);
  }

  std::cout << "\n=== All modal centering tests passed! ===" << std::endl;
  return 0;
}
