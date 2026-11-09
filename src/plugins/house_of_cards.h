#pragma once

// house_of_cards — replace 3D models with impostor cards.
//
// Fully based on Thomas R Murphy's Imposter Cards for Blender:
//   https://superhivemarket.com/products/imposter-cards-nacho-time
//   "Imposter Syndrome", Blender Conference 2026:
//   https://www.youtube.com/watch?v=qBPuZ-1StZc
//   https://conference.blender.org/2026/presentations/4266/
// The octahedral layout follows Ryan Brucks' octahedral impostors.
//
// A model is rendered from grid x grid directions spread over the sphere
// into one atlas: colour with alpha, plus depth along each view. At runtime
// each copy is one quad facing the camera. Per pixel, the shader casts the
// eye ray into the three baked views nearest the camera direction, refines
// the hit with their depth, and blends.
//
// The add-on's six axis-aligned views were tried as well. Scored against the
// real model they lost on every example (silhouette overlap 0.65-0.94 against
// 0.88-0.97), and their views sit up to 55 degrees off the camera, which
// needs a depth march at about 4x the per-pixel cost to avoid squashing.
//
// Bake offline with tools/house_of_cards (or bake() at load time), then:
//   house_of_cards::Cards cards;
//   house_of_cards::load("tree", cards);  // tree.cards, .png, _depth.png
//   ShaderType shader = house_of_cards::load_shader();
//   house_of_cards::draw_instanced(cards, shader, transforms, n, camera);
//
// Unlit, like raylib's default shader: nothing is baked for normals, so cards
// do not respond to scene lighting. raylib only, desktop GL (GLSL 330).

// TODO(backend-agnostic): the whole plugin is raylib calls and GLSL 330.
// Route baking and drawing through backend APIs so sokol/Metal get cards too.
#ifdef AFTER_HOURS_USE_RAYLIB

#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

#include "../developer.h"
#include "../drawing_helpers_3d.h"
#include "../logging.h"
#include "models.h"

namespace afterhours::house_of_cards {

struct Meta {
  int grid = 16; // views per side
  int tile = 64; // pixels per view
  float radius = 0.f;
  raylib::Vector3 center{};

