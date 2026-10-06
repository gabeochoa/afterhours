// scroll_item_anchor_test.cpp
// A logical anchor keeps an item -- named by a key the app owns -- at a place
// in the viewport, across delayed content, replaced entities, removed rows,
// zoom and resizes, and stops the moment the user scrolls.

#include "ui_test_harness.h"
#include <afterhours/src/plugins/ui/scroll_item_anchor.h>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

namespace {
struct Reader {
  ImmTestHarness h;
  std::vector<std::string> items;
  float zoom = 1.f;
  float view_height = 200.f;
  int slot = 0;
  EntityID list = -1;

  static float height_of(const std::string &item) { return 18.f + 7.f * (item.size() % 4); }

  void frame() {
    h.context().theme.ui_scale = zoom;
    h.begin_frame();
    auto result = virtual_list(
        h.context(), mk(h.root(), slot), items.size(),
        [&](size_t i) { return height_of(items[i]); }, [](size_t, Entity &) {},
        ComponentConfig{}.with_size({pixels(300), pixels(view_height)})
            .with_scaling_mode(ScalingMode::Adaptive));
    list = result.id();
    h.coll.merge_entity_arrays();
    std::vector<Entity *> mapping;
    for (const auto &e : h.coll.get_entities()) {
      if (!e) continue;
      mapping.resize(std::max(mapping.size(), static_cast<size_t>(e->id) + 1));
      mapping[static_cast<size_t>(e->id)] = e.get();
      if (e->has<UIComponent>()) e->get<UIComponent>().was_rendered_to_screen = true;
    }
    AutoLayout::autolayout(h.root().get<UIComponent>(), {800, 600}, mapping, false, zoom);
    MeasureScrollViews{}.for_each_with(view(), scroll(), view().get<UIComponent>(), 0.f);
  }
  Entity &view() { return AutoLayout::to_ent_static(list); }
  HasScrollView &scroll() { return view().get<HasScrollView>(); }
  std::optional<ItemGeometry> geometry(std::string_view key) {
    const auto it = std::find(items.begin(), items.end(), key);
    if (it == items.end()) return std::nullopt;
    const size_t i = static_cast<size_t>(it - items.begin());
    const auto &index = view().get<HasVirtualListIndex>();
    return ItemGeometry{virtual_row_offset(view(), i), index.heights[i] * index.last_scale};
  }
  AnchorStatus resolve(std::uint64_t generation) {
    return resolve_scroll_anchor(view(), generation,
                                 [&](std::string_view key) { return geometry(key); });
  }
  // Where `fraction` of the item's height sits, as a fraction of the band.
  float band_fraction(const std::string &key, float within, float top = 0.f, float bottom = 0.f) {
    const auto item = *geometry(key);
    const float y = item.top + within * item.height - scroll().scroll_offset.y - top;
    return y / (scroll().viewport_size->y - top - bottom);
  }
};

std::vector<std::string> numbered(size_t n) {
  std::vector<std::string> out;
  for (size_t i = 0; i < n; i++) out.push_back("row" + std::to_string(i));
  return out;
}
} // namespace

TEST(waits_for_delayed_content_then_applies_once) {
  Reader r;
  r.frame();
  request_scroll_anchor(r.view(), {"row120", .5f, .25f, 0.f, 0.f, 2});
  for (int i = 0; i < 5; i++) {
    r.frame();
    CHECK(r.resolve(1) == AnchorStatus::Pending); // no frame-count give-up
  }
  r.items = numbered(400);
  r.frame();
  CHECK(r.resolve(2) == AnchorStatus::Applied);
  CHECK_APPROX(r.band_fraction("row120", .5f) * 100.f, 25.f);
  const auto &s = r.scroll();
  CHECK(s.scroll_offset.y == s.scroll_target.y && s.scroll_target.y == s.last_eased_offset.y);
  // One-shot: later layout changes do not drag the view back.
  r.scroll().jump_to_y(0.f);
  CHECK(r.resolve(2) == AnchorStatus::Applied);
  CHECK(r.scroll().scroll_offset.y == 0.f);
}

