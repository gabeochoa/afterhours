#include <cassert>
#include <iostream>
#include <variant>

// Include developer.h directly to get fallback types
#include "../../../src/developer.h"

// Now include the rest of afterhours
#define AFTER_HOURS_ENTITY_HELPER
#define AFTER_HOURS_SYSTEM
#include "../../../ah.h"

using namespace afterhours;

// Test components for singleton enforcement
struct GameSettings : BaseComponent {
    int volume = 50;
    bool fullscreen = false;
};

struct PlayerData : BaseComponent {
    std::string name = "Player1";
    int score = 0;
};

// Example plugin implementation
struct MyPlugin {
    static void add_singleton_components(Entity& entity) {
        entity.addComponent<GameSettings>();
        EntityHelper::registerSingleton<GameSettings>(entity);
    }

    static void enforce_singletons(SystemManager& sm) {
        sm.register_update_system(
            std::make_unique<developer::EnforceSingleton<GameSettings>>());
    }

    static void register_update_systems(SystemManager&) {
        // No update systems for this example
    }
};

// Plugin with render systems
struct RenderPlugin {
    static void add_singleton_components(Entity&) {}
    static void enforce_singletons(SystemManager&) {}
    static void register_update_systems(SystemManager&) {}
    static void register_render_systems(SystemManager&) {}
};

