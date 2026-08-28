// imm::mk() resolves a call site to a persistent entity, and it is called once
// per widget per frame, so whatever it costs the whole UI pays every frame.
//
// Counted rather than timed: allocations are deterministic and say what is
// wrong, where a millisecond says only that something is.

#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include <afterhours/ah.h>
#include <afterhours/src/plugins/autolayout.h>
#include <afterhours/src/plugins/e2e_testing/ui_commands.h>
#include <afterhours/src/plugins/ui/component_init.h>
#include <afterhours/src/plugins/ui/imm_components.h>

#include <cstdio>
#include <cstdlib>

using namespace afterhours;

// UIContext needs these names to exist.
enum struct TestInputAction {
  None, WidgetMod, WidgetNext, WidgetBack, WidgetPress,
  WidgetUp, WidgetDown, WidgetLeft, WidgetRight,
};

static std::size_t g_allocs = 0;
static std::size_t g_bytes = 0;
static bool g_counting = false;
void *operator new(std::size_t n) {
  if (g_counting) {
    g_allocs++;
    g_bytes += n;
  }
  return std::malloc(n);
}
void operator delete(void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }

static int checks = 0;
static int failures = 0;
static void check(bool cond, const char *expr, int line) {
  checks++;
  if (!cond) {
    failures++;
    std::fprintf(stderr, "  FAIL: %s  (line %d)\n", expr, line);
  }
}
#define CHECK(expr) check((expr), #expr, __LINE__)

// Both the warm-up and the measured pass must come from ONE call site, or the
// second pass hashes differently and mints fresh entities: that measures
// creation, not the steady state a real frame is in.
static void build_widgets(Entity &parent, int n) {
  for (int i = 0; i < n; i++) ui::imm::mk(parent, i);
}

int main() {
  auto &coll = ui::UICollectionHolder::get().collection;
  Entity &parent = coll.createEntity();
  coll.merge_entity_arrays();

  // Warm the map so the first call's insert is not counted as steady state.
  build_widgets(parent, 64);

  // Steady state: every one of these is a cache hit, which is what a real
  // frame is doing for a widget that already exists.
  constexpr int N = 64;
  g_allocs = 0;
  g_bytes = 0;
  g_counting = true;
  build_widgets(parent, N);
  g_counting = false;

  // Where does what is left go? Measure the one call the cache-hit path makes.
  {
    ui::imm::mk(parent, 9001); // ensure it exists
    const EntityID id = ui::UICollectionHolder::get().collection.get_entities().back()->id;
    std::size_t a0 = g_allocs;
    g_counting = true;
    for (int i = 0; i < N; i++) {
      auto &e = ui::UICollectionHolder::getEntityForIDEnforce(id);
      (void)e;
    }
    g_counting = false;
    std::printf("  getEntityForIDEnforce: %.2f allocations per call\n",
                double(g_allocs - a0) / N);
    g_allocs = a0;
  }

  const double per_call = double(g_allocs) / N;
  const double bytes_per_call = double(g_bytes) / N;
  std::printf("  mk() steady state: %.2f allocations, %.1f bytes per call\n",
              per_call, bytes_per_call);
  std::printf("  a 500 widget frame therefore costs %.0f allocations\n",
              per_call * 500);

  // Resolving a call site to an entity it already has is a lookup. It needs no
  // heap at all, and a frame that pays one malloc per widget for it is paying
  // for the spelling rather than the work.
  CHECK(per_call < 1.0);

  std::printf("%d/%d checks passed\n", checks - failures, checks);
  if (failures == 0) std::printf("All checks passed!\n");
  return failures == 0 ? 0 : 1;
}
