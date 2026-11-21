// query_scratch_test.cpp
// G-P14: gen_into(scratch) reuses the caller's buffer capacity, so repeated
// queries allocate no result storage after warmup, unlike gen() which
// allocates a fresh RefEntities every call.

#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include <afterhours/ah.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <vector>

using namespace afterhours;

// Counts heap allocations of at least kBig bytes while enabled, which is
// the result-buffer size here; the query's own small bookkeeping is below
// it, so the count is the materialization's, not construction's.
static std::atomic<bool> g_counting{false};
static std::atomic<size_t> g_big_allocs{0};
static constexpr size_t kBig = 256;

static void note_alloc(size_t n) {
  if (g_counting.load(std::memory_order_relaxed) && n >= kBig)
    g_big_allocs.fetch_add(1, std::memory_order_relaxed);
}

void *operator new(size_t n) {
  note_alloc(n);
  void *p = std::malloc(n ? n : 1);
  if (!p) throw std::bad_alloc();
  return p;
}
void *operator new(size_t n, const std::nothrow_t &) noexcept {
  note_alloc(n);
  return std::malloc(n ? n : 1);
}
void *operator new(size_t n, std::align_val_t a) {
  note_alloc(n);
  void *p = nullptr;
  if (posix_memalign(&p, static_cast<size_t>(a), n ? n : 1) != 0)
    throw std::bad_alloc();
  return p;
}
void *operator new(size_t n, std::align_val_t a,
                   const std::nothrow_t &) noexcept {
  note_alloc(n);
  void *p = nullptr;
  if (posix_memalign(&p, static_cast<size_t>(a), n ? n : 1) != 0)
    return nullptr;
  return p;
}
void operator delete(void *p) noexcept { std::free(p); }
void operator delete(void *p, size_t) noexcept { std::free(p); }
void operator delete(void *p, std::align_val_t) noexcept { std::free(p); }
void operator delete(void *p, size_t, std::align_val_t) noexcept {
  std::free(p);
}

static int tests_run = 0;
static int tests_passed = 0;
static void check(bool cond, const char *expr, const char *file, int line) {
  tests_run++;
  if (cond)
    tests_passed++;
  else
    fprintf(stderr, "  FAIL: %s  (%s:%d)\n", expr, file, line);
}
#define CHECK(expr) check((expr), #expr, __FILE__, __LINE__)

struct Tag : BaseComponent {};

static std::vector<int> ids_of(const RefEntities &r) {
  std::vector<int> ids;
  for (const Entity &e : r) ids.push_back(e.id);
  return ids;
}

static size_t count_big_allocs = 0;
static void begin_count() {
  g_big_allocs.store(0, std::memory_order_relaxed);
  g_counting.store(true, std::memory_order_relaxed);
}
static void end_count() {
  g_counting.store(false, std::memory_order_relaxed);
  count_big_allocs = g_big_allocs.load(std::memory_order_relaxed);
}

int main() {
  constexpr int kCount = 200;
  constexpr int kIters = 8;

  EntityCollection ec;
  for (int i = 0; i < kCount; ++i)
    ec.createEntity().addComponent<Tag>();
  ec.cleanup();

  // gen() allocates one result buffer per fresh query.
  {
    begin_count();
    for (int i = 0; i < kIters; ++i) {
      auto r = EntityQuery(ec).whereHasComponent<Tag>().gen();
      (void)r;
    }
    end_count();
  }
  printf("gen(): %zu big allocs over %d fresh queries\n", count_big_allocs,
         kIters);
  CHECK(count_big_allocs >= static_cast<size_t>(kIters));

  // gen_into(): warmup grows the scratch once, then zero result allocs.
  RefEntities scratch;
  {
    auto warm = EntityQuery(ec).whereHasComponent<Tag>().gen_into(scratch);
    CHECK(warm.size() == static_cast<size_t>(kCount));
  }
  {
    begin_count();
    for (int i = 0; i < kIters; ++i) {
      auto &r = EntityQuery(ec).whereHasComponent<Tag>().gen_into(scratch);
      (void)r;
    }
    end_count();
  }
  printf("gen_into(): %zu big allocs over %d fresh queries\n", count_big_allocs,
         kIters);
  CHECK(count_big_allocs == 0);

  // Same results as gen(), in the same order.
  auto fresh = EntityQuery(ec).whereHasComponent<Tag>().gen();
  CHECK(ids_of(fresh) == ids_of(scratch));
  CHECK(scratch.capacity() >= scratch.size());

  printf("\n%d/%d checks passed\n", tests_passed, tests_run);
  return tests_passed == tests_run ? 0 : 1;
}
