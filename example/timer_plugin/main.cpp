#include <iostream>

#define AFTER_HOURS_ENTITY_HELPER
#define AFTER_HOURS_SYSTEM
#include "../../ah.h"
#include "../../src/plugins/timer.h"

using namespace afterhours;

int main(int, char **) {
    SystemManager systems;
    
    // Register timer systems
    timer::register_update_systems(systems);
    
    // Create an entity with a timer
    auto &entity1 = EntityHelper::createEntity();
    entity1.addComponent<timer::HasTimer>(2.0f, true); // 2 second timer, auto-reset
    
    // Create an entity with a trigger
    auto &entity2 = EntityHelper::createEntity();
    entity2.addComponent<timer::TriggerOnDt>(1.5f); // 1.5 second trigger
    
    std::cout << "Running timer example for 5 seconds..." << std::endl;
    std::cout << "Entity 1: 2s auto-reset timer" << std::endl;
    std::cout << "Entity 2: 1.5s trigger" << std::endl;
    std::cout << std::endl;
    
    float total_time = 0.0f;
    const float dt = 0.1f; // 100ms per frame
    
    for (int frame = 0; frame < 50; ++frame) {
        total_time += dt;
        
        // Check entity 1 timer
        if (entity1.has<timer::HasTimer>()) {
            auto &timer = entity1.get<timer::HasTimer>();
            if (timer.is_ready()) {
                std::cout << "Frame " << frame << " (" << total_time << "s): Entity 1 timer triggered!" << std::endl;
            }
        }
        
        // Check entity 2 trigger
        if (entity2.has<timer::TriggerOnDt>()) {
            auto &trigger = entity2.get<timer::TriggerOnDt>();
            if (trigger.test(dt)) {
                std::cout << "Frame " << frame << " (" << total_time << "s): Entity 2 trigger fired!" << std::endl;
            }
        }
        
        // Run systems
        systems.run(dt);
    }
    
    std::cout << "\nTimer plugin example completed successfully!" << std::endl;
    return 0;
}

