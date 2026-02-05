
// Layout Performance Benchmarks
// Based on PanGui's benchmark suite for comparing layout system performance
//
// These benchmarks measure only layout computation time, not node/tree creation,
// memory allocation, or tree destruction.
//
// To run: make && ./layout_performance.exe
//
// Methodology:
// - 100 warmup iterations, then sample for at least 5 seconds (minimum 100 iterations)
// - Time shown is average per iteration

#include <math.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <ostream>
#include <set>
#include <sstream>
#include <stack>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "../../shared/vector.h"

#define AFTER_HOURS_ENTITY_HELPER
#define AFTER_HOURS_ENTITY_QUERY
#define AFTER_HOURS_SYSTEM
#include "../../../ah.h"
#include "../../../src/plugins/autolayout.h"
#include "../../../src/plugins/ui.h"

#define CATCH_CONFIG_ENABLE_BENCHMARKING
#define CATCH_CONFIG_MAIN
#include "../../../vendor/catch2/catch.hpp"

using namespace afterhours;
using namespace afterhours::ui;

// ============================================================================
// Benchmark Infrastructure
// ============================================================================

constexpr static float WIDTH = 1280.f;
constexpr static float HEIGHT = 720.f;

static UIComponent &make_component(Entity &entity) {
    auto &cmp = entity.addComponent<ui::UIComponent>(entity.id);
    EntityHelper::merge_entity_arrays();
    return cmp;
}

static Entity &make_root() {
    auto &root = EntityHelper::createEntity();
    root.addComponent<ui::AutoLayoutRoot>();
    make_component(root)
        .set_desired_width(pixels(WIDTH))
        .set_desired_height(pixels(HEIGHT));
    return root;
}

static void run_layout(Entity &root_element) {
    std::map<EntityID, RefEntity> components;
    auto comps = EntityQuery().whereHasComponent<ui::UIComponent>().gen();
    for (Entity &entity : comps) {
        components.emplace(entity.id, entity);
    }
    ui::AutoLayout::autolayout(root_element.get<UIComponent>(),
                               {(int)WIDTH, (int)HEIGHT}, components);
}

static void cleanup_entities() {
    // Clean up all entities for the next benchmark
    auto all = EntityQuery().gen();
    for (Entity &e : all) {
        EntityHelper::markIDForCleanup(e.id);
    }
    EntityHelper::cleanup();
}

// ============================================================================
// Layout Verification Helpers
// ============================================================================

static void print_rect(const char *name, const Rectangle &r) {
    std::cout << "  " << name << ": x=" << r.x << ", y=" << r.y
              << ", w=" << r.width << ", h=" << r.height << std::endl;
}

static void print_component(const char *name, Entity &entity) {
    auto &cmp = entity.get<UIComponent>();
    auto rect = cmp.rect();
    auto bounds = cmp.bounds();
    std::cout << "  " << name << ":" << std::endl;
    std::cout << "    rect:   x=" << rect.x << ", y=" << rect.y
              << ", w=" << rect.width << ", h=" << rect.height << std::endl;
    std::cout << "    bounds: x=" << bounds.x << ", y=" << bounds.y
              << ", w=" << bounds.width << ", h=" << bounds.height << std::endl;
}

// ============================================================================
// LAYOUT VERIFICATION TESTS - Run these to validate layout results
// ============================================================================

TEST_CASE("VERIFY: nested_vertical_stack layout", "[verify]") {
    // Small version: 5 children instead of 10000
    cleanup_entities();

    auto &root = make_root();
    auto &container = EntityHelper::createEntity();
    make_component(container)
        .set_desired_width(pixels(200.f))
        .set_desired_height(children())
        .set_flex_direction(FlexDirection::Column)
        .set_desired_padding(pixels(10.f), Axis::X)
        .set_desired_padding(pixels(10.f), Axis::Y)
        .set_parent(root);

    std::vector<Entity *> children_vec;
    for (int i = 0; i < 5; ++i) {
        auto &child = EntityHelper::createEntity();
        make_component(child)
            .set_desired_width(percent(1.f))
            .set_desired_height(pixels(20.f))  // Larger for visibility
            .set_parent(container);
        children_vec.push_back(&child);
    }

    run_layout(root);

    std::cout << "\n=== VERIFY: nested_vertical_stack ===" << std::endl;
    std::cout << "Structure: Container(200px wide, fit height, vertical, 10px padding)" << std::endl;
    std::cout << "           5 children (100% width, 20px height each)" << std::endl;
    std::cout << "\nExpected:" << std::endl;
    std::cout << "  Container: w=200+20=220 (with padding), h=5*20+20=120 (5 children + padding)" << std::endl;
    std::cout << "  Children: each 180px wide (200-20 padding), 20px tall, stacked vertically" << std::endl;
    std::cout << "\nActual:" << std::endl;
    print_component("root", root);
    print_component("container", container);
    for (size_t i = 0; i < children_vec.size(); ++i) {
        std::string name = "child[" + std::to_string(i) + "]";
        print_component(name.c_str(), *children_vec[i]);
    }

    cleanup_entities();
}