int main() {
    std::cout << "=== Developer Utilities Example ===" << std::endl;

    // Test 1: Fallback Vector2 type
    std::cout << "\n1. Fallback Vector2Type (MyVec2):" << std::endl;
    Vector2Type v1{3.0f, 4.0f};
    Vector2Type v2{1.0f, 2.0f};

    Vector2Type sum = v1 + v2;
    std::cout << "  v1 + v2 = (" << sum.x << ", " << sum.y << ")" << std::endl;
    assert(sum.x == 4.0f && sum.y == 6.0f);

    Vector2Type diff = v1 - v2;
    std::cout << "  v1 - v2 = (" << diff.x << ", " << diff.y << ")" << std::endl;
    assert(diff.x == 2.0f && diff.y == 2.0f);

    assert(v2 < v1);  // Lexicographic comparison
    assert(v1 == v1);
    std::cout << "  Comparison operators work correctly" << std::endl;

    // Test 2: distance_sq function
    std::cout << "\n2. distance_sq function:" << std::endl;
    Vector2Type a{0.0f, 0.0f};
    Vector2Type b{3.0f, 4.0f};
    float dist_sq = distance_sq(a, b);
    std::cout << "  distance_sq((0,0), (3,4)) = " << dist_sq << std::endl;
    assert(dist_sq == 25.0f);  // 3^2 + 4^2 = 25

    // Test 3: Fallback Color type
    std::cout << "\n3. Fallback ColorType (MyColor):" << std::endl;
    ColorType red{255, 0, 0, 255};
    std::cout << "  Red color: RGBA(" << static_cast<int>(red.r) << ", "
              << static_cast<int>(red.g) << ", "
              << static_cast<int>(red.b) << ", "
              << static_cast<int>(red.a) << ")" << std::endl;
    assert(red.r == 255 && red.g == 0 && red.b == 0 && red.a == 255);

    // Test 4: Fallback Rectangle type
    std::cout << "\n4. Fallback RectangleType (MyRectangle):" << std::endl;
    RectangleType rect{10.0f, 20.0f, 100.0f, 50.0f};
    std::cout << "  Rectangle: x=" << rect.x << ", y=" << rect.y
              << ", w=" << rect.width << ", h=" << rect.height << std::endl;
    assert(rect.x == 10.0f && rect.y == 20.0f);
    assert(rect.width == 100.0f && rect.height == 50.0f);

    // Test 5: Fallback Texture type
    std::cout << "\n5. Fallback TextureType (MyTexture):" << std::endl;
    TextureType tex{512.0f, 256.0f};
    std::cout << "  Texture: " << tex.width << "x" << tex.height << std::endl;
    assert(tex.width == 512.0f && tex.height == 256.0f);

    // Test 6: util::sgn function
    std::cout << "\n6. util::sgn (sign function):" << std::endl;
    std::cout << "  sgn(5) = " << util::sgn(5) << std::endl;
    std::cout << "  sgn(-3) = " << util::sgn(-3) << std::endl;
    std::cout << "  sgn(0) = " << util::sgn(0) << std::endl;
    assert(util::sgn(5) == 1);
    assert(util::sgn(-3) == -1);
    assert(util::sgn(0) == 0);

    // Float version
    std::cout << "  sgn(3.14f) = " << util::sgn(3.14f) << std::endl;
    std::cout << "  sgn(-2.5f) = " << util::sgn(-2.5f) << std::endl;
    assert(util::sgn(3.14f) == 1);
    assert(util::sgn(-2.5f) == -1);

    // Test 7: util::overloaded for std::visit
    std::cout << "\n7. util::overloaded for std::visit:" << std::endl;
    using Variant = std::variant<int, float, std::string>;

    auto visitor = util::overloaded{
        [](int i) { return "int: " + std::to_string(i); },
        [](float f) { return "float: " + std::to_string(f); },
        [](const std::string& s) { return "string: " + s; }
    };

    Variant v_int = 42;
    Variant v_float = 3.14f;
    Variant v_str = std::string("hello");

    std::cout << "  " << std::visit(visitor, v_int) << std::endl;
    std::cout << "  " << std::visit(visitor, v_float) << std::endl;
    std::cout << "  " << std::visit(visitor, v_str) << std::endl;

    // Test 8: PluginCore concept
    std::cout << "\n8. PluginCore concept:" << std::endl;
    static_assert(developer::PluginCore<MyPlugin>,
                  "MyPlugin must satisfy PluginCore");
    std::cout << "  MyPlugin satisfies PluginCore: yes" << std::endl;

    // Test 9: PluginWithRender concept
    std::cout << "\n9. PluginWithRender concept:" << std::endl;
    static_assert(developer::PluginWithRender<RenderPlugin>,
                  "RenderPlugin must satisfy PluginWithRender");
    std::cout << "  RenderPlugin satisfies PluginWithRender: yes" << std::endl;

    // Test 10: plugin_ok helper
    std::cout << "\n10. plugin_ok helper:" << std::endl;
    static_assert(developer::plugin_ok<MyPlugin>);
    static_assert(developer::plugin_ok<RenderPlugin>);
    std::cout << "  plugin_ok<MyPlugin>: true" << std::endl;
    std::cout << "  plugin_ok<RenderPlugin>: true" << std::endl;

    // Test 11: EnforceSingleton system
    std::cout << "\n11. EnforceSingleton system:" << std::endl;
    {
        EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();

        // Create exactly one entity with GameSettings
        auto& settings_entity = EntityHelper::createEntity();
        MyPlugin::add_singleton_components(settings_entity);
        EntityHelper::merge_entity_arrays();

        SystemManager sm;
        MyPlugin::enforce_singletons(sm);

        // This should pass - only one entity with GameSettings
        sm.run(1.0f);
        std::cout << "  Single GameSettings entity: passed" << std::endl;

        // Clean up
        EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();
    }

    // Test 12: Full plugin lifecycle
    std::cout << "\n12. Full plugin lifecycle:" << std::endl;
    {
        EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();

        // Step 1: Create manager entity and add singleton components
        auto& manager = EntityHelper::createEntity();
        MyPlugin::add_singleton_components(manager);
        EntityHelper::merge_entity_arrays();

        // Step 2: Access singleton entity and get component
        Entity& settings_entity = EntityHelper::get_singleton<GameSettings>().get();
        auto& settings = settings_entity.get<GameSettings>();
        std::cout << "  Initial volume: " << settings.volume << std::endl;
        assert(settings.volume == 50);

        // Step 3: Modify singleton
        settings.volume = 75;
        settings.fullscreen = true;

        // Step 4: Verify changes persist
        Entity& settings_entity2 = EntityHelper::get_singleton<GameSettings>().get();
        auto& settings2 = settings_entity2.get<GameSettings>();
        std::cout << "  Modified volume: " << settings2.volume << std::endl;
        assert(settings2.volume == 75);
        assert(settings2.fullscreen == true);

        std::cout << "  Plugin lifecycle: passed" << std::endl;

        EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();
    }

    std::cout << "\n=== All Developer Utilities tests passed! ===" << std::endl;
    return 0;
}
