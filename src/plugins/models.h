#pragma once

// 3D models: load by name, draw with a position / yaw / scale, draw many
// copies in one call per mesh (or batch them by model), swap shaders.
//
//   ModelLibrary::get().load("resources/tree.glb", "tree");
//   draw_model(ModelLibrary::get().get("tree"), pos, yaw_deg, {1, 1, 1});
//
// raylib only. The none and sokol backends get a stub whose loads always
// fail, so ModelLibrary::load returns false there instead of drawing nothing.

#include <cmath>
#include <string>
#include <vector>

#include "../developer.h"
#include "../graphics_common.h"
#include "../library.h"
#include "../logging.h"
#include "../singleton.h"
#include "color.h"

namespace afterhours {

// A model's axis-aligned bounds, e.g. to frame a camera, space copies apart,
// or test against a Frustum.
struct Bounds {
  Vector3Type min{}, max{};
  [[nodiscard]] Vector3Type center() const {
    return {(min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f,
            (min.z + max.z) * 0.5f};
  }
  [[nodiscard]] Vector3Type size() const {
    return {max.x - min.x, max.y - min.y, max.z - min.z};
  }
  // Radius of the sphere around center() that holds the box.
  [[nodiscard]] float radius() const {
    Vector3Type d = size();
    return 0.5f * std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
  }
};

// Lighting is a GLSL function the game supplies, so real models and
// house_of_cards impostors can be lit by the same code:
//
//   vec3 lighting(vec3 albedo, vec3 world_pos, vec3 world_normal);
//
// It may declare its own uniforms; `camPos` (the camera, in world space) is
// always declared for it. DEFAULT_LIGHTING is one directional light, ambient,
// and linear distance fog, driven by a Lighting through apply_lighting().
inline const char *DEFAULT_LIGHTING = R"(
uniform vec3 lightDir;     // direction the light travels, world space
uniform vec3 lightColor;
uniform vec3 ambientColor;
uniform vec3 fogColor;
uniform float fogNear;     // fog starts here...
uniform float fogFar;      // ...and is solid here; fogFar <= fogNear is off
vec3 lighting(vec3 albedo, vec3 world_pos, vec3 world_normal) {
  float diffuse = max(dot(world_normal, -normalize(lightDir)), 0.0);
  vec3 lit = albedo * (ambientColor + lightColor * diffuse);
  if (fogFar <= fogNear) return lit;
  float fog = clamp((distance(world_pos, camPos) - fogNear) / (fogFar - fogNear),
                    0.0, 1.0);
  return mix(lit, fogColor, fog);
}
)";

// Values for DEFAULT_LIGHTING's uniforms.
struct Lighting {
  Vector3Type direction{-0.4f, -1.f, -0.3f}; // where the light travels
  Color light{255, 245, 230, 255};
  Color ambient{110, 120, 140, 255};
  Color fog{135, 190, 230, 255};
  float fog_near = 0.f;
  float fog_far = 0.f; // <= fog_near: no fog
};

#ifdef AFTER_HOURS_USE_RAYLIB
using ModelType = raylib::Model;
using MatrixType = raylib::Matrix;
using graphics::ShaderType;

inline ModelType load_model(const char *filename) {
  return raylib::LoadModel(filename);
}
inline void unload_model(ModelType model) { raylib::UnloadModel(model); }

// Draws about the Y axis, which is what games with a ground plane want. The
// default tint is pure white: colors::UI_WHITE is RAYWHITE under raylib and
// would darken the model.
inline void draw_model(const ModelType &model, Vector3Type pos, float yaw_deg,
                       Vector3Type scale, Color tint = {255, 255, 255, 255}) {
  raylib::DrawModelEx(model, pos, {0.f, 1.f, 0.f}, yaw_deg, scale, tint);
}

// Axis-aligned bounds in the model's own space.
inline Bounds model_bounds(const ModelType &model) {
  raylib::BoundingBox b = raylib::GetModelBoundingBox(model);
  return {b.min, b.max};
}

inline int model_triangle_count(const ModelType &model) {
  int n = 0;
  for (int i = 0; i < model.meshCount; i++) n += model.meshes[i].triangleCount;
  return n;
}

// Sets every material's shader. The caller still owns the shader: unloading
// the model does not unload it.
inline void set_model_shader(ModelType &model, ShaderType shader) {
  for (int i = 0; i < model.materialCount; i++)
    model.materials[i].shader = shader;
}

// raylib's default shader has no per-instance transform, so
// draw_model_instanced needs this one (or any shader reading the
// instanceTransform attribute). Unlit, diffuse texture times colour, the same
// as the default. GLSL 330: desktop GL only. The caller unloads it with
// raylib::UnloadShader.
inline ShaderType load_instancing_shader() {
  const char *vs = R"(#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in mat4 instanceTransform;
uniform mat4 mvp;
out vec2 fragTexCoord;
void main() {
  fragTexCoord = vertexTexCoord;
  gl_Position = mvp * instanceTransform * vec4(vertexPosition, 1.0);
})";
  const char *fs = R"(#version 330
in vec2 fragTexCoord;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
out vec4 finalColor;
void main() { finalColor = texture(texture0, fragTexCoord) * colDiffuse; })";
  ShaderType shader = raylib::LoadShaderFromMemory(vs, fs);
  // raylib 5.5 looks the instance attribute up under MATRIX_MODEL; 5.6 moves
  // it to SHADER_LOC_VERTEX_INSTANCE_TX.
  shader.locs[raylib::SHADER_LOC_MATRIX_MODEL] =
      raylib::GetShaderLocationAttrib(shader, "instanceTransform");
  return shader;
}

