#include <iostream>
#include <string>

#define AFTER_HOURS_ENTITY_HELPER
#define AFTER_HOURS_SYSTEM
#include "../../ah.h"
#include "../../src/plugins/files.h"

using namespace afterhours;

int main(int, char **) {
    // Initialize the files plugin
    files::init("test_game", "resources");
    
    // Get resource path
    auto resource_path = files::get_resource_path("textures", "player.png");
    std::cout << "Resource path: " << resource_path.string() << std::endl;
    
    // Get save path
    auto save_path = files::get_save_path();
    std::cout << "Save path: " << save_path.string() << std::endl;
    
    // Get config path
    auto config_path = files::get_config_path();
    std::cout << "Config path: " << config_path.string() << std::endl;
    
    // Ensure directory exists
    auto test_dir = save_path / "test_dir";
    if (files::ensure_directory_exists(test_dir)) {
        std::cout << "Created directory: " << test_dir.string() << std::endl;
    }
    
    // Iterate over resources in a group (if they exist)
    std::cout << "\nResources in 'textures' group:" << std::endl;
    files::for_resources_in_group("textures", 
        [](const std::string &name, const std::string &path, const std::string &ext) {
            std::cout << "  - " << name << " (" << ext << ") at " << path << std::endl;
        });
    
    std::cout << "\nFiles plugin example completed successfully!" << std::endl;
    return 0;
}

