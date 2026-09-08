// viewport_test.cpp
// The letterbox transform was derived inline inside get_mouse_position and
// exposed nowhere, so an app presenting its own render texture re-derived it
// and kept the two in step with a comment. It also had no inverse, which an OS
// cursor rect, an IME candidate window or a drag hit region all need.

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

} // namespace

// Same aspect: no bars, scale is the ratio.
TEST(matching_aspect_letterboxes_nothing) {
  set_content_resolution(1280, 720);
  const auto vp = window_manager::content_viewport(2560, 1440);
  printf("  dest %.0fx%.0f at %.0f,%.0f scale %.2f\n", vp.dest.width,
         vp.dest.height, vp.dest.x, vp.dest.y, vp.scale);
  CHECK(near(vp.dest.x, 0.f));
  CHECK(near(vp.dest.y, 0.f));
  CHECK(near(vp.dest.width, 2560.f));
  CHECK(near(vp.scale, 0.5f)); // content px per window px
}

// A wider window than the content bars the sides, not the top.
TEST(a_wider_window_gets_pillarboxed) {
  set_content_resolution(1280, 720);
  const auto vp = window_manager::content_viewport(2000, 720);
  printf("  dest %.0fx%.0f at %.0f,%.0f\n", vp.dest.width, vp.dest.height,
         vp.dest.x, vp.dest.y);
  CHECK(near(vp.dest.height, 720.f));
  CHECK(near(vp.dest.width, 1280.f));
  CHECK(near(vp.dest.x, 360.f)); // (2000-1280)/2
  CHECK(near(vp.dest.y, 0.f));
}

TEST(a_taller_window_gets_letterboxed) {
  set_content_resolution(1280, 720);
  const auto vp = window_manager::content_viewport(1280, 1000);
  CHECK(near(vp.dest.width, 1280.f));
  CHECK(near(vp.dest.height, 720.f));
  CHECK(near(vp.dest.y, 140.f)); // (1000-720)/2
}

// The point of exposing it: a rect can go back the other way now.
TEST(window_and_content_round_trip) {
  set_content_resolution(1280, 720);
  const int win_w = 2000, win_h = 900;

  for (auto p : {Vector2Type{0.f, 0.f}, Vector2Type{640.f, 360.f},
                 Vector2Type{1279.f, 719.f}}) {
    const auto w = window_manager::content_to_window(p, win_w, win_h);
    const auto back = window_manager::window_to_content(w, win_w, win_h);
    printf("  content %.0f,%.0f -> window %.0f,%.0f -> %.0f,%.0f\n", p.x, p.y,
           w.x, w.y, back.x, back.y);
    CHECK(near(back.x, p.x));
    CHECK(near(back.y, p.y));
  }
}

// A cursor over a bar is outside the content, and reporting a clamped
// in-content position there would be a lie. Matches what get_mouse_position
// always did.
TEST(a_point_on_a_bar_is_returned_unchanged) {
  set_content_resolution(1280, 720);
  const auto p = Vector2Type{10.f, 400.f}; // inside the left pillar
  const auto mapped = window_manager::window_to_content(p, 2000, 720);
  CHECK(near(mapped.x, p.x));
  CHECK(near(mapped.y, p.y));
}

// No resolution registered means no letterbox, so an app that does not use one
// is unaffected.
TEST(without_a_content_resolution_it_is_identity) {
  set_content_resolution(800, 600);
  const auto vp = window_manager::content_viewport(800, 600);
  CHECK(near(vp.scale, 1.f));
  const auto p = Vector2Type{123.f, 456.f};
  const auto mapped = window_manager::window_to_content(p, 800, 600);
  CHECK(near(mapped.x, p.x));
  CHECK(near(mapped.y, p.y));
}

int main() { return ui_test::run_registered_tests("viewport"); }
