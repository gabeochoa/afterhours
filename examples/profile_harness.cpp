// profile_harness.cpp
// Long-running harness for sampling profilers (sample, Instruments, perf).
//
// Runs the specified benchmark in a tight loop for a configurable duration,
// giving the profiler enough samples to identify hot functions.
//
// Build:
//   make profile_harness
//
// Run:
//   ./profile_harness flat-1000        # flat layout, 1000 children
//   ./profile_harness deep-200         # deep nested layout, depth 200
//   ./profile_harness realistic-50     # 50 sections x 10 items
//   ./profile_harness entity-5000      # entity creation, 5000 entities
//   ./profile_harness calendar         # calendar with 30 meetings/day
//   ./profile_harness all              # run all benchmarks sequentially
//
// Profile with macOS sample:
//   ./profile_harness flat-1000 &
//   sample $! -f flat1000.txt
//   wait

#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include <afterhours/src/ecs.h>
#include <afterhours/src/plugins/autolayout.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <unistd.h>

using namespace afterhours;
using namespace afterhours::ui;

static constexpr double RUN_SECONDS = 5.0;

static double now_s() {
  return std::chrono::duration<double>(
             std::chrono::high_resolution_clock::now().time_since_epoch())
      .count();
}

struct LayoutHarness {
  std::vector<std::unique_ptr<Entity>> entities;
  std::vector<Entity *> mapping;

  Entity &make_entity() {
    entities.push_back(std::make_unique<Entity>());
    Entity &e = *entities.back();
    while (mapping.size() <= static_cast<size_t>(e.id))
      mapping.push_back(nullptr);
    mapping[e.id] = &e;
    return e;
  }

  Entity &make_ui(Size w, Size h, EntityID parent_id = -1) {
    Entity &e = make_entity();
    auto &ui = e.addComponent<UIComponent>(e.id);
    ui.set_desired_width(w);
    ui.set_desired_height(h);
    if (parent_id >= 0) {
      ui.set_parent(parent_id);
      mapping[parent_id]->get<UIComponent>().add_child(e.id);
    }
    return e;
  }

  void layout(Entity &root) {
    AutoLayout::autolayout(root.get<UIComponent>(), {1280, 720}, mapping, false);
  }
};

// Flat layout: 1 root + N children
static void run_flat(int n) {
  printf("Running: flat layout with %d children for %.0fs...\n", n, RUN_SECONDS);
  double start = now_s();
  long iters = 0;

  while (now_s() - start < RUN_SECONDS) {
    ENTITY_ID_GEN.store(0);
    LayoutHarness h;
    auto &root = h.make_ui(pixels(1280), pixels(720));
    root.get<UIComponent>().set_flex_direction(FlexDirection::Column);
    for (int i = 0; i < n; ++i)
      h.make_ui(percent(1.f), pixels(20), root.id);
    h.layout(root);
    iters++;
  }

  double elapsed = now_s() - start;
  printf("  %ld iterations in %.2fs (%.3f ms/iter)\n",
         iters, elapsed, (elapsed * 1000.0) / iters);
}

// Deep nested layout
static void run_deep(int depth) {
  printf("Running: deep layout with depth=%d for %.0fs...\n", depth, RUN_SECONDS);
  double start = now_s();
  long iters = 0;

  while (now_s() - start < RUN_SECONDS) {
    ENTITY_ID_GEN.store(0);
    LayoutHarness h;
    auto &root = h.make_ui(pixels(1280), pixels(720));
    Entity *parent = &root;
    for (int i = 0; i < depth; ++i) {
      auto &child = h.make_ui(percent(0.95f), percent(0.95f), parent->id);
      parent = &child;
    }
    h.layout(root);
    iters++;
  }

  double elapsed = now_s() - start;
  printf("  %ld iterations in %.2fs (%.3f ms/iter)\n",
         iters, elapsed, (elapsed * 1000.0) / iters);
}

// Realistic: sections x items
static void run_realistic(int sections) {
  int items_per = 10;
  int total = sections * items_per + sections + 1;
  printf("Running: realistic %d sections x %d items = %d widgets for %.0fs...\n",
         sections, items_per, total, RUN_SECONDS);
  double start = now_s();
  long iters = 0;

  while (now_s() - start < RUN_SECONDS) {
    ENTITY_ID_GEN.store(0);
    LayoutHarness h;
    auto &root = h.make_ui(pixels(1280), pixels(720));
    root.get<UIComponent>().set_flex_direction(FlexDirection::Column);
    for (int s = 0; s < sections; ++s) {
      auto &section = h.make_ui(percent(1.f), children(), root.id);
      section.get<UIComponent>().set_flex_direction(FlexDirection::Column);
      for (int i = 0; i < items_per; ++i)
        h.make_ui(percent(1.f), pixels(30), section.id);
    }
    h.layout(root);
    iters++;
  }

  double elapsed = now_s() - start;
  printf("  %ld iterations in %.2fs (%.3f ms/iter)\n",
         iters, elapsed, (elapsed * 1000.0) / iters);
}

