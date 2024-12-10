
#pragma once

#include <array>
#include <bitset>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <vector>

namespace afterhours {

#include "src/entity.h"

#if defined(AFTER_HOURS_ENTITY_HELPER)
#include "src/entity_helper.h"
#endif

#if defined(AFTER_HOURS_ENTITY_QUERY)
#include "src/entity_query.h"
#endif

}  // namespace afterhours
