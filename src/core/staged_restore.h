#pragma once

#include <utility>

#include "entity_collection.h"

namespace afterhours {

class StagedRestore {
public:
  EntityCollection &staging() { return stage; }
  const EntityCollection &staging() const { return stage; }
  bool committed() const { return is_committed; }

  void discard() { stage.delete_all_entities_NO_REALLY_I_MEAN_ALL(); }

  bool commit(EntityCollection &live) {
    return commit_if(live, [](EntityCollection &) { return true; });
  }

  template <typename Validate>
  bool commit_if(EntityCollection &live, Validate &&validate) {
    if (is_committed)
      return false;
    stage.merge_entity_arrays();
    if (!validate(stage))
      return false;
    live.swap_contents(stage);
    is_committed = true;
    discard();
    return true;
  }

private:
  EntityCollection stage;
  bool is_committed = false;
};

template <typename Build, typename Validate>
bool staged_restore(EntityCollection &live, Build &&build,
                    Validate &&validate) {
  StagedRestore restore;
  try {
    build(restore.staging());
    return restore.commit_if(live, std::forward<Validate>(validate));
  } catch (...) {
    return false;
  }
}

template <typename Build>
bool staged_restore(EntityCollection &live, Build &&build) {
  return staged_restore(live, std::forward<Build>(build),
                        [](EntityCollection &) { return true; });
}

}
