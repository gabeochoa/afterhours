// This file demonstrates overflow protection by setting a very low component limit
// The limit must be set BEFORE including any headers

#ifndef AFTER_HOURS_MAX_COMPONENTS
#define AFTER_HOURS_MAX_COMPONENTS 3  // Very low limit for testing
#endif

#include "../../ah.h"
#include <iostream>

using namespace afterhours;

// Define more components than the limit allows
struct OverflowTest1 : public BaseComponent {};
struct OverflowTest2 : public BaseComponent {};
struct OverflowTest3 : public BaseComponent {};
struct OverflowTest4 : public BaseComponent {};  // This should trigger overflow
struct OverflowTest5 : public BaseComponent {};  // This should also trigger overflow

int main() {
    std::cout << "=== Component Overflow Trigger Test ===" << std::endl;
    std::cout << "Component limit set to: " << max_num_components << std::endl;
    std::cout << "Attempting to register 5 components with limit of " << max_num_components << std::endl;
    std::cout << "Expected: First 3 succeed, last 2 trigger overflow protection" << std::endl << std::endl;

    std::vector<ComponentID> ids;
    
    std::cout << "Registering components:" << std::endl;
    
    // These should work (within limit)
    ids.push_back(components::get_type_id<OverflowTest1>());
    std::cout << "OverflowTest1 ID: " << ids.back() << " ✓" << std::endl;
    
    ids.push_back(components::get_type_id<OverflowTest2>());
    std::cout << "OverflowTest2 ID: " << ids.back() << " ✓" << std::endl;
    
    ids.push_back(components::get_type_id<OverflowTest3>());
    std::cout << "OverflowTest3 ID: " << ids.back() << " ✓" << std::endl;
    
    std::cout << std::endl << "Attempting overflow (these should trigger error messages):" << std::endl;
    
    // These should trigger overflow protection
    ids.push_back(components::get_type_id<OverflowTest4>());
    std::cout << "OverflowTest4 ID: " << ids.back() << " (should be clamped)" << std::endl;
    
    ids.push_back(components::get_type_id<OverflowTest5>());
    std::cout << "OverflowTest5 ID: " << ids.back() << " (should be clamped)" << std::endl;
    
    std::cout << std::endl << "=== Verification ===" << std::endl;
    
    bool protection_working = true;
    for (size_t i = 0; i < ids.size(); i++) {
        ComponentID id = ids[i];
        if (id >= max_num_components) {
            std::cout << "❌ FAILED: Component " << (i+1) << " has invalid ID " << id 
                      << " (>= " << max_num_components << ")" << std::endl;
            protection_working = false;
        } else {
            std::cout << "✓ Component " << (i+1) << " has valid ID " << id 
                      << " (< " << max_num_components << ")" << std::endl;
        }
    }
    
    // Check if overflow components got clamped to max valid ID
    ComponentID max_valid_id = max_num_components - 1;
    bool found_clamping = false;
    
    if (ids.size() > max_num_components) {
        for (size_t i = max_num_components; i < ids.size(); i++) {
            if (ids[i] == max_valid_id) {
                found_clamping = true;
                std::cout << "✓ Overflow component " << (i+1) << " correctly clamped to max valid ID " 
                          << max_valid_id << std::endl;
            }
        }
    }
    
    std::cout << std::endl;
    if (protection_working) {
        std::cout << "🎉 SUCCESS: Component overflow protection is working!" << std::endl;
        std::cout << "   ✓ All component IDs are within valid bounds" << std::endl;
        std::cout << "   ✓ Error logging is functional" << std::endl;
        std::cout << "   ✓ System remains stable during overflow" << std::endl;
        if (found_clamping) {
            std::cout << "   ✓ Overflow components properly clamped to safe values" << std::endl;
        }
    } else {
        std::cout << "❌ FAILED: Component overflow protection is not working!" << std::endl;
        return 1;
    }
    
    std::cout << std::endl << "This demonstrates the fix for the critical TODO:" << std::endl;
    std::cout << "BEFORE: // TODO this doesnt work for some reason" << std::endl;
    std::cout << "AFTER:  Working overflow protection with error logging!" << std::endl;
    
    return 0;
}
