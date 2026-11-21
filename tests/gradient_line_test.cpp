// gradient_line_test.cpp — imm gradient fill + line component (R-057).
#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

TEST(gradient_lerp_endpoints_and_midpoint) {
  const Color a{0, 10, 20, 0}, b{100, 210, 255, 200};
  Color t0 = gradient_lerp(a, b, 0.f), t1 = gradient_lerp(a, b, 1.f);
  CHECK(t0.r == 0 && t0.g == 10 && t0.b == 20 && t0.a == 0);
  CHECK(t1.r == 100 && t1.g == 210 && t1.b == 255 && t1.a == 200);
  Color mid = gradient_lerp(a, b, 0.5f);
  CHECK(mid.r == 50 && mid.g == 110 && mid.a == 100);
  Color clamped = gradient_lerp(a, b, 2.f);
  CHECK(clamped.r == 100 && clamped.a == 200);
}

TEST(gradient_buffer_command_carries_stops) {
  Arena arena(Arena::DEFAULT_CAPACITY);
  RenderCommandBuffer buf(arena);
  buf.add_gradient_rectangle(RectangleType{1, 2, 30, 40}, Color{255, 0, 0, 255},
                             Color{0, 0, 255, 255}, false, 3, 7);
  CHECK(buf.size() == 1);
  const auto &c = buf.commands()[0];
  CHECK(c.type == RenderPrimitiveType::GradientRectangle);
  CHECK(c.data.gradient.from.r == 255 && c.data.gradient.to.b == 255);
  CHECK(!c.data.gradient.horizontal);
  CHECK(c.data.gradient.rect.width == 30.f);
}

TEST(line_point_geometry) {
  const RectangleType r{10, 20, 100, 50};
  auto p0 = line_point(r, Vector2Type{0.f, 0.f});
  auto p1 = line_point(r, Vector2Type{1.f, 1.f});
  auto mid = line_point(r, Vector2Type{0.5f, 0.5f});
  CHECK_APPROX(p0.x, 10.f); CHECK_APPROX(p0.y, 20.f);
  CHECK_APPROX(p1.x, 110.f); CHECK_APPROX(p1.y, 70.f);
  CHECK_APPROX(mid.x, 60.f); CHECK_APPROX(mid.y, 45.f);
}

TEST(config_and_imm_line_attach_components) {
  ComponentConfig cfg =
      ComponentConfig{}.with_gradient(Color{1, 2, 3, 255}, Color{4, 5, 6, 255});
  CHECK(cfg.gradient.has_value() && cfg.gradient->horizontal);

  ImmTestHarness h;
  auto box = div(h.context(), mk(h.root(), 0),
                 ComponentConfig{}
                     .with_size(ComponentSize{pixels(100), pixels(50)})
                     .with_gradient(Color{255, 0, 0, 255}, Color{0, 0, 255, 255},
                                    false)
                     .with_debug_name("grad_box"));
  CHECK(box.ent().has<HasGradient>());
  if (box.ent().has<HasGradient>()) {
    const auto &g = box.ent().get<HasGradient>();
    CHECK(g.from.r == 255 && g.to.b == 255 && !g.horizontal);
  }
  auto ln = line(h.context(), mk(h.root(), 1), Vector2Type{0.f, 0.f},
                 Vector2Type{1.f, 1.f}, Color{0, 255, 0, 255}, 2.f,
                 ComponentConfig{}
                     .with_size(ComponentSize{pixels(80), pixels(40)})
                     .with_debug_name("diag"));
  CHECK(ln.ent().has<HasLine>());
  if (ln.ent().has<HasLine>()) {
    const auto &l = ln.ent().get<HasLine>();
    CHECK(l.color.g == 255 && l.thickness == 2.f);
    CHECK_APPROX(l.to.x, 1.f); CHECK_APPROX(l.to.y, 1.f);
  }
}

int main() { return ui_test::run_registered_tests("gradient_line"); }
