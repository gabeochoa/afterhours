#include <afterhours/src/plugins/particles.h>

#include <cmath>
#include <cstdio>
#include <string>

using namespace afterhours::particles;

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

int main() {
  printf("Running particles tests...\n\n");

  {
    Emitter<8> em;
    for (int i = 0; i < 8; ++i) {
      auto &p = em.spawn();
      p.life = 1.f;
    }
    check(em.count() == 8, "pool fills to capacity");
    auto &extra = em.spawn();
    extra.life = 5.f;
    check(em.count() == 8, "spawning past capacity recycles instead of growing");
  }

  {
    Emitter<16> em;
    em.gravity = {0.f, 1000.f};
    em.floor_y = 100.f;
    auto &p = em.spawn();
    p.pos = {0.f, 0.f};
    p.size = 4.f;
    p.life = 10.f;
    float peak_y = 0.f;
    for (int i = 0; i < 120; ++i) {
      em.update(1.f / 60.f);
      peak_y = std::max(peak_y, p.pos.y);
    }
    check(peak_y <= 100.f + 1e-3f, "a particle never passes the floor");
    check(p.pos.y > 50.f, "gravity pulls it down toward the floor");
  }

  {
    Emitter<4> em;
    auto &p = em.spawn();
    p.life = 0.1f;
    em.update(0.025f);
    em.update(0.025f);
    check(em.count() == 1 && std::fabs(p.progress() - 0.5f) < 1e-4f, "progress is age over life");
    em.update(0.03f);
    em.update(0.03f);
    check(em.count() == 0, "a particle dies when its age passes its life");
  }

  check(hash01(1) != hash01(2) && hash01(7) >= 0.f && hash01(7) <= 1.f, "hash01 is in range and varies");

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
