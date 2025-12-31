#include <cassert>
#include <iostream>

#include "../../src/core/snapshot.h"
#include "../../src/ecs.h"

using namespace afterhours;

// Demo component that stores a pointer-free reference (EntityHandle).
struct Targets : BaseComponent {
  EntityHandle target{};
};

// DTO used for snapshots (copyable + pointer-free).
struct TargetsDTO {
  EntityHandle target{};
};

int main() {
  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();

  Entity &a = EntityHelper::createEntity();
  Entity &b = EntityHelper::createEntity();
  EntityHelper::merge_entity_arrays();

  const EntityHandle hb = EntityHelper::handle_for(b);
  assert(hb.valid());

  a.addComponent<Targets>().target = hb;

  // Snapshot entities (handle/tags/type only; pointer-free).
  auto entities = snapshot::take_entities();
  assert(entities.size() >= 2);

  // Snapshot Targets into a pointer-free DTO.
  auto targets = snapshot::take_components<Targets, TargetsDTO>(
      [](const Targets &t) { return TargetsDTO{.target = t.target}; });
  assert(targets.size() == 1);

  // Validate captured handle matches.
  assert(targets[0].value.target.valid());
  assert(targets[0].value.target.slot == hb.slot);
  assert(targets[0].value.target.gen == hb.gen);

  std::cout << "snapshot_surface: OK\n";
  return 0;
}

