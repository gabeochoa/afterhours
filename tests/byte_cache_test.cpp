// ByteCache: the byte-budgeted LRU from decide-memory-budgets.md Option 1.
// floatinghotel's token cache (4 MiB), blob-page cache (32 MiB) and
// hand-rolled patch cache (32 MiB) all want this shape; these tests pin
// admission, eviction order, replacement, and the stats a host reads.

#include <afterhours/src/byte_cache.h>
#include <afterhours/src/core/text_cache.h>

#include <cstdio>
#include <string>

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

static std::size_t str_cost(const std::string &v) { return v.size(); }

int main() {
  // Admission, hit/miss, accounted bytes.
  {
    ByteCache<std::string, std::string> c(10, &str_cost);
    CHECK(c.put("a", std::string("12345")));       // 5
    CHECK(c.put("b", std::string("1234")));        // 4, total 9
    CHECK(c.accounted_bytes() == 9);
    CHECK(c.size() == 2);
    const std::string *v = c.get("a");
    CHECK(v != nullptr && *v == "12345");
    CHECK(c.get("nope") == nullptr);
    auto s = c.stats();
    CHECK(s.hits == 1 && s.misses == 1 && s.evictions == 0);
    CHECK(s.accounted_bytes == 9 && s.entries == 2 && s.budget_bytes == 10);
  }

  // LRU eviction: touching "a" protects it; "b" goes first.
  {
    ByteCache<std::string, std::string> c(10, &str_cost);
    c.put("a", std::string("12345"));
    c.put("b", std::string("1234"));
    CHECK(c.get("a") != nullptr);
    CHECK(c.put("c", std::string("12"))); // needs 2, evicts b (4)
    CHECK(c.contains("a") && c.contains("c") && !c.contains("b"));
    CHECK(c.accounted_bytes() == 7);
    CHECK(c.stats().evictions == 1);
  }

  // Oversize entries are rejected and leave the cache unchanged.
  {
    ByteCache<std::string, std::string> c(4, &str_cost);
    CHECK(c.put("a", std::string("12")));
    CHECK(!c.put("big", std::string("12345")));
    CHECK(!c.contains("big") && c.contains("a"));
    CHECK(c.accounted_bytes() == 2);
    CHECK(c.stats().rejections == 1);
    // An oversize replacement keeps the old value.
    CHECK(!c.put("a", std::string("12345")));
    const std::string *v = c.get("a");
    CHECK(v != nullptr && *v == "12");
  }

  // Replacement re-accounts and becomes most-recently-used.
  {
    ByteCache<int, std::string> c(
        10, [](const std::string &v) { return v.size(); });
    c.put(1, std::string("12345"));
    c.put(2, std::string("12345"));
    CHECK(c.put(1, std::string("1"))); // total now 6
    CHECK(c.accounted_bytes() == 6);
    CHECK(c.put(3, std::string("12345"))); // evicts 2, not just-touched 1
    CHECK(c.contains(1) && c.contains(3) && !c.contains(2));
    CHECK(c.stats().evictions == 1);
  }

  // shrink_to evicts LRU-first without changing the budget; erase/clear.
  {
    ByteCache<int, std::string> c(
        100, [](const std::string &v) { return v.size(); });
    c.put(1, std::string(30, 'a'));
    c.put(2, std::string(30, 'b'));
    c.put(3, std::string(30, 'c'));
    CHECK(c.get(1) != nullptr); // order LRU->MRU: 2,3,1
    c.shrink_to(60);
    CHECK(c.accounted_bytes() == 60 && c.budget() == 100);
    CHECK(!c.contains(2) && c.contains(1) && c.contains(3));
    CHECK(c.stats().evictions == 1);
    c.set_budget(30);
    CHECK(c.accounted_bytes() == 30 && c.size() == 1 && c.contains(1));
    CHECK(c.erase(1));
    CHECK(!c.erase(1));
    CHECK(c.accounted_bytes() == 0);
    c.put(9, std::string("xy"));
    c.clear();
    CHECK(c.empty() && c.accounted_bytes() == 0);
    CHECK(c.stats().hits == 1); // clear keeps lifetime counts
    c.reset_stats();
    CHECK(c.stats().hits == 0 && c.stats().misses == 0);
  }

  // Zero-cost values fit a zero budget; anything priced does not.
  {
    ByteCache<int, int> c(0, [](const int &) { return std::size_t{0}; });
    CHECK(c.put(1, 42));
    CHECK(c.accounted_bytes() == 0 && c.size() == 1);
    ByteCache<int, std::string> priced(
        0, [](const std::string &v) { return v.size() + 1; });
    CHECK(!priced.put(1, std::string("")));
    CHECK(priced.stats().rejections == 1);
  }

  // Default cost is sizeof(V): a fixed-size value cache is a count bound.
  {
    ByteCache<int, double> c(sizeof(double) * 2);
    CHECK(c.put(1, 1.5));
    CHECK(c.put(2, 2.5));
    CHECK(c.put(3, 3.5)); // evicts 1
    CHECK(!c.contains(1) && c.size() == 2);
    CHECK(c.accounted_bytes() == sizeof(double) * 2);
  }

  // TextMeasureCache now sits on ByteCache with its old defaults:
  // 4096-entry cap, hit/miss accounting, generation pruning unchanged.
  {
    ui::TextMeasureCache cache([](std::string_view text, std::string_view,
                                   float font_size, float) {
      return Vector2Type{static_cast<float>(text.size()) * font_size, font_size};
    });
    (void)cache.measure("hello", "face", 10.f);
    (void)cache.measure("hello", "face", 10.f);
    CHECK(cache.hits() == 1 && cache.misses() == 1);
    CHECK(cache.size() == 1);
    CHECK(cache.accounted_bytes() > 0);
    CHECK(cache.byte_stats().accounted_bytes == cache.accounted_bytes());
    cache.set_max_entries(2);
    (void)cache.measure("aa", "face", 10.f);
    (void)cache.measure("bbb", "face", 10.f);
    (void)cache.measure("cccc", "face", 10.f);
    CHECK(cache.size() <= 2);
    cache.clear();
    CHECK(cache.size() == 0 && cache.accounted_bytes() == 0);
  }

  std::printf("%d/%d checks passed\n", checks - failures, checks);
  if (failures == 0)
    std::printf("All checks passed!\n");
  return failures == 0 ? 0 : 1;
}
