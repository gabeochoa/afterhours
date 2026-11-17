#include <cstdio>

#include "../../../../src/frame_rate_stats.h"

int main() {
  afterhours::FrameRateStats stats(2.f);
  int lines = 0;

  for (int frame = 0; frame < 600; frame++) {
    const float dt = frame % 100 == 99 ? 0.1f : 1.f / 120.f;
    if (stats.add(dt)) {
      printf("[fps] avg %.1f fps (%.2f ms), worst %.1f ms over 2s\n",
             stats.fps(), stats.avg_frame_ms(), stats.max_frame_ms());
      lines++;
    }
  }
  return lines > 0 ? 0 : 1;
}