TEST_CASE("VERIFY: padding_and_margin layout", "[verify]") {
    // Small version: 3 children instead of 100
    cleanup_entities();

    auto &root = make_root();
    auto &container = EntityHelper::createEntity();
    make_component(container)
        .set_desired_width(pixels(1000.f))
        .set_desired_height(children())
        .set_flex_direction(FlexDirection::Column)
        .set_desired_padding(pixels(20.f), Axis::X)
        .set_desired_padding(pixels(20.f), Axis::Y)
        .set_parent(root);

    std::vector<Entity *> children_vec;
    for (int i = 0; i < 3; ++i) {
        auto &child = EntityHelper::createEntity();
        make_component(child)
            .set_desired_width(percent(1.f))
            .set_desired_height(pixels(20.f))
            .set_desired_margin(pixels(2.f), Axis::X)
            .set_desired_margin(pixels(2.f), Axis::Y)
            .set_parent(container);
        children_vec.push_back(&child);
    }

    run_layout(root);

    std::cout << "\n=== VERIFY: padding_and_margin ===" << std::endl;
    std::cout << "Structure: Container(1000px wide, fit height, vertical, 20px padding)" << std::endl;
    std::cout << "           3 children (100% width, 20px height, 2px margin)" << std::endl;
    std::cout << "\nExpected:" << std::endl;
    std::cout << "  Container: w=1000+40=1040 (with padding), h=3*20+40=100 (approx with padding)" << std::endl;
    std::cout << "  Children: each ~956px wide (1000-40 padding -4 margin), 16px tall (20-4 margin)" << std::endl;
    std::cout << "\nActual:" << std::endl;
    print_component("root", root);
    print_component("container", container);
    for (size_t i = 0; i < children_vec.size(); ++i) {
        std::string name = "child[" + std::to_string(i) + "]";
        print_component(name.c_str(), *children_vec[i]);
    }

    cleanup_entities();
}

TEST_CASE("VERIFY: wide_no_wrap_simple layout", "[verify]") {
    // Small version: 5 children instead of 1000
    cleanup_entities();

    auto &root = make_root();
    auto &container = EntityHelper::createEntity();
    make_component(container)
        .set_desired_width(pixels(100.f))
        .set_desired_height(pixels(100.f))
        .set_flex_direction(FlexDirection::Row)
        .set_flex_wrap(FlexWrap::NoWrap)
        .set_parent(root);

    std::vector<Entity *> children_vec;
    for (int i = 0; i < 5; ++i) {
        auto &child = EntityHelper::createEntity();
        make_component(child)
            .set_desired_width(pixels(10.f))
            .set_desired_height(pixels(10.f))
            .set_parent(container);
        children_vec.push_back(&child);
    }

    run_layout(root);

    std::cout << "\n=== VERIFY: wide_no_wrap_simple ===" << std::endl;
    std::cout << "Structure: Container(100x100, horizontal, no-wrap)" << std::endl;
    std::cout << "           5 children (10x10 each)" << std::endl;
    std::cout << "\nExpected:" << std::endl;
    std::cout << "  Container: 100x100" << std::endl;
    std::cout << "  Children: 10x10 each, at x=0,10,20,30,40 y=0" << std::endl;
    std::cout << "\nActual:" << std::endl;
    print_component("root", root);
    print_component("container", container);
    for (size_t i = 0; i < children_vec.size(); ++i) {
        std::string name = "child[" + std::to_string(i) + "]";
        print_component(name.c_str(), *children_vec[i]);
    }

    cleanup_entities();
}

