#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace afterhours::charts {

struct Point { double x = 0; double y = 0; };
struct Bounds { double min_x, max_x, min_y, max_y; };

inline bool finite(Point point) { return std::isfinite(point.x) && std::isfinite(point.y); }

inline std::optional<Bounds> bounds(std::span<const Point> points) {
    std::optional<Bounds> result;
    for (const auto point : points) {
        if (!finite(point)) continue;
        if (!result) { result = Bounds{point.x, point.x, point.y, point.y}; continue; }
        result->min_x = std::min(result->min_x, point.x);
        result->max_x = std::max(result->max_x, point.x);
        result->min_y = std::min(result->min_y, point.y);
        result->max_y = std::max(result->max_y, point.y);
    }
    return result;
}

inline double fraction(double value, double low, double high) {
    if (low == high) return 0.5;
    const double scale = std::max({std::abs(value), std::abs(low), std::abs(high)});
    return std::clamp((value / scale - low / scale) / (high / scale - low / scale), 0., 1.);
}

inline std::optional<std::size_t> nearest_x(std::span<const Point> points, double x) {
    std::optional<std::size_t> result;
    double distance = std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < points.size(); ++i) {
        if (!finite(points[i])) continue;
        const auto candidate = std::abs(points[i].x / 2 - x / 2);
        if (candidate >= distance) continue;
        distance = candidate;
        result = i;
    }
    return result;
}

}
