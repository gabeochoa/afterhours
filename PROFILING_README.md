# AfterHours Profiling System

The AfterHours library now includes an integrated profiling system based on [spall](https://github.com/colrdavidson/spall-web) that allows you to profile your application with minimal overhead.

## Features

- **Conditional Compilation**: Profiling is automatically enabled in DEBUG builds and disabled in RELEASE builds
- **Zero Overhead**: When disabled, all profiling calls become no-ops
- **Thread Safe**: Built-in thread safety for multi-threaded applications
- **RAII Support**: Automatic scope-based profiling with `PROFILE_SCOPE`
- **Flexible**: Manual begin/end profiling or automatic scope-based profiling

## Build Configuration

The profiling system automatically configures itself based on your build flags:

- **DEBUG builds**: Profiling enabled automatically
- **RELEASE builds**: Profiling disabled automatically
- **Manual override**: Define `ENABLE_PROFILING` to force enable/disable

### Example Build Commands

```bash
# Debug build with profiling enabled
g++ -DDEBUG -std=c++17 your_app.cpp -o your_app

# Release build with profiling disabled
g++ -DRELEASE -std=c++17 your_app.cpp -o your_app

# Force enable profiling
g++ -DENABLE_PROFILING -std=c++17 your_app.cpp -o your_app
```

## Usage

### Basic Setup

```cpp
#include "ah.h"

int main() {
    // Initialize profiler (only works when profiling is enabled)
    if (!profiling::g_profiler.init_file("profile.spall")) {
        std::cerr << "Failed to initialize profiler\n";
        return 1;
    }
    
    // Your application code here...
    
    // Shutdown profiler
    profiling::g_profiler.shutdown();
    return 0;
}
```

### Automatic Scope-Based Profiling (Recommended)

```cpp
void expensive_function() {
    PROFILE_SCOPE("expensive_function");
    
    // This entire function will be profiled automatically
    // The profile scope ends when the function returns
}

void function_with_args() {
    PROFILE_SCOPE_ARGS("function_with_args", "param=value");
    
    // Function with additional context
}
```

### Manual Profiling

```cpp
void manual_profiling() {
    PROFILE_BEGIN("manual_profiling");
    
    // Do some work...
    
    PROFILE_END();
}
```

### Periodic Flushing

```cpp
void main_loop() {
    for (int i = 0; i < 1000; ++i) {
        // Do work...
        
        if (i % 100 == 0) {
            PROFILE_FLUSH(); // Flush profiling data periodically
        }
    }
}
```

## Available Macros

- `PROFILE_SCOPE(name)`: Creates a profile scope for the current block
- `PROFILE_SCOPE_ARGS(name, args)`: Creates a profile scope with additional arguments
- `PROFILE_BEGIN(name)`: Manually begin profiling an event
- `PROFILE_END()`: Manually end profiling an event
- `PROFILE_FLUSH()`: Flush profiling data to disk

## Viewing Results

The profiler generates `.spall` files that can be viewed with:

1. **spall-web**: Web-based viewer at https://github.com/colrdavidson/spall-web
2. **spall**: Command-line tool for processing spall files

## Performance Impact

- **When enabled**: Minimal overhead (~10-50ns per profiling call)
- **When disabled**: Zero overhead (all calls become no-ops)
- **Memory**: Uses a 64KB buffer per thread when profiling is active

## Thread Safety

The profiling system is fully thread-safe and automatically handles:
- Thread ID detection
- Process ID detection
- Buffer management per thread
- Mutex-protected operations

## Example

See `example/profiling_demo/main.cpp` for a complete working example.

## Integration with Existing Code

The profiling system integrates seamlessly with existing AfterHours code. Simply add profiling calls where needed:

```cpp
// Before
void update_system() {
    // Update logic...
}

// After
void update_system() {
    PROFILE_SCOPE("update_system");
    // Update logic...
}
```

No other changes are required - the profiling will automatically work in debug builds and be completely disabled in release builds.