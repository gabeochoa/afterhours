// overdraw_audit_test.cpp
// progress_bar drew a full-width fill over a track it covered exactly: two
// identical boxes for every completed bar, and nothing flagged it because the
// output was right. This asks the same question of every composite.
//
// Occluded means: an opaque draw whose rect fully contains an earlier draw's
// rect, on the same or a higher layer. The earlier one cannot be seen.
//
// It reports rather than asserting a number, because "some overdraw" is not by
// itself a defect: a border under a fill is deliberate. The value is the list.

#include "ui_test_harness.h"

#include <array>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

namespace {

bool contains(const RectangleType &outer, const RectangleType &inner) {
  return outer.x <= inner.x + 0.5f && outer.y <= inner.y + 0.5f &&
         outer.x + outer.width >= inner.x + inner.width - 0.5f &&
         outer.y + outer.height >= inner.y + inner.height - 0.5f;
}

// Outlines don't hide anything, they only paint their border.
bool fills(const ui_test::DrawCall &c) {
  return c.color.a >= 255 && !c.op.ends_with("_lines");
}

// Text rects are always zero-width here: the capture backend has no font.
bool has_real_rect(const ui_test::DrawCall &c) { return c.op != "text"; }

bool same_rect(const RectangleType &a, const RectangleType &b) {
  auto near = [](float x, float y) { return std::abs(x - y) < 0.5f; };
  return near(a.x, b.x) && near(a.y, b.y) && near(a.width, b.width) &&
         near(a.height, b.height);
}

int report_occluded(const char *what,
                    const std::vector<ui_test::DrawCall> &calls) {
  int hidden = 0;
  for (size_t i = 0; i < calls.size(); i++) {
    if (!has_real_rect(calls[i]))
      continue;
    // Not occluded, just wasted: the call goes out and paints nothing.
    if (calls[i].rect.width <= 0.f || calls[i].rect.height <= 0.f) {
      printf("  %-22s draw %zu (%s) has zero area\n", what, i,
             calls[i].op.c_str());
      hidden++;
      continue;
    }
    for (size_t j = i + 1; j < calls.size(); j++) {
      if (!fills(calls[j]))
        continue;
      if (calls[j].layer < calls[i].layer)
        continue;
      if (!contains(calls[j].rect, calls[i].rect))
        continue;
      hidden++;
      printf("  %-22s draw %zu (%s %.0fx%.0f) hidden by %zu (%s%s)\n", what, i,
             calls[i].op.c_str(), calls[i].rect.width, calls[i].rect.height, j,
             calls[j].op.c_str(),
             same_rect(calls[i].rect, calls[j].rect) ? ", identical rect" : "");
      break;
    }
  }
  if (hidden == 0)
    printf("  %-22s clean\n", what);
  return hidden;
}

} // namespace

