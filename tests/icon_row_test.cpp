// icon_row must forward the caller's container config (gap, alignment,
// padding, absolute placement, grid flag) to its row instead of dropping
// it. Regression test for "icon_row inherits config and loses absolute
// placement, gap and grid flags" (AFTERHOURS_GAPS, Tier-4 b23).

#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

namespace {
const std::array<texture_manager::Rectangle, 4> kFrames{
    texture_manager::Rectangle{0, 0, 20, 20},
    texture_manager::Rectangle{20, 0, 20, 20},
    texture_manager::Rectangle{40, 0, 20, 20},
    texture_manager::Rectangle{60, 0, 20, 20}};

texture_manager::Texture sheet() { return texture_manager::Texture{}; }

// Lays out one icon_row with the given config; returns the row and its
// four item components' rects via the harness.
void layout_row(ImmTestHarness &h, ComponentConfig config) {
  icon_row(h.context(), mk(h.root(), 0), sheet(), kFrames, 1.f,
           std::move(config));
  h.layout_only();
}
} // namespace

// With no config the icons sit back to back: each step is one icon wide.
TEST(defaults_unchanged) {
  { // The inherit_from chain itself must carry the fields.
    ComponentConfig caller{};
    caller.with_justify_content(JustifyContent::FlexEnd)
        .with_align_items(AlignItems::Center)
        .with_gap(pixels(10));
    auto chained =
        ComponentConfig::inherit_from(caller, "icon_row")
            .with_gap(caller.flex_gap)
            .with_align_items(caller.align_items)
            .with_justify_content(caller.justify_content);
    CHECK(chained.justify_content == JustifyContent::FlexEnd);
    CHECK(chained.align_items == AlignItems::Center);
    CHECK_APPROX(chained.flex_gap.value, 10.f);
  }
  ImmTestHarness h;
  layout_row(h, ComponentConfig{}.with_debug_name("icons"));
  UIComponent *row = h.find("icons");
  UIComponent *first = h.find("icon_row_item_0");
  UIComponent *second = h.find("icon_row_item_1");
  CHECK(row != nullptr && first != nullptr && second != nullptr);
  if (!row || !first || !second)
    return;
  CHECK_APPROX(second->rect().x - first->rect().x, 20.f);
  CHECK_APPROX(row->desired_gap.value, 0.f);
  CHECK(!row->skip_grid_snap);
  CHECK(!first->skip_grid_snap);
}

// with_gap(pixels(10)) must space the icons 10px apart: step = 20 + 10.
TEST(gap_forwarded) {
  ImmTestHarness h;
  layout_row(h, ComponentConfig{}
                    .with_gap(pixels(10))
                    .with_debug_name("icons"));
  UIComponent *row = h.find("icons");
  UIComponent *first = h.find("icon_row_item_0");
  UIComponent *second = h.find("icon_row_item_1");
  CHECK(row != nullptr && first != nullptr && second != nullptr);
  if (!row || !first || !second)
    return;
  CHECK_APPROX(row->desired_gap.value, 10.f);
  CHECK_APPROX(second->rect().x - first->rect().x, 30.f);
}

// Padding insets the first icon from the row's edge.
TEST(padding_forwarded) {
  ImmTestHarness h;
  layout_row(h, ComponentConfig{}
                    .with_padding(Padding::all(pixels(8)))
                    .with_debug_name("icons"));
  UIComponent *row = h.find("icons");
  UIComponent *first = h.find("icon_row_item_0");
  CHECK(row != nullptr && first != nullptr);
  if (!row || !first)
    return;
  CHECK_APPROX(first->rect().x - row->rect().x, 8.f);
}

// align_items(Center) centres the 20px icons in the 60px row: with 4px
// padding the first icon sits 4 + (60 - 8 - 20) / 2 = 20px below the row.
TEST(align_items_forwarded) {
  ImmTestHarness h;
  layout_row(h, ComponentConfig{}
                    .with_size(ComponentSize{pixels(200), pixels(60)})
                    .with_padding(Padding::all(pixels(4)))
                    .with_align_items(AlignItems::Center)
                    .with_debug_name("icons"));
  UIComponent *row = h.find("icons");
  UIComponent *first = h.find("icon_row_item_0");
  CHECK(row != nullptr && first != nullptr);
  if (!row || !first)
    return;
  CHECK_APPROX(first->rect().y - row->rect().y, 20.f);
}

// justify_content(FlexEnd) pushes the 80px of icons to the row's far edge:
// with 4px padding the first icon sits 4 + (200 - 8 - 80) = 116px in.
TEST(justify_content_forwarded) {
  ImmTestHarness h;
  layout_row(h, ComponentConfig{}
                    .with_size(ComponentSize{pixels(200), pixels(40)})
                    .with_padding(Padding::all(pixels(4)))
                    .with_justify_content(JustifyContent::FlexEnd)
                    .with_debug_name("icons"));
  UIComponent *row = h.find("icons");
  UIComponent *first = h.find("icon_row_item_0");
  CHECK(row != nullptr && first != nullptr);
  if (!row || !first)
    return;
  CHECK_APPROX(first->rect().x - row->rect().x, 116.f);
}

// Absolute placement moves the whole row (and its icons) off the origin.
TEST(absolute_placement_forwarded) {
  ImmTestHarness h;
  layout_row(h, ComponentConfig{}
                    .with_absolute_position(pixels(50), pixels(30))
                    .with_debug_name("icons"));
  UIComponent *row = h.find("icons");
  UIComponent *first = h.find("icon_row_item_0");
  CHECK(row != nullptr && first != nullptr);
  if (!row || !first)
    return;
  CHECK_APPROX(row->rect().x, 50.f);
  CHECK_APPROX(row->rect().y, 30.f);
  CHECK(first->rect().x >= row->rect().x);
}

// The grid flag reaches the row and its items (WM set it per item by hand).
TEST(skip_grid_snap_forwarded) {
  ImmTestHarness h;
  layout_row(h, ComponentConfig{}
                    .with_skip_grid_snap()
                    .with_debug_name("icons"));
  UIComponent *row = h.find("icons");
  UIComponent *first = h.find("icon_row_item_0");
  CHECK(row != nullptr && first != nullptr);
  if (!row || !first)
    return;
  CHECK(row->skip_grid_snap);
  CHECK(first->skip_grid_snap);
}

int main() { return ui_test::run_registered_tests("icon row"); }
