// The render path asks for the same string's size every frame. Measured on
// wm: 995 requests a frame on one screen, 101 of them distinct, so nine in ten
// were already answered and across frames all of them were.
//
// These tests pin the key, because a memo that is too coarse returns the wrong
// size for the wrong font and a memo that is too fine never hits.

#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include <afterhours/ah.h>
#include <afterhours/src/measure_memo.h>

#include <cmath>
#include <cstdio>
#include <limits>

using namespace afterhours;

static int checks = 0;
static int failures = 0;
static void check(bool cond, const char *expr, int line) {
  checks++;
  if (!cond) {
    failures++;
    std::fprintf(stderr, "  FAIL: %s  (line %d)\n", expr, line);
  }
}
#define CHECK(expr) check((expr), #expr, __LINE__)

static measure_memo::Key k(const char *text, float size = 16.f,
                           float spacing = 1.f, std::uint64_t font = 7) {
  return measure_memo::key_for(text, size, spacing, font);
}

int main() {
  measure_memo::clear();

  // Same question, same answer, without asking the font twice.
  {
    Vector2Type out{};
    CHECK(!measure_memo::lookup(k("hello"), out));
    measure_memo::store(k("hello"), Vector2Type{40.f, 16.f});
    CHECK(measure_memo::lookup(k("hello"), out));
    CHECK(out.x == 40.f && out.y == 16.f);
  }

  // Everything that changes the answer must change the key, or a hit returns
  // the wrong size and text lays out wrong with nothing to show why.
  {
    Vector2Type out{};
    CHECK(!measure_memo::lookup(k("hello", 32.f), out));    // size
    CHECK(!measure_memo::lookup(k("hello", 16.f, 2.f), out)); // spacing
    CHECK(!measure_memo::lookup(k("hello", 16.f, 1.f, 9), out)); // font
    CHECK(!measure_memo::lookup(k("hellp"), out));          // text
  }

  // Nearly equal floats must not each get an entry, or the memo grows without
  // ever hitting.
  { CHECK(k("x", 16.0f) == k("x", 16.0f + 1e-6f)); }

  // Bad scale math must not reach an unsigned float cast.
  {
    CHECK(k("x", std::numeric_limits<float>::quiet_NaN()) != k("x", 16.f));
    CHECK(k("x", -1.f) != k("x", 16.f));
    CHECK(k("x", std::numeric_limits<float>::infinity()) != k("x", 16.f));
    CHECK(k("x", 16.f, -1.f) != k("x", 16.f, 1.f));
  }

  // One shared hash is not identity: a forged collision must miss.
  {
    measure_memo::clear();
    Vector2Type out{};
    const auto hello = k("hello");
    auto forged = hello;
    forged.check = k("world").check;
    measure_memo::store(hello, Vector2Type{40.f, 16.f});
    CHECK(!measure_memo::lookup(forged, out));
    measure_memo::store(forged, Vector2Type{99.f, 9.f});
    CHECK(measure_memo::lookup(forged, out));
    CHECK(out.x == 99.f && out.y == 9.f);
    CHECK(!measure_memo::lookup(hello, out));
    CHECK(measure_memo::index().size() == 1);
    CHECK(measure_memo::lru().size() == 1);
  }

  // A font reloaded under the same handle would keep serving old metrics.
  {
    Vector2Type out{};
    measure_memo::clear();
    CHECK(!measure_memo::lookup(k("hello"), out));
  }

  // Bounded: a process that measures unbounded distinct strings must not grow
  // without limit.
  {
    measure_memo::clear();
    for (std::size_t i = 0; i < measure_memo::kMaxEntries + 500; i++)
      measure_memo::store(k(std::to_string(i).c_str()), Vector2Type{1.f, 1.f});
    CHECK(measure_memo::index().size() <= measure_memo::kMaxEntries);
    CHECK(measure_memo::lru().size() == measure_memo::index().size());
  }

  // Hit and miss counts are the evidence the memo is working, so they have to
  // be right.
  {
    measure_memo::clear();
    const std::uint64_t before_h = measure_memo::hits();
    const std::uint64_t before_m = measure_memo::misses();
    Vector2Type out{};
    measure_memo::lookup(k("counted"), out); // miss
    measure_memo::store(k("counted"), Vector2Type{1.f, 1.f});
    measure_memo::lookup(k("counted"), out); // hit
    measure_memo::lookup(k("counted"), out); // hit
    CHECK(measure_memo::hits() - before_h == 2);
    CHECK(measure_memo::misses() - before_m == 1);
  }

  std::printf("%d/%d checks passed\n", checks - failures, checks);
  if (failures == 0) std::printf("All checks passed!\n");
  return failures == 0 ? 0 : 1;
}
