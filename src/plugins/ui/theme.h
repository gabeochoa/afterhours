#pragma once

#include "../color.h"
#include <bitset>

namespace afterhours {

namespace ui {

struct Theme {
  enum struct Usage {
    Font,
    DarkFont,
    Background,
    Primary,
    Secondary,
    Accent,
    Error,

    //
    Custom,
    Default,
    None,
  };

  static bool is_valid(Usage cu) {
    switch (cu) {
    case Usage::Font:
    case Usage::DarkFont:
    case Usage::Background:
    case Usage::Primary:
    case Usage::Secondary:
    case Usage::Accent:
    case Usage::Error:
      return true;
    case Usage::Custom:
    case Usage::Default:
    case Usage::None:
      return false;
    };
    return false;
  }

  Color font;
  Color darkfont;
  Color background;

  Color primary;
  Color secondary;
  Color accent;
  Color error;

  Theme()
      : font(colors::isabelline), darkfont(colors::oxford_blue),
        background(colors::oxford_blue), primary(colors::pacific_blue),
        secondary(colors::tea_green), accent(colors::orange_soda),
        // TODO find a better error color
        error(colors::red) {}

  Theme(Color f, Color df, Color bg, Color p, Color s, Color a, Color e)
      : font(f), darkfont(df), background(bg), primary(p), secondary(s),
        accent(a), error(e) {}

  Color from_usage(Usage cu, bool disabled = false) const {
    Color color;
    switch (cu) {
    case Usage::Font:
      color = font;
      break;
    case Usage::DarkFont:
      color = darkfont;
      break;
    case Usage::Background:
      color = background;
      break;
    case Usage::Primary:
      color = primary;
      break;
    case Usage::Secondary:
      color = secondary;
      break;
    case Usage::Accent:
      color = accent;
      break;
    case Usage::Error:
      color = error;
      break;
    case Usage::Default:
      log_warn("You should not be fetching 'default' color usage from theme, "
               "UI library should handle this??");
      color = primary;
      break;
    case Usage::Custom:
      log_warn("You should not be fetching 'custom' color usage from theme, "
               "UI library should handle this??");
      color = primary;
      break;
    case Usage::None:
      log_warn("You should not be fetching 'none' color usage from theme, "
               "UI library should handle this??");
      color = primary;
      break;
    }
    if (disabled) {
      return colors::darken(color, 0.5f);
    }
    return color;
  }

  std::bitset<4> rounded_corners = std::bitset<4>().set();
};

} // namespace ui

} // namespace afterhours
