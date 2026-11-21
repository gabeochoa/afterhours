
#pragma once

#include <cmath>

#include "../../developer.h"
#include "../../plugins/color.h"

namespace afterhours {

inline void begin_3d(Camera3D &cam) { raylib::BeginMode3D(cam); }
inline void end_3d() { raylib::EndMode3D(); }

inline void draw_cube(raylib::Vector3 pos, float w, float h, float d,
                      Color c) {
  raylib::DrawCube(pos, w, h, d, c);
}

inline void draw_cube_wires(raylib::Vector3 pos, float w, float h, float d,
                            Color c) {
  raylib::DrawCubeWiresV(pos, {w, h, d}, c);
}

inline void draw_plane(raylib::Vector3 center, Vector2Type size, Color c) {
  raylib::DrawPlane(center, size, c);
}

inline void draw_sphere(raylib::Vector3 pos, float radius, Color c) {
  raylib::DrawSphere(pos, radius, c);
}

inline void draw_sphere_wires(raylib::Vector3 pos, float radius, int rings,
                              int slices, Color c) {
  raylib::DrawSphereWires(pos, radius, rings, slices, c);
}

inline void draw_cylinder(raylib::Vector3 pos, float rtop, float rbot, float h,
                          int slices, Color c) {
  raylib::DrawCylinder(pos, rtop, rbot, h, slices, c);
}

inline void draw_cylinder_wires(raylib::Vector3 start, raylib::Vector3 end,
                                float rtop, float rbot, int slices, Color c) {
  raylib::DrawCylinderWiresEx(start, end, rtop, rbot, slices, c);
}

// Flat ellipse on the XZ plane through center, as a triangle fan.
inline void draw_ellipse_3d(raylib::Vector3 center, float radius_x,
                            float radius_z, Color c) {
  constexpr int SEGMENTS = 24;
  for (int i = 0; i < SEGMENTS; i++) {
    const float a0 = 360.f * static_cast<float>(i) / SEGMENTS * DEG2RAD;
    const float a1 = 360.f * static_cast<float>(i + 1) / SEGMENTS * DEG2RAD;
    raylib::DrawTriangle3D(
        center,
        {center.x + radius_x * std::cos(a1), center.y,
         center.z + radius_z * std::sin(a1)},
        {center.x + radius_x * std::cos(a0), center.y,
         center.z + radius_z * std::sin(a0)},
        c);
  }
}

inline void draw_line_3d(raylib::Vector3 a, raylib::Vector3 b, Color c) {
  raylib::DrawLine3D(a, b, c);
}

inline Vector2Type get_world_to_screen(raylib::Vector3 pos, Camera3D cam) {
  return raylib::GetWorldToScreen(pos, cam);
}

} // namespace afterhours
