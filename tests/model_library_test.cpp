#include <afterhours/src/model_library.h>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace afterhours;

static int run = 0, passed = 0;
static void check(bool c, const char *what) {
  run++;
  if (c) passed++; else fprintf(stderr, "  FAIL: %s\n", what);
}

static bool near(std::array<float, 3> a, float x, float y, float z) {
  return std::fabs(a[0] - x) < 1e-6f && std::fabs(a[1] - y) < 1e-6f && std::fabs(a[2] - z) < 1e-6f;
}

static const char *kGltf = R"({
  "asset": {"version": "2.0"},
  "accessors": [
    {"componentType": 5126, "count": 3, "type": "VEC3", "min": [-1, 0, 2], "max": [3, 4, 5]},
    {"componentType": 5126, "count": 3, "type": "VEC3", "min": [0, -7, 1], "max": [2, 1, 9]},
    {"componentType": 5126, "count": 3, "type": "VEC3", "min": [-99, -99, -99], "max": [99, 99, 99]}
  ],
  "meshes": [
    {"primitives": [{"attributes": {"POSITION": 0, "NORMAL": 2}}]},
    {"primitives": [{"attributes": {"POSITION": 1}}]}
  ]
})";

static std::vector<std::uint8_t> make_glb(const std::string &json) {
  std::string padded = json;
  while (padded.size() % 4) padded += ' ';
  std::vector<std::uint8_t> out;
  auto u32 = [&](std::uint32_t v) {
    for (int i = 0; i < 4; i++) out.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
  };
  for (char c : std::string("glTF")) out.push_back(static_cast<std::uint8_t>(c));
  u32(2); u32(static_cast<std::uint32_t>(12 + 8 + padded.size()));
  u32(static_cast<std::uint32_t>(padded.size())); u32(0x4E4F534A);
  for (char c : padded) out.push_back(static_cast<std::uint8_t>(c));
  return out;
}

int main() {
  struct FakeTexture { int id; };
  int uploads = 0;
  TextureRegistry<FakeTexture> reg;
  auto upload = [&](std::span<const std::uint8_t>) { return FakeTexture{++uploads}; };
  std::vector<std::uint8_t> a{1, 2, 3}, same_a{1, 2, 3}, b{9, 9};
  const FakeTexture &t1 = reg.get_or_upload(a, upload);
  const FakeTexture &t2 = reg.get_or_upload(same_a, upload);
  check(&t1 == &t2 && uploads == 1 && reg.upload_count == 1, "duplicate texture bytes -> one upload");
  check(reg.load("wood", a, upload).has_value() && uploads == 1, "named load shares hashed texture");
  check(reg.load("stone", b, upload).has_value() && uploads == 2 && reg.size() == 2, "distinct bytes upload once more");
  check(!reg.load("wood", b, upload).has_value(), "duplicate name rejected");
  check(reg.find("wood") == &t1 && reg.find("nope") == nullptr, "find by name");

  auto json_bounds = gltf_bounds_from_bytes(
      {reinterpret_cast<const std::uint8_t *>(kGltf), std::strlen(kGltf)});
  check(json_bounds.has_value(), "gltf JSON bounds parse");
  if (json_bounds) {
    check(near(json_bounds->min, -1, -7, 1), "gltf bounds min exact (union, NORMAL ignored)");
    check(near(json_bounds->max, 3, 4, 9), "gltf bounds max exact");
    check(near(json_bounds->center(), 1, -1.5f, 5), "gltf bounds center");
  }
  auto glb = make_glb(kGltf);
  auto glb_bounds = gltf_bounds_from_bytes(glb);
  check(glb_bounds && near(glb_bounds->min, -1, -7, 1) && near(glb_bounds->max, 3, 4, 9),
        "glb bounds from header chunk exact");

  auto missing = gltf_bounds_from_file("does/not/exist.glb");
  check(!missing && missing.error() == GltfError::FILE_OPEN_FAILED, "missing file -> FILE_OPEN_FAILED");
  std::vector<std::uint8_t> corrupt{'N', 'O', 'P', 'E'};
  auto bad = gltf_bounds_from_bytes(corrupt);
  check(!bad && bad.error() == GltfError::BAD_MAGIC, "corrupt magic -> BAD_MAGIC");
  glb.resize(16);
  auto trunc = gltf_bounds_from_bytes(glb);
  check(!trunc && trunc.error() == GltfError::TRUNCATED, "truncated glb -> TRUNCATED");
  const char *no_pos = R"({"accessors": [], "meshes": [{"primitives": [{"attributes": {}}]}]})";
  auto none = gltf_bounds_from_bytes({reinterpret_cast<const std::uint8_t *>(no_pos), std::strlen(no_pos)});
  check(!none && none.error() == GltfError::NO_POSITION_BOUNDS, "no POSITION -> NO_POSITION_BOUNDS");

  auto path = std::filesystem::temp_directory_path() / "afterhours_r204_fixture.glb";
  { std::ofstream out(path, std::ios::binary); auto full = make_glb(kGltf); out.write(reinterpret_cast<const char *>(full.data()), static_cast<std::streamsize>(full.size())); }
  auto from_file = gltf_bounds_from_file(path.string().c_str());
  check(from_file && near(from_file->max, 3, 4, 9), "glb bounds from file");
  std::error_code ec; std::filesystem::remove(path, ec);

  printf("model_library_test: %d/%d checks passed\n", passed, run);
  return passed == run ? 0 : 1;
}
