#include <cassert>
#include <cmath>
#include <iostream>
#include <string>

// Minimal test of toast data structures (without full UI rendering)
// The full toast plugin requires window_manager and UI context for rendering

namespace afterhours {

struct Color {
    unsigned char r, g, b, a;
};

namespace toast_ns {

enum class Level { Info, Success, Warning, Error, Custom };

// Simplified Toast component for testing
struct Toast {
    std::string message;
    Level level = Level::Info;
    Color custom_color = {100, 100, 100, 255};
    float duration = 3.0f;
    float elapsed = 0.0f;
    bool dismissed = false;

    Toast() = default;
    Toast(Level lvl, float dur, Color color = {100, 100, 100, 255})
        : level(lvl), custom_color(color), duration(dur) {}

    [[nodiscard]] float progress() const {
        if (duration <= 0.0f)
            return 0.0f;
        return 1.0f - std::clamp(elapsed / duration, 0.0f, 1.0f);
    }

    [[nodiscard]] bool is_expired() const {
        return dismissed || elapsed >= duration;
    }

    void dismiss() { dismissed = true; }
};

// Easing function from toast.h
[[nodiscard]] float ease_out_expo(float t) {
    return t >= 1.0f ? 1.0f : 1.0f - std::pow(2.0f, -10.0f * t);
}

// Icon helper from toast.h
[[nodiscard]] std::string icon_for_level(Level level) {
    switch (level) {
    case Level::Success:
        return "[OK]";
    case Level::Warning:
        return "[!]";
    case Level::Error:
        return "[X]";
    case Level::Custom:
        return "[*]";
    case Level::Info:
    default:
        return "[i]";
    }
}

} // namespace toast_ns

} // namespace afterhours

using namespace afterhours;
using namespace afterhours::toast_ns;

