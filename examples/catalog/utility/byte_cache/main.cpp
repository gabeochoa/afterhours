#include <cstdio>
#include <string>

#include "../../../../src/byte_cache.h"

int main() {
  using namespace afterhours;
  auto cost = [](const std::string &v) { return v.size(); };
  ByteCache<std::string, std::string> tokens(16, cost);
  ByteCache<int, std::string> pages(16, cost);

  tokens.put("int", std::string("type-name"));
  pages.put(1, std::string("blob-page-bytes"));
  (void)tokens.get("int");
  (void)tokens.get("missing");
  tokens.shrink_to(4);

  const auto s = tokens.stats();
  std::printf("byte_cache: tokens hits=%llu misses=%llu accounted=%zu "
              "pages_accounted=%zu\n",
              static_cast<unsigned long long>(s.hits),
              static_cast<unsigned long long>(s.misses), s.accounted_bytes,
              pages.accounted_bytes());
  return pages.accounted_bytes() == 15 && s.hits == 1 ? 0 : 1;
}