  [[nodiscard]] int views() const { return grid * grid; }
};

// CPU side of a bake: what the tool writes and upload() turns into Cards.
struct Baked {
  Meta meta;
  raylib::Image color{}; // RGBA8; alpha is coverage
  // Grayscale: height of the nearest surface along each view. 0.5 is the
  // plane through the centre, 0 and 1 the bounding sphere.
  raylib::Image depth{};
  // RGB: the surface normal in the model's own space, as n * 0.5 + 0.5. Only
  // read by a lit shader.
  raylib::Image normal{};
};

struct Cards {
  Meta meta;
  raylib::Texture2D color{};
  raylib::Texture2D depth{};
  raylib::Texture2D normal{};
  raylib::Mesh quad{};
  raylib::Material material{};
};

namespace detail {
using V3 = raylib::Vector3;
inline V3 add(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline V3 sub(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline V3 mul(V3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline float length(V3 a) { return std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z); }
inline V3 normalize(V3 a) { return mul(a, 1.f / length(a)); }
inline float sign(float v) { return v >= 0.f ? 1.f : -1.f; }

// Full-sphere octahedral map, Y up. Must match octa_decode in the shader.
inline V3 octa_decode(float u, float v) {
  float x = u * 2.f - 1.f, z = v * 2.f - 1.f;
  float y = 1.f - std::fabs(x) - std::fabs(z);
  if (y < 0.f) {
    float ox = x;
    x = (1.f - std::fabs(z)) * sign(x);
    z = (1.f - std::fabs(ox)) * sign(z);
  }
  return normalize({x, y, z});
}

// Direction from the centre toward the camera that baked view k.
inline V3 view_dir(const Meta &meta, int k) {
  float last = static_cast<float>(meta.grid - 1);
  return octa_decode(static_cast<float>(k % meta.grid) / last,
                     static_cast<float>(k / meta.grid) / last);
}

// Must match up_for in the shader: the view's up vector, off Y at the poles.
inline V3 up_for(V3 d) {
  return std::fabs(d.y) > 0.999f ? V3{0, 0, 1} : V3{0, 1, 0};
}

// Bilinear filtering pulls in the colour of transparent texels at the
// silhouette, which is black after the clear. Spread edge colour (and the
// matching depth and normal) outward so those texels hold something
// plausible; alpha stays 0. Texels out of reach get each extra's fallback.
struct Extra {
  raylib::Image *image;
  raylib::Color fallback;
};
inline void dilate(raylib::Image &color, std::initializer_list<Extra> extras,
                   int passes) {
  int w = color.width, h = color.height;
  auto *c = static_cast<raylib::Color *>(color.data);
  std::vector<unsigned char> filled(static_cast<size_t>(w * h));
  for (int i = 0; i < w * h; i++) {
    filled[static_cast<size_t>(i)] = c[i].a > 127;
    c[i].a = filled[static_cast<size_t>(i)] ? 255 : 0;
  }
  // Average the filled 8-neighbours of texel i into it, in `img`.
  auto spread = [&](raylib::Color *img, int x, int y, bool keep_alpha) {
    int r = 0, g = 0, b = 0, n = 0;
    for (int dy = -1; dy <= 1; dy++)
      for (int dx = -1; dx <= 1; dx++) {
        int nx = x + dx, ny = y + dy;
        if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
        int j = ny * w + nx;
        if (!filled[static_cast<size_t>(j)]) continue;
        r += img[j].r, g += img[j].g, b += img[j].b, n++;
      }
    if (n == 0) return false;
    raylib::Color &t = img[y * w + x];
    t.r = static_cast<unsigned char>(r / n);
    t.g = static_cast<unsigned char>(g / n);
    t.b = static_cast<unsigned char>(b / n);
    if (!keep_alpha) t.a = 255;
    return true;
  };
  for (int pass = 0; pass < passes; pass++) {
    std::vector<unsigned char> next = filled;
    for (int y = 0; y < h; y++)
      for (int x = 0; x < w; x++) {
        int i = y * w + x;
        if (filled[static_cast<size_t>(i)] || !spread(c, x, y, true)) continue;
        for (const Extra &e : extras)
          spread(static_cast<raylib::Color *>(e.image->data), x, y, false);
        next[static_cast<size_t>(i)] = 1;
      }
    filled.swap(next);
  }
  for (const Extra &e : extras) {
    auto *px = static_cast<raylib::Color *>(e.image->data);
    for (int i = 0; i < w * h; i++)
      if (!filled[static_cast<size_t>(i)]) px[i] = e.fallback;
  }
}

inline void blit(raylib::Image &dst, const raylib::Image &src, int ox,
                 int oy) {
  auto *s = static_cast<const raylib::Color *>(src.data);
  auto *t = static_cast<raylib::Color *>(dst.data);
  for (int y = 0; y < src.height; y++)
    std::memcpy(&t[(oy + y) * dst.width + ox], &s[y * src.width],
                sizeof(raylib::Color) * static_cast<size_t>(src.width));
}

inline raylib::Image read_target(const raylib::RenderTexture2D &rt) {
  raylib::Image img = raylib::LoadImageFromTexture(rt.texture);
  raylib::ImageFlipVertical(&img); // render textures are bottom-up
  raylib::ImageFormat(&img, raylib::PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
  return img;
}

inline const char *DEPTH_VS = R"(#version 330
in vec3 vertexPosition;
uniform mat4 mvp;
uniform mat4 matModel;
out vec3 worldPos;
void main() {
  worldPos = (matModel * vec4(vertexPosition, 1.0)).xyz;
  gl_Position = mvp * vec4(vertexPosition, 1.0);
})";

inline const char *DEPTH_FS = R"(#version 330
in vec3 worldPos;
uniform vec3 center;
uniform vec3 viewDir;
uniform float radius;
out vec4 finalColor;
void main() {
  float h = dot(worldPos - center, viewDir) / (2.0 * radius) + 0.5;
  finalColor = vec4(h, h, h, 1.0);
})";

inline const char *NORMAL_VS = R"(#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
uniform mat4 mvp;
uniform mat4 matNormal;
out vec3 normal;
void main() {
  normal = mat3(matNormal) * vertexNormal;
  gl_Position = mvp * vec4(vertexPosition, 1.0);
})";

inline const char *NORMAL_FS = R"(#version 330
in vec3 normal;
out vec4 finalColor;
void main() { finalColor = vec4(normalize(normal) * 0.5 + 0.5, 1.0); })";

inline const char *CARD_VS = R"(#version 330
in vec3 vertexPosition;
in mat4 instanceTransform;
uniform mat4 mvp;
uniform vec3 camPos;
uniform vec3 center;
uniform float radius;
out vec3 fragPos;
flat out vec3 fragCam;
flat out mat4 fragModel;    // for lighting: model space to world
flat out mat3 fragNormalMat;

vec3 up_for(vec3 d) { return abs(d.y) > 0.999 ? vec3(0, 0, 1) : vec3(0, 1, 0); }

void main() {
  fragModel = instanceTransform;
  fragNormalMat = transpose(inverse(mat3(instanceTransform)));
  // Everything below is in the model's own space, where the bake happened.
  vec3 cam = (inverse(instanceTransform) * vec4(camPos, 1.0)).xyz;
  vec3 to = cam - center;
  float dist = length(to);
  vec3 d = to / dist;
  vec3 r = normalize(cross(up_for(d), d));
  vec3 u = cross(d, r);
  // The quad sits in the plane through the centre, grown to cover the
  // bounding sphere's silhouette from this distance.
  float half_size = radius * dist /
                    sqrt(max(dist * dist - radius * radius, 0.01 * radius * radius));
  // GenMeshPlane lies in XZ facing +Y; x -> r, z -> -u keeps it facing us.
  vec3 p = center + (vertexPosition.x * r - vertexPosition.z * u) * half_size;
  fragPos = p;
  fragCam = cam;
  gl_Position = mvp * instanceTransform * vec4(p, 1.0);
})";

