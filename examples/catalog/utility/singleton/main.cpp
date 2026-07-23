#include <cassert>
#include <iostream>
#include <string>

#include "../../../src/singleton.h"

// Example 1: Basic singleton using SINGLETON macro
SINGLETON_FWD(GameConfig)
struct GameConfig {
    int screen_width = 1920;
    int screen_height = 1080;
    bool fullscreen = false;
    float volume = 0.8f;

    SINGLETON(GameConfig)

    void print() const {
        std::cout << "  - Resolution: " << screen_width << "x" << screen_height << std::endl;
        std::cout << "  - Fullscreen: " << (fullscreen ? "yes" : "no") << std::endl;
        std::cout << "  - Volume: " << volume << std::endl;
    }
};

// Example 2: Parameterized singleton using SINGLETON_PARAM macro
SINGLETON_FWD(DatabaseConnection)
struct DatabaseConnection {
    std::string connection_string;
    bool connected = false;

    struct Config {
        std::string host;
        int port;
        std::string database;
    };

    explicit DatabaseConnection(const Config& config) {
        connection_string = config.host + ":" + std::to_string(config.port) + "/" + config.database;
        connected = true;
    }

    SINGLETON_PARAM(DatabaseConnection, Config)

    void print() const {
        std::cout << "  - Connection: " << connection_string << std::endl;
        std::cout << "  - Status: " << (connected ? "connected" : "disconnected") << std::endl;
    }
};

// Example 3: Another basic singleton for testing multiple singletons
SINGLETON_FWD(AudioManager)
struct AudioManager {
    bool initialized = false;
    int active_channels = 0;

    SINGLETON(AudioManager)

    void init() {
        initialized = true;
        active_channels = 32;
    }
};

int main() {
    std::cout << "=== Singleton Pattern Example ===" << std::endl;

    // Test 1: Basic singleton creation
    std::cout << "\n1. Basic singleton (GameConfig):" << std::endl;
    GameConfig& config1 = GameConfig::get();
    config1.print();
    assert(config1.screen_width == 1920);
    assert(config1.screen_height == 1080);

    // Test 2: Singleton returns same instance
    std::cout << "\n2. Verifying same instance:" << std::endl;
    GameConfig& config2 = GameConfig::get();
    std::cout << "  - config1 address: " << &config1 << std::endl;
    std::cout << "  - config2 address: " << &config2 << std::endl;
    assert(&config1 == &config2);
    std::cout << "  - Same instance: yes" << std::endl;

    // Test 3: Modify singleton state
    std::cout << "\n3. Modifying singleton state:" << std::endl;
    config1.fullscreen = true;
    config1.volume = 0.5f;
    std::cout << "  - Modified via config1" << std::endl;
    std::cout << "  - Reading via config2:" << std::endl;
    std::cout << "    Fullscreen: " << (config2.fullscreen ? "yes" : "no") << std::endl;
    std::cout << "    Volume: " << config2.volume << std::endl;
    assert(config2.fullscreen == true);
    assert(config2.volume == 0.5f);

    // Test 4: create() is same as get() for basic singleton
    std::cout << "\n4. create() vs get():" << std::endl;
    GameConfig& config3 = GameConfig::create();
    std::cout << "  - create() address: " << &config3 << std::endl;
    assert(&config3 == &config1);
    std::cout << "  - create() returns same instance as get()" << std::endl;

    // Test 5: Parameterized singleton
    std::cout << "\n5. Parameterized singleton (DatabaseConnection):" << std::endl;
    DatabaseConnection::Config db_config{
        .host = "localhost",
        .port = 5432,
        .database = "gamedata"
    };
    DatabaseConnection* db1 = DatabaseConnection::create(db_config);
    db1->print();
    assert(db1->connected);
    assert(db1->connection_string == "localhost:5432/gamedata");

    // Test 6: Parameterized singleton returns same instance
    std::cout << "\n6. Parameterized singleton - same instance:" << std::endl;
    DatabaseConnection& db2 = DatabaseConnection::get();
    std::cout << "  - db1 address: " << db1 << std::endl;
    std::cout << "  - db2 address: " << &db2 << std::endl;
    assert(db1 == &db2);
    std::cout << "  - Same instance: yes" << std::endl;

    // Test 7: Multiple singletons coexist
    std::cout << "\n7. Multiple singletons coexist:" << std::endl;
    AudioManager& audio = AudioManager::get();
    audio.init();
    std::cout << "  - AudioManager initialized: " << (audio.initialized ? "yes" : "no") << std::endl;
    std::cout << "  - Active channels: " << audio.active_channels << std::endl;

    // Verify other singletons unaffected
    assert(GameConfig::get().fullscreen == true);
    assert(DatabaseConnection::get().connected == true);
    std::cout << "  - Other singletons unaffected: yes" << std::endl;

    // Test 8: Singleton state persists
    std::cout << "\n8. State persistence test:" << std::endl;
    {
        // Inner scope
        GameConfig& inner_config = GameConfig::get();
        inner_config.screen_width = 2560;
        inner_config.screen_height = 1440;
    }
    // State persists after scope exit
    std::cout << "  - After inner scope: " << GameConfig::get().screen_width << "x"
              << GameConfig::get().screen_height << std::endl;
    assert(GameConfig::get().screen_width == 2560);
    assert(GameConfig::get().screen_height == 1440);

    std::cout << "\n=== All singleton tests passed! ===" << std::endl;
    return 0;
}
