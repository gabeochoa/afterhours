// post_build_test.cpp
// Actions queued during build run after layout, in order, with stale owners dropped.

#define FMT_HEADER_ONLY
#include <fmt/format.h>
#define AFTER_HOURS_IMM_UI
#include <afterhours/src/plugins/ui.h>
#include <afterhours/ah.h>

#include <cstdio>
#include <string>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

static int checks_run = 0, checks_passed = 0;
static void check(bool cond, const char *expr, int line) {
  checks_run++;
  if (cond)
    checks_passed++;
  else
    fprintf(stderr, "  FAIL: %s  (line %d)\n", expr, line);
}
#define CHECK(e) check((e), #e, __LINE__)

static std::string g_order;
static float g_width_seen = 0.f;
static int g_stale_runs = 0;

struct BuildUI : System<DefaultUIContext> {
  virtual void for_each_with(Entity &entity, DefaultUIContext &ctx,
                             float) override {
    auto [left, right] = hsplit(ctx, mk(entity), {pixels(200), expand(1.f)});
    auto b = button(ctx, mk(left.ent(), 0), "Open");
    const EntityID id = b.id();
    ctx.post_build(42, [id] {
      g_order += "a";
      g_width_seen = UICollectionHolder::getEntityForIDEnforce(id)
                         .get<UIComponent>()
                         .rect()
                         .width;
    });
    ctx.post_build(99, [] { g_stale_runs++; });
    ctx.discard_post_build(99);
    ctx.post_build([] { g_order += "b"; });
    (void)right;
  }
};

int main() {
  printf("=== post_build ===\n\n");

  { // No phase runs it: queueing alone never executes.
    UIContext<DefaultAction> local;
    int ran = 0;
    local.post_build([&] { ran++; });
    CHECK(ran == 0);
    CHECK(local.pending_post_build_count() == 1);
  }

  SystemManager systems;
  ui::setup_with_resolution<>(systems, {800, 600},
                              std::make_unique<BuildUI>());
  systems.run(1.f);

  CHECK(g_order == "ab");
  CHECK(g_width_seen > 0.f); // laid out before the action ran
  CHECK(g_stale_runs == 0);

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
