// effects_uniform_cache_test.cpp
// Effect::set must not re-upload an unchanged uniform value, and must
// re-upload after a shader reload (locations and values both reset).
//
// Opens a hidden window: SetShaderValue needs a GL context.

#include <cstdio>
#include <fstream>
#include <string>

#include <afterhours/ah.h>
#include <afterhours/src/plugins/effects.h>

using namespace afterhours;

static int checks_run = 0;
static int checks_passed = 0;

static void check(bool cond, const std::string &what) {
  checks_run++;
  if (cond) {
    checks_passed++;
  } else {
    fprintf(stderr, "  FAIL: %s\n", what.c_str());
  }
}

static const char *VS = R"(#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
out vec2 fragTexCoord;
void main() { fragTexCoord = vertexTexCoord; gl_Position = vec4(vertexPosition, 1.0); })";
static const char *FS = R"(#version 330
in vec2 fragTexCoord;
uniform float uFloat;
uniform vec2 uVec2;
uniform vec4 uColor;
out vec4 finalColor;
void main() { finalColor = uColor * uFloat + vec4(uVec2, 0.0, 1.0); })";

int main() {
  raylib::SetTraceLogLevel(raylib::LOG_ERROR);
  raylib::SetConfigFlags(raylib::FLAG_WINDOW_HIDDEN);
  raylib::InitWindow(64, 64, "effects_uniform_cache_test");

  effects::Effect e;
  e.shader = raylib::LoadShaderFromMemory(VS, FS);
  e.loaded = e.shader.id != 0;
  check(e.loaded, "test shader compiles");

  // Same float 10x: one upload, not ten.
  for (int i = 0; i < 10; i++) e.set("uFloat", 1.5f);
  printf("  float x10 uploads=%d\n", e.upload_count);
  check(e.upload_count == 1, "unchanged float uploads once");

  // Changed value uploads again, then skips.
  e.set("uFloat", 2.5f);
  e.set("uFloat", 2.5f);
  printf("  float changed+repeat uploads=%d\n", e.upload_count);
  check(e.upload_count == 2, "changed float uploads, repeat skips");

  // Same vec2 10x: one upload.
  for (int i = 0; i < 10; i++) e.set("uVec2", Vector2Type{3.f, 4.f});
  printf("  vec2 x10 uploads=%d\n", e.upload_count);
  check(e.upload_count == 3, "unchanged vec2 uploads once");

  // Same colour 10x: one upload.
  for (int i = 0; i < 10; i++) e.set("uColor", ColorType{10, 20, 30, 255});
  printf("  color x10 uploads=%d\n", e.upload_count);
  check(e.upload_count == 4, "unchanged colour uploads once");

  // Locations are looked up once per uniform, however many sets.
  check(e.locations.size() == 3, "location cache holds one entry per uniform");

  // Reload clears both caches, so the same value uploads again.
  {
    const std::string path = "/tmp/effects_uniform_cache_test.fs";
    { std::ofstream out(path); out << FS; }
    e.path = path;
    e.reload();
    check(e.ok(), "reload loads the written shader");
    check(e.locations.empty(), "reload clears the location cache");
    const int before = e.upload_count;
    e.set("uFloat", 2.5f);
    printf("  after reload uploads=%d (before=%d)\n", e.upload_count, before);
    check(e.upload_count == before + 1, "same value re-uploads after reload");
  }

  raylib::CloseWindow();
  printf("effects_uniform_cache_test: %d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