int main() {
    std::cout << "=== Toast Plugin Data Structures Example ===" << std::endl;

    // Test 1: Toast Level enum
    std::cout << "\n1. Toast Level enum values:" << std::endl;
    std::cout << "  - Info: " << static_cast<int>(Level::Info) << std::endl;
    std::cout << "  - Success: " << static_cast<int>(Level::Success) << std::endl;
    std::cout << "  - Warning: " << static_cast<int>(Level::Warning) << std::endl;
    std::cout << "  - Error: " << static_cast<int>(Level::Error) << std::endl;
    std::cout << "  - Custom: " << static_cast<int>(Level::Custom) << std::endl;

    assert(static_cast<int>(Level::Info) == 0);
    assert(static_cast<int>(Level::Success) == 1);
    assert(static_cast<int>(Level::Warning) == 2);
    assert(static_cast<int>(Level::Error) == 3);
    assert(static_cast<int>(Level::Custom) == 4);

    // Test 2: Toast icons
    std::cout << "\n2. Toast icons:" << std::endl;
    std::cout << "  - Info icon: " << icon_for_level(Level::Info) << std::endl;
    std::cout << "  - Success icon: " << icon_for_level(Level::Success) << std::endl;
    std::cout << "  - Warning icon: " << icon_for_level(Level::Warning) << std::endl;
    std::cout << "  - Error icon: " << icon_for_level(Level::Error) << std::endl;
    std::cout << "  - Custom icon: " << icon_for_level(Level::Custom) << std::endl;

    assert(icon_for_level(Level::Info) == "[i]");
    assert(icon_for_level(Level::Success) == "[OK]");
    assert(icon_for_level(Level::Warning) == "[!]");
    assert(icon_for_level(Level::Error) == "[X]");
    assert(icon_for_level(Level::Custom) == "[*]");

    // Test 3: Toast component defaults
    std::cout << "\n3. Toast component defaults:" << std::endl;
    Toast toast;
    std::cout << "  - Default level: " << static_cast<int>(toast.level) << " (Info)" << std::endl;
    std::cout << "  - Default duration: " << toast.duration << "s" << std::endl;
    std::cout << "  - Elapsed: " << toast.elapsed << "s" << std::endl;
    std::cout << "  - Dismissed: " << (toast.dismissed ? "yes" : "no") << std::endl;

    assert(toast.level == Level::Info);
    assert(toast.duration == 3.0f);
    assert(toast.elapsed == 0.0f);
    assert(!toast.dismissed);

    // Test 4: Toast progress calculation
    std::cout << "\n4. Toast progress (time remaining):" << std::endl;
    Toast progress_test(Level::Info, 4.0f);

    progress_test.elapsed = 0.0f;
    std::cout << "  - At start (0.0s): " << (progress_test.progress() * 100) << "%" << std::endl;
    assert(std::abs(progress_test.progress() - 1.0f) < 0.001f);

    progress_test.elapsed = 1.0f;
    std::cout << "  - At 1.0s: " << (progress_test.progress() * 100) << "%" << std::endl;
    assert(std::abs(progress_test.progress() - 0.75f) < 0.001f);

    progress_test.elapsed = 2.0f;
    std::cout << "  - At 2.0s: " << (progress_test.progress() * 100) << "%" << std::endl;
    assert(std::abs(progress_test.progress() - 0.5f) < 0.001f);

    progress_test.elapsed = 4.0f;
    std::cout << "  - At end (4.0s): " << (progress_test.progress() * 100) << "%" << std::endl;
    assert(std::abs(progress_test.progress() - 0.0f) < 0.001f);

    // Test 5: Toast expiration
    std::cout << "\n5. Toast expiration:" << std::endl;
    Toast expire_test(Level::Success, 3.0f);

    expire_test.elapsed = 1.5f;
    std::cout << "  - Halfway (1.5s): expired=" << (expire_test.is_expired() ? "yes" : "no") << std::endl;
    assert(!expire_test.is_expired());

    expire_test.elapsed = 3.0f;
    std::cout << "  - At duration (3.0s): expired=" << (expire_test.is_expired() ? "yes" : "no") << std::endl;
    assert(expire_test.is_expired());

    expire_test.elapsed = 5.0f;
    std::cout << "  - Past duration (5.0s): expired=" << (expire_test.is_expired() ? "yes" : "no") << std::endl;
    assert(expire_test.is_expired());

    // Test 6: Toast dismiss
    std::cout << "\n6. Toast dismiss (manual close):" << std::endl;
    Toast dismiss_test(Level::Warning, 10.0f);
    dismiss_test.elapsed = 1.0f;

    std::cout << "  - Before dismiss: expired=" << (dismiss_test.is_expired() ? "yes" : "no") << std::endl;
    assert(!dismiss_test.is_expired());

    dismiss_test.dismiss();
    std::cout << "  - After dismiss: expired=" << (dismiss_test.is_expired() ? "yes" : "no") << std::endl;
    assert(dismiss_test.is_expired());
    assert(dismiss_test.dismissed);

    // Test 7: Easing function for animations
    std::cout << "\n7. Ease-out-expo animation curve:" << std::endl;
    std::cout << "  - ease(0.0): " << ease_out_expo(0.0f) << " (start)" << std::endl;
    std::cout << "  - ease(0.25): " << ease_out_expo(0.25f) << std::endl;
    std::cout << "  - ease(0.5): " << ease_out_expo(0.5f) << std::endl;
    std::cout << "  - ease(0.75): " << ease_out_expo(0.75f) << std::endl;
    std::cout << "  - ease(1.0): " << ease_out_expo(1.0f) << " (end)" << std::endl;

    assert(ease_out_expo(0.0f) == 0.0f); // Starts at 0
    assert(ease_out_expo(0.5f) > 0.9f);  // Already high at midpoint
    assert(ease_out_expo(1.0f) == 1.0f); // Ends at 1.0

    // Test 8: Toast with custom color
    std::cout << "\n8. Toast with custom color:" << std::endl;
    Color custom_color = {255, 128, 0, 255}; // Orange
    Toast custom_toast(Level::Custom, 5.0f, custom_color);

    std::cout << "  - Level: Custom" << std::endl;
    std::cout << "  - Color: RGBA(" << (int)custom_toast.custom_color.r << ", "
              << (int)custom_toast.custom_color.g << ", "
              << (int)custom_toast.custom_color.b << ", "
              << (int)custom_toast.custom_color.a << ")" << std::endl;
    std::cout << "  - Duration: " << custom_toast.duration << "s" << std::endl;

    assert(custom_toast.level == Level::Custom);
    assert(custom_toast.custom_color.r == 255);
    assert(custom_toast.custom_color.g == 128);
    assert(custom_toast.duration == 5.0f);

    // Test 9: Simulating toast update loop
    std::cout << "\n9. Simulating toast update loop:" << std::endl;
    Toast sim_toast(Level::Error, 2.0f);
    sim_toast.message = "Error: Connection failed";

    float dt = 0.5f;  // 500ms per frame
    int frame = 0;

    std::cout << "  Message: " << icon_for_level(sim_toast.level) << " " << sim_toast.message << std::endl;
    while (!sim_toast.is_expired()) {
        std::cout << "  Frame " << frame << ": elapsed=" << sim_toast.elapsed
                  << "s, progress=" << (sim_toast.progress() * 100) << "%" << std::endl;
        sim_toast.elapsed += dt;
        frame++;
    }
    std::cout << "  Toast expired after " << frame << " frames" << std::endl;

    assert(frame == 4);  // 2.0s / 0.5s = 4 frames

    // Test 10: Zero duration edge case
    std::cout << "\n10. Zero duration edge case:" << std::endl;
    Toast zero_toast(Level::Info, 0.0f);
    std::cout << "  - Duration: " << zero_toast.duration << "s" << std::endl;
    std::cout << "  - Progress: " << zero_toast.progress() << std::endl;
    std::cout << "  - Is expired: " << (zero_toast.is_expired() ? "yes" : "no") << std::endl;

    assert(zero_toast.progress() == 0.0f);  // Returns 0 for zero duration
    assert(zero_toast.is_expired());  // Immediately expired

    std::cout << "\n=== All toast data structure tests passed! ===" << std::endl;
    std::cout << "\nNote: Full toast rendering requires UI context and window_manager." << std::endl;
    return 0;
}