// A shader for draw_model that lights each pixel with `lighting` (see
// DEFAULT_LIGHTING). Put it on a model with set_model_shader; call
// apply_lighting (or set your own uniforms) before drawing.
inline ShaderType load_lit_shader(const char *lighting = DEFAULT_LIGHTING) {
  const char *vs = R"(#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;
uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
out vec3 fragPos;
out vec3 fragNormal;
out vec2 fragTexCoord;
out vec4 fragColor;
void main() {
  fragPos = (matModel * vec4(vertexPosition, 1.0)).xyz;
  fragNormal = normalize(mat3(matNormal) * vertexNormal);
  fragTexCoord = vertexTexCoord;
  fragColor = vertexColor;
  gl_Position = mvp * vec4(vertexPosition, 1.0);
})";
  std::string fs = std::string(R"(#version 330
in vec3 fragPos;
in vec3 fragNormal;
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 camPos;
out vec4 finalColor;
)") + lighting + R"(
void main() {
  vec4 albedo = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
  finalColor = vec4(lighting(albedo.rgb, fragPos, normalize(fragNormal)), albedo.a);
})";
  ShaderType shader = raylib::LoadShaderFromMemory(vs, fs.c_str());
  if (raylib::GetShaderLocation(shader, "camPos") == -1)
    log_error("load_lit_shader: shader failed to compile");
  return shader;
}

namespace detail_models {
inline raylib::Vector3 rgb(Color c) {
  return {c.r / 255.f, c.g / 255.f, c.b / 255.f};
}
inline void set_uniform(ShaderType shader, const char *name, const void *v,
                        int type) {
  int loc = raylib::GetShaderLocation(shader, name);
  if (loc != -1) raylib::SetShaderValue(shader, loc, v, type);
}
} // namespace detail_models

// Sets DEFAULT_LIGHTING's uniforms, plus camPos, on any shader built with it
// (load_lit_shader, house_of_cards::load_shader).
inline void apply_lighting(ShaderType shader, const Lighting &l,
                           Vector3Type camera_pos) {
  using namespace detail_models;
  raylib::Vector3 ld{l.direction.x, l.direction.y, l.direction.z};
  raylib::Vector3 cp{camera_pos.x, camera_pos.y, camera_pos.z};
  raylib::Vector3 lc = rgb(l.light), ac = rgb(l.ambient), fc = rgb(l.fog);
  set_uniform(shader, "lightDir", &ld, raylib::SHADER_UNIFORM_VEC3);
  set_uniform(shader, "lightColor", &lc, raylib::SHADER_UNIFORM_VEC3);
  set_uniform(shader, "ambientColor", &ac, raylib::SHADER_UNIFORM_VEC3);
  set_uniform(shader, "fogColor", &fc, raylib::SHADER_UNIFORM_VEC3);
  set_uniform(shader, "fogNear", &l.fog_near, raylib::SHADER_UNIFORM_FLOAT);
  set_uniform(shader, "fogFar", &l.fog_far, raylib::SHADER_UNIFORM_FLOAT);
  set_uniform(shader, "camPos", &cp, raylib::SHADER_UNIFORM_VEC3);
}