struct PosComp : BaseComponent {
  float x = 0, y = 0;
  PosComp() = default;
  PosComp(float x_, float y_) : x(x_), y(y_) {}
};

// Entity creation only
static void run_entity(int count) {
  printf("Running: entity creation (%d entities) for %.0fs...\n", count, RUN_SECONDS);
  double start = now_s();
  long iters = 0;

  while (now_s() - start < RUN_SECONDS) {
    EntityCollection collection;
    EntityHelper::set_default_collection(&collection);
    for (int i = 0; i < count; ++i) {
      auto &e = EntityHelper::createEntity();
      e.addComponent<PosComp>(static_cast<float>(i), static_cast<float>(i));
      (void)e;
    }
    collection.merge_entity_arrays();
    EntityHelper::set_default_collection(nullptr);
    iters++;
  }

  double elapsed = now_s() - start;
  printf("  %ld iterations in %.2fs (%.3f ms/iter)\n",
         iters, elapsed, (elapsed * 1000.0) / iters);
}

// Calendar-like layout (entity + layout, no render)
static void run_calendar() {
  int num_days = 7;
  int meetings_per_day = 30;
  int visible_hours = 14;

  printf("Running: calendar %d days x %d meetings for %.0fs...\n",
         num_days, meetings_per_day, RUN_SECONDS);
  double start = now_s();
  long iters = 0;

  while (now_s() - start < RUN_SECONDS) {
    ENTITY_ID_GEN.store(0);
    LayoutHarness h;
    auto &root = h.make_ui(pixels(1280), pixels(720));
    root.get<UIComponent>().set_flex_direction(FlexDirection::Column);

    auto &header = h.make_ui(percent(1.f), pixels(50), root.id);
    header.get<UIComponent>().set_flex_direction(FlexDirection::Row);
    h.make_ui(pixels(60), percent(1.f), header.id);
    for (int d = 0; d < num_days; ++d)
      h.make_ui(expand(1), percent(1.f), header.id);

    auto &body = h.make_ui(percent(1.f), expand(1), root.id);
    body.get<UIComponent>().set_flex_direction(FlexDirection::Row);

    auto &gutter = h.make_ui(pixels(60), percent(1.f), body.id);
    gutter.get<UIComponent>().set_flex_direction(FlexDirection::Column);
    for (int hr = 0; hr < visible_hours; ++hr)
      h.make_ui(percent(1.f), expand(1), gutter.id);

    for (int d = 0; d < num_days; ++d) {
      auto &day_col = h.make_ui(expand(1), percent(1.f), body.id);
      for (int m = 0; m < meetings_per_day; ++m) {
        float top_pct = static_cast<float>(m) / static_cast<float>(meetings_per_day);
        float height_pct = 1.f / static_cast<float>(visible_hours);
        auto &block = h.make_ui(percent(0.95f), percent(height_pct), day_col.id);
        block.get<UIComponent>().absolute = true;
        block.get<UIComponent>().set_desired_margin(percent(top_pct), Axis::top);
      }
    }

    h.layout(root);
    iters++;
  }

  double elapsed = now_s() - start;
  printf("  %ld iterations in %.2fs (%.3f ms/iter)\n",
         iters, elapsed, (elapsed * 1000.0) / iters);
}

int main(int argc, char *argv[]) {
  const char *mode = argc > 1 ? argv[1] : "all";

  printf("afterhours profile harness (%.0fs per benchmark)\n", RUN_SECONDS);
  printf("PID: %d\n", getpid());
  printf("=============================================\n");
  printf("  Attach profiler now, e.g.:\n");
  printf("    sample %d -f profile.txt\n\n", getpid());

  bool all = strcmp(mode, "all") == 0;

  if (all || strcmp(mode, "flat-1000") == 0)  run_flat(1000);
  if (all || strcmp(mode, "flat-2000") == 0)  run_flat(2000);
  if (all || strcmp(mode, "deep-200") == 0)   run_deep(200);
  if (all || strcmp(mode, "realistic-50") == 0) run_realistic(50);
  if (all || strcmp(mode, "entity-5000") == 0) run_entity(5000);
  if (all || strcmp(mode, "calendar") == 0)    run_calendar();

  printf("\nDone.\n");
  return 0;
}