// load_shader assembles: "#version 330", "#define LIT" when lit, CARD_FS_HEAD,
// the game's lighting function when lit, then CARD_FS_BODY.
inline const char *CARD_FS_HEAD = R"(
in vec3 fragPos;
flat in vec3 fragCam;
flat in mat4 fragModel;
flat in mat3 fragNormalMat;
uniform sampler2D texture0; // colour atlas
uniform sampler2D depthTex;
uniform sampler2D normalTex;
uniform vec3 camPos;
uniform vec4 colDiffuse;
uniform vec3 center;
uniform float radius;
uniform int grid;
uniform int tile;
uniform int steps;
out vec4 finalColor;
)";

inline const char *CARD_FS_BODY = R"(
vec3 up_for(vec3 d) { return abs(d.y) > 0.999 ? vec3(0, 0, 1) : vec3(0, 1, 0); }
vec2 sgn(vec2 v) { return vec2(v.x >= 0.0 ? 1.0 : -1.0, v.y >= 0.0 ? 1.0 : -1.0); }

vec3 octa_decode(vec2 uv) {
  vec2 p = uv * 2.0 - 1.0;
  vec3 n = vec3(p.x, 1.0 - abs(p.x) - abs(p.y), p.y);
  if (n.y < 0.0) n.xz = (1.0 - abs(n.zx)) * sgn(n.xz);
  return normalize(n);
}

vec2 octa_encode(vec3 d) {
  d /= abs(d.x) + abs(d.y) + abs(d.z);
  vec2 p = d.xz;
  if (d.y < 0.0) p = (1.0 - abs(d.zx)) * sgn(d.xz);
  return p * 0.5 + 0.5;
}

