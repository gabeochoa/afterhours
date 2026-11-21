#pragma once

#include <utility>

#include "entity_collection.h"

namespace afterhours {

class StagedRestore {
public:
  EntityCollection &staging() { return staging_; }
  const EntityCollection &staging() const { return staging_; }
  bool committed() const { return committed_; }

  void discard() { staging_.delete_all_entities_NO_REALLY_I_MEAN_ALL(); }

  bool commit(EntityCollection &live) {
    return commit_if(live, [](EntityCollection &) { return true; });
  }

  template <typename Validate>
  bool commit_if(EntityCollection &live, Validate &&validate) {
    if (committed_)
      return false;
    staging_.merge_entity_arrays();
    if (!validate(staging_))
      return false;
    live.swap_contents(staging_);
    committed_ = true;
    discard();
    return true;
  }

private:
  EntityCollection staging_;
  bool committed_ = false;
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
