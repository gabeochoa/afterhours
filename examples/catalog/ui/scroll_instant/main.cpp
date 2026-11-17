#define AFTER_HOURS_USE_NONE

#include <cstdio>

#include "../../../../src/plugins/animation/track.h"
#include "../../../../src/plugins/ui/components.h"

using afterhours::ui::HasScrollView;

int main() {
  HasScrollView view;
  view.content_size = {0, 1000};
  view.viewport_size = {0, 200};
  view.scroll_smoothing = 0.25f;
  view.scroll_target.y = 400.f;
  view.ease_scroll(1.f / 60.f);
  std::printf("gliding: offset %.1f of target %.1f\n", view.scroll_offset.y,
              view.scroll_target.y);
  if (view.scroll_offset.y <= 0.f || view.scroll_offset.y >= 400.f)
    return 1;

  afterhours::motion::set_instant(true);
  view.ease_scroll(1.f / 60.f);
  std::printf("instant: offset %.1f of target %.1f\n", view.scroll_offset.y,
              view.scroll_target.y);
  afterhours::motion::set_instant(false);
  return view.scroll_offset.y == 400.f ? 0 : 1;
}
