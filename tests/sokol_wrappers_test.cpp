#if !defined(AFTER_HOURS_USE_RAYLIB) && !defined(AFTER_HOURS_USE_VULKAN) && !defined(AFTER_HOURS_USE_SOKOL)
#define AFTER_HOURS_USE_SOKOL
#endif

#include <afterhours/src/graphics.h>
#include <afterhours/src/backends/sokol/drawing_helpers.h>
#include <afterhours/src/core/key_codes.h>

#include <cstdio>
#include <string>

namespace g = afterhours::graphics;
namespace keys = afterhours::keys;

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

static void key_down(int key, bool repeat = false) {
  sapp_event ev{};
  ev.type = SAPP_EVENTTYPE_KEY_DOWN;
  ev.key_code = static_cast<sapp_keycode>(key);
  ev.key_repeat = repeat;
  g::metal_detail::sokol_event_cb(&ev);
}

static sg_sampler_desc sampler_of(const TextureType &t) {
  return sg_query_sampler_desc({t.sampler_id});
}

int main() {
  printf("=== sokol wrappers ===\n\n");

  key_down(keys::A);
  key_down(keys::B);
  key_down(keys::A, true);
  check(g::get_key_pressed() == keys::A, "first key pressed comes out first");
  check(g::get_key_pressed() == keys::B, "then the second");
  check(g::get_key_pressed() == keys::NULL_KEY, "a held key is not queued again");
  key_down(keys::C);
  g::metal_detail::input_begin_frame();
  check(g::get_key_pressed() == keys::NULL_KEY, "the queue is per frame");

  g::Config cfg;
  cfg.display = g::DisplayMode::Headless;
  cfg.width = 64;
  cfg.height = 64;
  cfg.title = "wrappers test";
  if (!g::init(cfg)) {
    printf("SKIP: headless Metal init failed (no GPU available)\n");
    return 0;
  }

  const unsigned char px[16] = {255, 0, 0, 255, 0, 255, 0, 255,
                                0, 0, 255, 255, 255, 255, 255, 255};
  TextureType tex = afterhours::metal_texture_detail::load_texture_from_pixels(px, 2, 2);
  check(sampler_of(tex).wrap_u == SG_WRAP_CLAMP_TO_EDGE, "textures load clamped");
  afterhours::set_texture_wrap(tex, afterhours::TEXTURE_WRAP_REPEAT);
  check(sampler_of(tex).wrap_u == SG_WRAP_REPEAT &&
            sampler_of(tex).wrap_v == SG_WRAP_REPEAT,
        "set_texture_wrap repeats on both axes");
  afterhours::set_texture_filter(tex, afterhours::TEXTURE_FILTER_TRILINEAR);
  check(sampler_of(tex).wrap_u == SG_WRAP_REPEAT,
        "set_texture_filter keeps the wrap");
  afterhours::set_texture_wrap(tex, afterhours::TEXTURE_WRAP_MIRROR_REPEAT);
  check(sampler_of(tex).wrap_u == SG_WRAP_MIRRORED_REPEAT &&
            sampler_of(tex).mipmap_filter == SG_FILTER_LINEAR,
        "set_texture_wrap keeps the trilinear filter");
  afterhours::unload_texture(tex);

  afterhours::Font font = afterhours::load_font_from_file(
      AFTERHOURS_TEST_FIXTURES "/AtkinsonHyperlegible-Regular.ttf");
  check(font.id != FONS_INVALID, "fixture font loads");

  afterhours::capture::clear();
  afterhours::capture::enable();
  g::begin_drawing();
  afterhours::draw_fps(4, 8);
  g::end_drawing();
  afterhours::capture::disable();
  bool drew = false;
  for (const auto &c : afterhours::capture::calls())
    if (c.op == "text" && c.text == "60 FPS" && c.rect.x == 4 && c.rect.y == 8 &&
        c.color.g == 158)
      drew = true;
  check(drew, "draw_fps draws \"60 FPS\" in raylib's lime at 20px");

  afterhours::unload_font(font);
  check(font.id == FONS_INVALID, "unload_font clears the handle");

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
