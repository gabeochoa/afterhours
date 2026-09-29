// Widgets nothing rebuilds get destroyed once they are past the grace window.
#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include <afterhours/ah.h>
#include <afterhours/src/plugins/ui/entity_management.h>
#include <afterhours/src/plugins/ui/ui_collection.h>

#include <cstdio>

using namespace afterhours;
using namespace afterhours::ui;

static int tests_run = 0;
static int tests_passed = 0;

static void check(bool cond, const char *expr, const char *file, int line) {
  tests_run++;
  if (cond) {
    tests_passed++;
  } else {
    fprintf(stderr, "  FAIL: %s  (%s:%d)\n", expr, file, line);
  }
}

#define CHECK(expr) check((expr), #expr, __FILE__, __LINE__)

// Register one element under a made-up call-site hash, as mk() would.
static EntityID make_element(imm::UI_UUID hash) {
  Entity &e = UICollectionHolder::get().collection.createEntity();
  imm::existing_ui_elements[hash] = {e.id, imm::ui_build_frame};
  UICollectionHolder::get().collection.merge_entity_arrays();
  return e.id;
}

static bool alive(EntityID id) {
  return UICollectionHolder::getEntityForID(id).valid();
}

static void sweep_frames(int n) {
  for (int i = 0; i < n; i++) {
    imm::retire_unbuilt_ui_elements();
    UICollectionHolder::get().collection.cleanup();
  }
}

static void reset_world() {
  imm::existing_ui_elements.clear();
  imm::ui_element_children.clear();
  imm::ui_build_frame = 0;
  UICollectionHolder::get().collection.delete_all_entities_NO_REALLY_I_MEAN_ALL();
}

static void test_unbuilt_widget_is_retired_after_grace() {
  reset_world();
  imm::ui_retire_grace_frames = 10;
  EntityID id = make_element(1);

  sweep_frames(10);
  CHECK(alive(id)); // inside the window, still here

  sweep_frames(2);
  CHECK(!alive(id));
  CHECK(!imm::existing_ui_elements.contains(1));
}

static void test_rebuilding_keeps_it_alive() {
  reset_world();
  imm::ui_retire_grace_frames = 10;
  EntityID id = make_element(1);

  // Restamp every frame, the way mk() does for a widget still being built.
  for (int i = 0; i < 40; i++) {
    imm::existing_ui_elements[1].last_built_frame = imm::ui_build_frame;
    sweep_frames(1);
  }
  CHECK(alive(id));
}

static void test_grace_zero_disables_the_sweep() {
  reset_world();
  imm::ui_retire_grace_frames = 0;
  EntityID id = make_element(1);

  sweep_frames(500);
  CHECK(alive(id));
  CHECK(imm::existing_ui_elements.contains(1));
}

// The map and the collection have to agree; clearing one used to orphan the
// other, which turned a bounded map into an unbounded entity collection.
static void test_clear_destroys_rather_than_orphans() {
  reset_world();
  imm::ui_retire_grace_frames = 90;
  EntityID a = make_element(1);
  EntityID b = make_element(2);

  imm::clear_existing_ui_elements();
  UICollectionHolder::get().collection.cleanup();

  CHECK(!alive(a));
  CHECK(!alive(b));
  CHECK(imm::existing_ui_elements.empty());
}

// A slot keeps its entity as the item in it changes, and says when it changed.
// Keying on the item instead would mint an entity per row ever scrolled past,
// which is the leak the windowing exists to avoid.
//
// One call site on purpose: the source location is part of a widget's
// identity, so calling mk_keyed from four lines would be four slots.
static EntityID build_slot(Entity &parent, size_t key, bool *changed) {
  return imm::detail::mk_keyed(parent, 0, key, changed).first.get().id;
}

static void test_keyed_slot_recycles_and_reports() {
  reset_world();
  imm::ui_retire_grace_frames = 90;
  Entity &parent = UICollectionHolder::get().collection.createEntity();
  UICollectionHolder::get().collection.merge_entity_arrays();

  bool changed = false;
  const EntityID first = build_slot(parent, 100, &changed);
  CHECK(!changed); // a slot with no history has not changed

  // Same slot, same item: no change, same entity.
  CHECK(build_slot(parent, 100, &changed) == first);
  CHECK(!changed);

  // Same slot, different item: same entity, and the caller is told.
  CHECK(build_slot(parent, 101, &changed) == first);
  CHECK(changed);

  // And it settles again rather than latching.
  CHECK(build_slot(parent, 101, &changed) == first);
  CHECK(!changed);
}

// One call site on purpose: mk(parent, index) is the documented way to build
// siblings, and the index is part of the widget's identity.
static EntityID build_child(Entity &parent, int index) {
  return imm::mk(parent, index).first.get().id;
}

static bool record_exists_for(EntityID id) {
  for (const auto &[hash, record] : imm::existing_ui_elements)
    if (record.id == id)
      return true;
  return false;
}

static Entity &entity_for(EntityID id) {
  return UICollectionHolder::getEntityForIDEnforce(id);
}

