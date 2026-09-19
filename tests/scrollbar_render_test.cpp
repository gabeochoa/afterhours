#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

static void check_layers(bool batched) {
  ui_test::ImmTestHarness h;
  h.begin_frame();
  const ColorType bar_color{231, 17, 53, 255};
  std::vector<EntityID> owners;
  for (int layer : {0, 10, 20}) {
    auto view = div(h.context(), mk(h.root(), layer), ComponentConfig{}
        .with_size({pixels(200), pixels(100)})
        .with_absolute_position(0, 0).with_render_layer(layer)
        .with_overflow(Overflow::Scroll, Axis::Y));
    owners.push_back(view.ent().id);
    view.ent().get<HasScrollView>().scrollbar_thumb_color = bar_color;
    div(h.context(), mk(view.ent(), 0), ComponentConfig{}
        .with_size({pixels(200), pixels(400)}).with_render_layer(layer)
        .with_custom_background(ColorType{40, 50, 60, 255}));
  }
  h.layout_only();
  for (auto id : owners) {
    auto &entity = UICollectionHolder::getEntityForID(id).asE();
    auto &scroll = entity.get<HasScrollView>();
    scroll.viewport_size = {200, 100};
    scroll.content_size = {200, 400};
  }
  const auto &calls = batched ? h.render_batched() : h.render();
  int previous_layer = 0;
  int thumbs = 0;
  std::array<ColorType, 3> last_colors{};
  for (const auto &call : calls) {
    CHECK(call.layer >= previous_layer);
    previous_layer = call.layer;
    last_colors[static_cast<size_t>(call.layer / 10)] = call.color;
    if (call.color.r != bar_color.r || call.color.g != bar_color.g) continue;
    CHECK(call.entity_id == owners[static_cast<size_t>(call.layer / 10)]);
    CHECK(call.rect.height < 100.f);
    ++thumbs;
  }
  CHECK(thumbs == 3);
  for (const auto &color : last_colors) CHECK(color.r == bar_color.r && color.g == bar_color.g);
}

TEST(immediate_scrollbars_stay_below_higher_layers) { check_layers(false); }
TEST(batched_scrollbars_stay_below_higher_layers) { check_layers(true); }

int main() { return ui_test::run_registered_tests("scrollbar rendering"); }
