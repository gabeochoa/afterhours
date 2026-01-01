#pragma once

namespace afterhours {

// EntityID is part of the public ECS surface and is used broadly across the
// codebase. Keep the definition in a tiny header to avoid include cycles when
// introducing component pools and stores.
using EntityID = int;

// Conventional sentinel for "no entity".
inline constexpr EntityID INVALID_ENTITY_ID = -1;

} // namespace afterhours

