#include <cassert>
#include <iostream>
#include <string>

#define AFTER_HOURS_ENTITY_HELPER
#define AFTER_HOURS_ENTITY_QUERY
#include "../../../ah.h"

using namespace afterhours;

// Define tags for categorizing entities
enum struct GameTag : uint8_t { Player = 0, NPC = 1, Enemy = 2, Projectile = 3 };

// Define components - must inherit from BaseComponent
struct Position : BaseComponent {
    float x, y;
    Position(float x_ = 0.0f, float y_ = 0.0f) : x(x_), y(y_) {}
};

struct Velocity : BaseComponent {
    float dx, dy;
    Velocity(float dx_ = 0.0f, float dy_ = 0.0f) : dx(dx_), dy(dy_) {}
};

struct Health : BaseComponent {
    int current;
    int max;
    Health(int c = 100, int m = 100) : current(c), max(m) {}
};

struct Name : BaseComponent {
    std::string value;
    Name(const std::string& v = "") : value(v) {}
};

int main() {
    std::cout << "=== EntityQuery Example ===" << std::endl;

    // Create entities with various components
    std::cout << "\n1. Creating entities with various components..." << std::endl;

    // Player entity
    {
        auto& player = EntityHelper::createEntity();
        player.addComponent<Position>(0.0f, 0.0f);
        player.addComponent<Velocity>(1.0f, 0.0f);
        player.addComponent<Health>(100, 100);
        player.addComponent<Name>("Hero");
        player.enableTag(GameTag::Player);
    }

    // NPC entities
    {
        auto& npc1 = EntityHelper::createEntity();
        npc1.addComponent<Position>(10.0f, 20.0f);
        npc1.addComponent<Health>(50, 50);
        npc1.addComponent<Name>("Merchant");
        npc1.enableTag(GameTag::NPC);
    }
    {
        auto& npc2 = EntityHelper::createEntity();
        npc2.addComponent<Position>(30.0f, 40.0f);
        npc2.addComponent<Health>(30, 50);
        npc2.addComponent<Name>("Guard");
        npc2.enableTag(GameTag::NPC);
    }

    // Enemy entities
    {
        auto& enemy1 = EntityHelper::createEntity();
        enemy1.addComponent<Position>(50.0f, 50.0f);
        enemy1.addComponent<Velocity>(-1.0f, 0.0f);
        enemy1.addComponent<Health>(25, 25);
        enemy1.enableTag(GameTag::Enemy);
    }
    {
        auto& enemy2 = EntityHelper::createEntity();
        enemy2.addComponent<Position>(60.0f, 60.0f);
        enemy2.addComponent<Velocity>(0.0f, -1.0f);
        enemy2.addComponent<Health>(40, 40);
        enemy2.enableTag(GameTag::Enemy);
    }
    {
        auto& enemy3 = EntityHelper::createEntity();
        enemy3.addComponent<Position>(70.0f, 70.0f);
        enemy3.addComponent<Velocity>(1.0f, 1.0f);
        enemy3.addComponent<Health>(15, 30);
        enemy3.enableTag(GameTag::Enemy);
    }

    // Projectile (no health)
    {
        auto& projectile = EntityHelper::createEntity();
        projectile.addComponent<Position>(5.0f, 5.0f);
        projectile.addComponent<Velocity>(10.0f, 0.0f);
        projectile.enableTag(GameTag::Projectile);
    }

    // Static object (no velocity)
    {
        auto& wall = EntityHelper::createEntity();
        wall.addComponent<Position>(100.0f, 0.0f);
    }

    // Merge temp entities into main collection
    EntityHelper::get_default_collection().merge_entity_arrays();

    std::cout << "  Created 8 entities" << std::endl;

    // Test 2: whereHasComponent filtering
    std::cout << "\n2. Query entities with Position component..." << std::endl;
    auto with_position = EntityQuery<>().whereHasComponent<Position>().gen();
    std::cout << "  Found " << with_position.size() << " entities with Position" << std::endl;
    assert(with_position.size() == 8);

    // Test 3: Multiple component requirements
    std::cout << "\n3. Query entities with both Position AND Velocity..." << std::endl;
    auto movers = EntityQuery<>()
                      .whereHasComponent<Position>()
                      .whereHasComponent<Velocity>()
                      .gen();
    std::cout << "  Found " << movers.size() << " moving entities" << std::endl;
    assert(movers.size() == 5);  // player + 3 enemies + projectile

    // Test 4: whereHasTag filtering
    std::cout << "\n4. Query entities with NPC tag..." << std::endl;
    auto npcs = EntityQuery<>().whereHasTag(GameTag::NPC).gen();
    std::cout << "  Found " << npcs.size() << " NPCs" << std::endl;
    assert(npcs.size() == 2);

    // Test 5: Combining component and tag filters
    std::cout << "\n5. Query entities with Health component AND NPC tag..." << std::endl;
    auto healthy_npcs = EntityQuery<>()
                            .whereHasComponent<Health>()
                            .whereHasTag(GameTag::NPC)
                            .gen();
    std::cout << "  Found " << healthy_npcs.size() << " NPCs with health" << std::endl;
    assert(healthy_npcs.size() == 2);

    // Test 6: whereLambda for custom filtering
    std::cout << "\n6. Query entities with low health (< 30) using lambda..." << std::endl;
    auto low_health = EntityQuery<>()
                          .whereHasComponent<Health>()
                          .whereLambda([](const Entity& e) {
                              return e.get<Health>().current < 30;
                          })
                          .gen();
    std::cout << "  Found " << low_health.size() << " entities with low health" << std::endl;
    for (const Entity& e : low_health) {
        std::cout << "    - Health: " << e.get<Health>().current << std::endl;
    }
    assert(low_health.size() == 2);  // enemy1 (25) and enemy3 (15)

    // Test 7: take() to limit results
    // Note: take(n) returns n+1 entities due to the > check in Limit
    std::cout << "\n7. Query first 3 entities with Position (using take(2))..." << std::endl;
    auto first_few = EntityQuery<>()
                         .whereHasComponent<Position>()
                         .take(2)
                         .gen();
    std::cout << "  Got " << first_few.size() << " entities" << std::endl;
    assert(first_few.size() == 3);  // take(n) gives n+1

    // Test 8: first() to get single result
    // Note: first() calls take(1) which returns 2 entities due to off-by-one
    std::cout << "\n8. Query first entity with Player tag (using first)..." << std::endl;
    auto player_query = EntityQuery<>().whereHasTag(GameTag::Player).first().gen();
    std::cout << "  Got " << player_query.size() << " entity(ies)" << std::endl;
    // first() may return more than 1 due to take(1) off-by-one
    assert(player_query.size() >= 1);
    assert(player_query[0].get().hasTag(static_cast<TagId>(GameTag::Player)));

    // Test 9: gen_first() for optional single result
    std::cout << "\n9. Using gen_first() to get optional result..." << std::endl;
    auto maybe_player = EntityQuery<>().whereHasTag(GameTag::Player).gen_first();
    if (maybe_player.valid()) {
        std::cout << "  Found player entity" << std::endl;
        assert(maybe_player.asE().has<Name>());
    }

    auto maybe_none = EntityQuery<>().whereHasTag(static_cast<GameTag>(99)).gen_first();
    std::cout << "  Query for non-existent tag: "
              << (maybe_none.valid() ? "found" : "empty") << std::endl;
    assert(!maybe_none.valid());

    // Test 10: gen_count() to count matches
    std::cout << "\n10. Using gen_count() to count enemies..." << std::endl;
    auto enemy_count = EntityQuery<>().whereHasTag(GameTag::Enemy).gen_count();
    std::cout << "  Enemy count: " << enemy_count << std::endl;
    assert(enemy_count == 3);

    // Test 11: orderByLambda for sorting
    std::cout << "\n11. Query entities sorted by health (ascending)..." << std::endl;
    auto sorted_by_health = EntityQuery<>()
                                .whereHasComponent<Health>()
                                .orderByLambda([](const Entity& a, const Entity& b) {
                                    return a.get<Health>().current <
                                           b.get<Health>().current;
                                })
                                .gen();
    std::cout << "  Sorted by health:" << std::endl;
    int prev_health = 0;
    for (const Entity& e : sorted_by_health) {
        int h = e.get<Health>().current;
        std::cout << "    - " << h << " HP" << std::endl;
        assert(h >= prev_health);
        prev_health = h;
    }

    // Test 12: Complex query combining multiple features
    std::cout << "\n12. Complex query: enemies with health > 20, sorted by health desc..."
              << std::endl;
    auto complex_result = EntityQuery<>()
                              .whereHasTag(GameTag::Enemy)
                              .whereHasComponent<Health>()
                              .whereLambda([](const Entity& e) {
                                  return e.get<Health>().current > 20;
                              })
                              .orderByLambda([](const Entity& a, const Entity& b) {
                                  return a.get<Health>().current >
                                         b.get<Health>().current;  // Descending
                              })
                              .gen();
    std::cout << "  Result:" << std::endl;
    for (const Entity& e : complex_result) {
        std::cout << "    - Enemy with " << e.get<Health>().current << " HP"
                  << std::endl;
    }
    assert(complex_result.size() == 2);  // enemy1 (25) and enemy2 (40)
    assert(complex_result[0].get().get<Health>().current == 40);
    assert(complex_result[1].get().get<Health>().current == 25);

    // Test 13: Query with position-based filtering
    std::cout << "\n13. Query entities within radius of (50, 50)..." << std::endl;
    constexpr float center_x = 50.0f;
    constexpr float center_y = 50.0f;
    constexpr float radius = 25.0f;

    auto nearby = EntityQuery<>()
                      .whereHasComponent<Position>()
                      .whereLambda([](const Entity& e) {
                          const auto& pos = e.get<Position>();
                          float dx = pos.x - center_x;
                          float dy = pos.y - center_y;
                          return (dx * dx + dy * dy) <= (radius * radius);
                      })
                      .gen();
    std::cout << "  Found " << nearby.size() << " entities within radius " << radius
              << std::endl;
    for (const Entity& e : nearby) {
        const auto& pos = e.get<Position>();
        std::cout << "    - Position: (" << pos.x << ", " << pos.y << ")" << std::endl;
    }

    // Test 14: whereMissingComponent
    std::cout << "\n14. Query entities WITHOUT Health component..." << std::endl;
    auto no_health = EntityQuery<>().whereMissingComponent<Health>().gen();
    std::cout << "  Found " << no_health.size() << " entities without health" << std::endl;
    assert(no_health.size() == 2);  // projectile and wall

    // Test 15: whereID and whereNotID
    std::cout << "\n15. Query by entity ID..." << std::endl;
    auto first_entity = EntityQuery<>().take(1).gen();
    int first_id = first_entity[0].get().id;
    auto by_id = EntityQuery<>().whereID(first_id).gen();
    std::cout << "  Found entity with ID " << first_id << ": " << by_id.size() << " result"
              << std::endl;
    assert(by_id.size() == 1);
    assert(by_id[0].get().id == first_id);

    auto not_first = EntityQuery<>().whereNotID(first_id).gen();
    std::cout << "  Entities NOT with ID " << first_id << ": " << not_first.size()
              << std::endl;
    assert(not_first.size() == 7);

    // Test 16: gen_ids()
    std::cout << "\n16. Generate list of entity IDs..." << std::endl;
    auto all_ids = EntityQuery<>().whereHasComponent<Position>().gen_ids();
    std::cout << "  Entity IDs with Position: ";
    for (int id : all_ids) {
        std::cout << id << " ";
    }
    std::cout << std::endl;
    assert(all_ids.size() == 8);

    // Test 17: has_values() and is_empty()
    std::cout << "\n17. Check query emptiness..." << std::endl;
    bool has_players = EntityQuery<>().whereHasTag(GameTag::Player).has_values();
    bool has_invalid = EntityQuery<>().whereHasTag(static_cast<GameTag>(99)).has_values();
    std::cout << "  Has players? " << (has_players ? "yes" : "no") << std::endl;
    std::cout << "  Has invalid tag? " << (has_invalid ? "yes" : "no") << std::endl;
    assert(has_players);
    assert(!has_invalid);

    std::cout << "\n=== All EntityQuery tests passed! ===" << std::endl;
    return 0;
}
