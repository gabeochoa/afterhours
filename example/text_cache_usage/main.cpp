#include <cassert>
#include <cstdio>
#include <string_view>

#include "../../src/core/text_cache.h"

int main(int, char **) {
  using namespace afterhours;
  using namespace afterhours::ui;

  TextMeasureCache cache([](std::string_view text, std::string_view,
                            float font_size, float spacing) {
    Vector2Type size;
    size.x =
        static_cast<float>(text.size()) * font_size * 0.5f + spacing * 0.1f;
    size.y = font_size;
    return size;
  });

  auto a = cache.measure(std::string_view("hello"),
                         std::string_view("default"), 16.0f, 1.0f);
  auto b = cache.measure(std::string_view("hello"),
                         std::string_view("default"), 16.0f, 1.0f);
  auto c = cache.measure(std::string_view("world"),
                         std::string_view("default"), 16.0f, 1.0f);

  assert(a.x == b.x && a.y == b.y);
  assert(c.x != 0.0f || c.y != 0.0f);
  assert(cache.hits() == 1);
  assert(cache.misses() == 2);

  cache.end_frame();
  std::printf("text_cache_usage: ok (entries=%zu, hit_rate=%.1f%%)\n",
              cache.size(), cache.hit_rate());
  return 0;
}

