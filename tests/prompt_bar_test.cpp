#include "ui_test_harness.h"

#include <afterhours/src/graphics.h>
#include <afterhours/src/plugins/ui/prompt_bar.h>

#include <filesystem>
#include <fstream>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

namespace {
Entity &ent_for(ImmTestHarness &h, const char *name) {
  UIComponent *cmp = h.find(name);
  return UICollectionHolder::getEntityForID(cmp->id).asE();
}
bool same_color(Color a, Color b) {
  return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}
} // namespace

TEST(keycap_centers_short_and_wide_keys) {
  ImmTestHarness h;
  h.begin_frame();
  auto a = keycap(h.context(), mk(h.root(), 0), "A",
                  ComponentConfig{}.with_debug_name("cap_a"));
  auto enter = keycap(h.context(), mk(h.root(), 1), "Enter",
                      ComponentConfig{}.with_debug_name("cap_enter"));
  auto esc = keycap(h.context(), mk(h.root(), 2), "Esc",
                    ComponentConfig{}.with_debug_name("cap_esc"));
  h.layout_only();

  for (auto *r : {&a, &enter, &esc}) {
    CHECK(r->ent().has<HasLabel>());
    if (r->ent().has<HasLabel>())
      CHECK(r->ent().get<HasLabel>().alignment == TextAlignment::Center);
    CHECK(r->ent().get<UIComponent>().skip_when_tabbing);
  }
  auto *cap_a = h.find("cap_a");
  auto *cap_enter = h.find("cap_enter");
  auto *cap_esc = h.find("cap_esc");
  CHECK(cap_a != nullptr && cap_enter != nullptr && cap_esc != nullptr);
  if (cap_a && cap_enter && cap_esc) {
    CHECK(cap_a->rect().width > 0.f && cap_a->rect().height > 0.f);
    CHECK(cap_a->rect().width >= cap_a->rect().height);
    CHECK(cap_enter->rect().width >= cap_enter->rect().height);
    CHECK(cap_enter->rect().width > cap_a->rect().width);
    CHECK(cap_esc->rect().width > cap_a->rect().width);
    CHECK_APPROX(cap_a->rect().height, 28.f);
    CHECK_APPROX(cap_enter->rect().height, 28.f);
  }
}

TEST(keycap_respects_custom_size_and_color) {
  ImmTestHarness h;
  h.begin_frame();
  const Color bg{34, 80, 140, 255};
  const Color fg{255, 220, 90, 255};
  keycap(h.context(), mk(h.root(), 0), "Tab",
         ComponentConfig{}
             .with_size(ComponentSize{pixels(56.f), pixels(36.f)})
             .with_custom_background(bg)
             .with_custom_text_color(fg)
             .with_debug_name("cap_custom"));
  h.layout_only();

  auto *cap = h.find("cap_custom");
  CHECK(cap != nullptr);
  if (cap) {
    CHECK_APPROX(cap->rect().width, 56.f);
    CHECK_APPROX(cap->rect().height, 36.f);
  }
  Entity &e = ent_for(h, "cap_custom");
  CHECK(e.has<HasColor>());
  if (e.has<HasColor>())
    CHECK(same_color(e.get<HasColor>().color(), bg));
  CHECK(e.has<HasLabel>());
  if (e.has<HasLabel>() && e.get<HasLabel>().explicit_text_color.has_value())
    CHECK(same_color(e.get<HasLabel>().explicit_text_color.value(), fg));
}

TEST(prompt_bar_renders_items_in_a_row_without_overlap) {
  ImmTestHarness h;
  h.begin_frame();
  std::vector<PromptItem> items = {{"Enter", "SELECT"}, {"Esc", "BACK"}, {"Y", "Reset"}};
  prompt_bar(h.context(), mk(h.root(), 0), std::span<const PromptItem>(items),
             ComponentConfig{}.with_debug_name("bar"));
  h.layout_only();

  auto *bar = h.find("bar");
  CHECK(bar != nullptr);
  if (bar)
    CHECK(bar->flex_direction == FlexDirection::Row);

  UIComponent *prev_item = nullptr;
  for (int i = 0; i < 3; ++i) {
    auto *item = h.find(fmt::format("prompt_item_{}", i));
    auto *cap = h.find(fmt::format("prompt_keycap_{}", i));
    auto *label = h.find(fmt::format("prompt_label_{}", i));
    CHECK(item != nullptr && cap != nullptr && label != nullptr);
    if (!item || !cap || !label)
      continue;
    CHECK(item->rect().width > 0.f && item->rect().height > 0.f);
    CHECK(cap->rect().width > 0.f && cap->rect().height > 0.f);
    CHECK(cap->rect().width >= cap->rect().height);
    CHECK(label->rect().width > 0.f && label->rect().height > 0.f);
    if (prev_item)
      CHECK(item->rect().x >=
            prev_item->rect().x + prev_item->rect().width - 0.5f);
    CHECK(cap->rect().x >= item->rect().x - 0.5f);
    CHECK(label->rect().x >= cap->rect().x + cap->rect().width - 0.5f);
    Entity &cap_ent = ent_for(h, fmt::format("prompt_keycap_{}", i).c_str());
    CHECK(cap_ent.has<HasLabel>());
    if (cap_ent.has<HasLabel>())
      CHECK(cap_ent.get<HasLabel>().alignment == TextAlignment::Center);
    prev_item = item;
  }
}

