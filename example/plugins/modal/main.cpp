#include <cassert>
#include <iostream>

// Minimal test of modal data structures (without full UI rendering)
// The full modal plugin requires window_manager and UI context for rendering

// Test DialogResult and ClosedBy enums directly
namespace afterhours {

enum class DialogResult {
    Pending,
    Confirmed,
    Cancelled,
    Dismissed,
    Custom,
};

enum class ClosedBy {
    Any,
    CloseRequest,
    None
};

// Simplified ModalConfig for testing builder pattern
struct ModalConfig {
    float width = 400.f;
    float height = 200.f;
    std::string title;
    bool center_on_screen = true;
    ClosedBy closed_by = ClosedBy::CloseRequest;
    bool show_close_button = true;
    int render_layer = 1000;

    ModalConfig& with_size(float w, float h) {
        width = w;
        height = h;
        return *this;
    }

    ModalConfig& with_title(const std::string& t) {
        title = t;
        return *this;
    }

    ModalConfig& with_closed_by(ClosedBy cb) {
        closed_by = cb;
        return *this;
    }

    ModalConfig& with_show_close_button(bool show) {
        show_close_button = show;
        return *this;
    }

    ModalConfig& with_render_layer(int layer) {
        render_layer = layer;
        return *this;
    }
};

// Simplified Modal component for testing
struct Modal {
    bool was_open_last_frame = false;
    DialogResult result = DialogResult::Pending;
    std::string return_value;
    ClosedBy closed_by = ClosedBy::CloseRequest;
    bool show_close_button = true;
    size_t open_order = 0;
    int render_layer = 1000;
    std::string title;
    bool pending_close = false;
    DialogResult pending_close_result = DialogResult::Pending;

    void open_with(const ModalConfig& config) {
        result = DialogResult::Pending;
        closed_by = config.closed_by;
        show_close_button = config.show_close_button;
        render_layer = config.render_layer;
        title = config.title;
        pending_close = false;
        pending_close_result = DialogResult::Pending;
    }

    void request_close(DialogResult close_result = DialogResult::Dismissed) {
        pending_close = true;
        pending_close_result = close_result;
    }
};

} // namespace afterhours

using namespace afterhours;

