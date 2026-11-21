// attachment_test.cpp — AttachedTo relation: pose inheritance, reparent,
// cleanup detach, type-filter rejection.
#define FMT_HEADER_ONLY
#include <fmt/format.h>
#include <afterhours/ah.h>
#include <afterhours/src/attachment.h>
#include <cmath>
#include <cstdio>
using namespace afterhours;
static int run = 0, passed = 0;
static void check(bool c, const char *e, const char *f, int l) {
  run++; if (c) passed++; else fprintf(stderr, "  FAIL: %s (%s:%d)\n", e, f, l);
}
#define CHECK(e) check((e), #e, __FILE__, __LINE__)
static bool near(float a, float b) { return std::fabs(a - b) < 0.001f; }

static void test_inherits_pose() {
  EntityCollection ec; auto &p = ec.createEntity(); auto &c = ec.createEntity();
  ec.merge_entity_arrays();
  p.addComponent<HasPose2D>(100.f, 50.f, 90.f);
  CHECK(attachment::attach(ec, c, p, {10, 0, 15}, "hand"));
  CHECK(c.get<AttachedTo>().socket == "hand");
  auto w = attachment::world_pose(ec, c);
  CHECK(w && near(w->x, 100) && near(w->y, 60) && near(w->angle_deg, 105));
  p.get<HasPose2D>().pose = {0, 0, 0};
  w = attachment::world_pose(ec, c);
  CHECK(w && near(w->x, 10) && near(w->y, 0));
}
static void test_reparent_moves() {
  EntityCollection ec; auto &a = ec.createEntity(); auto &b = ec.createEntity(); auto &c = ec.createEntity();
  ec.merge_entity_arrays();
  a.addComponent<HasPose2D>(0.f, 0.f); b.addComponent<HasPose2D>(500.f, 0.f);
  CHECK(attachment::attach(ec, c, a, {1, 0}));
  CHECK(attachment::attach(ec, c, b, {2, 0}));
  CHECK(attachment::parent_of(c) == b.id);
  CHECK(attachment::children_of(ec, a.id).empty());
  CHECK(attachment::children_of(ec, b.id) == std::vector<EntityID>{c.id});
  CHECK(!attachment::attach(ec, b, c)); // cycle rejected (c's parent is b)
  CHECK(!attachment::attach(ec, a, a)); // self rejected
  attachment::detach(c); CHECK(!attachment::is_attached(c));
}
static void test_cleanup_detaches() {
  EntityCollection ec; auto &p = ec.createEntity(); auto &c = ec.createEntity(); auto &d = ec.createEntity();
  ec.merge_entity_arrays();
  CHECK(attachment::attach(ec, c, p)); CHECK(attachment::attach(ec, d, p));
  p.cleanup = true;
  CHECK(attachment::prune(ec) == 2);
  CHECK(!attachment::is_attached(c) && !attachment::is_attached(d));
  // Child marked cleanup also loses its relation in prune.
  auto &p2 = ec.createEntity(); ec.merge_entity_arrays();
  CHECK(attachment::attach(ec, c, p2)); c.cleanup = true;
  CHECK(attachment::prune(ec) == 1); CHECK(!attachment::is_attached(c));
  ec.cleanup();
  CHECK(attachment::find(ec, p.id) == nullptr);
}
static void test_filter_rejection() {
  EntityCollection ec; auto &p = ec.createEntity(); auto &c = ec.createEntity();
  ec.merge_entity_arrays();
  auto only_pose_parents = [](const Entity &, const Entity &parent) { return parent.has<HasPose2D>(); };
  CHECK(!attachment::attach(ec, c, p, {}, "", only_pose_parents));
  CHECK(!attachment::is_attached(c)); // rejection leaves no relation
  p.addComponent<HasPose2D>(1.f, 2.f);
  CHECK(attachment::attach(ec, c, p, {}, "", only_pose_parents));
}
int main() {
  test_inherits_pose(); test_reparent_moves(); test_cleanup_detaches(); test_filter_rejection();
  printf("%d/%d tests passed.\n", passed, run);
  return passed == run ? 0 : 1;
}
