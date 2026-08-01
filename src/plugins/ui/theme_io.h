#pragma once

// Reading and writing Theme as a flat text file, so tweaking a color is an
// edit-and-save instead of a recompile.
//
//   # comments run to end of line
//   background = #0C1B33
//   primary    = #47A8BD
//   roundness  = 0.5
//   click_activation_mode = Press
//
// Colors are driven off Theme::Usage rather than a hand-written list, so a
// usage added to the enum is serialized without touching this file. The
// scalars use member-pointer tables for the same reason: the field list is
// written once and both save and load read it.
//
// Not covered: language_fonts and font_sizing. Fonts are asset wiring, not the
// color-iteration loop this exists for, and FontSizing overloads its sign to
// mean "user-set vs interpolated", which does not survive a naive round trip.

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include "../../core/system.h"
#include "theme.h"

namespace afterhours {
namespace ui {
namespace theme_io {

// ---------------------------------------------------------------------------
// Field tables. Add a member here and it is read, written, and round-tripped.
// ---------------------------------------------------------------------------
inline constexpr std::pair<std::string_view, float Theme::*> float_fields[] = {
    {"roundness", &Theme::roundness},
    {"focus_ring_thickness", &Theme::focus_ring_thickness},
    {"focus_ring_offset", &Theme::focus_ring_offset},
    {"disabled_opacity", &Theme::disabled_opacity},
    {"ui_scale", &Theme::ui_scale},
};

// ---------------------------------------------------------------------------
// Key matching. A hand-edited file should not care about case or underscores,
// so "font_muted", "FontMuted" and "fontmuted" all name Usage::FontMuted.
// ---------------------------------------------------------------------------
inline std::string normalize(std::string_view s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    if (c == '_' || c == '-' || c == ' ')
      continue;
    out.push_back(static_cast<char>(
        (c >= 'A' && c <= 'Z') ? (c - 'A' + 'a') : c));
  }
  return out;
}

// "FontMuted" -> "font_muted", so written files match the C++ field names.
inline std::string to_snake(std::string_view camel) {
  std::string out;
  for (size_t i = 0; i < camel.size(); i++) {
    char c = camel[i];
    if (c >= 'A' && c <= 'Z') {
      if (i > 0)
        out.push_back('_');
      out.push_back(static_cast<char>(c - 'A' + 'a'));
    } else {
      out.push_back(c);
    }
  }
  return out;
}

// Enum value whose name matches `text` ignoring case/underscores.
template <typename E> std::optional<E> enum_from(std::string_view text) {
  const std::string want = normalize(text);
  for (E value : magic_enum::enum_values<E>())
    if (normalize(magic_enum::enum_name(value)) == want)
      return value;
  return std::nullopt;
}

// ---------------------------------------------------------------------------
// Scalars
// ---------------------------------------------------------------------------

/// #RGB is not accepted; only #RRGGBB and #RRGGBBAA. Alpha defaults to opaque.
inline std::optional<Color> parse_color(std::string_view text) {
  if (!text.empty() && text.front() == '#')
    text.remove_prefix(1);
  if (text.size() != 6 && text.size() != 8)
    return std::nullopt;

  unsigned int channels[4] = {0, 0, 0, 255};
  for (size_t i = 0; i < text.size(); i += 2) {
    unsigned int value = 0;
    for (size_t j = 0; j < 2; j++) {
      const char c = text[i + j];
      unsigned int digit;
      if (c >= '0' && c <= '9')
        digit = static_cast<unsigned int>(c - '0');
      else if (c >= 'a' && c <= 'f')
        digit = static_cast<unsigned int>(c - 'a' + 10);
      else if (c >= 'A' && c <= 'F')
        digit = static_cast<unsigned int>(c - 'A' + 10);
      else
        return std::nullopt;
      value = value * 16 + digit;
    }
    channels[i / 2] = value;
  }
  return Color{static_cast<unsigned char>(channels[0]),
               static_cast<unsigned char>(channels[1]),
               static_cast<unsigned char>(channels[2]),
               static_cast<unsigned char>(channels[3])};
}

inline std::string format_color(const Color &c) {
  char buf[10];
  snprintf(buf, sizeof(buf), "#%02X%02X%02X%02X", c.r, c.g, c.b, c.a);
  return std::string(buf);
}

/// Rejects trailing garbage ("1.0abc"), which strtof would silently accept.
inline std::optional<float> parse_float(std::string_view text) {
  const std::string owned(text);
  try {
    size_t consumed = 0;
    const float value = std::stof(owned, &consumed);
    if (consumed != owned.size())
      return std::nullopt;
    return value;
  } catch (...) {
    return std::nullopt;
  }
}

// ---------------------------------------------------------------------------
// Read
// ---------------------------------------------------------------------------

struct ApplyResult {
  int applied = 0;
  int errors = 0;
};

inline std::string_view trim(std::string_view s) {
  const auto is_space = [](char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
  };
  while (!s.empty() && is_space(s.front()))
    s.remove_prefix(1);
  while (!s.empty() && is_space(s.back()))
    s.remove_suffix(1);
  return s;
}

/// Applies every recognized `key = value` onto `theme`, leaving unmentioned
/// properties at whatever they already were -- so a file may be a partial
/// override of a base theme.
///
/// A bad line costs that one property, not the file: it is counted in
/// `errors`, logged with its line number, and parsing continues. This is a
/// hand-edited file, so a typo must not silently swap in a default theme.
inline ApplyResult read_into(std::string_view text, Theme &theme) {
  ApplyResult result;
  int line_number = 0;

  while (!text.empty()) {
    const size_t newline = text.find('\n');
    std::string_view line = text.substr(0, newline);
    text = (newline == std::string_view::npos) ? std::string_view{}
                                               : text.substr(newline + 1);
    line_number++;

    // Comments have to be stripped per-side of the '=', because a color value
    // legitimately starts with '#'. On the key side any '#' is a comment; on
    // the value side only a '#' after the first character is.
    const size_t equals = line.find('=');
    if (equals == std::string_view::npos) {
      if (const size_t comment = line.find('#');
          comment != std::string_view::npos)
        line = line.substr(0, comment);
      line = trim(line);
      if (line.empty())
        continue;
      log_warn("theme line {}: expected 'key = value', got '{}'", line_number,
               std::string(line));
      result.errors++;
      continue;
    }

    const std::string_view key = trim(line.substr(0, equals));
    if (key.find('#') != std::string_view::npos)
      continue; // the whole line is commented out, e.g. "# primary = #FFF"

    std::string_view value = trim(line.substr(equals + 1));
    const size_t value_comment =
        value.find('#', (!value.empty() && value.front() == '#') ? 1 : 0);
    if (value_comment != std::string_view::npos)
      value = trim(value.substr(0, value_comment));

    if (key.empty() || value.empty()) {
      log_warn("theme line {}: expected 'key = value', got '{}'", line_number,
               std::string(trim(line)));
      result.errors++;
      continue;
    }
    const std::string key_norm = normalize(key);

    const auto fail = [&](const char *what) {
      log_warn("theme line {}: {} '{}' for '{}'", line_number, what,
               std::string(value), std::string(key));
      result.errors++;
    };

    // Colors, keyed by Theme::Usage name.
    if (auto usage = enum_from<Theme::Usage>(key);
        usage && Theme::is_valid(*usage)) {
      if (auto color = parse_color(value)) {
        theme.set_color(*usage, *color);
        result.applied++;
      } else {
        fail("bad color");
      }
      continue;
    }

    bool matched = false;
    for (const auto &[name, member] : float_fields) {
      if (key_norm != normalize(name))
        continue;
      matched = true;
      if (auto number = parse_float(value)) {
        theme.*member = *number;
        result.applied++;
      } else {
        fail("bad number");
      }
      break;
    }
    if (matched)
      continue;

    if (key_norm == normalize("click_activation_mode")) {
      if (auto mode = enum_from<ClickActivationMode>(value)) {
        theme.click_activation_mode = *mode;
        result.applied++;
      } else {
        fail("unknown mode");
      }
      continue;
    }
    if (key_norm == normalize("highlight_mode")) {
      if (auto mode = enum_from<HighlightMode>(value)) {
        theme.highlight_mode = *mode;
        result.applied++;
      } else {
        fail("unknown mode");
      }
      continue;
    }

    log_warn("theme line {}: unknown key '{}'", line_number, std::string(key));
    result.errors++;
  }

  return result;
}

// ---------------------------------------------------------------------------
// Write
// ---------------------------------------------------------------------------

inline std::string to_string(const Theme &theme) {
  std::ostringstream out;
  out << "# afterhours theme\n\n";

  for (Theme::Usage usage : magic_enum::enum_values<Theme::Usage>()) {
    if (!Theme::is_valid(usage))
      continue; // Custom/Default/None are markers, not colors.
    out << to_snake(magic_enum::enum_name(usage)) << " = "
        << format_color(theme.color_ref(usage)) << "\n";
  }

  out << "\n";
  for (const auto &[name, member] : float_fields)
    out << name << " = " << (theme.*member) << "\n";

  out << "\nclick_activation_mode = "
      << magic_enum::enum_name(theme.click_activation_mode) << "\n";
  out << "highlight_mode = " << magic_enum::enum_name(theme.highlight_mode)
      << "\n";

  return out.str();
}

// ---------------------------------------------------------------------------
// Files
// ---------------------------------------------------------------------------

inline bool save(const Theme &theme, const std::filesystem::path &path) {
  std::ofstream file(path);
  if (!file) {
    log_error("could not open theme file for writing: {}", path.string());
    return false;
  }
  file << to_string(theme);
  return file.good();
}

/// Starts from `base` (a default Theme unless given), so a file listing only
/// `primary` changes only that. std::nullopt means the file could not be read;
/// a file that parsed with errors still returns the theme it managed to build,
/// since dropping every good line over one typo is worse than a warning.
inline std::optional<Theme> load(const std::filesystem::path &path,
                                 Theme base = Theme{}) {
  std::ifstream file(path);
  if (!file) {
    log_warn("could not open theme file: {}", path.string());
    return std::nullopt;
  }
  std::ostringstream buffer;
  buffer << file.rdbuf();
  read_into(buffer.str(), base);
  return base;
}

// ---------------------------------------------------------------------------
// Hot reload
// ---------------------------------------------------------------------------

/// Watches a theme file and reloads it into ThemeDefaults when it changes, so
/// a color tweak shows up on save instead of on rebuild. Register it like any
/// other system:
///
///     ui::setup<>(systems,
///                 std::make_unique<ui::theme_io::HotReloadTheme>("app.theme"),
///                 std::make_unique<MyUISystem>());
///
/// Writes ThemeDefaults, which BeginUIContextManager copies into every context
/// each frame, so a reload lands on the next frame with nothing else to wire.
///
/// Unlike load(), the file here is authoritative: it is read onto a default
/// Theme, so deleting a line restores that property's default rather than
/// leaving the last value stuck. The file this seeds is complete, so that only
/// comes up if you delete lines yourself.
struct HotReloadTheme : System<> {
  std::filesystem::path path;
  float poll_seconds;
  float elapsed;
  std::filesystem::file_time_type stamp{};
  bool tried_seed = false;

