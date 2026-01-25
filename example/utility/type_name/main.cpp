#include <cassert>
#include <iostream>
#include <string>
#include <string_view>

#include "../../../src/type_name.h"

struct PlayerData {
  std::string name;
  int score = 0;
};

enum class Mode { Easy, Hard };

int main() {
  std::cout << "=== type_name Example ===" << std::endl;

  constexpr std::string_view int_name = type_name<int>();
  constexpr std::string_view string_name = type_name<std::string>();
  constexpr std::string_view player_name = type_name<PlayerData>();
  constexpr std::string_view mode_name = type_name<Mode>();

  static_assert(!int_name.empty());
  static_assert(!player_name.empty());

  std::cout << "int -> " << int_name << std::endl;
  std::cout << "std::string -> " << string_name << std::endl;
  std::cout << "PlayerData -> " << player_name << std::endl;
  std::cout << "Mode -> " << mode_name << std::endl;

  assert(int_name.find("int") != std::string_view::npos);

  std::cout << "Example completed successfully!" << std::endl;
  return 0;
}