TEST(audit_composites_for_hidden_draws) {
  struct Case {
    const char *name;
    void (*build)(ImmTestHarness &);
  };

  static const Case cases[] = {
      {"progress_bar_full",
       [](ImmTestHarness &h) {
         progress_bar(h.context(), mk(h.root(), 0), 1.0f,
                      ComponentConfig{}.with_size(
                          ComponentSize{pixels(200), pixels(24)}));
       }},
      {"progress_bar_half",
       [](ImmTestHarness &h) {
         progress_bar(h.context(), mk(h.root(), 0), 0.5f,
                      ComponentConfig{}.with_size(
                          ComponentSize{pixels(200), pixels(24)}));
       }},
      // The other end of the case that started this: a fill of nothing.
      {"progress_bar_empty",
       [](ImmTestHarness &h) {
         progress_bar(h.context(), mk(h.root(), 0), 0.0f,
                      ComponentConfig{}.with_size(
                          ComponentSize{pixels(200), pixels(24)}));
       }},
      {"circular_progress_full",
       [](ImmTestHarness &h) {
         circular_progress(h.context(), mk(h.root(), 0), 1.0f,
                           ComponentConfig{}.with_size(
                               ComponentSize{pixels(64), pixels(64)}));
       }},
      {"circular_progress_empty",
       [](ImmTestHarness &h) {
         circular_progress(h.context(), mk(h.root(), 0), 0.0f,
                           ComponentConfig{}.with_size(
                               ComponentSize{pixels(64), pixels(64)}));
       }},
      {"toggle_switch_on",
       [](ImmTestHarness &h) {
         static bool on = true;
         toggle_switch(h.context(), mk(h.root(), 0), on,
                       ComponentConfig{}
                           .with_size(ComponentSize{pixels(200), pixels(32)})
                           .with_label("sound"));
       }},
      {"toggle_switch_off",
       [](ImmTestHarness &h) {
         static bool off = false;
         toggle_switch(h.context(), mk(h.root(), 0), off,
                       ComponentConfig{}
                           .with_size(ComponentSize{pixels(200), pixels(32)})
                           .with_label("sound"));
       }},
      {"checkbox_on",
       [](ImmTestHarness &h) {
         static bool on = true;
         checkbox(h.context(), mk(h.root(), 0), on,
                  ComponentConfig{}
                      .with_size(ComponentSize{pixels(200), pixels(32)})
                      .with_label("enabled"));
       }},
      {"checkbox_off",
       [](ImmTestHarness &h) {
         static bool off = false;
         checkbox(h.context(), mk(h.root(), 0), off,
                  ComponentConfig{}
                      .with_size(ComponentSize{pixels(200), pixels(32)})
                      .with_label("enabled"));
       }},
      {"slider_mid",
       [](ImmTestHarness &h) {
         static float v = 0.5f;
         slider(h.context(), mk(h.root(), 0), v,
                ComponentConfig{}
                    .with_size(ComponentSize{pixels(300), pixels(32)})
                    .with_label("volume"));
       }},
      {"slider_zero",
       [](ImmTestHarness &h) {
         static float v = 0.f;
         slider(h.context(), mk(h.root(), 0), v,
                ComponentConfig{}
                    .with_size(ComponentSize{pixels(300), pixels(32)})
                    .with_label("volume"));
       }},
      {"slider_full",
       [](ImmTestHarness &h) {
         static float v = 1.f;
         slider(h.context(), mk(h.root(), 0), v,
                ComponentConfig{}
                    .with_size(ComponentSize{pixels(300), pixels(32)})
                    .with_label("volume"));
       }},
      {"button",
       [](ImmTestHarness &h) {
         button(h.context(), mk(h.root(), 0),
                ComponentConfig{}
                    .with_size(ComponentSize{pixels(160), pixels(40)})
                    .with_label("ok"));
       }},
      {"toggle_button_on",
       [](ImmTestHarness &h) {
         static bool on = true;
         primitive::toggle_button(h.context(), mk(h.root(), 0),
                       ComponentConfig{}
                           .with_size(ComponentSize{pixels(160), pixels(40)})
                           .with_label("bold"),
                       on);
       }},
      {"divider_x",
       [](ImmTestHarness &h) {
         divider(h.context(), mk(h.root(), 0), Axis::X,
                 ComponentConfig{}.with_size(
                     ComponentSize{pixels(200), pixels(4)}));
       }},
      {"button_group",
       [](ImmTestHarness &h) {
         static const std::array<std::string_view, 3> labels{"a", "b", "c"};
         button_group(h.context(), mk(h.root(), 0), labels,
                      ComponentConfig{}.with_size(
                          ComponentSize{pixels(300), pixels(40)}));
       }},
      {"radio_group",
       [](ImmTestHarness &h) {
         static const std::array<std::string_view, 3> labels{"a", "b", "c"};
         static size_t sel = 1;
         radio_group(h.context(), mk(h.root(), 0), labels, sel,
                     ComponentConfig{}.with_size(
                         ComponentSize{pixels(300), pixels(120)}));
       }},
      {"dropdown_closed",
       [](ImmTestHarness &h) {
         static const std::array<std::string_view, 3> opts{"a", "b", "c"};
         static size_t idx = 0;
         dropdown(h.context(), mk(h.root(), 0), opts, idx,
                  ComponentConfig{}.with_size(
                      ComponentSize{pixels(200), pixels(32)}));
       }},
      {"pagination",
       [](ImmTestHarness &h) {
         static const std::array<std::string_view, 3> opts{"1", "2", "3"};
         static size_t idx = 1;
         pagination(h.context(), mk(h.root(), 0), opts, idx,
                    ComponentConfig{}.with_size(
                        ComponentSize{pixels(300), pixels(40)}));
       }},
      {"navigation_bar",
       [](ImmTestHarness &h) {
         static const std::array<std::string_view, 3> opts{"a", "b", "c"};
         static size_t idx = 0;
         navigation_bar(h.context(), mk(h.root(), 0), opts, idx,
                        ComponentConfig{}.with_size(
                            ComponentSize{pixels(400), pixels(48)}));
       }},
      {"tab_container",
       [](ImmTestHarness &h) {
         static const std::array<std::string_view, 3> tabs{"a", "b", "c"};
         static size_t active = 0;
         tab_container(h.context(), mk(h.root(), 0), tabs, active,
                       ComponentConfig{}.with_size(
                           ComponentSize{pixels(400), pixels(200)}));
       }},
      {"tray",
       [](ImmTestHarness &h) {
         tray(h.context(), mk(h.root(), 0),
              ComponentConfig{}.with_size(
                  ComponentSize{pixels(300), pixels(60)}));
       }},
  };

  for (const auto &c : cases) {
    ImmTestHarness h;
    c.build(h);
    report_occluded(c.name, h.render());
  }

  // Nothing to assert: the report is the point. A composite that draws a box
  // nobody can see is worth a look, not an automatic failure.
  CHECK(true);
}

int main() { return ui_test::run_registered_tests("overdraw audit"); }