  explicit HotReloadTheme(std::filesystem::path file, float poll = 0.25f)
      : path(std::move(file)), poll_seconds(poll),
        // Start due, so an existing file is picked up on the first frame.
        elapsed(poll) {}

  bool should_iterate() const override { return false; }

  void once(float dt) override {
    elapsed += dt;
    if (elapsed < poll_seconds)
      return;
    elapsed = 0.f;

    std::error_code ec;
    const auto modified = std::filesystem::last_write_time(path, ec);
    if (ec) {
      // No file yet: write the live theme out once so there is something to
      // edit, rather than making the user learn the format from the source.
      if (!tried_seed) {
        tried_seed = true;
        if (save(imm::ThemeDefaults::get().theme, path))
          log_info("wrote starting theme to {}", path.string());
      }
      return;
    }
    if (modified == stamp)
      return;
    stamp = modified;

    std::ifstream file(path);
    if (!file)
      return;
    std::ostringstream buffer;
    buffer << file.rdbuf();

    Theme fresh;
    const ApplyResult result = read_into(buffer.str(), fresh);
    // ponytail: catches the common case of reading while an editor is
    // mid-write (file momentarily empty). A partially-flushed file that still
    // has some valid lines will apply and then correct itself on the next
    // save; atomic read-retry if that turns out to be annoying in practice.
    if (result.applied == 0) {
      log_warn("theme file {} had nothing usable; keeping current theme",
               path.string());
      return;
    }
    imm::ThemeDefaults::get().theme = fresh;
  }
};

} // namespace theme_io
} // namespace ui
} // namespace afterhours
