#pragma once

#include "../charts.h"
#include "imm_components.h"

namespace afterhours::ui::imm {

struct ChartSeries {
    std::string name;
    std::vector<charts::Point> points;
    Color color;
};

struct LineChartOptions {
    std::string unit;
    std::optional<std::size_t> selected_index;
    float label_font_size = 12;
};

inline ElementResult line_chart(HasUIContext auto &ctx, EntityParent parent,
                                 std::vector<ChartSeries> series,
                                 LineChartOptions options = {},
                                 ComponentConfig config = {}) {
    const auto mouse = ctx.mouse.pos;
    const auto *fonts = EntityHelper::get_singleton_cmp<FontManager>();
    const auto font = fonts ? fonts->get_active_font() : get_default_font();
    const auto text_color = ctx.theme.from_usage(Theme::Usage::Font);
    const auto grid_color = ctx.theme.from_usage(Theme::Usage::Secondary);
    if (config.size.is_default) config.with_size({pixels(480), pixels(220)});
    config.with_on_draw_fg([series = std::move(series), options = std::move(options),
                           mouse, font, text_color, grid_color](RectangleType rect) {
        if (rect.width < 120 || rect.height < 90) return;
        auto draw_text = [font](const char *text, float x, float y, float size, Color color) {
            draw_text_ex(font, text, {x, y}, size, 1, color);
        };
        const float label_size = std::max(1.f, options.label_font_size);
        const float left = std::max(58.f, label_size * 4.f + 10.f);
        const float vertical = std::max(28.f, label_size + 12.f);
        const RectangleType plot{rect.x + left, rect.y + vertical, rect.width - left - 16, rect.height - vertical * 2};
        if (plot.width <= 0 || plot.height <= 0) return;
        std::optional<charts::Bounds> extent;
        for (const auto &line : series) {
            auto b = charts::bounds(line.points);
            if (!b) continue;
            if (!extent) { extent = b; continue; }
            extent->min_x = std::min(extent->min_x, b->min_x);
            extent->max_x = std::max(extent->max_x, b->max_x);
            extent->min_y = std::min(extent->min_y, b->min_y);
            extent->max_y = std::max(extent->max_y, b->max_y);
        }
        if (!extent) {
            draw_text("No samples", plot.x, plot.y, std::max(14.f, label_size), text_color);
            return;
        }
        auto position = [&](charts::Point point) -> Vector2Type {
            return {plot.x + static_cast<float>(charts::fraction(point.x, extent->min_x, extent->max_x)) * plot.width,
                    plot.y + (1.f - static_cast<float>(charts::fraction(point.y, extent->min_y, extent->max_y))) * plot.height};
        };
        for (int i = 0; i <= 4; ++i) {
            const float y = plot.y + plot.height * static_cast<float>(i) / 4.f;
            draw_line_ex({plot.x, y}, {plot.x + plot.width, y}, 1, grid_color);
        }
        const auto high = fmt::format("{:.3g}", extent->max_y);
        const auto low = fmt::format("{:.3g}", extent->min_y);
        draw_text(high.c_str(), rect.x + 4, plot.y, label_size, text_color);
        draw_text(low.c_str(), rect.x + 4, plot.y + plot.height - label_size, label_size, text_color);
        const auto start = fmt::format("{:.3g}", extent->min_x);
        const auto end = fmt::format("{:.3g}", extent->max_x);
        draw_text(start.c_str(), plot.x, plot.y + plot.height + 6, label_size, text_color);
        draw_text(end.c_str(), plot.x + plot.width - label_size * 3.5f, plot.y + plot.height + 6, label_size, text_color);
        float legend_x = plot.x;
        const bool hovered = is_mouse_inside(mouse, plot);
        for (const auto &line : series) {
            if (legend_x + 100 < rect.x + rect.width) {
                draw_text(line.name.c_str(), legend_x, rect.y + 5, label_size, line.color);
                legend_x += std::max(130.f, label_size * 10.f);
            }
            std::optional<Vector2Type> previous;
            for (const auto point : line.points) {
                if (!charts::finite(point)) { previous.reset(); continue; }
                const auto at = position(point);
                if (previous) draw_line_ex(*previous, at, 2, line.color);
                if (line.points.size() == 1) draw_circle(static_cast<int>(at.x), static_cast<int>(at.y), 3, line.color);
                previous = at;
            }
            auto selected = options.selected_index;
            if (hovered) {
                const double x = std::lerp(extent->min_x, extent->max_x, static_cast<double>((mouse.x - plot.x) / plot.width));
                selected = charts::nearest_x(line.points, x);
            }
            if (!selected || *selected >= line.points.size() || !charts::finite(line.points[*selected])) continue;
            const auto point = line.points[*selected];
            const auto at = position(point);
            draw_circle(static_cast<int>(at.x), static_cast<int>(at.y), 4, line.color);
            const auto label = fmt::format("{:.2f} {}", point.y, options.unit);
            draw_text(label.c_str(), std::clamp(at.x, plot.x, plot.x + std::max(0.f, plot.width - 100)),
                      std::max(plot.y, at.y - label_size - 6), label_size, text_color);
        }
    });
    return div(ctx, parent, config);
}

}