vec3 view_dir(int k) {
  return octa_decode(vec2(k % grid, k / grid) / float(grid - 1));
}

// Premultiplied colour of the eye ray as baked view k saw it, and where it
// hit (model space) and the normal there. The nearest views are close to the
// camera direction, so a few fixed-point steps converge: intersect the ray
// with the plane at height h along dk, move h to the baked depth there, and
// go again.
vec4 sample_view(int k, vec3 O, vec3 D, out vec3 hit, out vec3 n) {
  hit = vec3(0.0);
  n = vec3(0.0);
  vec3 dk = view_dir(k);
  vec3 rk = normalize(cross(up_for(dk), dk));
  vec3 uk = cross(dk, rk);
  vec2 origin = vec2(k % grid, k / grid);
  float ht = 0.5 / float(tile);
  float denom = dot(D, dk);
  if (abs(denom) < 1e-4) return vec4(0.0);
  float h = 0.0;
  vec2 uv = vec2(0.0);
  vec3 X = vec3(0.0);
  for (int s = 0; s <= steps; s++) {
    float t = (h - dot(O - center, dk)) / denom;
    X = O + t * D - center;
    uv = vec2(0.5 + dot(X, rk) / (2.0 * radius), 0.5 - dot(X, uk) / (2.0 * radius));
    if (s == steps) break;
    vec2 a = (origin + clamp(uv, ht, 1.0 - ht)) / float(grid);
    h = (texture(depthTex, a).r - 0.5) * 2.0 * radius;
  }
  if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) return vec4(0.0);
  vec2 a = (origin + clamp(uv, ht, 1.0 - ht)) / float(grid);
  vec4 c = texture(texture0, a);
#ifdef LIT
  hit = center + X;
  n = texture(normalTex, a).rgb * 2.0 - 1.0;
#endif
  return vec4(c.rgb * c.a, c.a);
}

void main() {
  vec3 O = fragCam;
  vec3 D = normalize(fragPos - fragCam);
  vec3 v = normalize(fragCam - center);
  // The camera direction lands in one cell of the view grid; blend the
  // three corners of the triangle it falls in.
  vec2 g = octa_encode(v) * float(grid - 1);
  vec2 cell = min(floor(g), vec2(grid - 2));
  vec2 f = g - cell;
  int i = int(cell.x), j = int(cell.y);
  int k0, k1, k2;
  vec3 w;
  if (f.x + f.y < 1.0) {
    k0 = j * grid + i; k1 = j * grid + i + 1; k2 = (j + 1) * grid + i;
    w = vec3(1.0 - f.x - f.y, f.x, f.y);
  } else {
    k0 = (j + 1) * grid + i + 1; k1 = (j + 1) * grid + i; k2 = j * grid + i + 1;
    w = vec3(f.x + f.y - 1.0, 1.0 - f.x, 1.0 - f.y);
  }
  vec3 h0, h1, h2, n0, n1, n2;
  vec4 c0 = sample_view(k0, O, D, h0, n0);
  vec4 c1 = sample_view(k1, O, D, h1, n1);
  vec4 c2 = sample_view(k2, O, D, h2, n2);
  vec4 c = w.x * c0 + w.y * c1 + w.z * c2;
  if (c.a < 0.5) discard;
  vec3 albedo = (c.rgb / c.a) * colDiffuse.rgb;
#ifdef LIT
  // Blend hit points and normals the way the colours were, then light.
  vec3 hit = (w.x * c0.a * h0 + w.y * c1.a * h1 + w.z * c2.a * h2) / c.a;
  vec3 n = w.x * c0.a * n0 + w.y * c1.a * n1 + w.z * c2.a * n2;
  vec3 world_pos = (fragModel * vec4(hit, 1.0)).xyz;
  vec3 world_normal = normalize(fragNormalMat * n);
  finalColor = vec4(lighting(albedo, world_pos, world_normal), 1.0);
#else
  finalColor = vec4(albedo, 1.0);
#endif
})";
} // namespace detail