TEST_CASE("VERIFY: wide_wrapping layout", "[verify]") {
    // Small version: 15 children that should wrap into 2 rows
    cleanup_entities();

    auto &root = make_root();
    auto &container = EntityHelper::createEntity();
    make_component(container)
        .set_desired_width(pixels(100.f))
        .set_desired_height(children())
        .set_flex_direction(FlexDirection::Row)
        .set_flex_wrap(FlexWrap::Wrap)
        .set_parent(root);

    std::vector<Entity *> children_vec;
    for (int i = 0; i < 15; ++i) {
        auto &child = EntityHelper::createEntity();
        make_component(child)
            .set_desired_width(pixels(10.f))
            .set_desired_height(pixels(10.f))
            .set_parent(container);
        children_vec.push_back(&child);
    }

    run_layout(root);

    std::cout << "\n=== VERIFY: wide_wrapping ===" << std::endl;
    std::cout << "Structure: Container(100px wide, fit height, horizontal, wrap)" << std::endl;
    std::cout << "           15 children (10x10 each)" << std::endl;
    std::cout << "\nExpected:" << std::endl;
    std::cout << "  Container: 100px wide, 20px tall (2 rows of 10 children, 5 per row doesn't fit)" << std::endl;
    std::cout << "  Children: 10x10 each, wrapped at container width" << std::endl;
    std::cout << "  Row 1: children 0-9 at y=0, Row 2: children 10-14 at y=10" << std::endl;
    std::cout << "\nActual:" << std::endl;
    print_component("root", root);
    print_component("container", container);
    for (size_t i = 0; i < children_vec.size(); ++i) {
        std::string name = "child[" + std::to_string(i) + "]";
        print_component(name.c_str(), *children_vec[i]);
    }

    cleanup_entities();
}

TEST_CASE("VERIFY: fit_nesting layout", "[verify]") {
    // Minimal version: 2x2x2x3 = 24 nodes instead of 10x10x10x100
    cleanup_entities();

    auto &root = make_root();
    auto &level0 = EntityHelper::createEntity();
    make_component(level0)
        .set_desired_width(pixels(1000.f))
        .set_desired_height(children())
        .set_flex_direction(FlexDirection::Column)
        .set_desired_padding(pixels(5.f), Axis::X)
        .set_desired_padding(pixels(5.f), Axis::Y)
        .set_parent(root);

    std::vector<Entity *> level1_vec, level2_vec, level3_vec, leaf_vec;

    // 2 level-1 containers
    for (int i = 0; i < 2; ++i) {
        auto &level1 = EntityHelper::createEntity();
        make_component(level1)
            .set_desired_width(percent(1.f))
            .set_desired_height(children())
            .set_flex_direction(FlexDirection::Row)
            .set_parent(level0);
        level1_vec.push_back(&level1);

        // 2 level-2 containers
        for (int j = 0; j < 2; ++j) {
            auto &level2 = EntityHelper::createEntity();
            make_component(level2)
                .set_desired_width(percent(0.5f))
                .set_desired_height(children())
                .set_flex_direction(FlexDirection::Column)
                .set_parent(level1);
            level2_vec.push_back(&level2);

            // 2 level-3 containers
            for (int k = 0; k < 2; ++k) {
                auto &level3 = EntityHelper::createEntity();
                make_component(level3)
                    .set_desired_width(percent(1.f))
                    .set_desired_height(children())
                    .set_flex_direction(FlexDirection::Row)
                    .set_parent(level2);
                level3_vec.push_back(&level3);

                // 3 leaf nodes
                for (int l = 0; l < 3; ++l) {
                    auto &leaf = EntityHelper::createEntity();
                    make_component(leaf)
                        .set_desired_width(percent(0.33f))
                        .set_desired_height(pixels(10.f))
                        .set_parent(level3);
                    leaf_vec.push_back(&leaf);
                }
            }
        }
    }

    run_layout(root);

    std::cout << "\n=== VERIFY: fit_nesting ===" << std::endl;
    std::cout << "Structure: Deeply nested 2x2x2x3 = 24 leaf nodes" << std::endl;
    std::cout << "  Level0: 1000px wide, fit height, vertical, 5px padding" << std::endl;
    std::cout << "  Level1: 100% width, fit height, horizontal (2 of these)" << std::endl;
    std::cout << "  Level2: 50% width, fit height, vertical (4 total)" << std::endl;
    std::cout << "  Level3: 100% width, fit height, horizontal (8 total)" << std::endl;
    std::cout << "  Leaves: 33% width, 10px height (24 total)" << std::endl;
    std::cout << "\nExpected:" << std::endl;
    std::cout << "  Level0 height: 2 level1 rows * (2 level3 rows * 10px) = ~40px + padding" << std::endl;
    std::cout << "\nActual:" << std::endl;
    print_component("root", root);
    print_component("level0", level0);

    std::cout << "\n  Level 1 containers:" << std::endl;
    for (size_t i = 0; i < level1_vec.size(); ++i) {
        std::string name = "level1[" + std::to_string(i) + "]";
        print_component(name.c_str(), *level1_vec[i]);
    }

    std::cout << "\n  Level 2 containers (first 2):" << std::endl;
    for (size_t i = 0; i < std::min(level2_vec.size(), size_t(2)); ++i) {
        std::string name = "level2[" + std::to_string(i) + "]";
        print_component(name.c_str(), *level2_vec[i]);
    }

    std::cout << "\n  Level 3 containers (first 2):" << std::endl;
    for (size_t i = 0; i < std::min(level3_vec.size(), size_t(2)); ++i) {
        std::string name = "level3[" + std::to_string(i) + "]";
        print_component(name.c_str(), *level3_vec[i]);
    }

    std::cout << "\n  Leaf nodes (first 6):" << std::endl;
    for (size_t i = 0; i < std::min(leaf_vec.size(), size_t(6)); ++i) {
        std::string name = "leaf[" + std::to_string(i) + "]";
        print_component(name.c_str(), *leaf_vec[i]);
    }

    cleanup_entities();
}

