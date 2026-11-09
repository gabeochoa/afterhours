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
#include "../library.h"
#include "../logging.h"
#include "../singleton.h"
#include "color.h"

#ifdef AFTER_HOURS_USE_RAYLIB
// TODO(backend-agnostic): plugins should not reach into raylib. Move the
// instanced-draw cleanup behind a backend call and drop this include.
// For tidying vertex state after instanced draws; same convention as
// graphics_common.h.
namespace raylib {
#include <rlgl.h>
} // namespace raylib
#endif

namespace afterhours {

#ifdef AFTER_HOURS_USE_RAYLIB
using ModelType = raylib::Model;

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

// Sets every material's shader. The caller still owns the shader: unloading
// the model does not unload it.
inline void set_model_shader(ModelType &model, raylib::Shader shader) {
  for (int i = 0; i < model.materialCount; i++)
    model.materials[i].shader = shader;
}

// raylib's default shader has no per-instance transform, so
// draw_model_instanced needs this one (or any shader reading the
// instanceTransform attribute). Unlit, diffuse texture times colour, the same
// as the default. GLSL 330: desktop GL only. The caller unloads it with
// raylib::UnloadShader.
inline raylib::Shader load_instancing_shader() {
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
  raylib::Shader shader = raylib::LoadShaderFromMemory(vs, fs);
  // raylib 5.5 looks the instance attribute up under MATRIX_MODEL; 5.6 moves
  // it to SHADER_LOC_VERTEX_INSTANCE_TX.
  shader.locs[raylib::SHADER_LOC_MATRIX_MODEL] =
      raylib::GetShaderLocationAttrib(shader, "instanceTransform");
  return shader;
}

// One DrawMeshInstanced per mesh instead of one DrawModelEx per copy. The
// model's materials are used with their shader swapped for `shader`, without
// modifying the model. model.transform is not applied; raylib bakes glTF node
// transforms into the meshes, so it is identity for loaded files.
inline void draw_model_instanced(const ModelType &model, raylib::Shader shader,
                                 const raylib::Matrix *transforms, int count) {
  if (count <= 0) return;
  int loc = shader.locs[raylib::SHADER_LOC_MATRIX_MODEL];
  for (int i = 0; i < model.meshCount; i++) {
    raylib::Material material = model.materials[model.meshMaterial[i]];
    material.shader = shader;
    raylib::DrawMeshInstanced(model.meshes[i], material, transforms, count);
    // DrawMeshInstanced leaves the four instance-matrix attributes enabled on
    // the mesh's VAO, pointing at the buffer it just deleted. Until they are
    // off again, draw_model of this model draws nothing.
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
inline raylib::Matrix model_transform(Vector3Type pos, float yaw_deg,
                                      Vector3Type scale) {
  float a = yaw_deg * DEG2RAD;
  float c = std::cos(a), s = std::sin(a);
  raylib::Matrix m{};
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
  void add(const ModelType &model, const raylib::Matrix &transform) {
    group_for(model).transforms.push_back(transform);
  }
  void add(const ModelType &model, Vector3Type pos, float yaw_deg,
           Vector3Type scale) {
    add(model, model_transform(pos, yaw_deg, scale));
  }

  // `shader` must read the instanceTransform attribute, as
  // load_instancing_shader's does.
  void flush(raylib::Shader shader) {
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
    std::vector<raylib::Matrix> transforms;
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
using ModelType = ModelStub;

inline ModelType load_model(const char *) { return {}; }
inline void unload_model(ModelType) {}
template <typename V>
inline void draw_model(const ModelType &, V, float, V,
                       Color = {255, 255, 255, 255}) {}
template <typename S> inline void set_model_shader(ModelType &, S) {}
inline ShaderStub load_instancing_shader() { return {}; }
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
