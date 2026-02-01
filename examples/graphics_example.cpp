// graphics_example.cpp
// Demonstrates the afterhours graphics module with headless rendering
//
// Build:
//   clang++ -std=c++20 -DAFTER_HOURS_USE_RAYLIB \
//     -I/opt/homebrew/Cellar/raylib/5.5/include \
//     -isystem ../.. \
//     graphics_example.cpp \
//     ../../src/graphics/graphics.cpp \
//     ../../src/graphics/raylib/raylib_backend.cpp \
//     ../../src/graphics/raylib/raylib_headless.cpp \
//     ../../src/graphics/platform/headless_gl_macos.cpp \
//     -L/opt/homebrew/Cellar/raylib/5.5/lib -lraylib \
//     -framework OpenGL -framework CoreFoundation \
//     -o graphics_example
//
// Run:
//   ./graphics_example

#include <afterhours/src/graphics/graphics.h>
#include <iostream>
#include <filesystem>

namespace graphics = afterhours::graphics;

int main() {
    std::cout << "=== Afterhours Graphics Module Example ===\n\n";

    // Configure for headless rendering
    graphics::Config config{};
    config.display = graphics::DisplayMode::Headless;
    config.width = 800;
    config.height = 600;
    config.title = "Graphics Example";
    config.target_fps = 60;
    config.time_scale = 1.0f;

    std::cout << "1. Initializing headless graphics backend...\n";
    if (!graphics::init(config)) {
        std::cerr << "   FAILED to initialize graphics backend\n";
        return 1;
    }
    std::cout << "   OK - Backend initialized\n";

    // Query backend state
    std::cout << "\n2. Querying backend state...\n";
    std::cout << "   is_headless(): " << (graphics::is_headless() ? "true" : "false") << "\n";
    std::cout << "   get_frame_count(): " << graphics::get_frame_count() << "\n";
    std::cout << "   get_delta_time(): " << graphics::get_delta_time() << " seconds\n";

    // Render a few frames
    std::cout << "\n3. Rendering frames...\n";
    for (int i = 0; i < 5; ++i) {
        graphics::begin_frame();

        // In a real app, you'd do your rendering here using raylib functions
        // The graphics module manages the render texture for you

        graphics::end_frame();
        std::cout << "   Frame " << graphics::get_frame_count() << " rendered\n";
    }

    // Capture a screenshot
    std::cout << "\n4. Capturing screenshot...\n";
    std::filesystem::path output_path = "graphics_example_output.png";

    // Render one more frame with content before capturing
    graphics::begin_frame();
    // Note: To actually draw something, you'd use raylib drawing functions here
    // This example just demonstrates the API - the image will be blank
    graphics::end_frame();

    if (graphics::capture_frame(output_path)) {
        std::cout << "   OK - Saved to: " << output_path << "\n";
    } else {
        std::cerr << "   FAILED to capture frame\n";
    }

    // Demonstrate auto-capture feature
    std::cout << "\n5. Testing auto-capture (every 2 frames)...\n";
    std::filesystem::path capture_dir = "graphics_example_captures/";
    graphics::capture_every_n_frames(2, capture_dir);

    for (int i = 0; i < 6; ++i) {
        graphics::begin_frame();
        graphics::end_frame();
    }
    std::cout << "   OK - Check " << capture_dir << " for captured frames\n";

    graphics::stop_auto_capture();

    // Get render texture (for advanced usage)
    std::cout << "\n6. Accessing render texture...\n";
    auto& rt = graphics::get_render_texture();
    std::cout << "   Render texture ID: " << rt.id << "\n";
    std::cout << "   Texture size: " << rt.texture.width << "x" << rt.texture.height << "\n";

    // Cleanup
    std::cout << "\n7. Shutting down...\n";
    graphics::shutdown();
    std::cout << "   OK - Graphics backend shut down\n";

    std::cout << "\n=== Example Complete ===\n";
    return 0;
}
