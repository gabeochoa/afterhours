#include <iostream>

#include "../../ah.h"

using namespace afterhours;

int main() {
#if defined(AFTER_HOURS_ASSIGN_HANDLES_ON_CREATE)
  std::cout << "AFTER_HOURS_ASSIGN_HANDLES_ON_CREATE: ON\n";
#else
  std::cout << "AFTER_HOURS_ASSIGN_HANDLES_ON_CREATE: OFF\n";
#endif

  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();

  Entity &e = EntityHelper::createEntity();
  const int id = e.id;
  const EntityHandle h_pre = EntityHelper::handle_for(e);

  std::cout << "created temp entity id=" << id
            << " handle_pre.valid=" << (h_pre.valid() ? "true" : "false")
            << "\n";

  // Even if the handle resolves (opt-in), the query should still miss temp
  // entities unless forced to merge.
  {
    EntityQuery<> q({.ignore_temp_warning = true});
    q.whereID(id);
    std::cout << "query(temp, no-merge).has_values="
              << (q.has_values() ? "true" : "false") << "\n";
  }

#if defined(AFTER_HOURS_ASSIGN_HANDLES_ON_CREATE)
  {
    auto opt = EntityHelper::resolve(h_pre);
    std::cout << "resolve(temp_handle).valid=" << (opt.valid() ? "true" : "false")
              << "\n";
  }
#endif

  EntityHelper::merge_entity_arrays();
  const EntityHandle h_post = EntityHelper::handle_for(e);

  std::cout << "after merge: handle_post.valid="
            << (h_post.valid() ? "true" : "false") << "\n";

  {
    auto opt = EntityHelper::resolve(h_post);
    std::cout << "resolve(post_handle).valid=" << (opt.valid() ? "true" : "false")
              << "\n";
  }

  {
    EntityQuery<> q({.ignore_temp_warning = true});
    q.whereID(id);
    std::cout << "query(post-merge).has_values="
              << (q.has_values() ? "true" : "false") << "\n";
  }

  return 0;
}

