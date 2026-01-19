#pragma once

/// Text Input Plugin
///
/// Provides text input components and utilities for building text editing UIs.
///
/// Organization:
/// - storage.h     - TextStorage concept and StringStorage implementation
/// - state.h       - HasTextInputState and related component types
/// - utils.h       - UTF-8 utilities, cursor manipulation, word navigation
/// - selection.h   - TextSelection for multi-line/selection support
/// - line_index.h  - LineIndex for offset/row/column mapping
/// - text_layout.h - TextLayoutCache for word wrap and visual line mapping
/// - systems.h     - ECS systems for text editing (blink, line index, selection)
/// - component.h   - The text_input() immediate-mode widget
///
/// Usage:
/// ```cpp
/// #include <afterhours/src/plugins/ui/text_input/text_input.h>
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
#include "selection.h"
#include "line_index.h"
#include "text_layout.h"
#include "systems.h"
#include "component.h"

