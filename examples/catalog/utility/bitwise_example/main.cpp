#include <cassert>
#include <iostream>

#include "../../../src/bitwise.h"

// Define a flags enum for testing
enum struct Flags : int {
    None = 0,
    Read = 1 << 0,    // 1
    Write = 1 << 1,   // 2
    Execute = 1 << 2, // 4
    Admin = 1 << 3,   // 8
    All = Read | Write | Execute | Admin
};

int main() {
    std::cout << "=== Bitwise Operations for Enums Example ===" << std::endl;

    // Test 1: OR operator (|)
    std::cout << "\n1. OR operator (|):" << std::endl;
    Flags rw = Flags::Read | Flags::Write;
    std::cout << "  Read | Write = " << static_cast<int>(rw) << std::endl;
    assert(static_cast<int>(rw) == 3);  // 1 | 2 = 3

    Flags rwx = rw | Flags::Execute;
    std::cout << "  (Read | Write) | Execute = " << static_cast<int>(rwx) << std::endl;
    assert(static_cast<int>(rwx) == 7);  // 3 | 4 = 7

    // Test 2: AND operator (&) with auto_bool
    std::cout << "\n2. AND operator (&) with auto_bool:" << std::endl;
    Flags permissions = Flags::Read | Flags::Write;

    // auto_bool allows implicit conversion to bool
    if (permissions & Flags::Read) {
        std::cout << "  Has Read permission: yes" << std::endl;
    } else {
        assert(false && "Should have Read permission");
    }

    if (permissions & Flags::Execute) {
        assert(false && "Should not have Execute permission");
    } else {
        std::cout << "  Has Execute permission: no" << std::endl;
    }

    // Test 3: XOR operator (^)
    std::cout << "\n3. XOR operator (^):" << std::endl;
    Flags toggle = Flags::Read ^ Flags::Write;
    std::cout << "  Read ^ Write = " << static_cast<int>(toggle) << std::endl;
    assert(static_cast<int>(toggle) == 3);  // 1 ^ 2 = 3

    // XOR with same value cancels out
    Flags cancelled = toggle ^ Flags::Read;
    std::cout << "  (Read ^ Write) ^ Read = " << static_cast<int>(cancelled) << std::endl;
    assert(static_cast<int>(cancelled) == 2);  // 3 ^ 1 = 2 (Write only)

    // Test 4: NOT operator (~)
    std::cout << "\n4. NOT operator (~):" << std::endl;
    Flags inverted = ~Flags::Read;
    std::cout << "  ~Read = " << static_cast<int>(inverted) << std::endl;
    // ~1 = -2 in two's complement (all bits except bit 0)
    assert(static_cast<int>(inverted) == ~1);

    // Test 5: OR assignment (|=)
    std::cout << "\n5. OR assignment (|=):" << std::endl;
    Flags flags = Flags::Read;
    std::cout << "  Initial flags: " << static_cast<int>(flags) << std::endl;
    flags |= Flags::Write;
    std::cout << "  After |= Write: " << static_cast<int>(flags) << std::endl;
    assert(static_cast<int>(flags) == 3);
    flags |= Flags::Execute;
    std::cout << "  After |= Execute: " << static_cast<int>(flags) << std::endl;
    assert(static_cast<int>(flags) == 7);

    // Test 6: AND assignment (&=)
    std::cout << "\n6. AND assignment (&=):" << std::endl;
    Flags mask = Flags::Read | Flags::Write | Flags::Execute;
    std::cout << "  Initial: " << static_cast<int>(mask) << std::endl;
    mask &= (Flags::Read | Flags::Write);  // Remove Execute
    std::cout << "  After &= (Read | Write): " << static_cast<int>(mask) << std::endl;
    assert(static_cast<int>(mask) == 3);  // Only Read and Write remain

    // Test 7: XOR assignment (^=)
    std::cout << "\n7. XOR assignment (^=):" << std::endl;
    Flags toggleFlags = Flags::Read | Flags::Write;
    std::cout << "  Initial: " << static_cast<int>(toggleFlags) << std::endl;
    toggleFlags ^= Flags::Write;  // Toggle off Write
    std::cout << "  After ^= Write: " << static_cast<int>(toggleFlags) << std::endl;
    assert(static_cast<int>(toggleFlags) == 1);  // Only Read
    toggleFlags ^= Flags::Write;  // Toggle on Write
    std::cout << "  After ^= Write again: " << static_cast<int>(toggleFlags) << std::endl;
    assert(static_cast<int>(toggleFlags) == 3);  // Read and Write

    // Test 8: auto_bool explicit bool conversion
    std::cout << "\n8. auto_bool explicit bool conversion:" << std::endl;
    Flags check = Flags::Read | Flags::Execute;

    auto result_read = check & Flags::Read;
    auto result_write = check & Flags::Write;

    std::cout << "  Has Read: " << (static_cast<bool>(result_read) ? "yes" : "no") << std::endl;
    std::cout << "  Has Write: " << (static_cast<bool>(result_write) ? "yes" : "no") << std::endl;
    assert(static_cast<bool>(result_read) == true);
    assert(static_cast<bool>(result_write) == false);

    // Test 9: Combining operations
    std::cout << "\n9. Combining operations:" << std::endl;
    Flags combined = (Flags::Read | Flags::Write) ^ Flags::Admin;
    std::cout << "  (Read | Write) ^ Admin = " << static_cast<int>(combined) << std::endl;
    assert(static_cast<int>(combined) == 11);  // 3 ^ 8 = 11

    // Check individual flags
    bool hasRead = static_cast<bool>(combined & Flags::Read);
    bool hasWrite = static_cast<bool>(combined & Flags::Write);
    bool hasExecute = static_cast<bool>(combined & Flags::Execute);
    bool hasAdmin = static_cast<bool>(combined & Flags::Admin);

    std::cout << "  Read: " << hasRead << ", Write: " << hasWrite
              << ", Execute: " << hasExecute << ", Admin: " << hasAdmin << std::endl;
    assert(hasRead && hasWrite && !hasExecute && hasAdmin);

    // Test 10: Using None and All
    std::cout << "\n10. Using None and All:" << std::endl;
    Flags none = Flags::None;
    std::cout << "  None = " << static_cast<int>(none) << std::endl;
    assert(static_cast<int>(none) == 0);

    // Note: Flags::All already uses | in its definition, which works with these operators
    Flags all = Flags::Read | Flags::Write | Flags::Execute | Flags::Admin;
    std::cout << "  All flags combined = " << static_cast<int>(all) << std::endl;
    assert(static_cast<int>(all) == 15);  // 1 + 2 + 4 + 8 = 15

    // Test 11: Removing flags using AND with NOT
    std::cout << "\n11. Removing flags with AND + NOT:" << std::endl;
    Flags allFlags = Flags::Read | Flags::Write | Flags::Execute;
    std::cout << "  Initial: " << static_cast<int>(allFlags) << std::endl;
    allFlags &= ~Flags::Write;  // Remove Write permission
    std::cout << "  After removing Write: " << static_cast<int>(allFlags) << std::endl;
    assert(!static_cast<bool>(allFlags & Flags::Write));  // Write should be gone
    assert(static_cast<bool>(allFlags & Flags::Read));    // Read still present
    assert(static_cast<bool>(allFlags & Flags::Execute)); // Execute still present

    std::cout << "\n=== All Bitwise tests passed! ===" << std::endl;
    return 0;
}