int main() {
    std::cout << "=== Modal Plugin Data Structures Example ===" << std::endl;

    // Test 1: DialogResult states
    std::cout << "\n1. DialogResult enum values:" << std::endl;
    DialogResult pending = DialogResult::Pending;
    DialogResult confirmed = DialogResult::Confirmed;
    DialogResult cancelled = DialogResult::Cancelled;
    DialogResult dismissed = DialogResult::Dismissed;
    DialogResult custom = DialogResult::Custom;

    std::cout << "  - Pending: " << static_cast<int>(pending) << std::endl;
    std::cout << "  - Confirmed: " << static_cast<int>(confirmed) << std::endl;
    std::cout << "  - Cancelled: " << static_cast<int>(cancelled) << std::endl;
    std::cout << "  - Dismissed: " << static_cast<int>(dismissed) << std::endl;
    std::cout << "  - Custom: " << static_cast<int>(custom) << std::endl;

    assert(pending != confirmed);
    assert(cancelled != dismissed);
    assert(static_cast<int>(pending) == 0);

    // Test 2: ClosedBy modes
    std::cout << "\n2. ClosedBy enum values:" << std::endl;
    ClosedBy any = ClosedBy::Any;
    ClosedBy close_request = ClosedBy::CloseRequest;
    ClosedBy none = ClosedBy::None;

    std::cout << "  - Any (light dismiss): " << static_cast<int>(any) << std::endl;
    std::cout << "  - CloseRequest (escape only): " << static_cast<int>(close_request) << std::endl;
    std::cout << "  - None (manual only): " << static_cast<int>(none) << std::endl;

    assert(any != close_request);
    assert(close_request != none);

    // Test 3: ModalConfig builder pattern
    std::cout << "\n3. ModalConfig builder pattern:" << std::endl;
    ModalConfig config = ModalConfig()
        .with_size(500.f, 300.f)
        .with_title("Confirm Delete")
        .with_closed_by(ClosedBy::Any)
        .with_show_close_button(true)
        .with_render_layer(2000);

    std::cout << "  - Size: " << config.width << "x" << config.height << std::endl;
    std::cout << "  - Title: " << config.title << std::endl;
    std::cout << "  - ClosedBy: " << static_cast<int>(config.closed_by) << " (Any)" << std::endl;
    std::cout << "  - Show close button: " << (config.show_close_button ? "yes" : "no") << std::endl;
    std::cout << "  - Render layer: " << config.render_layer << std::endl;

    assert(config.width == 500.f);
    assert(config.height == 300.f);
    assert(config.title == "Confirm Delete");
    assert(config.closed_by == ClosedBy::Any);
    assert(config.show_close_button == true);
    assert(config.render_layer == 2000);

    // Test 4: Modal component initialization
    std::cout << "\n4. Modal component state:" << std::endl;
    Modal modal;
    std::cout << "  - Initial result: " << static_cast<int>(modal.result) << " (Pending)" << std::endl;
    std::cout << "  - Pending close: " << (modal.pending_close ? "yes" : "no") << std::endl;
    assert(modal.result == DialogResult::Pending);
    assert(modal.pending_close == false);

    // Test 5: Modal open_with configuration
    std::cout << "\n5. Modal open_with():" << std::endl;
    modal.open_with(config);
    std::cout << "  - Title set: " << modal.title << std::endl;
    std::cout << "  - ClosedBy: " << static_cast<int>(modal.closed_by) << std::endl;
    std::cout << "  - Render layer: " << modal.render_layer << std::endl;
    assert(modal.title == "Confirm Delete");
    assert(modal.closed_by == ClosedBy::Any);
    assert(modal.render_layer == 2000);

    // Test 6: Modal request_close
    std::cout << "\n6. Modal request_close():" << std::endl;
    modal.request_close(DialogResult::Confirmed);
    std::cout << "  - Pending close: " << (modal.pending_close ? "yes" : "no") << std::endl;
    std::cout << "  - Pending result: " << static_cast<int>(modal.pending_close_result) << " (Confirmed)" << std::endl;
    assert(modal.pending_close == true);
    assert(modal.pending_close_result == DialogResult::Confirmed);

    // Test 7: Simulating modal close processing
    std::cout << "\n7. Processing close request:" << std::endl;
    if (modal.pending_close) {
        modal.result = modal.pending_close_result;
        modal.pending_close = false;
        modal.pending_close_result = DialogResult::Pending;
        std::cout << "  - Modal closed" << std::endl;
        std::cout << "  - Final result: " << static_cast<int>(modal.result) << " (Confirmed)" << std::endl;
    }
    assert(modal.result == DialogResult::Confirmed);
    assert(modal.pending_close == false);

    // Test 8: Modal with different close scenarios
    std::cout << "\n8. Different close scenarios:" << std::endl;

    // Scenario: User clicks backdrop
    Modal modal2;
    modal2.request_close(DialogResult::Dismissed);
    std::cout << "  - Backdrop click: Dismissed (" << static_cast<int>(modal2.pending_close_result) << ")" << std::endl;
    assert(modal2.pending_close_result == DialogResult::Dismissed);

    // Scenario: User presses Escape
    Modal modal3;
    modal3.request_close(DialogResult::Cancelled);
    std::cout << "  - Escape pressed: Cancelled (" << static_cast<int>(modal3.pending_close_result) << ")" << std::endl;
    assert(modal3.pending_close_result == DialogResult::Cancelled);

    // Scenario: User clicks custom button
    Modal modal4;
    modal4.request_close(DialogResult::Custom);
    std::cout << "  - Custom button: Custom (" << static_cast<int>(modal4.pending_close_result) << ")" << std::endl;
    assert(modal4.pending_close_result == DialogResult::Custom);

    // Test 9: ModalConfig defaults
    std::cout << "\n9. ModalConfig defaults:" << std::endl;
    ModalConfig defaults;
    std::cout << "  - Default width: " << defaults.width << std::endl;
    std::cout << "  - Default height: " << defaults.height << std::endl;
    std::cout << "  - Default closed_by: CloseRequest" << std::endl;
    std::cout << "  - Default render_layer: " << defaults.render_layer << std::endl;
    assert(defaults.width == 400.f);
    assert(defaults.height == 200.f);
    assert(defaults.closed_by == ClosedBy::CloseRequest);
    assert(defaults.render_layer == 1000);

    std::cout << "\n=== All modal data structure tests passed! ===" << std::endl;
    std::cout << "\nNote: Full modal rendering requires UI context and window_manager." << std::endl;
    return 0;
}
