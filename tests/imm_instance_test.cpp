#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

// One call site, built twice under the same parent (R-060 / hanabi #596).
static std::pair<EntityID, EntityID> build_subtree(
    ui_test::ImmTestHarness &h, Entity &parent) {
  auto root_pair = mk(parent, 0);
  auto &root_ent = deref(root_pair).first;
  div(h.context(), root_pair,
      ComponentConfig{}.with_size({pixels(100), pixels(50)}));
  auto child_pair = mk(root_ent, 0);
  auto &child_ent = deref(child_pair).first;
  div(h.context(), child_pair,
      ComponentConfig{}.with_size({pixels(50), pixels(20)}));
  return {root_ent.id, child_ent.id};
}

static std::pair<EntityID, EntityID> build_instance(
    ui_test::ImmTestHarness &h, Entity &parent, EntityID key) {
  auto root_pair = mk_instance(parent, key);
  auto &root_ent = deref(root_pair).first;
  div(h.context(), root_pair,
      ComponentConfig{}.with_size({pixels(100), pixels(50)}));
  auto child_pair = mk(root_ent, 0);
  auto &child_ent = deref(child_pair).first;
  div(h.context(), child_pair,
      ComponentConfig{}.with_size({pixels(50), pixels(20)}));
  return {root_ent.id, child_ent.id};
}

TEST(mk_instance_gives_each_instance_its_own_entities) {
  ui_test::ImmTestHarness h;
  std::pair<EntityID, EntityID> a, b;
  for (int frame = 0; frame < 2; ++frame) {
    h.begin_frame();
    auto a2 = build_instance(h, h.root(), 1);
    auto b2 = build_instance(h, h.root(), 2);
    CHECK(a2.first != b2.first);
    CHECK(a2.second != b2.second);
    if (frame == 0) { a = a2; b = b2; }
    else { CHECK(a2 == a); CHECK(b2 == b); }
    h.layout_only();
  }
}

TEST(imm_scope_salts_a_whole_subtree_function) {
  ui_test::ImmTestHarness h;
  h.begin_frame();
  std::pair<EntityID, EntityID> a, b;
  { ImmScope scope(1); a = build_subtree(h, h.root()); }
  { ImmScope scope(2); b = build_subtree(h, h.root()); }
  CHECK(a.first != b.first);
  CHECK(a.second != b.second);
  h.layout_only();
}

TEST(single_unscoped_build_is_unchanged) {
  ui_test::ImmTestHarness h;
  EntityID first = -1;
  for (int frame = 0; frame < 2; ++frame) {
    h.begin_frame();
    auto built = build_subtree(h, h.root());
    if (frame == 0) first = built.first;
    else CHECK(built.first == first);
    h.layout_only();
  }
}

int main() { return ui_test::run_registered_tests("imm instance"); }
