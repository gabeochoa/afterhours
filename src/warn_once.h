#pragma once

// One-shot diagnostics.
//
// A warning on a per-frame path prints every frame forever unless it is gated,
// which is why five different places had each grown their own guard -- two sets
// keyed differently, a static set, and two bools living on UIComponent.
//
//   warn_once(entity.id, "'{}' will never fire: ...", name);
//   warn_once(font_key, "No font registered for '{}'", font_key);
//
// `key` is what makes two reports distinct: an entity id, a font name, a value.
// Each call site gets its own gate, so the same key at two sites cannot silence
// one of them.
//
// Separate from logging.h on purpose. That header is on the ECS core path and
// goes out of its way to avoid heavy includes; this needs <set>.

#include <set>
#include <utility>

#include "logging.h"

namespace log_detail {
template <typename Key> inline bool warn_gate(const void *site, Key key) {
  static std::set<std::pair<const void *, Key>> seen;
  return seen.insert({site, std::move(key)}).second;
}
} // namespace log_detail

// Expands log_warn at the call site rather than wrapping it, so it picks up
// whichever logging mode is in effect -- including the one a test substitutes
// via AFTER_HOURS_REPLACE_LOGGING.
#define warn_once(key, ...)                                                    \
  do {                                                                         \
    /* Address is unique per expansion, which is what scopes the gate. */      \
    static const char afterhours_warn_site = 0;                                \
    if (::log_detail::warn_gate(&afterhours_warn_site, (key)))                 \
      log_warn(__VA_ARGS__);                                                   \
  } while (0)
