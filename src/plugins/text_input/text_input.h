#pragma once

/// Text Input Plugin
///
/// Provides text input components and utilities for building text editing UIs.
///
/// Organization:
/// - storage.h   - TextStorage concept and StringStorage implementation
/// - state.h     - HasTextInputState and related component types
/// - utils.h     - UTF-8 utilities, cursor manipulation, CJK detection
/// - component.h - The text_input() immediate-mode widget
///
/// Usage:
/// ```cpp
/// #include <afterhours/src/plugins/text_input/text_input.h>
///
/// std::string username;
/// if (afterhours::text_input::text_input(ctx, mk(parent), username,
///     ComponentConfig{}.with_label("Username"))) {
///   // Text changed
/// }
/// ```
///
/// Or use the backward-compatible ui::imm::text_input() function.

#include "storage.h"
#include "state.h"
#include "utils.h"
#include "component.h"