TEST(prompt_bar_initializer_list_and_empty_bar) {
  ImmTestHarness h;
  h.begin_frame();
  auto result = prompt_bar(h.context(), mk(h.root(), 0),
                           {{"Enter", "SELECT"}, {"Esc", "BACK"}});
  CHECK(!static_cast<bool>(result));
  CHECK(result.as<int>() == -1);
  std::vector<PromptItem> none;
  auto empty = prompt_bar(h.context(), mk(h.root(), 1),
                          std::span<const PromptItem>(none));
  CHECK(!static_cast<bool>(empty));
  h.layout_only();
  CHECK(h.find("prompt_keycap_0") != nullptr);
  CHECK(h.find("prompt_label_1") != nullptr);
}

TEST(prompt_bar_clickable_on_press_fires_with_index) {
  ImmTestHarness h;
  int fired = 0;
  int last_fired = -1;
  Entity *clickable = nullptr;
  for (int phase = 0; phase < 3; ++phase) {
    h.begin_frame();
    if (clickable && clickable->has<HasClickListener>())
      clickable->get<HasClickListener>().down = (phase == 1);
    std::vector<PromptItem> items = {
        {"Enter", "SELECT", [&] { fired++; last_fired = 0; }},
        {"Esc", "BACK"},
        {"B", "Back", [&] { fired++; last_fired = 2; }},
    };
    auto result = prompt_bar(h.context(), mk(h.root(), 0),
                             std::span<const PromptItem>(items));
    const bool got_click = static_cast<bool>(result);
    const int got_index = result.as<int>();
    h.layout_only();
    if (phase == 1) {
      CHECK(got_click);
      CHECK(got_index == 0);
      CHECK(fired == 1);
      CHECK(last_fired == 0);
    } else {
      CHECK(!got_click);
      CHECK(fired == (phase == 2 ? 1 : 0));
    }
    auto *item_cmp = h.find("prompt_item_0");
    CHECK(item_cmp != nullptr);
    if (item_cmp)
      clickable = &UICollectionHolder::getEntityForID(item_cmp->id).asE();
    auto *mid = h.find("prompt_item_1");
    CHECK(mid != nullptr);
    if (mid) {
      Entity &mid_ent = UICollectionHolder::getEntityForID(mid->id).asE();
      CHECK(mid->skip_when_tabbing);
      CHECK(!mid_ent.has<HasClickListener>());
    }
  }
}

TEST(prompt_bar_respects_bar_and_child_overrides) {
  ImmTestHarness h;
  h.begin_frame();
  std::vector<PromptItem> items = {{"L", "Prev"}, {"R", "Next"}};
  const Color bar_bg{18, 24, 38, 255};
  const Color cap_bg{52, 68, 96, 255};
  prompt_bar(h.context(), mk(h.root(), 0), std::span<const PromptItem>(items),
             ComponentConfig{}
                 .with_custom_background(bar_bg)
                 .with_gap(pixels(24.f))
                 .with_debug_name("bar_custom"),
             ComponentConfig{}.with_custom_background(cap_bg),
             ComponentConfig{}.with_custom_text_color(Color{230, 238, 250, 255}));
  h.layout_only();

  auto *bar = h.find("bar_custom");
  auto *first = h.find("prompt_item_0");
  auto *second = h.find("prompt_item_1");
  CHECK(bar != nullptr && first != nullptr && second != nullptr);
  if (bar)
    CHECK_APPROX(bar->desired_gap.value, 24.f);
  if (first && second)
    CHECK(second->rect().x >= first->rect().x + first->rect().width + 20.f);
  Entity &cap_ent = ent_for(h, "prompt_keycap_0");
  CHECK(cap_ent.has<HasColor>());
  if (cap_ent.has<HasColor>())
    CHECK(same_color(cap_ent.get<HasColor>().color(), cap_bg));
}