TEST_CASE("VERIFY: expand sizing layout", "[verify]") {
    // Test expand() distributes space proportionally
    cleanup_entities();

    auto &root = make_root();
    auto &container = EntityHelper::createEntity();
    make_component(container)
        .set_desired_width(pixels(300.f))  // Fixed width container
        .set_desired_height(pixels(100.f))
        .set_flex_direction(FlexDirection::Row)
        .set_flex_wrap(FlexWrap::NoWrap)
        .set_parent(root);

    // Create 3 children: expand(1), expand(2), expand(1)
    // Should get 75px, 150px, 75px respectively
    auto &child1 = EntityHelper::createEntity();
    make_component(child1)
        .set_desired_width(expand(1.f))
        .set_desired_height(pixels(50.f))
        .set_parent(container);

    auto &child2 = EntityHelper::createEntity();
    make_component(child2)
        .set_desired_width(expand(2.f))  // Gets 2x share
        .set_desired_height(pixels(50.f))
        .set_parent(container);

    auto &child3 = EntityHelper::createEntity();
    make_component(child3)
        .set_desired_width(expand(1.f))
        .set_desired_height(pixels(50.f))
        .set_parent(container);

    run_layout(root);

    std::cout << "\n=== VERIFY: expand sizing ===" << std::endl;
    std::cout << "Structure: Container(300px wide, horizontal, no-wrap)" << std::endl;
    std::cout << "           3 children with expand(1), expand(2), expand(1)" << std::endl;
    std::cout << "\nExpected:" << std::endl;
    std::cout << "  Container: 300x100" << std::endl;
    std::cout << "  Child1: w=75 (1/4 of 300)" << std::endl;
    std::cout << "  Child2: w=150 (2/4 of 300)" << std::endl;
    std::cout << "  Child3: w=75 (1/4 of 300)" << std::endl;
    std::cout << "\nActual:" << std::endl;
    print_component("root", root);
    print_component("container", container);
    print_component("child1 (expand=1)", child1);
    print_component("child2 (expand=2)", child2);
    print_component("child3 (expand=1)", child3);

    cleanup_entities();
}

TEST_CASE("VERIFY: nested expand weights layout", "[verify]") {
    // Test nested containers with different expand weights (1:2)
    cleanup_entities();

    auto &root = make_root();
    auto &container = EntityHelper::createEntity();
    make_component(container)
        .set_desired_width(pixels(90.f))  // Fixed width: children should get 30:60
        .set_desired_height(pixels(50.f))
        .set_flex_direction(FlexDirection::Row)
        .set_flex_wrap(FlexWrap::NoWrap)
        .set_parent(root);

    // expand(1) + expand(2) = 3 total weight
    // child1 gets 1/3 * 90 = 30
    // child2 gets 2/3 * 90 = 60
    auto &child1 = EntityHelper::createEntity();
    make_component(child1)
        .set_desired_width(expand(1.f))
        .set_desired_height(pixels(50.f))
        .set_parent(container);

    auto &child2 = EntityHelper::createEntity();
    make_component(child2)
        .set_desired_width(expand(2.f))
        .set_desired_height(pixels(50.f))
        .set_parent(container);

    run_layout(root);

    std::cout << "\n=== VERIFY: nested expand weights ===" << std::endl;
    std::cout << "Structure: Container(90px wide, horizontal)" << std::endl;
    std::cout << "           2 children with expand(1), expand(2)" << std::endl;
    std::cout << "\nExpected:" << std::endl;
    std::cout << "  Container: 90x50" << std::endl;
    std::cout << "  Child1: w=30 (1/3 of 90)" << std::endl;
    std::cout << "  Child2: w=60 (2/3 of 90)" << std::endl;
    std::cout << "\nActual:" << std::endl;
    print_component("root", root);
    print_component("container", container);
    print_component("child1 (expand=1)", child1);
    print_component("child2 (expand=2)", child2);

    cleanup_entities();
}

// ============================================================================
// SUPPORTED BENCHMARKS - These work with current layout features
// ============================================================================