// Renders `model` from every view. Needs a GL context (an open window,
// hidden is fine). Drawn unlit with raylib's default shader. The default,
// 16x16 views of 64px (a 1024px atlas), beat 8x8 views of 128px (same memory)
// on silhouette overlap for the example models, since fewer views ghost more
// between them. tools/house_of_cards compare measures it.
inline Baked bake(const ModelType &model, int grid = 16, int tile = 64) {
  using namespace detail;
  Baked out;
  out.meta.grid = grid < 2 ? 2 : grid;
  out.meta.tile = tile;
  raylib::BoundingBox bb = raylib::GetModelBoundingBox(model);
  out.meta.center = mul(add(bb.min, bb.max), 0.5f);
  out.meta.radius = length(sub(bb.max, bb.min)) * 0.5f;
  if (out.meta.radius <= 0.f) out.meta.radius = 1.f;
  const Meta &m = out.meta;

  out.color = raylib::GenImageColor(m.grid * tile, m.grid * tile,
                                    {0, 0, 0, 0});
  out.depth = raylib::GenImageColor(m.grid * tile, m.grid * tile,
                                    {128, 128, 128, 255});
  out.normal = raylib::GenImageColor(m.grid * tile, m.grid * tile,
                                     {128, 255, 128, 255});

  raylib::RenderTexture2D rt = raylib::LoadRenderTexture(tile, tile);
  raylib::Shader depth_shader =
      raylib::LoadShaderFromMemory(DEPTH_VS, DEPTH_FS);
  raylib::Shader normal_shader =
      raylib::LoadShaderFromMemory(NORMAL_VS, NORMAL_FS);
  int loc_center = raylib::GetShaderLocation(depth_shader, "center");
  int loc_dir = raylib::GetShaderLocation(depth_shader, "viewDir");
  int loc_radius = raylib::GetShaderLocation(depth_shader, "radius");
  raylib::SetShaderValue(depth_shader, loc_center, &m.center,
                         raylib::SHADER_UNIFORM_VEC3);
  raylib::SetShaderValue(depth_shader, loc_radius, &m.radius,
                         raylib::SHADER_UNIFORM_FLOAT);
  std::vector<raylib::Shader> saved(static_cast<size_t>(model.materialCount));

  for (int k = 0; k < m.views(); k++) {
    V3 d = view_dir(m, k);
    raylib::Camera3D cam{};
    cam.position = add(m.center, mul(d, m.radius * 3.f));
    cam.target = m.center;
    cam.up = up_for(d);
    cam.fovy = m.radius * 2.f; // orthographic: height of the view in units
    cam.projection = raylib::CAMERA_ORTHOGRAPHIC;

    // One pass: the model drawn with `shader` (or its own materials) into
    // rt, read back upright.
    auto pass = [&](const raylib::Shader *shader) {
      if (shader)
        for (int i = 0; i < model.materialCount; i++) {
          saved[static_cast<size_t>(i)] = model.materials[i].shader;
          model.materials[i].shader = *shader;
        }
      raylib::BeginTextureMode(rt);
      raylib::ClearBackground({0, 0, 0, 0});
      raylib::BeginMode3D(cam);
      raylib::DrawModel(model, {0, 0, 0}, 1.f, raylib::WHITE);
      raylib::EndMode3D();
      raylib::EndTextureMode();
      if (shader)
        for (int i = 0; i < model.materialCount; i++)
          model.materials[i].shader = saved[static_cast<size_t>(i)];
      return read_target(rt);
    };
    raylib::Image color = pass(nullptr);
    raylib::SetShaderValue(depth_shader, loc_dir, &d,
                           raylib::SHADER_UNIFORM_VEC3);
    raylib::Image depth = pass(&depth_shader);
    raylib::Image normal = pass(&normal_shader);

    dilate(color,
           {{&depth, {128, 128, 128, 255}}, {&normal, {128, 255, 128, 255}}},
           tile / 8);
    int ox = (k % m.grid) * tile, oy = (k / m.grid) * tile;
    blit(out.color, color, ox, oy);
    blit(out.depth, depth, ox, oy);
    blit(out.normal, normal, ox, oy);
    raylib::UnloadImage(color);
    raylib::UnloadImage(depth);
    raylib::UnloadImage(normal);
  }

  raylib::UnloadShader(depth_shader);
  raylib::UnloadShader(normal_shader);
  raylib::UnloadRenderTexture(rt);
  raylib::ImageFormat(&out.depth, raylib::PIXELFORMAT_UNCOMPRESSED_GRAYSCALE);
  raylib::ImageFormat(&out.normal, raylib::PIXELFORMAT_UNCOMPRESSED_R8G8B8);
  return out;
}

