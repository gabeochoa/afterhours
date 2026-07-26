#pragma once

// Lightweight home for the translation Language enum, split out of
// translation.h so headers that only need the enum (e.g. ui/theme.h, pulled by
// most of the UI plugin) don't drag in translation.h's heavy fmt dependency
// (<fmt/args.h> + <fmt/format.h>, ~0.4s to parse per TU). translation.h itself
// includes this, so existing users are unaffected.

namespace afterhours {
namespace translation {

enum struct Language { English, Korean, Japanese };

} // namespace translation
} // namespace afterhours
