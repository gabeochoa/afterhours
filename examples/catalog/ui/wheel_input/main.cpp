#include <cstdio>

#include "../../../../ah.h"
#include "../../../../src/graphics.h"
#define AFTER_HOURS_IMM_UI
#include "../../../../src/plugins/e2e_testing/platform_test_input.h"
#include "../../../../src/plugins/ui.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

struct FilesView : System<DefaultUIContext> {
  EntityID file_list = -1;
  EntityID tab_strip = -1;

  void for_each_with(Entity &root, DefaultUIContext &ctx, float) override {
    auto page = div(ctx, mk(root, 0),
                    ComponentConfig{}
                        .with_size({pixels(300), pixels(160)})
                        .with_overflow(Overflow::Scroll, Axis::Y));
    div(ctx, mk(page.ent(), 0),
        ComponentConfig{}.with_size({pixels(300), pixels(100)}).with_label("Filters"));
    auto list = div(ctx, mk(page.ent(), 1),
                    ComponentConfig{}
                        .with_size({pixels(300), pixels(120)})
                        .with_overflow(Overflow::Scroll, Axis::Y));
    for (int i = 0; i < 12; ++i)
      div(ctx, mk(list.ent(), i),
          ComponentConfig{}.with_size({pixels(300), pixels(24)}).with_label("file"));
    auto &page_scroll = page.ent().get<HasScrollView>();
    page_scroll.scroll_offset.y = page_scroll.scroll_target.y = 48;
    file_list = list.id();

    auto strip = div(ctx, mk(root, 1),
                     ComponentConfig{}
                         .with_size({pixels(300), pixels(30)})
                         .with_absolute_position(0, 170)
                         .with_flex_direction(FlexDirection::Row)
                         .with_overflow(Overflow::Scroll, Axis::X));
    for (int i = 0; i < 8; ++i)
      div(ctx, mk(strip.ent(), i),
          ComponentConfig{}.with_size({pixels(100), pixels(30)}).with_label("tab"));
    tab_strip = strip.id();
  }
};

static float scrolled(EntityID id, bool horizontal) {
  const auto &s = UICollectionHolder::getEntityForIDEnforce(id).get<HasScrollView>();
  return horizontal ? s.scroll_target.x : s.scroll_target.y;
}

static void wheel_at(SystemManager &systems, float x, float y, bool shift) {
  testing::test_input::set_mouse_position(x, y);
  if (shift)
    testing::input_injector::set_key_held(keys::LEFT_SHIFT);
  testing::input_injector::set_mouse_wheel(0, -1);
  systems.run(1.f / 60.f);
  testing::input_injector::reset_all();
}

int main() {
  graphics::Config config;
  config.display = graphics::DisplayMode::Headless;
  config.width = 400;
  config.height = 220;
  if (!graphics::init(config))
    return 1;

  SystemManager systems;
  auto view = std::make_unique<FilesView>();
  FilesView &files = *view;
  ui::setup_with_resolution<>(systems, {400, 220}, std::move(view));
  testing::platform_input::set_test_mode(true);
  systems.run(1.f / 60.f);
  systems.run(1.f / 60.f);

  wheel_at(systems, 100, 70, false);
  const float list_y = scrolled(files.file_list, false);
  wheel_at(systems, 100, 185, true);
  const float strip_x = scrolled(files.tab_strip, true);
  const bool list_left_alone = scrolled(files.file_list, false) == list_y;

  std::printf("wheel over a visible row of the scrolled-up file list moved it "
              "%.0f px; Shift+wheel over the tab strip moved it %.0f px and the "
              "list %s\n",
              static_cast<double>(list_y), static_cast<double>(strip_x),
              list_left_alone ? "not at all" : "too");
  graphics::shutdown();
  return list_y > 0.f && strip_x > 0.f && list_left_alone ? 0 : 1;
}
