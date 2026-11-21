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

// R-104: an auto-tracked resolution (should_refetch) cached at launch must
// not letterbox the pointer after a native resize the app never drove
// through the library. Live size is 1100x820 (headless_resolution stands in
// for the backend's live fetch); the cache still says 1100x760.
TEST(auto_resolution_stale_after_resize_maps_unletterboxed) {
  auto *pcr = EntityHelper::get_singleton_cmp<
      window_manager::ProvidesCurrentResolution>();
  CHECK(pcr != nullptr);
  pcr->current_resolution = window_manager::Resolution{1100, 760};
  pcr->should_refetch = true;
  const auto saved_headless = window_manager::headless_resolution;
  window_manager::headless_resolution = window_manager::Resolution{1100, 820};
  const auto mapped =
      window_manager::window_to_content(Vector2Type{140.f, 400.f}, 1100, 820);
  printf("  stale-auto mapped %.1f,%.1f (want 140,400)\n", mapped.x, mapped.y);
  CHECK(near(mapped.x, 140.f));
  CHECK(near(mapped.y, 400.f));
  window_manager::headless_resolution = saved_headless;
  set_content_resolution(1280, 720);
}

// Collect must refresh an auto resolution and leave a fixed one alone.
TEST(collect_refreshes_auto_but_not_fixed_resolution) {
  Entity &e = EntityHelper::createPermanentEntity();
  const auto saved_headless = window_manager::headless_resolution;
  window_manager::headless_resolution = window_manager::Resolution{1100, 820};
  window_manager::CollectCurrentResolution collect;
  window_manager::ProvidesCurrentResolution auto_pcr;
  auto_pcr.current_resolution = window_manager::Resolution{1100, 760};
  collect.for_each_with(e, auto_pcr, 0.f);
  CHECK(auto_pcr.current_resolution.width == 1100);
  CHECK(auto_pcr.current_resolution.height == 820);
  CHECK(auto_pcr.should_refetch);
  window_manager::ProvidesCurrentResolution fixed_pcr(
      window_manager::Resolution{1280, 720});
  collect.for_each_with(e, fixed_pcr, 0.f);
  CHECK(fixed_pcr.current_resolution.width == 1280);
  CHECK(fixed_pcr.current_resolution.height == 720);
  window_manager::headless_resolution = saved_headless;
}

// A saved resolution index past this machine's list falls back to current.
TEST(stale_resolution_index_falls_back_to_current) {
  set_content_resolution(1280, 720);
  window_manager::ProvidesAvailableWindowResolutions available(
      std::vector<window_manager::Resolution>{{1280, 720}});
  const auto chosen = available.on_data_changed(2);
  CHECK(chosen.width == 1280);
  CHECK(chosen.height == 720);

  window_manager::ProvidesAvailableWindowResolutions empty(
      std::vector<window_manager::Resolution>{});
  CHECK(empty.current_index() == 0);
  const auto unchanged = empty.on_data_changed(0);
  CHECK(unchanged.width == 1280);
  CHECK(unchanged.height == 720);
}

int main() { return ui_test::run_registered_tests("viewport"); }
