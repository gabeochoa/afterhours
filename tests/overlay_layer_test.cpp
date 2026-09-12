#include "ui_test_harness.h"

#include <cstdio>
#include <string>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

static int checks_run = 0;
static int checks_passed = 0;

static void check(bool cond, const std::string &what) {
  checks_run++;
  if (cond) {
    checks_passed++;
  } else {
    fprintf(stderr, "  FAIL: %s\n", what.c_str());
  }
}

int main() {
  printf("=== overlay render layer ===\n\n");

  // An absolute element with no overlay stays on its parent's layer. This is
  // the case that broke when absolute defaulted to layer 1: file_tree's
  // full-screen underlay jumped above the content and hid it.
  {
    ImmTestHarness h;
    auto root = div(h.context(), mk(h.root(), 0),
                    ComponentConfig{}.with_debug_name("ol_root"));
    div(h.context(), mk(root.ent(), 0),
        ComponentConfig{}
            .with_absolute_position()
            .with_debug_name("ol_underlay"));
    h.layout_only();

    UIComponent *r = h.find("ol_root");
    UIComponent *u = h.find("ol_underlay");
    check(r != nullptr && u != nullptr, "both built");
    if (r && u) {
      printf("  root %d, plain absolute %d\n", r->render_layer,
             u->render_layer);
      check(u->render_layer == r->render_layer,
            "a plain absolute element does not jump above its parent");
    }
  }

  // with_overlay is the opt-in that says "above".
  {
    ImmTestHarness h;
    auto root = div(h.context(), mk(h.root(), 0),
                    ComponentConfig{}.with_debug_name("ov_root"));
    div(h.context(), mk(root.ent(), 0),
        ComponentConfig{}.with_overlay().with_debug_name("ov_dot"));
    h.layout_only();

    UIComponent *r = h.find("ov_root");
    UIComponent *d = h.find("ov_dot");
    check(r != nullptr && d != nullptr, "both built");
    if (r && d) {
      printf("  root %d, overlay %d\n", r->render_layer, d->render_layer);
      check(d->render_layer > r->render_layer, "an overlay is above its parent");
      check(d->render_layer == r->render_layer + 1, "by one level by default");
    }
  }

  // It is absolute too, so a caller does not have to say both.
  {
    ImmTestHarness h;
    auto root = div(h.context(), mk(h.root(), 0),
                    ComponentConfig{}.with_debug_name("ab_root"));
    div(h.context(), mk(root.ent(), 0),
        ComponentConfig{}.with_overlay().with_debug_name("ab_dot"));
    h.layout_only();
    UIComponent *d = h.find("ab_dot");
    check(d != nullptr && d->absolute, "with_overlay implies absolute");
  }

  // Relative, not a fixed number: an overlay inside an overlay clears the one
  // it sits in. A hardcoded layer could not express this.
  {
    ImmTestHarness h;
    auto root = div(h.context(), mk(h.root(), 0),
                    ComponentConfig{}.with_debug_name("nest_root"));
    auto mid = div(h.context(), mk(root.ent(), 0),
                   ComponentConfig{}.with_overlay().with_debug_name("nest_mid"));
    div(h.context(), mk(mid.ent(), 0),
        ComponentConfig{}.with_overlay().with_debug_name("nest_top"));
    h.layout_only();

    UIComponent *m = h.find("nest_mid");
    UIComponent *t = h.find("nest_top");
    check(m != nullptr && t != nullptr, "both built");
    if (m && t) {
      printf("  nested: mid %d, top %d\n", m->render_layer, t->render_layer);
      check(t->render_layer > m->render_layer,
            "a nested overlay clears the overlay it is in");
    }
  }

  // More than one level, for a caller that needs to clear something specific.
  {
    ImmTestHarness h;
    auto root = div(h.context(), mk(h.root(), 0),
                    ComponentConfig{}.with_debug_name("far_root"));
    div(h.context(), mk(root.ent(), 0),
        ComponentConfig{}.with_overlay(5).with_debug_name("far_dot"));
    h.layout_only();
    UIComponent *r = h.find("far_root");
    UIComponent *d = h.find("far_dot");
    if (r && d)
      check(d->render_layer == r->render_layer + 5, "levels_above is honoured");
  }

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