TEST_CASE("nested_vertical_stack - 10001 nodes", "[benchmark][supported]") {
    // Width(Pixels(200)).Height(Fit).Vertical().Padding(10).Gap(5)
    // {
    //     Repeat(10000)
    //     {
    //         Width(Expand).Height(Pixels(1)) {}
    //     }
    // }

    BENCHMARK_ADVANCED("nested_vertical_stack")(Catch::Benchmark::Chronometer meter) {
        cleanup_entities();

        auto &root = make_root();
        auto &container = EntityHelper::createEntity();
        make_component(container)
            .set_desired_width(pixels(200.f))
            .set_desired_height(children())  // Fit = children()
            .set_flex_direction(FlexDirection::Column)
            .set_desired_padding(pixels(10.f), Axis::X)
            .set_desired_padding(pixels(10.f), Axis::Y)
            // Note: Gap is not directly supported, using padding as approximation
            .set_parent(root);

        // Create 10,000 children
        for (int i = 0; i < 10000; ++i) {
            auto &child = EntityHelper::createEntity();
            make_component(child)
                .set_desired_width(percent(1.f))  // Expand approximated as 100%
                .set_desired_height(pixels(1.f))
                .set_parent(container);
        }

        meter.measure([&root] {
            run_layout(root);
            return root.get<UIComponent>().rect().height;
        });

        cleanup_entities();
    };
}

TEST_CASE("padding_and_margin - 101 nodes", "[benchmark][supported]") {
    // Width(Pixels(1000)).Height(Fit).Vertical().Padding(20).Gap(10)
    // {
    //     Repeat(100)
    //     {
    //         Width(Expand).Height(Pixels(20)).Margin(2) {}
    //     }
    // }

    BENCHMARK_ADVANCED("padding_and_margin")(Catch::Benchmark::Chronometer meter) {
        cleanup_entities();

        auto &root = make_root();
        auto &container = EntityHelper::createEntity();
        make_component(container)
            .set_desired_width(pixels(1000.f))
            .set_desired_height(children())
            .set_flex_direction(FlexDirection::Column)
            .set_desired_padding(pixels(20.f), Axis::X)
            .set_desired_padding(pixels(20.f), Axis::Y)
            .set_parent(root);

        // Create 100 children with margin
        for (int i = 0; i < 100; ++i) {
            auto &child = EntityHelper::createEntity();
            make_component(child)
                .set_desired_width(percent(1.f))
                .set_desired_height(pixels(20.f))
                .set_desired_margin(pixels(2.f), Axis::X)
                .set_desired_margin(pixels(2.f), Axis::Y)
                .set_parent(container);
        }

        meter.measure([&root] {
            run_layout(root);
            return root.get<UIComponent>().rect().height;
        });

        cleanup_entities();
    };
}

TEST_CASE("wide_no_wrap_simple_few - 1001 nodes", "[benchmark][supported]") {
    // Width(Pixels(100)).Height(Pixels(100)).Horizontal()
    // {
    //     Repeat(1000)
    //     {
    //         Width(Pixels(10)).Height(Pixels(10)) {}
    //     }
    // }

    BENCHMARK_ADVANCED("wide_no_wrap_simple_few")(Catch::Benchmark::Chronometer meter) {
        cleanup_entities();

        auto &root = make_root();
        auto &container = EntityHelper::createEntity();
        make_component(container)
            .set_desired_width(pixels(100.f))
            .set_desired_height(pixels(100.f))
            .set_flex_direction(FlexDirection::Row)
            .set_flex_wrap(FlexWrap::NoWrap)
            .set_parent(root);

        // Create 1,000 children
        for (int i = 0; i < 1000; ++i) {
            auto &child = EntityHelper::createEntity();
            make_component(child)
                .set_desired_width(pixels(10.f))
                .set_desired_height(pixels(10.f))
                .set_parent(container);
        }

        meter.measure([&root] {
            run_layout(root);
            return root.get<UIComponent>().rect().width;
        });

        cleanup_entities();
    };
}

TEST_CASE("wide_no_wrap_simple_many - 100001 nodes", "[benchmark][supported]") {
    // Width(Pixels(100)).Height(Pixels(100)).Horizontal()
    // {
    //     Repeat(100000)
    //     {
    //         Width(Pixels(10)).Height(Pixels(10)) {}
    //     }
    // }

    BENCHMARK_ADVANCED("wide_no_wrap_simple_many")(Catch::Benchmark::Chronometer meter) {
        cleanup_entities();

        auto &root = make_root();
        auto &container = EntityHelper::createEntity();
        make_component(container)
            .set_desired_width(pixels(100.f))
            .set_desired_height(pixels(100.f))
            .set_flex_direction(FlexDirection::Row)
            .set_flex_wrap(FlexWrap::NoWrap)
            .set_parent(root);

        // Create 100,000 children
        for (int i = 0; i < 100000; ++i) {
            auto &child = EntityHelper::createEntity();
            make_component(child)
                .set_desired_width(pixels(10.f))
                .set_desired_height(pixels(10.f))
                .set_parent(container);
        }

        meter.measure([&root] {
            run_layout(root);
            return root.get<UIComponent>().rect().width;
        });

        cleanup_entities();
    };
}

