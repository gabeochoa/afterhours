#pragma once

#include "../../logging.h"
#include <string>

namespace afterhours {
namespace ui {

// Severity mode for validation checks
enum struct ValidationMode {
  Silent, // No checks (production default)
  Warn,   // Log warnings for violations (development default)
  Strict  // Assert/fail on violations (testing mode)
};

// Centralized configuration for all UI design rule enforcement
struct ValidationConfig {
  ValidationMode mode = ValidationMode::Silent;

  // === Spacing & Layout (Design Rules Section A) ===
  // Enforce 4/8/16-based spacing rhythm for margins and padding
  bool enforce_spacing_rhythm = false;
  // Enforce no fractional pixel positions
  bool enforce_pixel_alignment = false;

  // === Screen Safety (Design Rules Section C) ===
  // Ensure all elements stay within safe area margins
  bool enforce_screen_bounds = false;
  // Minimum distance from screen edges in pixels
  float safe_area_margin = 16.0f;

  // === Container Integrity (Design Rules Section D) ===
  // Ensure children stay within parent bounds
  bool enforce_child_containment = false;
  // Flag content that would overflow its container
  bool enforce_overflow_detection = false;

  // === Accessibility (Design Rules Section E) ===
  // Enforce WCAG AA minimum contrast ratio
  bool enforce_contrast_ratio = false;
  // Minimum contrast ratio threshold (4.5 = WCAG AA for normal text)
  float min_contrast_ratio = 4.5f;

  // === Typography (Design Rules Section F) ===
  // Enforce minimum font size for readability
  bool enforce_min_font_size = false;
  // Minimum font size in pixels
  float min_font_size = 14.0f;

  // === Resolution Independence (Design Rules Section G) ===
  // Flag components using Dim::Pixels instead of resolution-relative units
  // (screen_pct, h720, percent, etc.)
  bool enforce_resolution_independence = false;
  // Pixel values at or below this threshold are allowed (e.g. 1-2px borders)
  float resolution_independence_pixel_threshold = 4.0f;

  // === Debug Helpers ===
  // Draw red borders around elements with violations
  bool highlight_violations = false;
  // Dump component tree when a violation is detected
  bool log_component_tree = false;

  // ============================================================
  // Preset configurations
  // ============================================================

  // Enable all validations in warn mode (good for development)
  ValidationConfig &enable_development_mode() {
    mode = ValidationMode::Warn;
    enforce_screen_bounds = true;
    enforce_child_containment = true;
    enforce_contrast_ratio = true;
    enforce_min_font_size = true;
    enforce_resolution_independence = true;
    highlight_violations = true;
    return *this;
  }

  // Enable all validations in strict mode (good for testing)
  ValidationConfig &enable_strict_mode() {
    mode = ValidationMode::Strict;
    enforce_spacing_rhythm = true;
    enforce_pixel_alignment = true;
    enforce_screen_bounds = true;
    enforce_child_containment = true;
    enforce_overflow_detection = true;
    enforce_contrast_ratio = true;
    enforce_min_font_size = true;
    enforce_resolution_independence = true;
    return *this;
  }

  // TV-safe configuration (accounts for overscan)
  ValidationConfig &enable_tv_safe_mode() {
    mode = ValidationMode::Warn;
    enforce_screen_bounds = true;
    safe_area_margin = 32.0f; // Larger margin for TV overscan
    return *this;
  }

  // Check if any validation is enabled
  [[nodiscard]] bool any_enabled() const {
    return enforce_spacing_rhythm || enforce_pixel_alignment ||
           enforce_screen_bounds || enforce_child_containment ||
           enforce_overflow_detection || enforce_contrast_ratio ||
           enforce_min_font_size || enforce_resolution_independence;
  }

  // Check if mode allows logging
  [[nodiscard]] bool should_log() const {
    return mode == ValidationMode::Warn || mode == ValidationMode::Strict;
  }

  // Check if mode requires assertion/failure
  [[nodiscard]] bool should_assert() const {
    return mode == ValidationMode::Strict;
  }
};

// ============================================================
// Violation tracking component
// ============================================================

// Component added to entities that have validation violations
struct ValidationViolation : BaseComponent {
  std::string message;
  std::string category;
  float severity = 1.0f; // 0.0 = minor, 1.0 = critical

  ValidationViolation() = default;
  ValidationViolation(const std::string &msg, const std::string &cat,
                      float sev = 1.0f)
      : message(msg), category(cat), severity(sev) {}
};

// ============================================================
// Violation reporting utilities
// ============================================================

namespace validation {

// Report a validation violation based on current config
inline void report_violation(const ValidationConfig &config,
                             const std::string &category,
                             const std::string &message, EntityID entity_id,
                             float severity = 1.0f) {
  if (config.mode == ValidationMode::Silent) {
    return;
  }

  std::string full_message = "[UI Validation] " + category + ": " + message +
                             " (entity: " + std::to_string(entity_id) + ")";

  if (config.mode == ValidationMode::Warn) {
    // Warn mode: always use log_warn (never asserts)
    log_warn("{}", full_message);
  } else if (config.mode == ValidationMode::Strict) {
    // Strict mode: use log_error which will assert
    log_error("{}", full_message);
  }

  if (config.should_assert()) {
    log_error("STRICT MODE: {}", full_message);
    // In strict mode, we log as error but don't actually assert
    // This allows collecting all violations before failing
  }
}

// Check if a value follows 4/8/16 spacing rhythm
inline bool is_valid_spacing(float value) {
  // Allow 0 and values divisible by 4
  if (value == 0.0f)
    return true;
  float remainder = std::fmod(value, 4.0f);
  return remainder < 0.001f || remainder > 3.999f;
}

// Check if a position is pixel-aligned (no fractional pixels)
inline bool is_pixel_aligned(float value) {
  float fractional = value - std::floor(value);
  return fractional < 0.001f || fractional > 0.999f;
}

} // namespace validation

} // namespace ui
} // namespace afterhours
