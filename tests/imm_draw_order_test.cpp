// imm_draw_order_test.cpp
// World draws from an app render system registered before the UI pass land
// under imm, and app draws registered with register_render_systems_after_ui
// land over it, in one capture buffer (#189 kept pre-UI draws alive; this
// pins the paint order itself).

#ifndef AFTER_HOURS_USE_NONE
#define AFTER_HOURS_USE_NONE 1
#endif

#define FMT_HEADER_ONLY
#include <fmt/format.h>

#define AFTER_HOURS_IMM_UI
#include <afterhours/src/plugins/ui.h>

#include <afterhours/ah.h>
#include <afterhours/src/plugins/autolayout.h>

#include <cstdio>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

static int checks_run = 0, checks_passed = 0;
static void check(bool cond, const char *expr, int line) {
  checks_run++;
  if (cond) checks_passed++;
  else fprintf(stderr, "  FAIL: %s  (line %d)\n", expr, line);
}
#define CHECK(e) check((e), #e, __LINE__)

struct BuildUI : System<DefaultUIContext> {
  void for_each_with(Entity &entity, DefaultUIContext &ctx, float) override {
    div(ctx, mk(entity), ComponentConfig{}
            .with_size({pixels(100), pixels(40)})
            .with_label("World UI")
            .with_custom_background(Color{10, 200, 10, 255}));
  }
};

struct DrawRect : System<> {
  Color color;
  explicit DrawRect(Color c) : color(c) {}
  bool should_iterate() const override { return false; }
  void once(float) override {
    draw_rectangle(RectangleType{0, 0, 4, 4}, color);
  }
};

static int first_with_color(unsigned char r) {
  const auto &calls = capture::calls();
  for (size_t i = 0; i < calls.size(); i++)
    if (calls[i].color.r == r) return (int) i;
  return -1;
}

int main() {
  printf("=== imm draw order ===\n\n");
  SystemManager systems;
  ui::setup_with_resolution<>(systems, {800, 600}, std::make_unique<BuildUI>());
  // The debug-overlay render system reads the collector; ui::run creates it,
  // setup() leaves it to the app.
  auto &in = EntityHelper::createEntity();
  in.addComponent<input::InputCollector>();
  EntityHelper::registerSingleton<input::InputCollector>(in);

  // World first, then the whole imm pass, then the app's over-UI layer.
  systems.register_render_system(std::make_unique<DrawRect>(Color{200, 0, 0, 255}));
  ui::register_render_systems_after_ui<DefaultAction>(
      systems, DefaultAction::None,
      std::make_unique<DrawRect>(Color{0, 0, 200, 255}));

  capture::enable();
  capture::begin_frame();
  systems.run(1.f / 60.f);

  const int world = first_with_color(200);
  const int imm = first_with_color(10);
  int over = -1;
  for (size_t i = 0; i < capture::calls().size(); i++)
    if (capture::calls()[i].color.b == 200) { over = (int) i; break; }
  CHECK(world >= 0);
  CHECK(imm > world);
  CHECK(over > imm);

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) { printf("FAILURES\n"); return 1; }
  printf("All checks passed!\n");
  return 0;
}
