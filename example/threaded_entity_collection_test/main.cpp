#define CATCH_CONFIG_MAIN
#include "../../vendor/catch2/catch.hpp"

#include "../../src/ecs.h"

#include <atomic>
#include <thread>

namespace afterhours {

TEST_CASE("ECS: two threads can run independent EntityCollections (TLS binding)",
          "[ECS][EntityCollection][Thread]") {
  struct Marker : BaseComponent {
    int v = 0;
  };

  afterhours::EntityCollection a;
  afterhours::EntityCollection b;

  std::atomic_bool ok_a{true};
  std::atomic_bool ok_b{true};

  std::thread ta([&] {
    try {
      afterhours::ScopedEntityCollection _c(a);
      Entity &e0 = EntityHelper::createEntity();
      Entity &e1 = EntityHelper::createEntity();
      if (!(e0.id == 0 && e1.id == 1))
        ok_a = false;
      e0.addComponent<Marker>().v = 111;
      e1.addComponent<Marker>().v = 112;
      EntityHelper::merge_entity_arrays();
    } catch (...) {
      ok_a = false;
    }
  });

  std::thread tb([&] {
    try {
      afterhours::ScopedEntityCollection _c(b);
      Entity &e0 = EntityHelper::createEntity();
      Entity &e1 = EntityHelper::createEntity();
      if (!(e0.id == 0 && e1.id == 1))
        ok_b = false;
      e0.addComponent<Marker>().v = 221;
      e1.addComponent<Marker>().v = 222;
      EntityHelper::merge_entity_arrays();
    } catch (...) {
      ok_b = false;
    }
  });

  ta.join();
  tb.join();

  REQUIRE(ok_a.load());
  REQUIRE(ok_b.load());

  // Validate isolation from the main thread by explicitly switching collections.
  {
    afterhours::ScopedEntityCollection _c(a);
    auto ents = EntityQuery<>({.ignore_temp_warning = true}).gen();
    REQUIRE(ents.size() == 2);
    const int v0 = ents[0].get().get<Marker>().v;
    const int v1 = ents[1].get().get<Marker>().v;
    REQUIRE((v0 == 111 || v0 == 112));
    REQUIRE((v1 == 111 || v1 == 112));
    REQUIRE(v0 != v1);
  }
  {
    afterhours::ScopedEntityCollection _c(b);
    auto ents = EntityQuery<>({.ignore_temp_warning = true}).gen();
    REQUIRE(ents.size() == 2);
    const int v0 = ents[0].get().get<Marker>().v;
    const int v1 = ents[1].get().get<Marker>().v;
    REQUIRE((v0 == 221 || v0 == 222));
    REQUIRE((v1 == 221 || v1 == 222));
    REQUIRE(v0 != v1);
  }
}

} // namespace afterhours