// One DrawMeshInstanced per mesh instead of one DrawModelEx per copy. The
// model's materials are used with their shader swapped for `shader`, without
// modifying the model. model.transform is not applied; raylib bakes glTF node
// transforms into the meshes, so it is identity for loaded files.
inline void draw_model_instanced(const ModelType &model, ShaderType shader,
                                 const MatrixType *transforms, int count) {
  if (count <= 0) return;
  int loc = shader.locs[raylib::SHADER_LOC_MATRIX_MODEL];
  for (int i = 0; i < model.meshCount; i++) {
    raylib::Material material = model.materials[model.meshMaterial[i]];
    material.shader = shader;
    raylib::DrawMeshInstanced(model.meshes[i], material, transforms, count);
    // DrawMeshInstanced leaves the four instance-matrix attributes enabled on
    // the mesh's VAO, pointing at the buffer it just deleted. Until they are
    // off again, draw_model of this model draws nothing.
    // TODO(backend-agnostic): plugins should not call rlgl. Move this cleanup
    // behind a backend call.
    if (loc < 0 || !raylib::rlEnableVertexArray(model.meshes[i].vaoId))
      continue;
    for (int c = 0; c < 4; c++) {
      raylib::rlDisableVertexAttribute(static_cast<unsigned int>(loc + c));
      raylib::rlSetVertexAttributeDivisor(static_cast<unsigned int>(loc + c), 0);
    }
    raylib::rlDisableVertexArray();
  }
}

// The matrix draw_model builds for one copy: scale, rotate about Y, then move
// to pos. For filling draw_model_instanced or InstanceBatcher without raymath.
inline MatrixType model_transform(Vector3Type pos, float yaw_deg,
                                      Vector3Type scale) {
  float a = yaw_deg * DEG2RAD;
  float c = std::cos(a), s = std::sin(a);
  MatrixType m{};
  m.m0 = c * scale.x, m.m4 = 0.f, m.m8 = s * scale.z, m.m12 = pos.x;
  m.m1 = 0.f, m.m5 = scale.y, m.m9 = 0.f, m.m13 = pos.y;
  m.m2 = -s * scale.x, m.m6 = 0.f, m.m10 = c * scale.z, m.m14 = pos.z;
  m.m15 = 1.f;
  return m;
}

// Collects one transform per copy, grouped by model, and draws each group
// with one draw_model_instanced. Call add() while walking the entities, then
// flush() once a frame. flush() empties the groups but keeps their storage,
// so a steady scene stops allocating after its first frame.
//
// Groups are keyed by the model's address; ModelLibrary models keep theirs
// while loaded. Copies that need a different shader or uniforms go in a
// second batcher.
struct InstanceBatcher {
  void add(const ModelType &model, const MatrixType &transform) {
    group_for(model).transforms.push_back(transform);
  }
  void add(const ModelType &model, Vector3Type pos, float yaw_deg,
           Vector3Type scale) {
    add(model, model_transform(pos, yaw_deg, scale));
  }

  // `shader` must read the instanceTransform attribute, as
  // load_instancing_shader's does.
  void flush(ShaderType shader) {
    for (Group &g : groups) {
      draw_model_instanced(*g.model, shader, g.transforms.data(),
                           static_cast<int>(g.transforms.size()));
      g.transforms.clear();
    }
  }

  void clear() {
    for (Group &g : groups) g.transforms.clear();
  }

