
#pragma once

#include <cmath>

#include "../../developer.h"
#include "../../graphics.h"
#include "../../plugins/color.h"

namespace afterhours {

namespace detail_3d {
constexpr float PI = 3.14159265358979323846f;

inline void set_color(const Color c) { sgl_c4b(c.r, c.g, c.b, c.a); }

inline float screen_w() {
  return static_cast<float>(sapp_width()) / sapp_dpi_scale();
}
inline float screen_h() {
  return static_cast<float>(sapp_height()) / sapp_dpi_scale();
}
} // namespace detail_3d

inline void begin_3d(Camera3D &cam) {
  sgl_matrix_mode_projection();
  sgl_push_matrix();
  sgl_load_identity();

  float aspect = detail_3d::screen_w() / detail_3d::screen_h();

  if (cam.projection == 0) {
    sgl_perspective(sgl_rad(cam.fovy), aspect, 0.01f, 1000.0f);
  } else {
    float top = cam.fovy / 2.0f;
    float right = top * aspect;
    sgl_ortho(-right, right, -top, top, 0.01f, 1000.0f);
  }

  sgl_matrix_mode_modelview();
  sgl_push_matrix();
  sgl_load_identity();
  sgl_lookat(cam.position.x, cam.position.y, cam.position.z, cam.target.x,
             cam.target.y, cam.target.z, cam.up.x, cam.up.y, cam.up.z);
}

inline void end_3d() {
  sgl_matrix_mode_modelview();
  sgl_pop_matrix();
  sgl_matrix_mode_projection();
  sgl_pop_matrix();
}

template <typename V>
inline void draw_cube(V pos, float w, float h, float d, Color c) {
  float x0 = pos.x - w / 2, x1 = pos.x + w / 2;
  float y0 = pos.y - h / 2, y1 = pos.y + h / 2;
  float z0 = pos.z - d / 2, z1 = pos.z + d / 2;

  detail_3d::set_color(c);
  sgl_begin_quads();
  // Front
  sgl_v3f(x0, y0, z1);
  sgl_v3f(x1, y0, z1);
  sgl_v3f(x1, y1, z1);
  sgl_v3f(x0, y1, z1);
  // Back
  sgl_v3f(x1, y0, z0);
  sgl_v3f(x0, y0, z0);
  sgl_v3f(x0, y1, z0);
  sgl_v3f(x1, y1, z0);
  // Top
  sgl_v3f(x0, y1, z1);
  sgl_v3f(x1, y1, z1);
  sgl_v3f(x1, y1, z0);
  sgl_v3f(x0, y1, z0);
  // Bottom
  sgl_v3f(x0, y0, z0);
  sgl_v3f(x1, y0, z0);
  sgl_v3f(x1, y0, z1);
  sgl_v3f(x0, y0, z1);
  // Right
  sgl_v3f(x1, y0, z1);
  sgl_v3f(x1, y0, z0);
  sgl_v3f(x1, y1, z0);
  sgl_v3f(x1, y1, z1);
  // Left
  sgl_v3f(x0, y0, z0);
  sgl_v3f(x0, y0, z1);
  sgl_v3f(x0, y1, z1);
  sgl_v3f(x0, y1, z0);
  sgl_end();
}

template <typename V>
inline void draw_cube_wires(V pos, float w, float h, float d, Color c) {
  float x0 = pos.x - w / 2, x1 = pos.x + w / 2;
  float y0 = pos.y - h / 2, y1 = pos.y + h / 2;
  float z0 = pos.z - d / 2, z1 = pos.z + d / 2;

  detail_3d::set_color(c);
  sgl_begin_lines();
  // Bottom face
  sgl_v3f(x0, y0, z0);
  sgl_v3f(x1, y0, z0);
  sgl_v3f(x1, y0, z0);
  sgl_v3f(x1, y0, z1);
  sgl_v3f(x1, y0, z1);
  sgl_v3f(x0, y0, z1);
  sgl_v3f(x0, y0, z1);
  sgl_v3f(x0, y0, z0);
  // Top face
  sgl_v3f(x0, y1, z0);
  sgl_v3f(x1, y1, z0);
  sgl_v3f(x1, y1, z0);
  sgl_v3f(x1, y1, z1);
  sgl_v3f(x1, y1, z1);
  sgl_v3f(x0, y1, z1);
  sgl_v3f(x0, y1, z1);
  sgl_v3f(x0, y1, z0);
  // Vertical edges
  sgl_v3f(x0, y0, z0);
  sgl_v3f(x0, y1, z0);
  sgl_v3f(x1, y0, z0);
  sgl_v3f(x1, y1, z0);
  sgl_v3f(x1, y0, z1);
  sgl_v3f(x1, y1, z1);
  sgl_v3f(x0, y0, z1);
  sgl_v3f(x0, y1, z1);
  sgl_end();
}

template <typename V3, typename V2>
inline void draw_plane(V3 center, V2 size, Color c) {
  float hw = size.x / 2.0f;
  float hd = size.y / 2.0f;

  detail_3d::set_color(c);
  sgl_begin_quads();
  sgl_v3f(center.x - hw, center.y, center.z - hd);
  sgl_v3f(center.x - hw, center.y, center.z + hd);
  sgl_v3f(center.x + hw, center.y, center.z + hd);
  sgl_v3f(center.x + hw, center.y, center.z - hd);
  sgl_end();
}

template <typename V> inline void draw_sphere(V pos, float radius, Color c) {
  constexpr int RINGS = 12;
  constexpr int SLICES = 12;

  detail_3d::set_color(c);

  for (int i = 0; i < RINGS; i++) {
    float phi0 = detail_3d::PI * static_cast<float>(i) / RINGS;
    float phi1 = detail_3d::PI * static_cast<float>(i + 1) / RINGS;
    float sp0 = sinf(phi0), cp0 = cosf(phi0);
    float sp1 = sinf(phi1), cp1 = cosf(phi1);

    sgl_begin_triangle_strip();
    for (int j = 0; j <= SLICES; j++) {
      float theta = 2.0f * detail_3d::PI * static_cast<float>(j) / SLICES;
      float st = sinf(theta), ct = cosf(theta);
      sgl_v3f(pos.x + radius * sp0 * ct, pos.y + radius * cp0,
              pos.z + radius * sp0 * st);
      sgl_v3f(pos.x + radius * sp1 * ct, pos.y + radius * cp1,
              pos.z + radius * sp1 * st);
    }
    sgl_end();
  }
}

template <typename V>
inline void draw_sphere_wires(V pos, float radius, int rings, int slices,
                              Color c) {
  detail_3d::set_color(c);

  for (int i = 1; i < rings; i++) {
    float phi = detail_3d::PI * static_cast<float>(i) / rings;
    float sp = sinf(phi), cp = cosf(phi);
    sgl_begin_line_strip();
    for (int j = 0; j <= slices; j++) {
      float theta = 2.0f * detail_3d::PI * static_cast<float>(j) / slices;
      sgl_v3f(pos.x + radius * sp * cosf(theta), pos.y + radius * cp,
              pos.z + radius * sp * sinf(theta));
    }
    sgl_end();
  }

  for (int j = 0; j < slices; j++) {
    float theta = 2.0f * detail_3d::PI * static_cast<float>(j) / slices;
    float st = sinf(theta), ct = cosf(theta);
    sgl_begin_line_strip();
    for (int i = 0; i <= rings; i++) {
      float phi = detail_3d::PI * static_cast<float>(i) / rings;
      sgl_v3f(pos.x + radius * sinf(phi) * ct, pos.y + radius * cosf(phi),
              pos.z + radius * sinf(phi) * st);
    }
    sgl_end();
  }
}

template <typename V>
inline void draw_cylinder(V pos, float rtop, float rbot, float h, int slices,
                          Color c) {
  detail_3d::set_color(c);
  float y0 = pos.y;
  float y1 = pos.y + h;

  sgl_begin_triangle_strip();
  for (int i = 0; i <= slices; i++) {
    float a = 2.0f * detail_3d::PI * static_cast<float>(i) / slices;
    float ca = cosf(a), sa = sinf(a);
    sgl_v3f(pos.x + rbot * ca, y0, pos.z + rbot * sa);
    sgl_v3f(pos.x + rtop * ca, y1, pos.z + rtop * sa);
  }
  sgl_end();

  if (rtop > 0.0f) {
    sgl_begin_triangle_strip();
    for (int i = 0; i <= slices; i++) {
      float a = 2.0f * detail_3d::PI * static_cast<float>(i) / slices;
      sgl_v3f(pos.x, y1, pos.z);
      sgl_v3f(pos.x + rtop * cosf(a), y1, pos.z + rtop * sinf(a));
    }
    sgl_end();
  }

  if (rbot > 0.0f) {
    sgl_begin_triangle_strip();
    for (int i = 0; i <= slices; i++) {
      float a = 2.0f * detail_3d::PI * static_cast<float>(i) / slices;
      sgl_v3f(pos.x + rbot * cosf(a), y0, pos.z + rbot * sinf(a));
      sgl_v3f(pos.x, y0, pos.z);
    }
    sgl_end();
  }
}

template <typename V>
inline void draw_cylinder_wires(V start, V end, float rtop, float rbot,
                                int slices, Color c) {
  detail_3d::set_color(c);

  sgl_begin_line_strip();
  for (int i = 0; i <= slices; i++) {
    float a = 2.0f * detail_3d::PI * static_cast<float>(i) / slices;
    sgl_v3f(end.x + rtop * cosf(a), end.y, end.z + rtop * sinf(a));
  }
  sgl_end();

  sgl_begin_line_strip();
  for (int i = 0; i <= slices; i++) {
    float a = 2.0f * detail_3d::PI * static_cast<float>(i) / slices;
    sgl_v3f(start.x + rbot * cosf(a), start.y, start.z + rbot * sinf(a));
  }
  sgl_end();

  sgl_begin_lines();
  for (int i = 0; i < slices; i++) {
    float a = 2.0f * detail_3d::PI * static_cast<float>(i) / slices;
    float ca = cosf(a), sa = sinf(a);
    sgl_v3f(start.x + rbot * ca, start.y, start.z + rbot * sa);
    sgl_v3f(end.x + rtop * ca, end.y, end.z + rtop * sa);
  }
  sgl_end();
}

template <typename V> inline void draw_line_3d(V a, V b, Color c) {
  detail_3d::set_color(c);
  sgl_begin_lines();
  sgl_v3f(a.x, a.y, a.z);
  sgl_v3f(b.x, b.y, b.z);
  sgl_end();
}

template <typename V>
inline Vector2Type get_world_to_screen(V pos, Camera3D cam) {
  float sw = detail_3d::screen_w();
  float sh = detail_3d::screen_h();
  float aspect = sw / sh;

  // View basis vectors
  float fx = cam.target.x - cam.position.x;
  float fy = cam.target.y - cam.position.y;
  float fz = cam.target.z - cam.position.z;
  float fl = sqrtf(fx * fx + fy * fy + fz * fz);
  if (fl < 1e-6f)
    return {0, 0};
  fx /= fl;
  fy /= fl;
  fz /= fl;

  float rx = fy * cam.up.z - fz * cam.up.y;
  float ry = fz * cam.up.x - fx * cam.up.z;
  float rz = fx * cam.up.y - fy * cam.up.x;
  float rl = sqrtf(rx * rx + ry * ry + rz * rz);
  if (rl < 1e-6f)
    return {0, 0};
  rx /= rl;
  ry /= rl;
  rz /= rl;

  float ux = ry * fz - rz * fy;
  float uy = rz * fx - rx * fz;
  float uz = rx * fy - ry * fx;

  float dx = pos.x - cam.position.x;
  float dy = pos.y - cam.position.y;
  float dz = pos.z - cam.position.z;
  float vx = rx * dx + ry * dy + rz * dz;
  float vy = ux * dx + uy * dy + uz * dz;
  float vz = fx * dx + fy * dy + fz * dz;

  if (cam.projection == 0) {
    if (vz < 0.001f)
      return {0, 0};
    float fov_rad = cam.fovy * detail_3d::PI / 180.0f;
    float half_h = tanf(fov_rad * 0.5f);
    float ndcx = vx / (vz * half_h * aspect);
    float ndcy = vy / (vz * half_h);
    return {(ndcx * 0.5f + 0.5f) * sw, (0.5f - ndcy * 0.5f) * sh};
  } else {
    float top = cam.fovy / 2.0f;
    float right_ext = top * aspect;
    float ndcx = vx / right_ext;
    float ndcy = vy / top;
    return {(ndcx * 0.5f + 0.5f) * sw, (0.5f - ndcy * 0.5f) * sh};
  }
}

} // namespace afterhours