inline void unload(Baked &baked) {
  raylib::UnloadImage(baked.color);
  raylib::UnloadImage(baked.depth);
  raylib::UnloadImage(baked.normal);
  baked = {};
}

// Writes <prefix>.png, <prefix>_depth.png, <prefix>_normal.png and
// <prefix>.cards.
inline bool save(const Baked &baked, const std::string &prefix) {
  const Meta &m = baked.meta;
  if (!raylib::ExportImage(baked.color, (prefix + ".png").c_str()) ||
      !raylib::ExportImage(baked.depth, (prefix + "_depth.png").c_str()) ||
      !raylib::ExportImage(baked.normal, (prefix + "_normal.png").c_str()))
    return false;
  FILE *f = std::fopen((prefix + ".cards").c_str(), "w");
  if (!f) return false;
  std::fprintf(f, "grid %d\ntile %d\nradius %f\ncenter %f %f %f\n", m.grid,
               m.tile, static_cast<double>(m.radius),
               static_cast<double>(m.center.x),
               static_cast<double>(m.center.y),
               static_cast<double>(m.center.z));
  std::fclose(f);
  return true;
}

namespace detail {
inline void finish(Cards &cards) {
  for (raylib::Texture2D *t : {&cards.color, &cards.depth, &cards.normal}) {
    raylib::SetTextureFilter(*t, raylib::TEXTURE_FILTER_BILINEAR);
    raylib::SetTextureWrap(*t, raylib::TEXTURE_WRAP_CLAMP);
  }
  cards.quad = raylib::GenMeshPlane(2.f, 2.f, 1, 1);
  cards.material = raylib::LoadMaterialDefault();
  cards.material.maps[raylib::MATERIAL_MAP_DIFFUSE].texture = cards.color;
  cards.material.maps[raylib::MATERIAL_MAP_HEIGHT].texture = cards.depth;
  cards.material.maps[raylib::MATERIAL_MAP_NORMAL].texture = cards.normal;
}
} // namespace detail

inline Cards upload(const Baked &baked) {
  Cards cards;
  cards.meta = baked.meta;
  cards.color = raylib::LoadTextureFromImage(baked.color);
  cards.depth = raylib::LoadTextureFromImage(baked.depth);
  cards.normal = raylib::LoadTextureFromImage(baked.normal);
  detail::finish(cards);
  return cards;
}

// Reads what save() wrote. False, with `out` untouched, if any part is
// missing or the .cards file does not parse.
inline bool load(const std::string &prefix, Cards &out) {
  Cards cards;
  FILE *f = std::fopen((prefix + ".cards").c_str(), "r");
  if (!f) {
    log_error("house_of_cards: cannot open {}.cards", prefix);
    return false;
  }
  int n = std::fscanf(f, "grid %d tile %d radius %f center %f %f %f",
                      &cards.meta.grid, &cards.meta.tile, &cards.meta.radius,
                      &cards.meta.center.x, &cards.meta.center.y,
                      &cards.meta.center.z);
  std::fclose(f);
  if (n != 6 || cards.meta.grid < 2 || cards.meta.radius <= 0.f) {
    log_error("house_of_cards: {}.cards does not parse", prefix);
    return false;
  }
  cards.color = raylib::LoadTexture((prefix + ".png").c_str());
  cards.depth = raylib::LoadTexture((prefix + "_depth.png").c_str());
  cards.normal = raylib::LoadTexture((prefix + "_normal.png").c_str());
  if (cards.color.id == 0 || cards.depth.id == 0 || cards.normal.id == 0) {
    log_error("house_of_cards: missing {}.png, _depth.png or _normal.png",
              prefix);
    raylib::UnloadTexture(cards.color);
    raylib::UnloadTexture(cards.depth);
    raylib::UnloadTexture(cards.normal);
    return false;
  }
  detail::finish(cards);
  out = cards;
  return true;
}

