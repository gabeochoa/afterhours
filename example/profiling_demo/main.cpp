#include "../../ah.h"
#include <iostream>
#include <thread>
#include <chrono>

void expensive_function() {
    PROFILE_SCOPE("expensive_function");
    
    // Simulate some work
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    {
        PROFILE_SCOPE("inner_loop");
        for (int i = 0; i < 1000; ++i) {
            // Some computation
            volatile int x = i * i;
            (void)x;
        }
    }
}

void another_function() {
    PROFILE_SCOPE_ARGS("another_function", "param=test");
    
    // Simulate work
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
}

int main() {
    std::cout << "AfterHours Profiling Demo\n";
    std::cout << "========================\n\n";
    
    // Initialize profiler
    if (!profiling::g_profiler.init_file("profile_demo.spall")) {
        std::cerr << "Failed to initialize profiler!\n";
        return 1;
    }
    
    std::cout << "Profiler initialized successfully!\n";
    std::cout << "Profiling data will be written to: profile_demo.spall\n\n";
    
    // Profile some functions
    std::cout << "Running profiled functions...\n";
    
    for (int i = 0; i < 5; ++i) {
        std::cout << "Iteration " << (i + 1) << "/5\n";
        
        expensive_function();
        another_function();
        
        // Flush profiling data periodically
        PROFILE_FLUSH();
    }
    
    // Shutdown profiler
    profiling::g_profiler.shutdown();
    
    std::cout << "\nProfiling completed!\n";
    std::cout << "You can now open 'profile_demo.spall' in a spall viewer\n";
    std::cout << "like https://github.com/colrdavidson/spall-web\n";
    
    return 0;
}