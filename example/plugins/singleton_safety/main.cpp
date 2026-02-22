#include <cassert>
#include <iostream>

#define AFTER_HOURS_ENTITY_HELPER
#define AFTER_HOURS_SYSTEM
#include "../../../ah.h"
#include "../../../src/plugins/files.h"

using namespace afterhours;

struct TestSingleton : BaseComponent {
  int value = 42;
};

struct NeverRegistered : BaseComponent {
  int x = 0;
};

int main(int, char **) {
  std::cout << "=== Singleton Safety Example ===" << std::endl;
  std::cout << "Tests has_singleton() and get_provider() null-safety"
            << std::endl;

  // Test 1: has_singleton returns false before registration
  std::cout << "\n1. has_singleton before registration:" << std::endl;
  bool exists = EntityHelper::has_singleton<TestSingleton>();
  std::cout << "  - has_singleton<TestSingleton>() = "
            << (exists ? "true" : "false") << std::endl;
  assert(!exists);

  // Test 2: get_singleton_cmp returns nullptr when no singleton registered
  std::cout << "\n2. get_singleton_cmp before registration:" << std::endl;
  auto *cmp = EntityHelper::get_singleton_cmp<TestSingleton>();
  std::cout << "  - get_singleton_cmp<TestSingleton>() = "
            << (cmp ? "non-null" : "nullptr") << std::endl;
  assert(cmp == nullptr);

  // Test 3: files::get_provider() returns nullptr before files::init()
  std::cout << "\n3. files::get_provider() before init:" << std::endl;
  auto *provider = files::get_provider();
  std::cout << "  - get_provider() = "
            << (provider ? "non-null" : "nullptr") << std::endl;
  assert(provider == nullptr);

  // Test 4: Register a singleton and verify has_singleton returns true
  std::cout << "\n4. After registering singleton:" << std::endl;
  Entity &singleton = EntityHelper::createEntity();
  TestSingleton ts;
  ts.value = 99;
  singleton.addComponent<TestSingleton>(std::move(ts));
  EntityHelper::registerSingleton<TestSingleton>(singleton);

  exists = EntityHelper::has_singleton<TestSingleton>();
  std::cout << "  - has_singleton<TestSingleton>() = "
            << (exists ? "true" : "false") << std::endl;
  assert(exists);

  cmp = EntityHelper::get_singleton_cmp<TestSingleton>();
  std::cout << "  - get_singleton_cmp<TestSingleton>() = "
            << (cmp ? "non-null" : "nullptr") << std::endl;
  assert(cmp != nullptr);
  std::cout << "  - value = " << cmp->value << std::endl;
  assert(cmp->value == 99);

  // Test 5: files::get_provider() returns non-null after files::init()
  std::cout << "\n5. files::get_provider() after init:" << std::endl;
  files::init("test_game", "resources");
  provider = files::get_provider();
  std::cout << "  - get_provider() = "
            << (provider ? "non-null" : "nullptr") << std::endl;
  assert(provider != nullptr);

  // Test 6: Calling get_provider() multiple times is safe
  std::cout << "\n6. Multiple get_provider() calls:" << std::endl;
  for (int i = 0; i < 5; i++) {
    auto *p = files::get_provider();
    assert(p != nullptr);
  }
  std::cout << "  - 5 consecutive calls all returned non-null" << std::endl;

  // Test 7: has_singleton with type that was never registered
  std::cout << "\n7. has_singleton for unregistered type:" << std::endl;
  bool never_exists = EntityHelper::has_singleton<NeverRegistered>();
  std::cout << "  - has_singleton<NeverRegistered>() = "
            << (never_exists ? "true" : "false") << std::endl;
  assert(!never_exists);

  std::cout << "\n=== All singleton safety tests passed! ===" << std::endl;
  return 0;
}
