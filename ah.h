
#pragma once

#include <array>
#include <bitset>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <vector>

namespace afterhours {

#if !defined(AFTER_HOURS_MAX_COMPONENTS)
#define AFTER_HOURS_MAX_COMPONENTS 128
#endif

#include "src/entity.h"

#if defined(AFTER_HOURS_ENTITY_HELPER)
#include "src/entity_helper.h"
#endif

#if defined(AFTER_HOURS_ENTITY_QUERY)
#include "src/entity_query.h"
#endif

#if defined(AFTER_HOURS_SYSTEM)
#include "src/system.h"
#endif

} // namespace afterhours
