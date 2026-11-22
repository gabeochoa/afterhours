
#pragma once

#include "../../developer.h"
#include "../../plugins/color.h"

namespace afterhours {

inline void begin_3d(Camera3D &) { log_error("@notimplemented begin_3d"); }
inline void end_3d() { log_error("@notimplemented end_3d"); }

template <typename V>
inline void draw_cube(V, float, float, float, Color) {
  log_error("@notimplemented draw_cube");
}
template <typename V>
inline void draw_cube_wires(V, float, float, float, Color) {
  log_error("@notimplemented draw_cube_wires");
}
template <typename V3, typename V2>
inline void draw_plane(V3, V2, Color) {
  log_error("@notimplemented draw_plane");
}
template <typename V>
inline void draw_sphere(V, float, Color) {
  log_error("@notimplemented draw_sphere");
}
template <typename V>
inline void draw_sphere_wires(V, float, int, int, Color) {
  log_error("@notimplemented draw_sphere_wires");
}
template <typename V>
inline void draw_cylinder(V, float, float, float, int, Color) {
  log_error("@notimplemented draw_cylinder");
}
template <typename V>
inline void draw_cylinder_wires(V, V, float, float, int, Color) {
  log_error("@notimplemented draw_cylinder_wires");
}
template <typename V> inline void draw_line_3d(V, V, Color) {
  log_error("@notimplemented draw_line_3d");
}
template <typename V>
inline Vector2Type get_world_to_screen(V, Camera3D) {
  log_error("@notimplemented get_world_to_screen");
  return {0, 0};
}

} // namespace afterhours