// A widget's children are linked to it when they are created, so when the
// parent is retired its whole subtree goes in the same sweep -- even a child
// that is still being rebuilt (its own grace window has not run out).
static void test_retiring_a_parent_retires_its_subtree() {
  reset_world();
  imm::ui_retire_grace_frames = 10;
  Entity &root = UICollectionHolder::get().collection.createEntity();
  UICollectionHolder::get().collection.merge_entity_arrays();

  const EntityID parent = build_child(root, 0);
  const EntityID child = build_child(entity_for(parent), 0);
  const EntityID grandchild = build_child(entity_for(child), 0);
  // A second tree whose parent keeps being built: it must be untouched.
  const EntityID live_parent = build_child(root, 1);
  const EntityID live_child = build_child(entity_for(live_parent), 0);

  // Rebuild everything except `parent` until the sweep takes it.
  for (int i = 0; i < 40 && alive(parent); i++) {
    build_child(root, 1);
    build_child(entity_for(live_parent), 0);
    build_child(entity_for(parent), 0);
    build_child(entity_for(child), 0);
    sweep_frames(1);
  }

  CHECK(!alive(parent));
  // Same sweep, not one grace window later.
  CHECK(!alive(child));
  CHECK(!alive(grandchild));
  CHECK(!record_exists_for(child));
  CHECK(!record_exists_for(grandchild));
  CHECK(alive(live_parent));
  CHECK(alive(live_child));
}

// An entity created outside mk() has no record, so nothing would ever
// retire it; linking it at creation gives it its parent's lifetime. Moving
// the link to a living parent before the old one goes spares it.
static void test_linked_raw_child_follows_its_parent() {
  reset_world();
  imm::ui_retire_grace_frames = 10;
  Entity &root = UICollectionHolder::get().collection.createEntity();
  UICollectionHolder::get().collection.merge_entity_arrays();

  const EntityID dying_parent = build_child(root, 0);
  const EntityID live_parent = build_child(root, 1);
  Entity &raw = UICollectionHolder::get().collection.createEntity();
  Entity &adopted = UICollectionHolder::get().collection.createEntity();
  UICollectionHolder::get().collection.merge_entity_arrays();
  imm::link_ui_child(dying_parent, raw.id);
  imm::link_ui_child(dying_parent, adopted.id);
  imm::unlink_ui_child(dying_parent, adopted.id);
  imm::link_ui_child(live_parent, adopted.id);

  for (int i = 0; i < 40 && alive(dying_parent); i++) {
    build_child(root, 1);
    sweep_frames(1);
  }

  CHECK(!alive(dying_parent));
  CHECK(!alive(raw.id));
  CHECK(alive(live_parent));
  CHECK(alive(adopted.id));
}

// Clearing every element must reach linked raw children too, not just the
// recorded ones.
static void test_clear_reaches_linked_raw_children() {
  reset_world();
  imm::ui_retire_grace_frames = 90;
  Entity &root = UICollectionHolder::get().collection.createEntity();
  UICollectionHolder::get().collection.merge_entity_arrays();

  const EntityID parent = build_child(root, 0);
  Entity &raw = UICollectionHolder::get().collection.createEntity();
  UICollectionHolder::get().collection.merge_entity_arrays();
  imm::link_ui_child(parent, raw.id);

  imm::clear_existing_ui_elements();
  UICollectionHolder::get().collection.cleanup();

  CHECK(!alive(parent));
  CHECK(!alive(raw.id));
  CHECK(imm::ui_element_children.empty());
}

// Grace 0 means the app does its own lifetime: no sweep, no cascade.
static void test_grace_zero_disables_the_cascade_too() {
  reset_world();
  imm::ui_retire_grace_frames = 0;
  Entity &root = UICollectionHolder::get().collection.createEntity();
  UICollectionHolder::get().collection.merge_entity_arrays();

  const EntityID parent = build_child(root, 0);
  const EntityID child = build_child(entity_for(parent), 0);

  sweep_frames(500);
  CHECK(alive(parent));
  CHECK(alive(child));
}

int main() {
  printf("Running widget retirement tests...\n\n");
  printf("  keyed_slot_recycles_and_reports\n");
  test_keyed_slot_recycles_and_reports();
  printf("  retiring_a_parent_retires_its_subtree\n");
  test_retiring_a_parent_retires_its_subtree();
  printf("  linked_raw_child_follows_its_parent\n");
  test_linked_raw_child_follows_its_parent();
  printf("  clear_reaches_linked_raw_children\n");
  test_clear_reaches_linked_raw_children();
  printf("  grace_zero_disables_the_cascade_too\n");
  test_grace_zero_disables_the_cascade_too();
  printf("  unbuilt_widget_is_retired_after_grace\n");
  test_unbuilt_widget_is_retired_after_grace();
  printf("  rebuilding_keeps_it_alive\n");
  test_rebuilding_keeps_it_alive();
  printf("  grace_zero_disables_the_sweep\n");
  test_grace_zero_disables_the_sweep();
  printf("  clear_destroys_rather_than_orphans\n");
  test_clear_destroys_rather_than_orphans();

  printf("\n%d/%d tests passed.\n", tests_passed, tests_run);
  if (tests_passed != tests_run) {
    printf("SOME TESTS FAILED!\n");
    return 1;
  }
  printf("ALL TESTS PASSED.\n");
  return 0;
}
