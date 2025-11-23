#include <deque>
#include <iostream>

#define AFTER_HOURS_ENTITY_HELPER
#define AFTER_HOURS_SYSTEM
#include "../../ah.h"
#include "../../src/plugins/pathfinding.h"
#include "../shared/vector.h"

using namespace afterhours;
using Vec2 = Vector2Type;

// Simple walkability checker - allow all positions except (5,5)
bool is_walkable(const Vec2 &pos) { return !(pos.x == 5.0f && pos.y == 5.0f); }

int main(int, char **) {
    SystemManager systems;

    // Initialize pathfinding plugin
    pathfinding::init();

    // Register singleton enforcement
    pathfinding::enforce_singletons(systems);

    // Register update systems
    pathfinding::register_update_systems(systems);

    // Register walkability checker
    pathfinding::register_walkability_checker(is_walkable);

    // Set algorithm (BFS is default)
    pathfinding::set_algorithm(pathfinding::Algorithm::BFS);

    // Test synchronous pathfinding
    Vec2 start{0.0f, 0.0f};
    Vec2 end{10.0f, 10.0f};

    std::cout << "Finding path from (" << start.x << ", " << start.y << ") to ("
              << end.x << ", " << end.y << ")..." << std::endl;

    auto path = pathfinding::find_path_sync(start, end, is_walkable);

    if (path.empty()) {
        std::cout << "No path found!" << std::endl;
    } else {
        std::cout << "Path found with " << path.size()
                  << " steps:" << std::endl;
        for (size_t i = 0; i < path.size(); ++i) {
            std::cout << "  Step " << i << ": (" << path[i].x << ", "
                      << path[i].y << ")" << std::endl;
        }
    }

    // Test with A* algorithm
    std::cout << "\nTrying with A* algorithm..." << std::endl;
    pathfinding::set_algorithm(pathfinding::Algorithm::AStar);

    path = pathfinding::find_path_sync(start, end, is_walkable);

    if (path.empty()) {
        std::cout << "No path found!" << std::endl;
    } else {
        std::cout << "Path found with " << path.size()
                  << " steps:" << std::endl;
        for (size_t i = 0; i < path.size() && i < 5; ++i) {
            std::cout << "  Step " << i << ": (" << path[i].x << ", "
                      << path[i].y << ")" << std::endl;
        }
        if (path.size() > 5) {
            std::cout << "  ... (" << (path.size() - 5) << " more steps)"
                      << std::endl;
        }
    }

    // Test async pathfinding with entity
    std::cout << "\nTesting async pathfinding..." << std::endl;
    auto &entity = EntityHelper::createEntity();
    entity.addComponent<pathfinding::CanPathfind>();

    pathfinding::request_path(entity.id, Vec2{0.0f, 0.0f}, Vec2{3.0f, 3.0f},
                              [](const std::deque<Vec2> &path) {
                                  std::cout << "Async path received with "
                                            << path.size() << " steps"
                                            << std::endl;
                              });

    // Process a few frames to allow async processing
    for (int i = 0; i < 10; ++i) {
        systems.run(0.1f);
    }

    std::cout << "\nPathfinding plugin example completed successfully!"
              << std::endl;
    return 0;
}