TEST_CASE("wide_wrapping - 10001 nodes", "[benchmark][supported]") {
    // Width(Pixels(100)).Wrap(Auto).Horizontal()
    // {
    //     Repeat(10000)
    //     {
    //         Width(Pixels(10)).Height(Pixels(10)) {}
    //     }
    // }

    BENCHMARK_ADVANCED("wide_wrapping")(Catch::Benchmark::Chronometer meter) {
        cleanup_entities();

        auto &root = make_root();
        auto &container = EntityHelper::createEntity();
        make_component(container)
            .set_desired_width(pixels(100.f))
            .set_desired_height(children())
            .set_flex_direction(FlexDirection::Row)
            .set_flex_wrap(FlexWrap::Wrap)
            .set_parent(root);

        // Create 10,000 children
        for (int i = 0; i < 10000; ++i) {
            auto &child = EntityHelper::createEntity();
            make_component(child)
                .set_desired_width(pixels(10.f))
                .set_desired_height(pixels(10.f))
                .set_parent(container);
        }

        meter.measure([&root] {
            run_layout(root);
            return root.get<UIComponent>().rect().height;
        });

        cleanup_entities();
    };
}

TEST_CASE("fit_nesting - 101111 nodes", "[benchmark][supported]") {
    // Width(Pixels(1000)).Height(Fit).Vertical().Padding(5)
    // {
    //     Repeat(10)
    //     {
    //         Width(Expand).Height(Fit).Horizontal()
    //         {
    //             Repeat(10)
    //             {
    //                 Width(Expand).Height(Fit).Vertical()
    //                 {
    //                     Repeat(10)
    //                     {
    //                         Width(Expand).Height(Fit).Horizontal()
    //                         {
    //                             Repeat(100)
    //                             {
    //                                 Width(Expand).Height(Pixels(10)) {}
    //                             }
    //                         }
    //                     }
    //                 }
    //             }
    //         }
    //     }
    // }

    BENCHMARK_ADVANCED("fit_nesting")(Catch::Benchmark::Chronometer meter) {
        cleanup_entities();

        auto &root = make_root();
        auto &level0 = EntityHelper::createEntity();
        make_component(level0)
            .set_desired_width(pixels(1000.f))
            .set_desired_height(children())
            .set_flex_direction(FlexDirection::Column)
            .set_desired_padding(pixels(5.f), Axis::X)
            .set_desired_padding(pixels(5.f), Axis::Y)
            .set_parent(root);

        // 10 level-1 containers
        for (int i = 0; i < 10; ++i) {
            auto &level1 = EntityHelper::createEntity();
            make_component(level1)
                .set_desired_width(percent(1.f))
                .set_desired_height(children())
                .set_flex_direction(FlexDirection::Row)
                .set_parent(level0);

            // 10 level-2 containers
            for (int j = 0; j < 10; ++j) {
                auto &level2 = EntityHelper::createEntity();
                make_component(level2)
                    .set_desired_width(percent(0.1f))  // 1/10 of parent
                    .set_desired_height(children())
                    .set_flex_direction(FlexDirection::Column)
                    .set_parent(level1);

                // 10 level-3 containers
                for (int k = 0; k < 10; ++k) {
                    auto &level3 = EntityHelper::createEntity();
                    make_component(level3)
                        .set_desired_width(percent(1.f))
                        .set_desired_height(children())
                        .set_flex_direction(FlexDirection::Row)
                        .set_parent(level2);

                    // 100 leaf nodes
                    for (int l = 0; l < 100; ++l) {
                        auto &leaf = EntityHelper::createEntity();
                        make_component(leaf)
                            .set_desired_width(percent(0.01f))  // 1/100 of parent
                            .set_desired_height(pixels(10.f))
                            .set_parent(level3);
                    }
                }
            }
        }

        meter.measure([&root] {
            run_layout(root);
            return root.get<UIComponent>().rect().height;
        });

        cleanup_entities();
    };
}

// ============================================================================
// STUBBED BENCHMARKS - Require future features (Expand, Min/Max constraints)
// ============================================================================

/*
 * TODO: Enable these benchmarks once the following features are implemented:
 *
 * 1. Expand sizing (flex-grow equivalent)
 *    - Width(Expand(1)) - grow to fill available space with weight 1
 *    - Width(Expand(2)) - grow with weight 2 (gets 2x space vs weight 1)
 *
 * 2. Min/Max size constraints
 *    - .MinWidth(Pixels(x)) - minimum width constraint
 *    - .MaxWidth(Pixels(x)) - maximum width constraint
 *    - .MinWidth(Expand(1)) - use expand as minimum constraint
 *
 * 3. Aspect ratio
 *    - Height(Ratio(0.5)) - height is 0.5x the width
 */

