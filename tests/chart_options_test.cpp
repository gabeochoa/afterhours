#include "ui_test_harness.h"
#include <afterhours/src/plugins/ui/line_chart.h>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

static bool same(const Color &c, int r, int g, int b) { return c.r == r && c.g == g && c.b == b; }

TEST(chart_options_defaults_and_colors) {
  LineChartOptions def;
  CHECK(def.stroke_width == 2.f);
  CHECK(def.show_hover);
  CHECK(!def.x_domain && !def.y_domain && !def.grid_color && !def.label_color);

  ImmTestHarness h;
  LineChartOptions opts;
  opts.y_domain = std::pair{0.0, 200.0};
  opts.grid_color = Color{1, 2, 3, 255};
  opts.label_color = Color{4, 5, 6, 255};
  ChartSeries s{"s", {{0, 0}, {1, 100}}, Color{10, 20, 30, 255}};
  line_chart(h.context(), mk(h.root(), 0), {s}, opts,
             ComponentConfig{}.with_size({pixels(480), pixels(220)}).with_debug_name("chart"));
  h.render();
  auto lines = h.drawn("line");
  int grid = 0, series_lines = 0;
  float series_end_y = -1, grid_top = 1e9f, grid_bottom = -1;
  for (auto &c : lines) {
    if (same(c.color, 1, 2, 3)) { grid++; grid_top = std::min(grid_top, c.rect.y); grid_bottom = std::max(grid_bottom, c.rect.y); }
    if (same(c.color, 10, 20, 30)) { series_lines++; series_end_y = c.rect.y + c.rect.height; }
  }
  CHECK(grid == 5);
  CHECK(series_lines == 1);
  // y=100 in explicit [0,200] sits mid-plot, not at the top gridline.
  if (grid_bottom > grid_top) CHECK(series_end_y > grid_top + (grid_bottom - grid_top) * 0.3f);
  bool label_colored = false;
  for (auto &c : h.drawn("text")) if (same(c.color, 4, 5, 6)) label_colored = true;
  CHECK(label_colored);
}

int main() { return ui_test::run_registered_tests("chart_options_test"); }
