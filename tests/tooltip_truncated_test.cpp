
#include "ui_test_harness.h"

#include <afterhours/src/plugins/ui/tooltip.h>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

namespace {
TooltipState &fresh_state() {
  if (!EntityHelper::has_singleton<TooltipState>()) {
    auto &e = EntityHelper::createPermanentEntity();
    e.addComponent<TooltipState>();
    EntityHelper::registerSingleton<TooltipState>(e);
  }
  auto &state = *EntityHelper::get_singleton_cmp<TooltipState>();
  state.hovered = -1;
  state.showing = -1;
  state.elapsed = 0.f;
  state.text.clear();
  return state;
}

void hover_and_update(ImmTestHarness &h, Entity &ent) {
  h.context().hot_id = ent.id;
  UpdateTooltips<ui_test::TestInputAction> update;
  update.for_each_with(h.context_entity(), h.context(), 0.f);
  update.for_each_with(h.context_entity(), h.context(), 1.f);
}
}

int main() { return ui_test::run_registered_tests("tooltip truncated"); }

TEST(full_text_visible_no_tooltip) {
  ImmTestHarness h;
  auto &state = fresh_state();
  auto widget = div(h.context(), mk(h.root(), 0),
                    ComponentConfig{}
                        .with_size({pixels(200), pixels(40)})
                        .with_label("hello")
                        .with_tooltip("hello", 0.1f));
  h.render();
  CHECK(!widget.ent().get<HasLabel>().truncated);
  hover_and_update(h, widget.ent());
  CHECK(!state.is_showing());
}

TEST(ellipsized_label_tooltip_shows) {
  ImmTestHarness h;
  auto &state = fresh_state();
  const std::string long_label =
      "a really quite long label that cannot possibly fit in 120 pixels";
  auto widget = div(h.context(), mk(h.root(), 0),
                    ComponentConfig{}
                        .with_size({pixels(120), pixels(20)})
                        .with_label(long_label)
                        .with_text_overflow(TextOverflow::Ellipsis)
                        .with_tooltip(long_label, 0.1f));
  h.render();
  CHECK(widget.ent().get<HasLabel>().truncated);
  hover_and_update(h, widget.ent());
  CHECK(state.is_showing());
  CHECK(state.text == long_label);
}

TEST(explicit_tooltip_text_shows_when_not_truncated) {
  ImmTestHarness h;
  auto &state = fresh_state();
  auto widget = div(h.context(), mk(h.root(), 0),
                    ComponentConfig{}
                        .with_size({pixels(200), pixels(40)})
                        .with_label("hello")
                        .with_tooltip("something else entirely", 0.1f));
  h.render();
  CHECK(!widget.ent().get<HasLabel>().truncated);
  hover_and_update(h, widget.ent());
  CHECK(state.is_showing());
}

TEST(renderer_records_truncation_in_batched_path) {
  ImmTestHarness h;
  const std::string long_label =
      "a really quite long label that cannot possibly fit in 120 pixels";
  auto narrow = div(h.context(), mk(h.root(), 0),
                    ComponentConfig{}
                        .with_size({pixels(120), pixels(20)})
                        .with_label(long_label)
                        .with_text_overflow(TextOverflow::Ellipsis));
  auto wide = div(h.context(), mk(h.root(), 0),
                  ComponentConfig{}
                      .with_size({pixels(800), pixels(20)})
                      .with_label(long_label)
                      .with_text_overflow(TextOverflow::Ellipsis));
  h.render_batched();
  CHECK(narrow.ent().get<HasLabel>().truncated);
  CHECK(!wide.ent().get<HasLabel>().truncated);
}
