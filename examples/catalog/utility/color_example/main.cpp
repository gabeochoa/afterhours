#include <cassert>
#include <cmath>
#include <iostream>

#include "../../../src/plugins/color.h"

using namespace afterhours;
using namespace afterhours::colors;

// Helper to print color
void print_color(const std::string& name, const Color& c) {
    std::cout << "  " << name << ": RGBA("
              << static_cast<int>(c.r) << ", "
              << static_cast<int>(c.g) << ", "
              << static_cast<int>(c.b) << ", "
              << static_cast<int>(c.a) << ")" << std::endl;
}

// Helper for float comparison
bool approx_eq(float a, float b, float epsilon = 0.01f) {
    return std::abs(a - b) < epsilon;
}

int main() {
    std::cout << "=== Color Utilities Example ===" << std::endl;

    // Test 1: Basic color constants
    std::cout << "\n1. Basic color constants:" << std::endl;
    print_color("UI_BLACK", UI_BLACK);
    print_color("UI_WHITE", UI_WHITE);
    print_color("UI_RED", UI_RED);
    print_color("UI_GREEN", UI_GREEN);
    print_color("UI_BLUE", UI_BLUE);
    print_color("UI_PINK", UI_PINK);

    assert(UI_BLACK.r == 0 && UI_BLACK.g == 0 && UI_BLACK.b == 0);
    assert(UI_WHITE.r == 255 && UI_WHITE.g == 255 && UI_WHITE.b == 255);
    assert(UI_RED.r == 255 && UI_RED.g == 0 && UI_RED.b == 0);

    // Test 2: Named colors
    std::cout << "\n2. Named colors:" << std::endl;
    print_color("pacific_blue", pacific_blue);
    print_color("oxford_blue", oxford_blue);
    print_color("orange_soda", orange_soda);
    print_color("tea_green", tea_green);
    print_color("soft_green", soft_green);
    print_color("soft_purple", soft_purple);

    // Test 3: darken() function
    std::cout << "\n3. Color darkening:" << std::endl;
    Color dark_red = darken(UI_RED, 0.5f);
    print_color("UI_RED darkened by 0.5", dark_red);
    assert(dark_red.r == 127);  // 255 * 0.5
    assert(dark_red.a == UI_RED.a);  // Alpha preserved

    // Test 4: lighten() function
    std::cout << "\n4. Color lightening:" << std::endl;
    Color light_blue = lighten(UI_BLUE, 0.5f);
    print_color("UI_BLUE lightened by 0.5", light_blue);
    assert(light_blue.b == 255);  // Already max
    assert(light_blue.r > UI_BLUE.r);  // Other components increased
    assert(light_blue.a == UI_BLUE.a);  // Alpha preserved

    // Test 5: lerp() - linear interpolation
    std::cout << "\n5. Color interpolation (lerp):" << std::endl;
    Color mid_red_blue = lerp(UI_RED, UI_BLUE, 0.5f);
    print_color("Midpoint of RED and BLUE", mid_red_blue);
    assert(mid_red_blue.r == 128);  // Halfway from 255 to 0 (rounds to 128)
    assert(mid_red_blue.b == 127);  // Halfway from 0 to 255 (rounds to 127)

    Color quarter = lerp(UI_BLACK, UI_WHITE, 0.25f);
    print_color("25% from BLACK to WHITE", quarter);
    assert(quarter.r == 63);  // 255 * 0.25

    // Test 6: mix() - weighted blend
    std::cout << "\n6. Color mixing:" << std::endl;
    Color mixed = mix(UI_RED, UI_GREEN, 0.3f);
    print_color("30% GREEN into RED", mixed);
    // 70% red + 30% green
    assert(mixed.r == static_cast<unsigned char>(255 * 0.7f));
    assert(mixed.g == static_cast<unsigned char>(255 * 0.3f));

    // Test 7: get_opposite() - color inversion
    std::cout << "\n7. Color inversion (opposite):" << std::endl;
    Color inv_red = get_opposite(UI_RED);
    print_color("Opposite of RED", inv_red);
    assert(inv_red.r == 0 && inv_red.g == 255 && inv_red.b == 255);  // Cyan
    assert(inv_red.a == UI_RED.a);  // Alpha preserved

    // Test 8: Opacity functions
    std::cout << "\n8. Opacity manipulation:" << std::endl;
    Color semi_transparent = set_opacity(UI_RED, 128);
    print_color("RED with alpha 128", semi_transparent);
    assert(semi_transparent.a == 128);

    Color half_opacity = opacity_pct(UI_GREEN, 0.5f);
    print_color("GREEN at 50% opacity", half_opacity);
    assert(half_opacity.a == 127);  // 255 * 0.5

    // Test clamping
    Color clamped = opacity_pct(UI_BLUE, 1.5f);  // Should clamp to 1.0
    assert(clamped.a == 255);

    // Test 9: increase() function
    std::cout << "\n9. Color increase:" << std::endl;
    Color brighter = increase(Color{100, 100, 100, 255}, 50);
    print_color("Gray increased by 50", brighter);
    assert(brighter.r == 150 && brighter.g == 150 && brighter.b == 150);

    // Test 10: Luminance calculation (WCAG)
    std::cout << "\n10. Luminance calculation:" << std::endl;
    float white_lum = luminance(UI_WHITE);
    float black_lum = luminance(UI_BLACK);
    float red_lum = luminance(UI_RED);
    std::cout << "  White luminance: " << white_lum << " (should be ~1.0)" << std::endl;
    std::cout << "  Black luminance: " << black_lum << " (should be ~0.0)" << std::endl;
    std::cout << "  Red luminance: " << red_lum << std::endl;
    assert(approx_eq(white_lum, 1.0f));
    assert(approx_eq(black_lum, 0.0f));

    // Test 11: Brightness calculation
    std::cout << "\n11. Brightness calculation:" << std::endl;
    float white_bright = brightness(UI_WHITE);
    float black_bright = brightness(UI_BLACK);
    std::cout << "  White brightness: " << white_bright << std::endl;
    std::cout << "  Black brightness: " << black_bright << std::endl;
    assert(approx_eq(white_bright, 1.0f));
    assert(approx_eq(black_bright, 0.0f));

    // Test 12: is_light() and is_dark()
    std::cout << "\n12. Light/Dark classification:" << std::endl;
    std::cout << "  White is light: " << (is_light(UI_WHITE) ? "yes" : "no") << std::endl;
    std::cout << "  Black is dark: " << (is_dark(UI_BLACK) ? "yes" : "no") << std::endl;
    std::cout << "  Red is light: " << (is_light(UI_RED) ? "yes" : "no") << std::endl;
    assert(is_light(UI_WHITE));
    assert(is_dark(UI_BLACK));

    // Test 13: Contrast ratio (WCAG 2.1)
    std::cout << "\n13. Contrast ratio (WCAG 2.1):" << std::endl;
    float bw_contrast = contrast_ratio(UI_BLACK, UI_WHITE);
    std::cout << "  Black on White: " << bw_contrast << ":1 (should be ~21:1)" << std::endl;
    assert(bw_contrast >= 20.0f);  // Should be ~21:1

    float rw_contrast = contrast_ratio(UI_RED, UI_WHITE);
    std::cout << "  Red on White: " << rw_contrast << ":1" << std::endl;

    // Test 14: WCAG compliance levels
    std::cout << "\n14. WCAG compliance levels:" << std::endl;
    WCAGLevel bw_level = wcag_compliance(UI_BLACK, UI_WHITE);
    WCAGLevel rg_level = wcag_compliance(UI_RED, UI_GREEN);

    std::cout << "  Black on White: ";
    switch (bw_level) {
        case WCAGLevel::AAA: std::cout << "AAA"; break;
        case WCAGLevel::AA: std::cout << "AA"; break;
        case WCAGLevel::AAALarge: std::cout << "AAA Large"; break;
        case WCAGLevel::AALarge: std::cout << "AA Large"; break;
        case WCAGLevel::Fail: std::cout << "Fail"; break;
    }
    std::cout << std::endl;
    assert(bw_level == WCAGLevel::AAA);

    std::cout << "  Red on Green: ";
    switch (rg_level) {
        case WCAGLevel::AAA: std::cout << "AAA"; break;
        case WCAGLevel::AA: std::cout << "AA"; break;
        case WCAGLevel::AAALarge: std::cout << "AAA Large"; break;
        case WCAGLevel::AALarge: std::cout << "AA Large"; break;
        case WCAGLevel::Fail: std::cout << "Fail"; break;
    }
    std::cout << std::endl;

    // Test 15: meets_wcag_aa() and meets_wcag_aaa()
    std::cout << "\n15. WCAG compliance checks:" << std::endl;
    std::cout << "  Black on White meets AA: " << (meets_wcag_aa(UI_BLACK, UI_WHITE) ? "yes" : "no") << std::endl;
    std::cout << "  Black on White meets AAA: " << (meets_wcag_aaa(UI_BLACK, UI_WHITE) ? "yes" : "no") << std::endl;
    assert(meets_wcag_aa(UI_BLACK, UI_WHITE));
    assert(meets_wcag_aaa(UI_BLACK, UI_WHITE));

    // Test 16: auto_text_color()
    std::cout << "\n16. Auto text color selection:" << std::endl;
    Color text_on_white = auto_text_color(UI_WHITE);
    Color text_on_black = auto_text_color(UI_BLACK);
    Color text_on_blue = auto_text_color(UI_BLUE);
    print_color("Text on white background", text_on_white);
    print_color("Text on black background", text_on_black);
    print_color("Text on blue background", text_on_blue);

    // White background should get black text
    assert(text_on_white.r == UI_BLACK.r);
    // Black background should get white text
    assert(text_on_black.r == UI_WHITE.r);

    // Test 17: auto_text_color() with options
    std::cout << "\n17. Auto text color with custom options:" << std::endl;
    Color custom_light = Color{200, 200, 255, 255};  // Light blue
    Color custom_dark = Color{20, 20, 80, 255};      // Dark blue
    Color text = auto_text_color(UI_WHITE, custom_light, custom_dark);
    print_color("Best of light/dark blue on white", text);
    // Should pick dark option for white background
    assert(text.r == custom_dark.r);

    // Test 18: ensure_contrast()
    std::cout << "\n18. Ensure minimum contrast:" << std::endl;
    Color low_contrast = Color{200, 200, 200, 255};  // Light gray
    Color adjusted = ensure_contrast(low_contrast, UI_WHITE, 4.5f);
    print_color("Light gray adjusted for contrast on white", adjusted);
    float new_contrast = contrast_ratio(adjusted, UI_WHITE);
    std::cout << "  New contrast ratio: " << new_contrast << ":1" << std::endl;
    assert(new_contrast >= 4.5f);

    // Test 19: contrasting_shade()
    std::cout << "\n19. Contrasting shade generation:" << std::endl;
    Color shade = contrasting_shade(pacific_blue);
    print_color("Contrasting shade of pacific_blue", shade);
    float shade_contrast = contrast_ratio(shade, pacific_blue);
    std::cout << "  Contrast ratio: " << shade_contrast << ":1" << std::endl;

    // Test 20: Font weight suggestion
    std::cout << "\n20. Font weight suggestions:" << std::endl;
    FontWeight bw_weight = suggested_font_weight(UI_BLACK, UI_WHITE);
    FontWeight rw_weight = suggested_font_weight(UI_RED, UI_WHITE);
    std::cout << "  Black on white: weight " << static_cast<int>(bw_weight) << std::endl;
    std::cout << "  Red on white: weight " << static_cast<int>(rw_weight) << std::endl;
    assert(bw_weight == FontWeight::Light);  // High contrast allows light

    // Test 21: is_empty()
    std::cout << "\n21. Empty color check:" << std::endl;
    Color empty = {0, 0, 0, 0};
    Color not_empty = {0, 0, 0, 255};
    std::cout << "  {0,0,0,0} is empty: " << (is_empty(empty) ? "yes" : "no") << std::endl;
    std::cout << "  {0,0,0,255} is empty: " << (is_empty(not_empty) ? "yes" : "no") << std::endl;
    assert(is_empty(empty));
    assert(!is_empty(not_empty));

    // Test 22: comp_min() and comp_max()
    std::cout << "\n22. Component min/max:" << std::endl;
    Color test_color = {50, 150, 200, 255};
    std::cout << "  Color: RGB(" << 50 << ", " << 150 << ", " << 200 << ")" << std::endl;
    std::cout << "  Min component: " << static_cast<int>(comp_min(test_color)) << std::endl;
    std::cout << "  Max component: " << static_cast<int>(comp_max(test_color)) << std::endl;
    assert(comp_min(test_color) == 50);
    assert(comp_max(test_color) == 200);

    // Test 23: HasColor component
    std::cout << "\n23. HasColor component:" << std::endl;
    HasColor static_color(UI_RED);
    print_color("Static HasColor", static_color.color());
    assert(static_color.color().r == 255);
    assert(!static_color.is_dynamic);

    // Dynamic color with fetch function
    int call_count = 0;
    HasColor dynamic_color([&call_count]() {
        call_count++;
        return Color{static_cast<unsigned char>(call_count * 50), 0, 0, 255};
    });
    assert(dynamic_color.is_dynamic);
    Color c1 = dynamic_color.color();
    Color c2 = dynamic_color.color();
    std::cout << "  Dynamic color call 1: R=" << static_cast<int>(c1.r) << std::endl;
    std::cout << "  Dynamic color call 2: R=" << static_cast<int>(c2.r) << std::endl;
    assert(call_count >= 2);

    // Test set()
    static_color.set(UI_GREEN);
    assert(static_color.color().g == 255);

    std::cout << "\n=== All Color tests passed! ===" << std::endl;
    return 0;
}
