#include <cstdio>
#include <string>
#include <afterhours/ah.h>
#include <afterhours/src/graphics.h>
#include <afterhours/src/plugins/effects.h>
using namespace afterhours;
static int run = 0, passed = 0;
static void check(bool c, const std::string &w) { run++; if (c) passed++; else fprintf(stderr, "  FAIL: %s\n", w.c_str()); }
int main() {
  raylib::SetTraceLogLevel(raylib::LOG_ERROR);
  afterhours::graphics::Config cfg;
  cfg.display = afterhours::graphics::DisplayMode::Headless;
  cfg.width = 64;
  cfg.height = 64;
  if (!afterhours::graphics::init(cfg)) {
    fprintf(stderr, "scene_lights_test: headless graphics init failed\n");
    return 1;
  }
  check(std::string(effects::SCENE_LIGHTS_GLSL).find("ah_apply_scene_lights") != std::string::npos, "chunk ships helper");
  const std::string fs = "#version 330\nin vec2 fragTexCoord;\nin vec4 fragColor;\nout vec4 finalColor;\n" +
      std::string(effects::SCENE_LIGHTS_GLSL) +
      "void main(){ finalColor = vec4(ah_apply_scene_lights(vec3(1.0), vec3(0.0), vec3(0.0,0.0,1.0)), 1.0); }\n";
  effects::Effect effect;
  effect.shader = raylib::LoadShaderFromMemory(nullptr, fs.c_str());
  effect.loaded = effect.shader.id != 0;
  check(effect.ok(), "test shader compiles with chunk");
  effects::Effect::registry().push_back(&effect);
  auto pixel = [&]() {
    raylib::RenderTexture2D rt = raylib::LoadRenderTexture(16, 16);
    raylib::BeginTextureMode(rt); raylib::BeginShaderMode(effect.shader);
    raylib::DrawRectangle(0, 0, 16, 16, raylib::WHITE); raylib::EndShaderMode(); raylib::EndTextureMode();
    raylib::Image img = raylib::LoadImageFromTexture(rt.texture);
    raylib::ImageFormat(&img, raylib::PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    auto *px = static_cast<raylib::Color *>(img.data); raylib::Color c = px[0];
    raylib::UnloadImage(img); raylib::UnloadRenderTexture(rt); return c;
  };
  effects::SceneLights dark; dark.sun = {0,0,0,255}; dark.ambient = {0,0,0,255};
  check(effects::apply_scene_lights(dark) == 1, "broadcast hits one registered shader");
  check(pixel().r < 10, "dark lights render dark");
  effects::SceneLights bright = dark; bright.sun_direction = {0,0,-1}; bright.sun = {255,255,255,255};
  effects::apply_scene_lights(bright);
  check(pixel().r > 200, "bright sun renders bright (uniform readback via pixel)");
  printf("scene_lights: %d/%d checks passed\n", passed, run);
  afterhours::graphics::shutdown();
  return passed == run ? 0 : 1;
}