TEST(resolves_on_a_replaced_view_entity) {
  Reader r;
  r.items = numbered(400);
  r.frame();
  r.scroll().jump_to_y(1500.f);
  r.frame();
  auto anchor = capture_scroll_anchor(r.scroll(), "row90", *r.geometry("row90"), .1f);
  const EntityID old = r.list;
  r.slot = 1; // the app rebuilt its reader under a new entity
  r.frame();
  CHECK(r.list != old);
  request_scroll_anchor(r.view(), anchor);
  CHECK(r.resolve(0) == AnchorStatus::Applied);
  CHECK_APPROX(r.band_fraction("row90", anchor.within) * 100.f, 10.f);
}

TEST(a_removed_anchor_row_is_unavailable_and_leaves_the_view) {
  Reader r;
  r.items = numbered(400);
  r.frame();
  r.scroll().jump_to_y(700.f);
  r.frame();
  request_scroll_anchor(r.view(), {"row50", 0.f, 0.f, 0.f, 0.f, 3});
  r.items.erase(r.items.begin() + 50);
  r.frame();
  CHECK(r.resolve(3) == AnchorStatus::Unavailable);
  CHECK(r.scroll().scroll_offset.y == 700.f);
  // Content that moved past the requested generation is unavailable too.
  request_scroll_anchor(r.view(), {"row60", 0.f, 0.f, 0.f, 0.f, 3});
  CHECK(r.resolve(4) == AnchorStatus::Unavailable);
}

TEST(survives_zoom_resize_and_occlusion) {
  for (const float zoom : {1.4f, 2.f}) {
    Reader r;
    r.items = numbered(400);
    r.frame();
    r.scroll().jump_to_y(2000.f);
    r.frame();
    auto anchor = capture_scroll_anchor(r.scroll(), "row101", *r.geometry("row101"), .4f);
    r.zoom = zoom;
    r.frame();
    request_scroll_anchor(r.view(), anchor);
    CHECK(r.resolve(0) == AnchorStatus::Applied);
    CHECK_APPROX(r.band_fraction("row101", anchor.within) * 100.f, 40.f);

    r.view_height = 320.f;
    r.frame();
    request_scroll_anchor(r.view(), anchor);
    CHECK(r.resolve(0) == AnchorStatus::Applied);
    CHECK_APPROX(r.band_fraction("row101", anchor.within) * 100.f, 40.f);

    anchor.inset_top = 30.f * zoom; // a sticky header
    anchor.inset_bottom = 20.f * zoom;
    request_scroll_anchor(r.view(), anchor);
    CHECK(r.resolve(0) == AnchorStatus::Applied);
    CHECK_APPROX(r.band_fraction("row101", anchor.within, 30.f * zoom, 20.f * zoom) * 100.f, 40.f);
  }
}

TEST(real_wheel_input_cancels_a_pending_anchor) {
  Reader r;
  r.items = numbered(400);
  r.frame();
  request_scroll_anchor(r.view(), {"row200", 0.f, 0.f, 0.f, 0.f, 5});
  CHECK(r.resolve(4) == AnchorStatus::Pending);

  const auto rect = r.view().get<UIComponent>().rect();
  r.h.context().mouse.pos = {rect.x + 20, rect.y + 20};
  HandleScrollInput<ui_test::TestInputAction> wheel;
  wheel.context = &r.h.context();
  testing::test_input::detail::test_mode = true;
  testing::input_injector::reset_all();
  testing::input_injector::set_mouse_wheel(0, -3);
  wheel.for_each_with(r.view(), r.view().get<UIComponent>(), r.scroll(), 1.f / 60.f);
  testing::input_injector::reset_all();
  testing::test_input::detail::test_mode = false;
  const float wheeled = r.scroll().scroll_target.y;
  CHECK(wheeled > 0.f);

  r.frame();
  CHECK(r.resolve(5) == AnchorStatus::Cancelled);
  CHECK(r.scroll().scroll_target.y == wheeled);

  // Clamping and the list's own writes are not user input.
  request_scroll_anchor(r.view(), {"row200", 0.f, 0.f, 0.f, 0.f, 5});
  r.scroll().scroll_target.y = 1e9f;
  r.frame();
  CHECK(r.resolve(5) == AnchorStatus::Applied);
}

int main() { return ui_test::run_registered_tests("scroll_item_anchor"); }
