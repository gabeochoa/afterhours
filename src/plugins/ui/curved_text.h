#pragma once

#include <cmath>
#include <string_view>
#include <vector>

#include "../../developer.h"
#include "text_units.h"

namespace afterhours {
namespace ui {

struct ArcText {
  float radius = 120.f;
  float start_angle_deg = -90.f;
  bool clockwise = true;
};

struct CurvedGlyph {
  UnitSpan span;
  float center_x = 0.f;
  float center_y = 0.f;
  float angle_deg = 0.f;
  float rotation_deg = 0.f;
  float width = 0.f;
};

template <typename MeasureFn>
inline std::vector<CurvedGlyph> layout_text_on_arc(std::string_view text,
                                                   float center_x, float center_y,
                                                   const ArcText &arc,
                                                   MeasureFn &&measure) {
  std::vector<CurvedGlyph> out;
  if (text.empty() || arc.radius <= 0.f)
    return out;
  std::vector<UnitSpan> spans;
  split_graphemes(text, spans);
  const float dir = arc.clockwise ? 1.f : -1.f;
  float swept_deg = 0.f;
  for (const UnitSpan &sp : spans) {
    const float width = measure(text.substr(sp.begin, sp.size()));
    const float half_deg = (width * 0.5f / arc.radius) * RAD2DEG;
    const float angle = arc.start_angle_deg + dir * (swept_deg + half_deg);
    const float rad = angle * DEG2RAD;
    out.push_back(CurvedGlyph{.span = sp,
                              .center_x = center_x + arc.radius * std::cos(rad),
                              .center_y = center_y + arc.radius * std::sin(rad),
                              .angle_deg = angle,
                              .rotation_deg = angle + 90.f,
                              .width = width});
    swept_deg += half_deg * 2.f;
  }
  return out;
}

} // namespace ui
} // namespace afterhours
