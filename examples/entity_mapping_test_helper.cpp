#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include <afterhours/ah.h>
#include "entity_mapping_test_helper.h"

std::vector<int> create_entities_in_other_tu(int count) {
    std::vector<int> ids;
    ids.reserve(count);
    for (int i = 0; i < count; ++i) {
        auto e = std::make_unique<afterhours::Entity>();
        ids.push_back(e->id);
    }
    return ids;
}

std::atomic_int *get_entity_id_gen_address_from_other_tu() {
    return &afterhours::ENTITY_ID_GEN;
}
