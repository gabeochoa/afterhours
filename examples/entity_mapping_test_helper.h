#pragma once

#include <atomic>
#include <memory>
#include <vector>

namespace afterhours {
struct Entity;
}

// Creates entities from a *separate translation unit* so we can verify
// that ENTITY_ID_GEN is shared globally (inline) rather than per-TU (static).
// Returns the raw entity IDs assigned to the newly created entities.
std::vector<int> create_entities_in_other_tu(int count);

// Returns the address of ENTITY_ID_GEN as seen by the helper TU.
// If ENTITY_ID_GEN is `inline`, this should equal the address in any TU.
// If ENTITY_ID_GEN is `static`, each TU has its own copy at a different address.
std::atomic_int *get_entity_id_gen_address_from_other_tu();