  [[nodiscard]] size_t copies() const {
    size_t n = 0;
    for (const Group &g : groups) n += g.transforms.size();
    return n;
  }
  // Groups with at least one copy waiting: the draw calls flush() will make
  // per mesh.
  [[nodiscard]] size_t group_count() const {
    size_t n = 0;
    for (const Group &g : groups) n += g.transforms.empty() ? 0 : 1;
    return n;
  }

private:
  struct Group {
    const ModelType *model;
    std::vector<MatrixType> transforms;
  };
  // A scene has few distinct models, so a linear find beats hashing, and
  // groups draw in the order they were first seen.
  // TODO: not true for every game. A scene with hundreds of distinct models
  // makes each add() a long scan; measure, and consider a map from model to
  // group index (keeping first-seen draw order) or remembering the last group.
  std::vector<Group> groups;

  Group &group_for(const ModelType &model) {
    for (Group &g : groups)
      if (g.model == &model) return g;
    groups.push_back({&model, {}});
    return groups.back();
  }
};
#else
struct ModelStub {
  int meshCount = 0;
};
struct ShaderStub {};
struct MatrixStub {
  float m[16] = {};
};
using ModelType = ModelStub;
using MatrixType = MatrixStub;

inline Bounds model_bounds(const ModelType &) { return {}; }
inline int model_triangle_count(const ModelType &) { return 0; }

inline ModelType load_model(const char *) { return {}; }
inline void unload_model(ModelType) {}
template <typename V>
inline void draw_model(const ModelType &, V, float, V,
                       Color = {255, 255, 255, 255}) {}
template <typename S> inline void set_model_shader(ModelType &, S) {}
inline ShaderStub load_instancing_shader() { return {}; }
inline ShaderStub load_lit_shader(const char * = DEFAULT_LIGHTING) { return {}; }
template <typename S, typename V>
inline void apply_lighting(S, const Lighting &, V) {}
template <typename S, typename M>
inline void draw_model_instanced(const ModelType &, S, const M *, int) {}
struct InstanceBatcher {
  template <typename M> void add(const ModelType &, const M &) {}
  template <typename V> void add(const ModelType &, V, float, V) {}
  template <typename S> void flush(S) {}
  void clear() {}
  [[nodiscard]] size_t copies() const { return 0; }
  [[nodiscard]] size_t group_count() const { return 0; }
};
#endif

// raylib logs a warning and returns a model with no meshes rather than
// failing, so "loaded" has to be checked by hand.
inline bool is_model_loaded(const ModelType &model) {
  return model.meshCount > 0;
}

SINGLETON_FWD(ModelLibrary)
struct ModelLibrary {
  SINGLETON(ModelLibrary)

  // False, with nothing stored, when the file does not load or the name is
  // taken. Library::load would store the empty model and report success.
  bool load(const char *filename, const char *name) {
    if (impl.contains(name)) {
      log_warn("ModelLibrary: '{}' is already loaded, ignoring {}", name,
               filename);
      return false;
    }
    ModelType model = load_model(filename);
    if (!is_model_loaded(model)) {
      log_error("ModelLibrary: failed to load '{}' from {}", name, filename);
      unload_model(model);
      return false;
    }
    (void)impl.add(name, model);
    return true;
  }

  [[nodiscard]] ModelType &get(const std::string &name) {
    return impl.get(name);
  }
  [[nodiscard]] const ModelType &get(const std::string &name) const {
    return impl.get(name);
  }
  [[nodiscard]] bool contains(const std::string &name) const {
    return impl.contains(name);
  }
  [[nodiscard]] size_t size() const { return impl.size(); }

  // Library::unload_all frees the models but keeps the entries; clearing them
  // means a later get() fails loudly instead of handing back freed GPU data.
  void unload_all() {
    impl.unload_all();
    impl.storage.clear();
  }

private:
  struct ModelLibraryImpl : Library<ModelType> {
    ModelType convert_filename_to_object(const char *,
                                         const char *filename) override {
      return load_model(filename);
    }
    void unload(ModelType model) override { unload_model(model); }
  } impl;
};

} // namespace afterhours
