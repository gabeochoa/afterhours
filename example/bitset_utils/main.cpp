#include "../../src/bitset_utils.h"
#include <bitset>
#include <iostream>
#include <random>

int main() {
  std::cout << "=== Bitset Utilities Example ===\n\n";

  // Create a bitset with some bits set
  std::bitset<8> bitset(0b10110100); // Binary: 10110100
  std::cout << "Initial bitset: " << bitset << " (binary)\n";
  std::cout << "Initial bitset: 0x" << std::hex << bitset.to_ulong() << std::dec
            << " (hex)\n\n";

  // Test basic operations with enum values
  enum class Color {
    Red = 0,
    Green = 1,
    Blue = 2,
    Yellow = 3,
    Purple = 4,
    Orange = 5,
    Pink = 6,
    Cyan = 7
  };

  std::cout << "=== Enum Operations ===\n";
  std::bitset<8> color_bitset;

  // Set some colors
  bitset_utils::set(color_bitset, Color::Red);
  bitset_utils::set(color_bitset, Color::Blue);
  bitset_utils::set(color_bitset, Color::Yellow);

  std::cout << "Color bitset after setting Red, Blue, Yellow: " << color_bitset
            << "\n";
  std::cout << "Is Red set? "
            << (bitset_utils::test(color_bitset, Color::Red) ? "Yes" : "No")
            << "\n";
  std::cout << "Is Green set? "
            << (bitset_utils::test(color_bitset, Color::Green) ? "Yes" : "No")
            << "\n";

  // Reset a color
  bitset_utils::reset(color_bitset, Color::Blue);
  std::cout << "After resetting Blue: " << color_bitset << "\n";
  std::cout << "Is Blue set? "
            << (bitset_utils::test(color_bitset, Color::Blue) ? "Yes" : "No")
            << "\n\n";

  // Test finding bits
  std::cout << "=== Finding Bits ===\n";
  std::cout << "First enabled bit: "
            << bitset_utils::get_first_enabled_bit(bitset) << "\n";
  std::cout << "First disabled bit: "
            << bitset_utils::get_first_disabled_bit(bitset) << "\n";
  std::cout << "Next disabled bit from position 2: "
            << bitset_utils::get_next_disabled_bit(bitset, 2) << "\n";
  std::cout << "Index of 2nd set bit: "
            << bitset_utils::index_of_nth_set_bit(bitset, 2) << "\n";
  std::cout << "Index of 5th set bit: "
            << bitset_utils::index_of_nth_set_bit(bitset, 5) << "\n\n";

  // Test random bit selection
  std::cout << "=== Random Bit Selection ===\n";
  std::random_device rd;
  std::mt19937 gen(rd());

  std::cout << "Random enabled bits (5 samples): ";
  for (int i = 0; i < 5; ++i) {
    int random_bit = bitset_utils::get_random_enabled_bit(bitset, gen);
    std::cout << random_bit << " ";
  }
  std::cout << "\n";

  std::cout << "Random disabled bits (5 samples): ";
  for (int i = 0; i < 5; ++i) {
    int random_bit = bitset_utils::get_random_disabled_bit(bitset, gen);
    std::cout << random_bit << " ";
  }
  std::cout << "\n\n";

  // Test with different bitset sizes
  std::cout << "=== Different Bitset Sizes ===\n";
  std::bitset<16> large_bitset(0b1100110011001100);
  std::cout << "16-bit bitset: " << large_bitset << "\n";
  std::cout << "First enabled bit: "
            << bitset_utils::get_first_enabled_bit(large_bitset) << "\n";
  std::cout << "First disabled bit: "
            << bitset_utils::get_first_disabled_bit(large_bitset) << "\n";

  std::bitset<4> small_bitset(0b1010);
  std::cout << "4-bit bitset: " << small_bitset << "\n";
  std::cout << "All set bit indices: ";
  for (int i = 1; i <= 2; ++i) { // We know there are 2 set bits
    int index = bitset_utils::index_of_nth_set_bit(small_bitset, i);
    if (index != -1) {
      std::cout << index << " ";
    }
  }
  std::cout << "\n\n";

  // Test edge cases
  std::cout << "=== Edge Cases ===\n";
  std::bitset<8> empty_bitset(0b00000000);
  std::bitset<8> full_bitset(0b11111111);

  std::cout << "Empty bitset: " << empty_bitset << "\n";
  std::cout << "First enabled bit in empty: "
            << bitset_utils::get_first_enabled_bit(empty_bitset) << "\n";
  std::cout << "First disabled bit in empty: "
            << bitset_utils::get_first_disabled_bit(empty_bitset) << "\n";

  std::cout << "Full bitset: " << full_bitset << "\n";
  std::cout << "First enabled bit in full: "
            << bitset_utils::get_first_enabled_bit(full_bitset) << "\n";
  std::cout << "First disabled bit in full: "
            << bitset_utils::get_first_disabled_bit(full_bitset) << "\n";

  std::cout << "\n=== Example Complete ===\n";
  return 0;
}
