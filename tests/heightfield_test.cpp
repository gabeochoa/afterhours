#include <afterhours/src/plugins/heightfield.h>

#include <cmath>
#include <cstdio>

using namespace afterhours;

static int run = 0, passed = 0;
static void check(bool c, const char *e, int line) {
  run++;
  if (c) passed++; else fprintf(stderr, "  FAIL: %s (line %d)\n", e, line);
}
#define CHECK(e) check((e), #e, __LINE__)
#define NEAR(a, b) (std::fabs((a) - (b)) < 1e-4f)

int main() {
  // 3x3, cell=2, origin (10,20): heights row-major.
  Heightfield hf(3, 3, 2.f, {0, 2, 4, 2, 4, 6, 4, 6, 8}, 10.f, 20.f);
  CHECK(hf.valid());
  // Grid points exact.
  CHECK(NEAR(hf.sample_height(10, 20), 0.f));
  CHECK(NEAR(hf.sample_height(12, 20), 2.f));
  CHECK(NEAR(hf.sample_height(14, 24), 8.f));
  // Bilinear midpoints: centre is average of corners (0+2+2+4)/4=2 at (11,21)? gx=.5,gz=.5
  CHECK(NEAR(hf.sample_height(11, 21), 2.f));
  CHECK(NEAR(hf.sample_height(12, 21), 3.f)); // between rows at col 1: (2+4)/2
  // Edge clamp.
  CHECK(NEAR(hf.sample_height(-100, 20), 0.f));
  CHECK(NEAR(hf.sample_height(1000, 1000), 8.f));
  CHECK(NEAR(hf.sample_height(10, -5), 0.f));

  // Analytic slope: plane h = x_world_local i.e. heights = col*cell.
  Heightfield plane(3, 3, 1.f, {0, 1, 2, 0, 1, 2, 0, 1, 2});
  HeightfieldVec3 n = plane.normal_at(1, 1);
  float inv = 1.f / std::sqrt(2.f);
  CHECK(NEAR(n.x, -inv) && NEAR(n.y, inv) && NEAR(n.z, 0.f));
  Heightfield flat(2, 2, 1.f, {5, 5, 5, 5});
  HeightfieldVec3 fn = flat.normal_at(0.5f, 0.5f);
  CHECK(NEAR(fn.x, 0.f) && NEAR(fn.y, 1.f) && NEAR(fn.z, 0.f));
  // Invalid field is safe.
  Heightfield bad;
  CHECK(!bad.valid() && NEAR(bad.sample_height(1, 1), 0.f));
  CHECK(NEAR(bad.normal_at(0, 0).y, 1.f) && bad.build_tile_mesh(0, 0, 1, 1).indices.empty());

  // Tile mesh: whole 3x3 field as one 2x2-cell tile.
  HeightfieldMesh m = hf.build_tile_mesh(0, 0, 2, 2);
  CHECK(m.vertex_cols == 3 && m.vertex_rows == 3);
  CHECK(m.positions.size() == 27 && m.normals.size() == 27 && m.uvs.size() == 18);
  CHECK(m.indices.size() == 24); // 4 quads * 2 tris * 3
  CHECK(NEAR(m.positions[0], 10.f) && NEAR(m.positions[1], 0.f) && NEAR(m.positions[2], 20.f));
  CHECK(NEAR(m.positions[24], 14.f) && NEAR(m.positions[25], 8.f)); // last vertex
  CHECK(NEAR(m.uvs[16], 1.f) && NEAR(m.uvs[17], 1.f));
  bool idx_ok = true;
  for (auto i : m.indices) idx_ok &= i < 9;
  CHECK(idx_ok);
  // Partial/clamped tile: starting at last cell yields empty (no cells), 1x1 cell at (1,1) has 4 verts.
  CHECK(hf.build_tile_mesh(2, 2, 1, 1).indices.empty());
  HeightfieldMesh sub = hf.build_tile_mesh(1, 1, 5, 5);
  CHECK(sub.vertex_cols == 2 && sub.vertex_rows == 2 && sub.indices.size() == 6);
  CHECK(NEAR(sub.positions[0], 12.f) && NEAR(sub.positions[1], 4.f));

  printf("heightfield_test: %d/%d checks passed\n", passed, run);
  return passed == run ? 0 : 1;
}
