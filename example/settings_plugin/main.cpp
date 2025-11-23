#include <iostream>
#include <string>

#define AFTER_HOURS_ENTITY_HELPER
#define AFTER_HOURS_SYSTEM
#include "../../ah.h"
#include "../../src/plugins/files.h"
#include "../../src/plugins/settings.h"

using namespace afterhours;

// Example settings data structure with raw_string serialization (default)
struct GameSettings {
    int volume = 50;
    std::string player_name = "Player";
    bool fullscreen = false;
    
    // Required for raw_string format (default)
    std::string to_string() const {
        return std::to_string(volume) + "\n" + 
               player_name + "\n" + 
               (fullscreen ? "1" : "0");
    }
    
    static GameSettings from_string(const std::string &str) {
        GameSettings settings;
        size_t pos = 0;
        size_t next_pos = str.find('\n', pos);
        if (next_pos != std::string::npos) {
            settings.volume = std::stoi(str.substr(pos, next_pos - pos));
            pos = next_pos + 1;
            next_pos = str.find('\n', pos);
            if (next_pos != std::string::npos) {
                settings.player_name = str.substr(pos, next_pos - pos);
                pos = next_pos + 1;
                settings.fullscreen = (str.substr(pos) == "1");
            }
        }
        return settings;
    }
};

int main(int, char **) {
    SystemManager systems;
    
    // Initialize files plugin (needed for settings save path)
    files::init("test_game", "resources");
    files::enforce_singletons(systems);
    
    // Initialize settings plugin
    settings::init<GameSettings>("test_game", "settings.txt");
    settings::enforce_singletons<GameSettings>(systems);
    settings::register_update_systems<GameSettings>(systems);
    
    // Get and modify settings
    auto &settings_data = settings::get_data<GameSettings>();
    std::cout << "Initial settings:" << std::endl;
    std::cout << "  Volume: " << settings_data.volume << std::endl;
    std::cout << "  Player Name: " << settings_data.player_name << std::endl;
    std::cout << "  Fullscreen: " << (settings_data.fullscreen ? "Yes" : "No") << std::endl;
    
    // Modify settings
    settings_data.volume = 75;
    settings_data.player_name = "TestPlayer";
    settings_data.fullscreen = true;
    
    // Save settings
    if (settings::save<GameSettings>()) {
        std::cout << "\nSettings saved successfully!" << std::endl;
    } else {
        std::cout << "\nFailed to save settings!" << std::endl;
    }
    
    // Reset to defaults
    settings_data = GameSettings();
    
    // Load settings
    if (settings::load<GameSettings>()) {
        std::cout << "\nSettings loaded:" << std::endl;
        std::cout << "  Volume: " << settings_data.volume << std::endl;
        std::cout << "  Player Name: " << settings_data.player_name << std::endl;
        std::cout << "  Fullscreen: " << (settings_data.fullscreen ? "Yes" : "No") << std::endl;
    } else {
        std::cout << "\nFailed to load settings!" << std::endl;
    }
    
    std::cout << "\nSettings plugin example completed successfully!" << std::endl;
    return 0;
}