#if 0  // expand_with_max_constraint - 3001 nodes

TEST_CASE("expand_with_max_constraint - 3001 nodes", "[benchmark][requires-expand]") {
    // Width(Pixels(100)).Height(Fit).Horizontal()
    // {
    //     Repeat(1000)
    //     {
    //         Width(Pixels(100)).Height(Fit).Horizontal()
    //         {
    //             Width(Expand(1)).Height(Pixels(10)) {}
    //             Width(Expand(1)).Height(Pixels(10)).MaxWidth(Pixels(40)) {}
    //         }
    //     }
    // }
    //
    // Requires: Expand sizing + MaxWidth constraint

    BENCHMARK_ADVANCED("expand_with_max_constraint")(Catch::Benchmark::Chronometer meter) {
        cleanup_entities();

        auto &root = make_root();
        // TODO: Implement when Expand and MaxWidth are available

        meter.measure([&root] {
            run_layout(root);
            return root.get<UIComponent>().rect().height;
        });

        cleanup_entities();
    };
}

#endif  // expand_with_max_constraint

#if 0  // expand_with_min_constraint - 3001 nodes

TEST_CASE("expand_with_min_constraint - 3001 nodes", "[benchmark][requires-expand]") {
    // Width(Pixels(100)).Height(Fit).Horizontal()
    // {
    //     Repeat(1000)
    //     {
    //         Width(Pixels(100)).Height(Fit).Horizontal()
    //         {
    //             Width(Expand(1)).Height(Pixels(10)) {}
    //             Width(Expand(1)).Height(Pixels(10)).MinWidth(Pixels(60)) {}
    //         }
    //     }
    // }
    //
    // Requires: Expand sizing + MinWidth constraint

    BENCHMARK_ADVANCED("expand_with_min_constraint")(Catch::Benchmark::Chronometer meter) {
        cleanup_entities();

        auto &root = make_root();
        // TODO: Implement when Expand and MinWidth are available

        meter.measure([&root] {
            run_layout(root);
            return root.get<UIComponent>().rect().height;
        });

        cleanup_entities();
    };
}

#endif  // expand_with_min_constraint

#if 1  // flex_expand_equal_weights - 15001 nodes

TEST_CASE("flex_expand_equal_weights - 15001 nodes", "[benchmark][supported]") {
    // Width(Pixels(10000)).Height(Pixels(100)).Horizontal()
    // {
    //     Repeat(15000)
    //     {
    //         Width(Expand(1)).Height(Expand) {}
    //     }
    // }

    BENCHMARK_ADVANCED("flex_expand_equal_weights")(Catch::Benchmark::Chronometer meter) {
        cleanup_entities();

        auto &root = make_root();
        auto &container = EntityHelper::createEntity();
        make_component(container)
            .set_desired_width(pixels(10000.f))
            .set_desired_height(pixels(100.f))
            .set_flex_direction(FlexDirection::Row)
            .set_flex_wrap(FlexWrap::NoWrap)
            .set_parent(root);

        // Create 15,000 children with equal expand weight
        for (int i = 0; i < 15000; ++i) {
            auto &child = EntityHelper::createEntity();
            make_component(child)
                .set_desired_width(expand(1.f))
                .set_desired_height(expand(1.f))
                .set_parent(container);
        }

        meter.measure([&root] {
            run_layout(root);
            return root.get<UIComponent>().rect().width;
        });

        cleanup_entities();
    };
}

#endif  // flex_expand_equal_weights

#if 1  // flex_expand_weights - 15001 nodes

