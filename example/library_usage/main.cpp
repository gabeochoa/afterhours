#include <expected>
#include <iostream>
#include <memory>
#include <string>

#include "../../src/library.h"
#include "../../src/type_name.h"

// Example resource types
struct Texture {
  std::string filename;
  int width, height;

  Texture() : filename(""), width(0), height(0) {}
  Texture(const std::string &fname, int w, int h)
      : filename(fname), width(w), height(h) {}
};

struct Sound {
  std::string filename;
  float duration;

  Sound() : filename(""), duration(0.0f) {}
  Sound(const std::string &fname, float dur) : filename(fname), duration(dur) {}
};

// Example library implementations
class TextureLibrary : public Library<Texture> {
public:
  Texture convert_filename_to_object(const char *name,
                                     const char *filename) override {
    // Simulate loading texture metadata
    return Texture(filename, 128, 128); // Default size
  }

  void unload(Texture texture) override {
    // Simulate texture cleanup
    std::cout << "Unloading texture: " << texture.filename << std::endl;
  }
};

class SoundLibrary : public Library<Sound> {
public:
  Sound convert_filename_to_object(const char *name,
                                   const char *filename) override {
    // Simulate loading sound metadata
    return Sound(filename, 3.5f); // Default duration
  }

  void unload(Sound sound) override {
    // Simulate sound cleanup
    std::cout << "Unloading sound: " << sound.filename << std::endl;
  }
};

int main() {
  std::cout << "=== Afterhours Library Template Example ===" << std::endl;
  std::cout << std::endl;

  // Create library instances
  TextureLibrary textureLib;
  SoundLibrary soundLib;

  // Add some textures
  std::cout << "Adding textures..." << std::endl;
  auto result1 =
      textureLib.add("player_idle", Texture("player_idle.png", 64, 64));
  auto result2 =
      textureLib.add("player_walk", Texture("player_walk.png", 64, 64));
  auto result3 = textureLib.add("enemy_sprite", Texture("enemy.png", 32, 32));

  if (result1) {
    std::cout << "  Added texture: " << *result1 << std::endl;
  }
  if (result2) {
    std::cout << "  Added texture: " << *result2 << std::endl;
  }
  if (result3) {
    std::cout << "  Added texture: " << *result3 << std::endl;
  }

  // Add some sounds
  std::cout << std::endl << "Adding sounds..." << std::endl;
  auto soundResult1 = soundLib.add("jump", Sound("jump.wav", 0.5f));
  auto soundResult2 = soundLib.add("collect", Sound("collect.wav", 0.3f));

  if (soundResult1) {
    std::cout << "  Added sound: " << *soundResult1 << std::endl;
  }
  if (soundResult2) {
    std::cout << "  Added sound: " << *soundResult2 << std::endl;
  }

  // Test contains
  std::cout << std::endl << "Checking if resources exist..." << std::endl;
  std::cout << "  Has 'player_idle'? " << std::boolalpha
            << textureLib.contains("player_idle") << std::endl;
  std::cout << "  Has 'missing_texture'? " << std::boolalpha
            << textureLib.contains("missing_texture") << std::endl;
  std::cout << "  Has 'jump' sound? " << std::boolalpha
            << soundLib.contains("jump") << std::endl;

  // Test get
  std::cout << std::endl << "Retrieving resources..." << std::endl;
  try {
    auto &playerTexture = textureLib.get("player_idle");
    std::cout << "  Player texture: " << playerTexture.filename << " ("
              << playerTexture.width << "x" << playerTexture.height << ")"
              << std::endl;
  } catch (const std::exception &e) {
    std::cout << "  Error getting player texture: " << e.what() << std::endl;
  }

  try {
    auto &jumpSound = soundLib.get("jump");
    std::cout << "  Jump sound: " << jumpSound.filename
              << " (duration: " << jumpSound.duration << "s)" << std::endl;
  } catch (const std::exception &e) {
    std::cout << "  Error getting jump sound: " << e.what() << std::endl;
  }

  // Test prefix lookup
  std::cout << std::endl << "Testing prefix lookup..." << std::endl;
  auto playerTextures = textureLib.lookup("player");
  std::cout << "  Textures starting with 'player':" << std::endl;
  for (auto it = playerTextures.first; it != playerTextures.second; ++it) {
    std::cout << "    " << it->first << " -> " << it->second.filename
              << std::endl;
  }

  // Test random match
  std::cout << std::endl << "Testing random match..." << std::endl;
  auto randomPlayer = textureLib.get_random_match("player");
  if (randomPlayer) {
    std::cout << "  Random player texture: " << randomPlayer->filename
              << std::endl;
  } else {
    std::cout << "  No player textures found" << std::endl;
  }

  // Test library size and iteration
  std::cout << std::endl << "Library statistics..." << std::endl;
  std::cout << "  Texture library size: " << textureLib.size() << std::endl;
  std::cout << "  Sound library size: " << soundLib.size() << std::endl;

  std::cout << std::endl << "All textures:" << std::endl;
  for (const auto &[name, texture] : textureLib) {
    std::cout << "  " << name << " -> " << texture.filename << std::endl;
  }

  // Test load method
  std::cout << std::endl << "Testing load method..." << std::endl;
  textureLib.load("background.png", "background");
  std::cout << "  Loaded background texture" << std::endl;

  // Test duplicate handling
  std::cout << std::endl << "Testing duplicate handling..." << std::endl;
  auto duplicateResult =
      textureLib.add("player_idle", Texture("duplicate.png", 64, 64));
  if (!duplicateResult) {
    std::cout << "  Correctly rejected duplicate 'player_idle'" << std::endl;
  }

  // Cleanup
  std::cout << std::endl << "Cleaning up..." << std::endl;
  textureLib.unload_all();
  soundLib.unload_all();

  std::cout << std::endl << "Example completed successfully!" << std::endl;
  return 0;
}
