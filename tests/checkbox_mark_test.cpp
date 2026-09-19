#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

static void check_marks(bool batched) {
  ui_test::ImmTestHarness h;
  h.render_font()->load_font("custom", get_default_font());
  for (int phase = 0; phase < 5; ++phase) {
    h.begin_frame();
    std::array<EntityID, 3> ids{};
    for (int i = 0; i < 3; ++i) {
      const float size = 44.f * static_cast<float>(i + 1);
      auto parent = div(h.context(), mk(h.root(), i), ComponentConfig{}
          .with_size({pixels(160), pixels(160)}).with_absolute_position(i * 200.f, 0)
          .with_opacity(.5f));
      auto style = ComponentConfig{}.with_size({pixels(size), pixels(size)})
          .with_font("custom", pixels(12 + i * 30.f))
          .with_custom_text_color({200, 160, 120, 255}).with_text_inset(0)
          .with_disabled(i == 2).with_opacity(.5f);
      if (phase == 2) style.with_checkbox_indicators("yes", "no");
      if (phase == 3) style.with_checkbox_indicators("", "");
      auto pair = mk(parent.ent(), 0);
      auto &row = deref(pair).first;
      bool value = phase != 1;
      if (row.has<HasCheckboxState>()) row.get<HasCheckboxState>().on = value;
      checkbox(h.context(), pair, value, style);
      auto &indicator = UICollectionHolder::getEntityForID(row.get<UIComponent>().children.back()).asE();
      ids[static_cast<size_t>(i)] = indicator.id;
      CHECK(indicator.has<HasCheckboxMark>() == (phase == 0 || phase == 4));
      CHECK(indicator.get<HasLabel>().label == (phase == 2 ? "yes" : ""));
      CHECK(indicator.has<HasClickListener>() == (i != 2));
    }
    if (batched) h.render_batched(); else h.render();
    auto lines = h.drawn("line");
    if (phase != 0 && phase != 4) {
      CHECK(lines.empty());
      continue;
    }
    CHECK(lines.size() == 6);
    for (size_t i = 0; i < ids.size(); ++i) {
      std::vector<DrawCall> strokes;
      for (const auto &line : lines) if (line.entity_id == ids[i]) strokes.push_back(line);
      CHECK(strokes.size() == 2);
      if (strokes.size() != 2) continue;
      const auto rect = UICollectionHolder::getEntityForID(ids[i]).asE().get<UIComponent>().rect();
      CHECK(std::abs(strokes[0].rect.x - (rect.x + rect.width * .35f)) < .01f);
      CHECK(std::abs(strokes[0].rect.y - (rect.y + rect.height * .5f)) < .01f);
      CHECK(strokes[0].rect.height > 0);
      CHECK(strokes[1].rect.height < 0);
      CHECK(strokes[0].color.r == (i == 2 ? 100 : 200));
      CHECK(strokes[0].color.a == 63);
    }
  }
}

TEST(immediate_native_marks) { check_marks(false); }
TEST(batched_native_marks) { check_marks(true); }

TEST(click_updates_mark_and_custom_text_in_same_frame) {
  ui_test::ImmTestHarness h;
  bool value = false;
  Entity *previous = nullptr;
  for (int phase = 0; phase < 4; ++phase) {
    h.begin_frame();
    auto pair = mk(h.root(), 0);
    auto &row = deref(pair).first;
    if (phase != 0) {
      previous->get<HasClickListener>().down = true;
    }
    auto style = ComponentConfig{}.with_size({pixels(44), pixels(44)});
    if (phase >= 2) style.with_checkbox_indicators("yes", "no");
    checkbox(h.context(), pair, value, style);
    auto &indicator = UICollectionHolder::getEntityForID(row.get<UIComponent>().children.back()).asE();
    previous = &indicator;
    CHECK(value == (phase % 2 == 1));
    CHECK(indicator.has<HasCheckboxMark>() == (phase == 1));
    CHECK(indicator.get<HasLabel>().label == (phase == 2 ? "no" : phase == 3 ? "yes" : ""));
    h.layout_only();
  }
}

int main() { return ui_test::run_registered_tests("native checkbox marks"); }
