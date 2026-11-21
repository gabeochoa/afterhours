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
    auto dom = apply_domains(*b, std::pair{0.0, 10.0}, std::pair{100.0, 0.0});
    assert(dom.min_x == 0 && dom.max_x == 10 && dom.min_y == 0 && dom.max_y == 100);
    assert(fraction(15, dom.min_y, dom.max_y) == 0.15);
    assert(fraction(150, dom.min_y, dom.max_y) == 1);  // out-of-domain clamps to edge
    assert(fraction(-5, dom.min_y, dom.max_y) == 0);
    auto auto_dom = apply_domains(*b);
    assert(auto_dom.min_y == -5 && auto_dom.max_y == 10);
    auto constant = apply_domains(*bounds(singleton), {}, std::pair{10.0, 10.0});
    assert(fraction(10, constant.min_y, constant.max_y) == 0.5);
}
