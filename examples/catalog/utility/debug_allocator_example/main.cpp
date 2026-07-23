#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include "../../../src/debug_allocator.h"

using namespace afterhours::developer;

int main() {
    std::cout << "=== Debug Allocator Example ===" << std::endl;
    std::cout << "This allocator logs all allocations/deallocations to help debug memory usage.\n" << std::endl;

    // Test 1: Basic vector with debug allocator
    std::cout << "1. Vector with DebugAllocator<int>:" << std::endl;
    std::cout << "   Creating vector and adding elements..." << std::endl;
    {
        std::vector<int, DebugAllocator<int>> vec;

        std::cout << "   push_back(1):" << std::endl;
        vec.push_back(1);

        std::cout << "   push_back(2):" << std::endl;
        vec.push_back(2);

        std::cout << "   push_back(3):" << std::endl;
        vec.push_back(3);

        std::cout << "   Vector size: " << vec.size() << std::endl;
        assert(vec.size() == 3);
        assert(vec[0] == 1 && vec[1] == 2 && vec[2] == 3);

        std::cout << "   Vector going out of scope (deallocation)..." << std::endl;
    }

    std::cout << "\n2. Vector with reserve (single allocation):" << std::endl;
    {
        std::vector<int, DebugAllocator<int>> vec;

        std::cout << "   reserve(10):" << std::endl;
        vec.reserve(10);

        std::cout << "   Adding 5 elements (no reallocation expected)..." << std::endl;
        for (int i = 0; i < 5; i++) {
            vec.push_back(i * 10);
        }
        std::cout << "   No allocation messages above = reused reserved space" << std::endl;

        assert(vec.size() == 5);
        assert(vec.capacity() >= 10);

        std::cout << "   Cleanup:" << std::endl;
    }

    // Test 3: String allocator
    std::cout << "\n3. Vector of strings with DebugAllocator:" << std::endl;
    {
        std::vector<std::string, DebugAllocator<std::string>> strings;

        std::cout << "   reserve(3):" << std::endl;
        strings.reserve(3);

        std::cout << "   Adding strings:" << std::endl;
        strings.push_back("hello");
        strings.push_back("world");
        strings.push_back("!");

        assert(strings.size() == 3);
        assert(strings[0] == "hello");
        assert(strings[1] == "world");
        assert(strings[2] == "!");

        std::cout << "   Cleanup:" << std::endl;
    }

    // Test 4: Large allocation
    std::cout << "\n4. Large allocation test:" << std::endl;
    {
        std::vector<double, DebugAllocator<double>> large_vec;

        std::cout << "   reserve(1000):" << std::endl;
        large_vec.reserve(1000);

        std::cout << "   Allocation should show: 1000 elements * 8 bytes = 8000 bytes" << std::endl;

        assert(large_vec.capacity() >= 1000);

        std::cout << "   Cleanup:" << std::endl;
    }

    // Test 5: Multiple reallocations
    std::cout << "\n5. Multiple reallocations (exponential growth):" << std::endl;
    {
        std::vector<int, DebugAllocator<int>> growing;

        std::cout << "   Adding 20 elements to trigger multiple reallocations:" << std::endl;
        for (int i = 0; i < 20; i++) {
            growing.push_back(i);
        }

        std::cout << "   Final size: " << growing.size() << std::endl;
        std::cout << "   Final capacity: " << growing.capacity() << std::endl;
        assert(growing.size() == 20);

        std::cout << "   Cleanup:" << std::endl;
    }

    // Test 6: Copy allocator
    std::cout << "\n6. Allocator copy construction:" << std::endl;
    {
        DebugAllocator<int> alloc1;
        DebugAllocator<double> alloc2(alloc1);  // Rebind copy

        std::cout << "   Created allocator for int" << std::endl;
        std::cout << "   Created allocator for double (rebind from int allocator)" << std::endl;

        // Use the allocators
        std::vector<int, DebugAllocator<int>> vec1(alloc1);
        vec1.reserve(5);
        std::cout << "   Reserved 5 ints using alloc1" << std::endl;

        std::vector<double, DebugAllocator<double>> vec2(alloc2);
        vec2.reserve(5);
        std::cout << "   Reserved 5 doubles using alloc2" << std::endl;

        std::cout << "   Cleanup:" << std::endl;
    }

    std::cout << "\n=== All Debug Allocator tests passed! ===" << std::endl;
    std::cout << "\nNote: Green messages = allocations, Red messages = deallocations" << std::endl;

    return 0;
}
