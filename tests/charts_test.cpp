#include <afterhours/src/plugins/charts.h>
#include <cassert>

int main() {
    using namespace afterhours::charts;
    assert(!bounds({}));
    const std::vector<Point> points{{0, -5}, {1, 10}, {2, 10}};
    auto b = bounds(points);
    assert(b && b->min_y == -5 && b->max_y == 10);
    assert(fraction(10, 10, 10) == 0.5);
    assert(fraction(-5, -5, 10) == 0);
    assert(fraction(20, -5, 10) == 1);
    assert(fraction(0, -1e308, 1e308) == 0.5);
    assert(nearest_x(points, 1.8) == 2);
    const std::vector<Point> invalid{{0, NAN}, {INFINITY, 1}};
    assert(!bounds(invalid));
    assert(!nearest_x(invalid, 0));
    const std::vector<Point> singleton{{5, 10}};
    assert(bounds(singleton)->min_x == 5);
    assert(nearest_x(singleton, -100) == 0);
}
