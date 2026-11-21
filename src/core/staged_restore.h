#pragma once

#include <utility>

#include "entity_collection.h"

namespace afterhours {

// Staged world restore: build a candidate world in `staging()`, validate it,
// then commit it into the live collection in one swap. Until commit, the live
// collection is never mutated; a failed validation or a throwing build leaves
// it exactly as it was, and the candidate is simply discarded.
//
// Commit contract: the live collection takes over the candidate's entities,
// singletons, permanent ids and handle table. Handles minted against the old
// live world must be treated as invalid after commit (their slot/generation
// now belongs to the candidate world), and any system/cache state keyed on
// entity identity must be rebuilt — `swap_contents` bumps the collection
// version and invalidates secondary indexes to force that. The old live
// entities are destroyed at commit, not kept alive by the restore object.
class StagedRestore {
public:
  EntityCollection &staging() { return staging_; }
  const EntityCollection &staging() const { return staging_; }
  bool committed() const { return committed_; }

  void discard() { staging_.delete_all_entities_NO_REALLY_I_MEAN_ALL(); }

  bool commit(EntityCollection &live) {
    return commit_if(live, [](EntityCollection &) { return true; });
  }

  // A throwing validator propagates; the live world is still untouched
  // because the swap has not happened yet.
  template <typename Validate>
  bool commit_if(EntityCollection &live, Validate &&validate) {
    if (committed_)
      return false;
    staging_.merge_entity_arrays();
    if (!validate(staging_))
      return false;
    live.swap_contents(staging_);
    committed_ = true;
    discard(); // staging_ now holds the old live world; drop it at commit
    return true;
  }

private:
  EntityCollection staging_;
  bool committed_ = false;
};

// One-shot form: `build(staging)` restores the (already decoded) snapshot
// into the candidate, `validate(staging)` accepts or rejects it whole.
// A throwing build or validator is a failed restore, not a partial one.
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

} // namespace afterhours
