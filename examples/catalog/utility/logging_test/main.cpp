#include <cassert>
#include <iostream>

#include "../../../ah.h"

using namespace afterhours;

// Test component for logging demonstrations
struct LogTestComponent : public BaseComponent {};

int main() {
    std::cout << "=== Logging System Test ===" << std::endl;
    std::cout << "Testing all log levels and std::format formatting..." << std::endl
              << std::endl;

    // Test 1: Basic logging functions
    std::cout << "1. Testing basic log levels:" << std::endl;
    log_info("This is an info message - logging system is working!");
    log_warn("This is a warning message - used for non-critical issues");
    log_error("This is an error message - used for serious problems");

    // Test 2: Formatted logging using std::format syntax
    std::cout << std::endl << "2. Testing formatted logging with {{}} syntax:" << std::endl;
    log_info("Formatted info: Component limit is {}", max_num_components);
    log_warn("Formatted warning: Found {} potential issues", 3);
    log_error("Formatted error: Failed operation at line {} in file {}", 42, "test.cpp");

    // Test 3: Integration with component system
    std::cout << std::endl
              << "3. Testing logging integration with component system:"
              << std::endl;

    auto component_id = components::get_type_id<LogTestComponent>();
    log_info("Successfully registered LogTestComponent with ID {}", component_id);

    if (component_id < max_num_components) {
        log_info(
            "Component ID validation passed - ID {} is within bounds [0, {})",
            component_id, max_num_components);
    } else {
        log_error("Component ID validation failed - ID {} exceeds maximum {}",
                  component_id, max_num_components);
        assert(false && "Component ID out of bounds");
    }

    // Test 4: Various data types
    std::cout << std::endl << "4. Testing various data types:" << std::endl;
    log_info("Integer: {}, Float: {:.2f}, String: {}", 123, 45.67f, "test");
    log_warn("Large number: {}", 9876543210UL);
    log_error("Character: {}, Hex: {:#x}", 'A', 255);

    // Test 5: Multiple arguments
    std::cout << std::endl << "5. Testing multiple arguments:" << std::endl;
    log_info("Multi-arg test: {} has {} components with limit {}", "System", 5,
             max_num_components);

    // Test 6: Log level constants
    std::cout << std::endl << "6. Testing log level constants:" << std::endl;
    std::cout << "  VENDOR_LOG_TRACE = " << VENDOR_LOG_TRACE << std::endl;
    std::cout << "  VENDOR_LOG_INFO = " << VENDOR_LOG_INFO << std::endl;
    std::cout << "  VENDOR_LOG_WARN = " << VENDOR_LOG_WARN << std::endl;
    std::cout << "  VENDOR_LOG_ERROR = " << VENDOR_LOG_ERROR << std::endl;
    assert(VENDOR_LOG_TRACE == 1);
    assert(VENDOR_LOG_INFO == 2);
    assert(VENDOR_LOG_WARN == 3);
    assert(VENDOR_LOG_ERROR == 4);

    // Test 7: VALIDATE macro (currently a no-op)
    std::cout << std::endl << "7. Testing VALIDATE macro:" << std::endl;
    VALIDATE(true, "This should pass");
    std::cout << "  VALIDATE executed (currently a no-op)" << std::endl;

    // Test 8: log_trace (disabled by default)
    std::cout << std::endl << "8. Testing log_trace (disabled by default):" << std::endl;
    log_trace("This trace message should not appear");
    std::cout << "  log_trace called (no output expected as it's disabled)" << std::endl;

    // Test 9: log_clean (disabled by default)
    std::cout << std::endl << "9. Testing log_clean (disabled by default):" << std::endl;
    log_clean("This clean message should not appear");
    std::cout << "  log_clean called (no output expected as it's disabled)" << std::endl;

    std::cout << std::endl << "=== All Logging tests passed! ===" << std::endl;

    return 0;
}
