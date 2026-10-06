// byte_budget_cache_test.cpp
// ByteBudgetCache: LRU eviction by cost, replacement accounting, oversize
// rejection, observable stats, structured keys and shared values.

#include <afterhours/src/plugins/byte_budget_cache.h>

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using afterhours::ByteBudgetCache;

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

static std::size_t string_cost(const std::string &, const std::string &value) {
  return value.size();
}

static void evicts_least_recently_used() {
  ByteBudgetCache<std::string, std::string> cache(300, string_cost);
  check(cache.put("first", std::string(100, 'a')), "first fits");
  check(cache.put("second", std::string(100, 'b')), "second fits");
  check(cache.put("third", std::string(100, 'c')), "third fits");
  check(cache.get("first") != nullptr, "touch first");
  check(cache.put("fourth", std::string(100, 'd')), "fourth fits after eviction");
  check(cache.get("second") == nullptr, "least recently used entry was evicted");
  check(cache.get("first") && cache.get("third") && cache.get("fourth"), "others survive");
  auto stats = cache.stats();
  check(stats.bytes == 300 && stats.entries == 3 && stats.budget == 300, "bytes and entries accounted");
  check(stats.evictions == 1, "one eviction counted");
  check(stats.hits == 4 && stats.misses == 1, "hits and misses counted");
  check(cache.put("big", std::string(250, 'e')), "a large entry evicts several");
  stats = cache.stats();
  check(stats.entries == 1 && stats.bytes == 250 && stats.evictions == 4, "evicted until it fit");
}

static void replacement_and_oversize() {
  ByteBudgetCache<std::string, std::string> cache(300, string_cost);
  check(cache.put("key", std::string(100, 'a')), "insert");
  check(cache.put("key", std::string(40, 'b')), "replace");
  auto stats = cache.stats();
  check(stats.bytes == 40 && stats.entries == 1, "replacement re-accounts instead of adding");
  check(stats.evictions == 0, "replacement is not an eviction");
  check(*cache.get("key") == std::string(40, 'b'), "replacement value visible");
  check(cache.put("exact", std::string(260, 'c')), "an entry equal to the remaining budget fits");
  check(cache.stats().bytes == 300, "budget filled exactly");
  check(!cache.put("key", std::string(301, 'd')), "a value larger than the budget is rejected");
  stats = cache.stats();
  check(stats.rejections == 1, "rejection counted");
  check(!cache.contains("key"), "a rejected replacement removes the stale value");
  check(cache.contains("exact") && stats.bytes == 260, "rejection evicts nothing else");
  check(cache.erase("exact") && !cache.erase("exact"), "erase reports presence");
  check(cache.stats().bytes == 0, "erase releases bytes");
}

static void budget_changes_and_clear() {
  ByteBudgetCache<std::string, std::string> cache(300, string_cost);
  for (int i = 0; i < 3; ++i) cache.put(std::to_string(i), std::string(100, 'x'));
  cache.set_budget(150);
  auto stats = cache.stats();
  check(stats.entries == 1 && stats.bytes == 100 && stats.evictions == 2, "lowering the budget evicts");
  check(cache.contains("2"), "most recent survives a budget cut");
  cache.clear();
  stats = cache.stats();
  check(stats.entries == 0 && stats.bytes == 0, "clear empties");
  check(stats.evictions == 2, "clear is not counted as eviction");
  ByteBudgetCache<std::string, std::string> zero(0, string_cost);
  check(zero.put("empty", ""), "zero-cost entry fits a zero budget");
  check(!zero.put("one", "x"), "nonzero cost rejected by a zero budget");
}

struct PatchKey {
  std::string commit;
  int page = 0;
  bool operator==(const PatchKey &) const = default;
};
struct PatchKeyHash {
  std::size_t operator()(const PatchKey &key) const {
    return std::hash<std::string>{}(key.commit) ^ (std::hash<int>{}(key.page) << 1);
  }
};

static void structured_keys_and_shared_values() {
  using Pixels = std::shared_ptr<const std::vector<unsigned char>>;
  // The cost function may include the key; here it charges the commit name.
  ByteBudgetCache<PatchKey, Pixels, PatchKeyHash> cache(
      1000, [](const PatchKey &key, const Pixels &value) {
        return key.commit.size() + value->size();
      });
  auto first = std::make_shared<const std::vector<unsigned char>>(600, 1);
  check(cache.put({"abc", 0}, first), "structured key insert");
  check(cache.put({"abc", 1}, std::make_shared<const std::vector<unsigned char>>(100, 2)),
        "same commit, other page");
  check(cache.stats().bytes == 706, "cost includes key bytes when the cost function says so");
  Pixels held = *cache.get({"abc", 0});
  check(cache.put({"def", 0}, std::make_shared<const std::vector<unsigned char>>(500, 3)),
        "insert that forces eviction");
  check(!cache.contains({"abc", 0}), "shared entry evicted from the cache");
  check(held.get() == first.get() && held->size() == 600, "a shared reference outlives eviction");
  check(cache.stats().bytes == 503 && cache.stats().entries == 1, "evicted shared value no longer counted");
}

int main() {
  printf("=== byte budget cache tests ===\n\n");
  evicts_least_recently_used();
  replacement_and_oversize();
  budget_changes_and_clear();
  structured_keys_and_shared_values();
  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