TEST(render_three_styled_prompt_bars_to_png) {
  ImmTestHarness h;
  h.begin_frame();
  auto screen =
      div(h.context(), mk(h.root(), 0),
          ComponentConfig{}
              .with_size(ComponentSize{pixels(800.f), pixels(600.f)})
              .with_custom_background(Color{8, 11, 18, 255})
              .with_flex_direction(FlexDirection::Column)
              .with_padding(Padding::all(pixels(28.f)))
              .with_gap(pixels(28.f))
              .with_debug_name("shot_root"));

  div(h.context(), mk(screen.ent(), 90),
      ComponentConfig{}
          .with_label("Input prompts")
          .with_size(ComponentSize{children(), pixels(30.f)})
          .with_custom_text_color(Color{235, 242, 252, 255})
          .with_font_size(pixels(24.f))
          .with_debug_name("shot_title"));

  prompt_bar(h.context(), mk(screen.ent(), 0),
             {{"Enter", "SELECT"}, {"Esc", "BACK"}},
             ComponentConfig{}
                 .with_size(ComponentSize{percent(1.f), children()})
                 .with_custom_background(Color{13, 22, 38, 255})
                 .with_padding(Padding::all(pixels(14.f)))
                 .with_debug_name("shot_bar_dark"),
             ComponentConfig{}
                 .with_custom_background(Color{38, 64, 102, 255})
                 .with_custom_text_color(Color{160, 220, 255, 255})
                 .with_border(Color{90, 170, 235, 255}, pixels(1.f)),
             ComponentConfig{}
                 .with_size(ComponentSize{pixels(76.f), children()})
                 .with_custom_text_color(Color{225, 238, 252, 255}));

  prompt_bar(h.context(), mk(screen.ent(), 1),
             {{"Y", "Reset to default"}, {"B", "Back"}},
             ComponentConfig{}
                 .with_size(ComponentSize{percent(1.f), children()})
                 .with_custom_background(Color{16, 74, 44, 255})
                 .with_padding(Padding::all(pixels(14.f)))
                 .with_debug_name("shot_bar_sports"),
             ComponentConfig{}
                 .with_custom_background(Color{245, 196, 52, 255})
                 .with_custom_text_color(Color{24, 32, 18, 255})
                 .with_border(Color{255, 232, 140, 255}, pixels(1.f)),
             ComponentConfig{}
                 .with_size(ComponentSize{pixels(150.f), children()})
                 .with_custom_text_color(Color{235, 255, 238, 255}));

  prompt_bar(h.context(), mk(screen.ent(), 2),
             {{"L", "Prev"}, {"R", "Next"}},
             ComponentConfig{}
                 .with_size(ComponentSize{percent(1.f), children()})
                 .with_custom_background(Color{238, 241, 246, 255})
                 .with_padding(Padding::all(pixels(14.f)))
                 .with_debug_name("shot_bar_light"),
             ComponentConfig{}
                 .with_custom_background(Color{255, 255, 255, 255})
                 .with_custom_text_color(Color{28, 34, 46, 255})
                 .with_border(Color{178, 188, 202, 255}, pixels(1.f)),
             ComponentConfig{}
                 .with_size(ComponentSize{pixels(56.f), children()})
                 .with_custom_text_color(Color{28, 34, 46, 255}));

  h.layout_only();

  FontManager fonts;
  fonts.load_font(UIComponent::DEFAULT_FONT, get_default_font());
  fonts.load_font(UIComponent::UNSET_FONT, get_default_font());

  graphics::begin_frame();
  graphics::clear_background(Color{8, 11, 18, 255});
  RenderImm<ui_test::TestInputAction>{}.for_each_with_derived(
      h.root(), h.context(), fonts, 0.f);
  graphics::end_frame();

  auto png = capture_render_texture_png(graphics::get_render_texture());
  CHECK(png.has_value());
  if (!png)
    return;
  CHECK(png->bytes.size() > 100);
  const std::filesystem::path out = "/tmp/pr-prim-prompt-bar.png";
  std::ofstream file(out, std::ios::binary);
  file.write(reinterpret_cast<const char *>(png->bytes.data()),
             static_cast<std::streamsize>(png->bytes.size()));
  file.close();
  CHECK(std::filesystem::exists(out));
  CHECK(std::filesystem::file_size(out) > 100);
}

int main() {
  graphics::Config config;
  config.display = graphics::DisplayMode::Headless;
  config.width = 800;
  config.height = 600;
  if (!graphics::init(config))
    return 1;
  const int result = ui_test::run_registered_tests("prompt bar");
  graphics::shutdown();
  return result;
}
