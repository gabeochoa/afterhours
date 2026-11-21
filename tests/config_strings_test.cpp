// ComponentConfig strings were copied whole-config on the per-widget merge
// path: apply_overrides copied every string even for empty overrides, and
// merge_with_defaults copied the config again on the way in and out.
//
// Counted rather than timed (mk_cost style): long (>SSO) strings, warm the
// steady state, then N unchanged repeats of the targeted path must allocate 0.

#define AFTER_HOURS_USE_NONE 1
#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include <afterhours/ah.h>
#include <afterhours/src/plugins/ui/component_init.h>
#include <afterhours/src/plugins/ui/imm_components.h>

#include <cstdio>
#include <cstdlib>
#include <string>

using namespace afterhours;

static std::size_t g_allocs = 0;
static bool g_counting = false;
void *operator new(std::size_t n) {
  if (g_counting) g_allocs++;
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

int main() {
  using namespace ui::imm;
  const std::string L = "a-long-label-that-is-well-over-sso-limit-1234567890";
  const std::string F = "a-long-font-name-over-sso-limit-123456";

  ComponentConfig base;
  base.with_label(L)
      .with_debug_name("a-long-debug-name-over-sso-limit-123456")
      .with_font(F, ui::pixels(16.f))
      .with_tooltip("a-long-tooltip-text-over-sso-limit-1234567")
      .with_placeholder("a-long-placeholder-over-sso-limit-12345");

  // Behaviour: value-form merge still produces the same merged config.
  {
    ComponentConfig over;
    over.with_label("replacement-long-label-over-sso-limit-123456789")
        .with_tooltip("replacement-long-tooltip-over-sso-limit-1234");
    const ComponentConfig merged = base.apply_overrides(over);
    CHECK(merged.label == over.label);
    CHECK(merged.tooltip_text == over.tooltip_text);
    CHECK(merged.debug_name == base.debug_name);
    CHECK(merged.font_name == F);
    CHECK(merged.placeholder == base.placeholder);
    const ComponentConfig inh = ComponentConfig::inherit_from(base, "row");
    CHECK(inh.font_name == F);
    CHECK(inh.debug_name == "row");
  }

  auto &sd = UIStylingDefaults::get();
  sd.set_default_font("a-long-default-font-name-over-sso-12345",
                      ui::pixels(14.f));

  constexpr int N = 100;

  // Targeted hot path: the merge overwrite_defaults runs for every widget.
  // Warm it (first pass applies the default font), then unchanged repeats.
  {
    ComponentConfig cfg = base;
    sd.merge_with_defaults_into(ComponentType::Div, cfg);
    CHECK(cfg.label == L);
    g_allocs = 0;
    g_counting = true;
    for (int i = 0; i < N; i++)
      sd.merge_with_defaults_into(ComponentType::Div, cfg);
    g_counting = false;
    std::printf("  merge_with_defaults_into steady: %.2f alloc/call\n",
                double(g_allocs) / N);
    CHECK(g_allocs == 0);
    CHECK(cfg.font_name == F);
  }

  // Re-applying the same overrides in place (restyle on an unchanged frame).
  {
    ComponentConfig cfg;
    ComponentConfig over;
    over.with_label(L).with_debug_name(base.debug_name);
    cfg.apply_overrides_from(over); // warm
    g_allocs = 0;
    g_counting = true;
    for (int i = 0; i < N; i++) cfg.apply_overrides_from(over);
    g_counting = false;
    std::printf("  apply_overrides_from steady: %.2f alloc/call\n",
                double(g_allocs) / N);
    CHECK(g_allocs == 0);
    CHECK(cfg.label == L);
  }

  // inherit_from with real-world (short, SSO) debug/font names: no heap.
  {
    ComponentConfig parent;
    parent.with_font("default", ui::pixels(16.f));
    g_allocs = 0;
    g_counting = true;
    for (int i = 0; i < N; i++) {
      auto inh = ComponentConfig::inherit_from(parent, "tree_row");
      if (inh.debug_name != "tree_row") return 2;
    }
    g_counting = false;
    std::printf("  inherit_from (SSO names) steady: %.2f alloc/call\n",
                double(g_allocs) / N);
    CHECK(g_allocs == 0);
  }

  std::printf("%d/%d checks passed\n", checks - failures, checks);
  if (failures == 0) std::printf("All checks passed!\n");
  return failures == 0 ? 0 : 1;
}
