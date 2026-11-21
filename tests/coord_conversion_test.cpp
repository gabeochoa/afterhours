// coord_conversion_test.cpp
// layout_to_window / window_to_layout compose ui_scale and a scroll offset
// around the letterbox (content_viewport). Before, an app with design-unit
// coordinates divided the mouse by the zoom by hand and re-derived the
// letterbox itself; the mouse path already mapped window pixels to content
// pixels through window_to_content, but nothing exposed the layout layer.

#include "ui_test_harness.h"

using namespace afterhours;

namespace {

void set_content_resolution(int w, int h) {
  auto *pcr = EntityHelper::get_singleton_cmp<
      window_manager::ProvidesCurrentResolution>();
  if (pcr) {
    pcr->current_resolution.width = w;
    pcr->current_resolution.height = h;
    return;
  }
  Entity &e = EntityHelper::createPermanentEntity();
  e.addComponent<window_manager::ProvidesCurrentResolution>(
      window_manager::Resolution{w, h});
  EntityHelper::registerSingleton<window_manager::ProvidesCurrentResolution>(e);
}

bool near(float a, float b, float eps = 0.5f) { return std::abs(a - b) < eps; }

bool near_pt(Vector2Type a, Vector2Type b, float eps = 0.5f) {
  return near(a.x, b.x, eps) && near(a.y, b.y, eps);
}

bool near_rect(RectangleType a, RectangleType b, float eps = 0.5f) {
  return near(a.x, b.x, eps) && near(a.y, b.y, eps) &&
         near(a.width, b.width, eps) && near(a.height, b.height, eps);
}

} // namespace

// The floatinghotel case: a draggable panel holds design units, the pointer
// arrives as window pixels, zoom is not 100%, and the panel sits in a scroller.
TEST(layout_point_round_trips_through_letterbox_scale_and_scroll) {
  set_content_resolution(1280, 720);
  const int win_w = 2000, win_h = 900; // pillarboxed letterbox
  const float ui_scale = 1.4f;
  const Vector2Type scroll{30.f, 120.f};

  // Points must stay inside the content after scroll: layout*1.4 - scroll
  // is what lands on the letterbox, so y below ~86 is scrolled off the top.
  for (auto p : {Vector2Type{200.f, 150.f}, Vector2Type{640.f, 360.f},
                 Vector2Type{900.f, 550.f}}) {
    const auto win = window_manager::layout_to_window(p, win_w, win_h, ui_scale,
                                                      scroll);
    const auto back = window_manager::window_to_layout(win, win_w, win_h,
                                                       ui_scale, scroll);
    printf("  layout %.0f,%.0f -> window %.0f,%.0f -> %.0f,%.0f\n", p.x, p.y,
           win.x, win.y, back.x, back.y);
    CHECK(near_pt(back, p, 1.f));
  }
}

TEST(layout_rect_round_trips_through_letterbox_scale_and_scroll) {
  set_content_resolution(1280, 720);
  const int win_w = 2560, win_h = 1080; // letterboxed (1.25x scale)
  const float ui_scale = 2.f;
  const Vector2Type scroll{16.f, 8.f};
  const RectangleType r{100.f, 60.f, 240.f, 90.f};

  const auto win = window_manager::layout_rect_to_window(r, win_w, win_h,
                                                         ui_scale, scroll);
  // Letterbox: 1920x1080 at x=320, scale 2/3. Content origin is 2x layout
  // minus scroll (184, 112), which lands at (320 + 184*1.5, 112*1.5).
  printf("  rect %.0f,%.0f %.0fx%.0f -> window %.0f,%.0f %.0fx%.0f\n", r.x, r.y,
         r.width, r.height, win.x, win.y, win.width, win.height);
  CHECK(near_rect(win, RectangleType{596.f, 168.f, 720.f, 270.f}, 1.f));

  const auto back = window_manager::window_to_layout_rect(win, win_w, win_h,
                                                          ui_scale, scroll);
  CHECK(near_rect(back, r, 1.f));
}

// With no zoom and no scroll the layout helpers are the letterbox transforms.
TEST(layout_helpers_default_to_the_letterbox_transform) {
  set_content_resolution(1280, 720);
  const int win_w = 2000, win_h = 720;
  const Vector2Type p{640.f, 360.f};
  const auto via_layout =
      window_manager::layout_to_window(p, win_w, win_h);
  const auto via_content =
      window_manager::content_to_window(p, win_w, win_h);
  CHECK(near_pt(via_layout, via_content));

  const RectangleType r{10.f, 20.f, 100.f, 50.f};
  const auto rect_via_layout =
      window_manager::layout_rect_to_window(r, win_w, win_h);
  const auto corner = window_manager::content_to_window(
      Vector2Type{r.x, r.y}, win_w, win_h);
  CHECK(near_pt(Vector2Type{rect_via_layout.x, rect_via_layout.y}, corner));
  CHECK(near_rect(rect_via_layout,
                  RectangleType{corner.x, corner.y, r.width, r.height}));
}

// A non-positive scale would divide by zero on the way back; treat it as 1x.
TEST(non_positive_scale_is_one) {
  set_content_resolution(1280, 720);
  const Vector2Type p{100.f, 100.f};
  const auto with_zero = window_manager::layout_to_window(p, 1280, 720, 0.f);
  const auto with_one = window_manager::layout_to_window(p, 1280, 720, 1.f);
  CHECK(near_pt(with_zero, with_one));
  const auto back = window_manager::window_to_layout(with_zero, 1280, 720, 0.f);
  CHECK(near_pt(back, p));
}

int main() { return ui_test::run_registered_tests("coord_conversion"); }
