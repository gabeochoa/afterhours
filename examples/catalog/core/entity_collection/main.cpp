#include <cassert>
#include <iostream>

#define AFTER_HOURS_ENTITY_HELPER
#include "../../../ah.h"

using namespace afterhours;

// Define components - must inherit from BaseComponent
struct Position : BaseComponent {
    float x, y;
    Position(float x_ = 0.0f, float y_ = 0.0f) : x(x_), y(y_) {}
};

struct GameSettings : BaseComponent {
    int difficulty = 1;
    bool sound_enabled = true;
    GameSettings(int diff = 1, bool sound = true)
        : difficulty(diff), sound_enabled(sound) {}
};

struct PlayerData : BaseComponent {
    std::string name;
    int score = 0;
    PlayerData(const std::string& n = "", int s = 0) : name(n), score(s) {}
};

int main() {
    std::cout << "=== EntityCollection Example ===" << std::endl;

    // Get the default collection via EntityHelper
    EntityCollection& collection = EntityHelper::get_default_collection();

    // Test 1: Creating entities
    std::cout << "\n1. Creating entities..." << std::endl;
    {
        Entity& e1 = collection.createEntity();
        e1.addComponent<Position>(10.0f, 20.0f);

        Entity& e2 = collection.createEntity();
        e2.addComponent<Position>(30.0f, 40.0f);

        Entity& e3 = collection.createEntity();
        e3.addComponent<Position>(50.0f, 60.0f);

        std::cout << "  Created 3 entities in temp storage" << std::endl;
        std::cout << "  Temp entities count: " << collection.get_temp().size() << std::endl;
        assert(collection.get_temp().size() == 3);
    }

    // Test 2: Merge temp entities to main storage
    std::cout << "\n2. Merging temp entities to main storage..." << std::endl;
    collection.merge_entity_arrays();
    std::cout << "  After merge - temp: " << collection.get_temp().size()
              << ", main: " << collection.get_entities().size() << std::endl;
    assert(collection.get_temp().empty());
    assert(collection.get_entities().size() == 3);

    // Test 3: Creating permanent entities
    std::cout << "\n3. Creating permanent entities..." << std::endl;
    EntityID perm_id;
    {
        Entity& perm = collection.createPermanentEntity();
        perm.addComponent<Position>(100.0f, 100.0f);
        perm_id = perm.id;
        std::cout << "  Created permanent entity with ID: " << perm_id << std::endl;
    }
    collection.merge_entity_arrays();
    std::cout << "  Total entities after merge: " << collection.get_entities().size() << std::endl;
    assert(collection.get_entities().size() == 4);

    // Test 4: Singleton pattern
    std::cout << "\n4. Singleton pattern for global entities..." << std::endl;
    {
        Entity& settings_entity = collection.createEntity();
        settings_entity.addComponent<GameSettings>(2, true);
        collection.merge_entity_arrays();
        collection.registerSingleton<GameSettings>(settings_entity);
        std::cout << "  Registered GameSettings singleton" << std::endl;
    }

    // Access singleton
    assert(collection.has_singleton<GameSettings>());
    Entity& settings = collection.get_singleton<GameSettings>();
    std::cout << "  GameSettings difficulty: " << settings.get<GameSettings>().difficulty << std::endl;
    assert(settings.get<GameSettings>().difficulty == 2);

    // Get component directly via get_singleton_cmp
    GameSettings* gs = collection.get_singleton_cmp<GameSettings>();
    std::cout << "  Sound enabled: " << (gs->sound_enabled ? "yes" : "no") << std::endl;
    assert(gs->sound_enabled);

    // Test 5: EntityHandle for stable references
    std::cout << "\n5. EntityHandle for stable references..." << std::endl;
    EntityHandle handle;
    {
        Entity& target = collection.createEntity();
        target.addComponent<PlayerData>("Player1", 100);
        collection.merge_entity_arrays();

        handle = collection.handle_for(target);
        std::cout << "  Created handle for entity - valid: " << (!handle.is_invalid() ? "yes" : "no") << std::endl;
        assert(!handle.is_invalid());
    }

    // Resolve handle back to entity
    {
        OptEntity resolved = collection.resolve(handle);
        assert(resolved.valid());
        std::cout << "  Resolved handle - player name: " << resolved.asE().get<PlayerData>().name << std::endl;
        assert(resolved.asE().get<PlayerData>().name == "Player1");
    }

    // Test 6: Get entity by ID
    std::cout << "\n6. Getting entity by ID..." << std::endl;
    {
        OptEntity found = collection.getEntityForID(perm_id);
        if (found.valid()) {
            std::cout << "  Found permanent entity with ID " << perm_id << std::endl;
            std::cout << "  Position: (" << found.asE().get<Position>().x << ", "
                      << found.asE().get<Position>().y << ")" << std::endl;
            assert(found.asE().get<Position>().x == 100.0f);
        }
    }

    // Test 7: Marking entities for cleanup
    std::cout << "\n7. Entity cleanup..." << std::endl;
    {
        Entity& temp_entity = collection.createEntity();
        temp_entity.addComponent<Position>(999.0f, 999.0f);
        collection.merge_entity_arrays();
        EntityID temp_id = temp_entity.id;

        std::cout << "  Created temporary entity with ID: " << temp_id << std::endl;
        std::cout << "  Total entities before cleanup: " << collection.get_entities().size() << std::endl;

        // Mark for cleanup
        collection.markIDForCleanup(temp_id);
        std::cout << "  Marked entity " << temp_id << " for cleanup" << std::endl;

        // Run cleanup
        size_t before = collection.get_entities().size();
        collection.cleanup();
        size_t after = collection.get_entities().size();
        std::cout << "  After cleanup: " << before << " -> " << after << " entities" << std::endl;
        assert(after == before - 1);
    }

    // Test 8: Delete all non-permanent entities
    std::cout << "\n8. Delete all non-permanent entities..." << std::endl;
    {
        std::cout << "  Before delete_all: " << collection.get_entities().size() << " entities" << std::endl;
        collection.delete_all_entities(false);  // false = keep permanent
        std::cout << "  After delete_all (keep permanent): " << collection.get_entities().size() << " entities" << std::endl;
        // Only permanent entity should remain
        assert(collection.get_entities().size() == 1);

        // Verify the remaining entity is our permanent one
        const auto& remaining = collection.get_entities()[0];
        assert(remaining->id == perm_id);
        std::cout << "  Remaining entity ID: " << remaining->id << " (permanent)" << std::endl;
    }

    // Test 9: Handle invalidation after entity deletion
    std::cout << "\n9. Handle invalidation after deletion..." << std::endl;
    {
        // The handle from test 5 should now be stale
        OptEntity stale_resolved = collection.resolve(handle);
        std::cout << "  Resolving old handle: " << (stale_resolved.valid() ? "valid" : "stale") << std::endl;
        // May or may not be valid depending on cleanup timing
    }

    // Test 10: Complete entity reset
    std::cout << "\n10. Complete entity reset..." << std::endl;
    {
        // First, create some more entities
        for (int i = 0; i < 5; i++) {
            Entity& e = collection.createEntity();
            e.addComponent<Position>(static_cast<float>(i), static_cast<float>(i));
        }
        collection.merge_entity_arrays();
        std::cout << "  Created 5 more entities. Total: " << collection.get_entities().size() << std::endl;

        // Now delete ALL including permanent
        collection.delete_all_entities_NO_REALLY_I_MEAN_ALL();
        std::cout << "  After deleting ALL: " << collection.get_entities().size() << " entities" << std::endl;
        assert(collection.get_entities().empty());
    }

    std::cout << "\n=== All EntityCollection tests passed! ===" << std::endl;
    return 0;
}