TEST_CASE("flex_expand_weights - 15001 nodes", "[benchmark][supported]") {
    // Width(Pixels(10000)).Height(Pixels(100)).Horizontal()
    // {
    //     Repeat(5000)
    //     {
    //         Width(100).Height(100).Horizontal()
    //         {
    //             Width(Expand(1)).Height(Expand) {}
    //             Width(Expand(2)).Height(Expand) {}
    //         }
    //     }
    // }

    BENCHMARK_ADVANCED("flex_expand_weights")(Catch::Benchmark::Chronometer meter) {
        cleanup_entities();

        auto &root = make_root();
        auto &outer = EntityHelper::createEntity();
        make_component(outer)
            .set_desired_width(pixels(10000.f))
            .set_desired_height(pixels(100.f))
            .set_flex_direction(FlexDirection::Row)
            .set_flex_wrap(FlexWrap::Wrap)
            .set_parent(root);

        // Create 5,000 nested containers, each with 2 expand children (1:2 ratio)
        for (int i = 0; i < 5000; ++i) {
            auto &container = EntityHelper::createEntity();
            make_component(container)
                .set_desired_width(pixels(100.f))
                .set_desired_height(pixels(100.f))
                .set_flex_direction(FlexDirection::Row)
                .set_flex_wrap(FlexWrap::NoWrap)
                .set_parent(outer);

            auto &child1 = EntityHelper::createEntity();
            make_component(child1)
                .set_desired_width(expand(1.f))
                .set_desired_height(expand(1.f))
                .set_parent(container);

            auto &child2 = EntityHelper::createEntity();
            make_component(child2)
                .set_desired_width(expand(2.f))  // Gets 2x share
                .set_desired_height(expand(1.f))
                .set_parent(container);
        }

        meter.measure([&root] {
            run_layout(root);
            return root.get<UIComponent>().rect().width;
        });

        cleanup_entities();
    };
}

#endif  // flex_expand_weights

#if 0  // percentage_and_ratio - 10001 nodes

TEST_CASE("percentage_and_ratio - 10001 nodes", "[benchmark][requires-ratio]") {
    // Width(Pixels(1000)).Height(Pixels(1000)).Vertical()
    // {
    //     Repeat(10000)
    //     {
    //         Width(Percentage(0.5)).Height(Ratio(0.5)) {}
    //     }
    // }
    //
    // Requires: Aspect ratio support (Height based on Width)

    BENCHMARK_ADVANCED("percentage_and_ratio")(Catch::Benchmark::Chronometer meter) {
        cleanup_entities();

        auto &root = make_root();
        // TODO: Implement when aspect ratio is available

        meter.measure([&root] {
            run_layout(root);
            return root.get<UIComponent>().rect().height;
        });

        cleanup_entities();
    };
}

#endif  // percentage_and_ratio

#if 0  // perpendicular_expand_with_wrap - 12001 nodes

TEST_CASE("perpendicular_expand_with_wrap - 12001 nodes", "[benchmark][requires-expand]") {
    // Width(Pixels(100)).Height(Fit).Wrap(Auto).Horizontal()
    // {
    //     Repeat(1000)
    //     {
    //         Width(Pixels(10)).Height(Pixels(10)) {}
    //         Width(Pixels(1)).Height(Expand(1)) {}
    //         Width(Pixels(20)).Height(Pixels(20)) {}
    //         Width(Pixels(1)).Height(Expand(1)) {}
    //         Width(Pixels(30)).Height(Pixels(30)) {}
    //         Width(Pixels(1)).Height(Expand(1)) {}
    //         Width(Pixels(40)).Height(Pixels(40)) {}
    //         Width(Pixels(1)).Height(Expand(1)) {}
    //         Width(Pixels(50)).Height(Pixels(50)) {}
    //         Width(Pixels(1)).Height(Expand(1)) {}
    //         Width(Pixels(60)).Height(Pixels(60)) {}
    //         Width(Pixels(1)).Height(Expand(1)) {}
    //     }
    // }
    //
    // Requires: Expand in cross-axis combined with wrapping

    BENCHMARK_ADVANCED("perpendicular_expand_with_wrap")(Catch::Benchmark::Chronometer meter) {
        cleanup_entities();

        auto &root = make_root();
        // TODO: Implement when Expand in cross-axis with wrap is available

        meter.measure([&root] {
            run_layout(root);
            return root.get<UIComponent>().rect().height;
        });

        cleanup_entities();
    };
}

#endif  // perpendicular_expand_with_wrap

#if 0  // pixels_with_min_expand_constraint - 30001 nodes

TEST_CASE("pixels_with_min_expand_constraint - 30001 nodes", "[benchmark][requires-expand]") {
    // Width(Pixels(100)).Height(Fit).Horizontal()
    // {
    //     Repeat(10000)
    //     {
    //         Width(Pixels(100)).Height(Fit).Horizontal()
    //         {
    //             Width(Pixels(10)).Height(Pixels(10)).MinWidth(Expand(1)) {}
    //             Width(Pixels(10)).Height(Pixels(10)).MinWidth(Expand(1)) {}
    //         }
    //     }
    // }
    //
    // Requires: Expand as a min constraint (MinWidth(Expand(1)))

    BENCHMARK_ADVANCED("pixels_with_min_expand_constraint")(Catch::Benchmark::Chronometer meter) {
        cleanup_entities();

        auto &root = make_root();
        // TODO: Implement when Expand as min constraint is available

        meter.measure([&root] {
            run_layout(root);
            return root.get<UIComponent>().rect().height;
        });

        cleanup_entities();
    };
}

#endif  // pixels_with_min_expand_constraint
