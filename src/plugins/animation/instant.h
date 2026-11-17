#pragma once

namespace afterhours {
namespace motion {

inline bool &instant_flag() {
  static bool v = false;
  return v;
}
inline void set_instant(bool on) { instant_flag() = on; }
inline bool is_instant() { return instant_flag(); }

} // namespace motion
} // namespace afterhours