inline void unload(Cards &cards) {
  raylib::UnloadTexture(cards.color);
  raylib::UnloadTexture(cards.depth);
  raylib::UnloadTexture(cards.normal);
  raylib::UnloadMesh(cards.quad);
  // UnloadMaterial would unload the atlas textures a second time.
  raylib::MemFree(cards.material.maps);
  cards = {};
}

// Shared by every Cards; the caller unloads it with raylib::UnloadShader.
// With `lighting` (a GLSL lighting function, see DEFAULT_LIGHTING in
// models.h) cards are lit like real models drawn with load_lit_shader; call
// apply_lighting, or set the function's own uniforms, before drawing.
// Without it they are unlit, like raylib's default shader.
inline ShaderType load_shader(const char *lighting = nullptr) {
  std::string fs = std::string("#version 330\n") +
                   (lighting ? "#define LIT\n" : "") + detail::CARD_FS_HEAD +
                   (lighting ? lighting : "") + detail::CARD_FS_BODY;
  ShaderType shader = raylib::LoadShaderFromMemory(detail::CARD_VS, fs.c_str());
  // raylib 5.5 looks the instance attribute up under MATRIX_MODEL; 5.6 moves
  // it to SHADER_LOC_VERTEX_INSTANCE_TX.
  shader.locs[raylib::SHADER_LOC_MATRIX_MODEL] =
      raylib::GetShaderLocationAttrib(shader, "instanceTransform");
  shader.locs[raylib::SHADER_LOC_MAP_HEIGHT] =
      raylib::GetShaderLocation(shader, "depthTex");
  shader.locs[raylib::SHADER_LOC_MAP_NORMAL] =
      raylib::GetShaderLocation(shader, "normalTex");
  // A failed compile falls back to raylib's default shader, which has no
  // depthTex, and every card would draw as a flat square.
  if (shader.locs[raylib::SHADER_LOC_MAP_HEIGHT] == -1)
    log_error("house_of_cards: card shader failed to compile");
  return shader;
}

// One instanced draw for every copy. `transforms` place the model as they
// would for draw_model_instanced. parallax_steps is how many times each view
// refines the hit with its depth; 0 treats the views as flat cards.
inline void draw_instanced(Cards &cards, ShaderType shader,
                           const MatrixType *transforms, int count,
                           const Camera3D &camera,
                           int parallax_steps = 2) {
  if (count <= 0) return;
  const Meta &m = cards.meta;
  auto set = [&](const char *name, const void *value, int type) {
    raylib::SetShaderValue(shader, raylib::GetShaderLocation(shader, name),
                           value, type);
  };
  set("camPos", &camera.position, raylib::SHADER_UNIFORM_VEC3);
  set("center", &m.center, raylib::SHADER_UNIFORM_VEC3);
  set("radius", &m.radius, raylib::SHADER_UNIFORM_FLOAT);
  set("grid", &m.grid, raylib::SHADER_UNIFORM_INT);
  set("tile", &m.tile, raylib::SHADER_UNIFORM_INT);
  set("steps", &parallax_steps, raylib::SHADER_UNIFORM_INT);
  cards.material.shader = shader;
  raylib::DrawMeshInstanced(cards.quad, cards.material, transforms, count);
}

} // namespace afterhours::house_of_cards

#endif
