// dump_ui_test.cpp
// Unit tests for the dump_ui XML output, using the immediate-mode UI API
//
// Tests:
//   Helper functions:
//     - xml_escape handles &, <, >, " and passes through normal text
//     - truncate_text truncates at 80 chars with "..."
//
//   Low-level dump_ui_node:
//     - Leaf node emits self-closing tag
//     - Parent-child emits nested tags
//     - Depth guard (> 64) produces no output
//     - Unrendered root produces no output
//
//   Immediate-mode UI trees:
//     - Single div with label dumps correctly
//     - vstack with children produces nested XML
//     - hstack row of buttons produces correct names and text
//     - Nested card layout (vstack > hstack > vstack) dumps hierarchy
//     - Named components appear as name= attributes
//     - Hidden nodes emit hidden="true"
//     - Scroll view emits scroll_x/scroll_y attributes
//
// Build:
//   make dump_ui_test
// Run:
//   ./dump_ui_test

#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include <afterhours/ah.h>
#include <afterhours/src/plugins/autolayout.h>
#include <afterhours/src/plugins/e2e_testing/ui_commands.h>
#include <afterhours/src/plugins/ui/component_init.h>
#include <afterhours/src/plugins/ui/imm_components.h>

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using namespace afterhours::testing::ui_commands;

// Minimal InputAction enum with values required by UIContext
enum struct TestInputAction {
  None,
  WidgetMod,
  WidgetNext,
  WidgetBack,
  WidgetPress,
  WidgetUp,
  WidgetDown,
  WidgetLeft,
  WidgetRight,
};

// ============================================================================
// Test helpers
// ============================================================================

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name)                                                             \
  static void test_##name();                                                   \
  struct Register_##name {                                                     \
    Register_##name() { register_test(#name, test_##name); }                   \
  } register_##name##_instance;                                                \
  static void test_##name()

struct TestEntry {
  const char *name;
  void (*fn)();
};

static std::vector<TestEntry> &test_registry() {
  static std::vector<TestEntry> r;
  return r;
}

static void register_test(const char *name, void (*fn)()) {
  test_registry().push_back({name, fn});
}

static void check(bool cond, const char *expr, const char *file, int line) {
  tests_run++;
  if (cond) {
    tests_passed++;
  } else {
    fprintf(stderr, "  FAIL: %s  (%s:%d)\n", expr, file, line);
  }
}

#define CHECK(expr) check((expr), #expr, __FILE__, __LINE__)

static size_t count_occurrences(const std::string &haystack,
                                const std::string &needle) {
  size_t count = 0;
  size_t pos = 0;
  while ((pos = haystack.find(needle, pos)) != std::string::npos) {
    count++;
    pos += needle.size();
  }
  return count;
}

// ============================================================================
// Test harness for immediate-mode UI
// ============================================================================

struct ImmTestHarness {
  EntityCollection &coll;
  Entity *ctx_entity = nullptr;
  UIContext<TestInputAction> *ctx = nullptr;
  Entity *root_entity = nullptr;

  ImmTestHarness() : coll(UICollectionHolder::get().collection) {
    // Clear any leftover state from previous tests
    imm::clear_existing_ui_elements();

    // Create UIContext singleton in the UI collection
    ctx_entity = &coll.createEntity();
    ctx = &ctx_entity->addComponent<UIContext<TestInputAction>>();
    ctx->screen_width = 800;
    ctx->screen_height = 600;

    // Create a root entity that acts as the layout root
    root_entity = &coll.createEntity();
    root_entity->addComponent<UIComponent>(root_entity->id);
    root_entity->addComponent<AutoLayoutRoot>();
    auto &root_cmp = root_entity->get<UIComponent>();
    root_cmp.set_desired_width(pixels(800));
    root_cmp.set_desired_height(pixels(600));
  }

  ~ImmTestHarness() {
    // Clean up all entities
    for (const auto &e : coll.get_entities()) {
      if (e)
        e->cleanup = true;
    }
    coll.cleanup();
    imm::clear_existing_ui_elements();
  }

  UIContext<TestInputAction> &context() { return *ctx; }
  Entity &root() { return *root_entity; }

  // Run autolayout and mark everything as rendered
  void layout_and_render() {
    coll.merge_entity_arrays();

    // Build entity mapping
    auto &entities = coll.get_entities();
    EntityID max_id = 0;
    for (const auto &e : entities) {
      if (e)
        max_id = std::max(max_id, e->id);
    }
    std::vector<Entity *> mapping(static_cast<size_t>(max_id) + 1, nullptr);
    for (const auto &e : entities) {
      if (e)
        mapping[static_cast<size_t>(e->id)] = e.get();
    }

    // Run autolayout
    AutoLayout::autolayout(root_entity->get<UIComponent>(),
                           window_manager::Resolution{800, 600}, mapping, false,
                           1.0f);

    // Mark all UI entities as rendered (no real renderer in test)
    for (const auto &e : entities) {
      if (e && e->has<UIComponent>()) {
        e->get<UIComponent>().was_rendered_to_screen = true;
      }
    }
  }

  // Dump the root entity to XML
  std::string dump() {
    std::string xml;
    xml.reserve(4096);
    dump_ui_node(xml, *root_entity, 0);
    return xml;
  }

  // Dump a specific entity to XML
  std::string dump(Entity &entity) {
    std::string xml;
    xml.reserve(4096);
    dump_ui_node(xml, entity, 0);
    return xml;
  }
};

// ============================================================================
// Tests: helper functions
// ============================================================================

TEST(xml_escape_special_chars) {
  CHECK(xml_escape("hello") == "hello");
  CHECK(xml_escape("a&b") == "a&amp;b");
  CHECK(xml_escape("a<b") == "a&lt;b");
  CHECK(xml_escape("a>b") == "a&gt;b");
  CHECK(xml_escape("a\"b") == "a&quot;b");
  CHECK(xml_escape("&<>\"") == "&amp;&lt;&gt;&quot;");
  CHECK(xml_escape("") == "");
}

TEST(truncate_text_short) {
  CHECK(truncate_text("hello") == "hello");
  CHECK(truncate_text("hello", 5) == "hello");
}

TEST(truncate_text_long) {
  std::string long_str(100, 'x');
  std::string result = truncate_text(long_str);
  CHECK(result.size() == 83); // 80 + "..."
  CHECK(result.substr(80) == "...");
}

TEST(truncate_text_custom_limit) {
  CHECK(truncate_text("abcdefgh", 3) == "abc...");
}

// ============================================================================
// Tests: low-level dump_ui_node
// ============================================================================

TEST(dump_depth_guard) {
  ImmTestHarness h;
  h.layout_and_render();
  std::string xml;
  // Depth 65 should produce nothing
  dump_ui_node(xml, h.root(), 65);
  CHECK(xml.empty());
}

TEST(dump_unrendered_root) {
  ImmTestHarness h;
  h.layout_and_render();
  h.root().get<UIComponent>().was_rendered_to_screen = false;

  std::string xml;
  dump_ui_node(xml, h.root(), 0);
  CHECK(xml.empty());
}

// ============================================================================
// Tests: immediate-mode UI trees
// ============================================================================

// A single div with a label
TEST(imm_single_div_with_label) {
  ImmTestHarness h;

  imm::div(h.context(), imm::mk(h.root(), 0),
           ComponentConfig{}
               .with_label("Hello World")
               .with_size(ComponentSize{pixels(200), pixels(40)})
               .with_debug_name("greeting"));

  h.layout_and_render();
  std::string xml = h.dump();

  // Root should contain the child div
  CHECK(xml.find("name=\"greeting\"") != std::string::npos);
  CHECK(xml.find("text=\"Hello World\"") != std::string::npos);
  // Should have nesting (root > child)
  CHECK(xml.find("</ui>") != std::string::npos);
}

// vstack with two children
TEST(imm_vstack_with_children) {
  ImmTestHarness h;

  auto col = imm::vstack(h.context(), imm::mk(h.root(), 0),
                          ComponentConfig{}
                              .with_size(ComponentSize{pixels(300), pixels(200)})
                              .with_debug_name("column"));

  imm::div(h.context(), imm::mk(col.ent(), 0),
           ComponentConfig{}
               .with_label("Item 1")
               .with_size(ComponentSize{pixels(300), pixels(40)})
               .with_debug_name("item1"));

  imm::div(h.context(), imm::mk(col.ent(), 1),
           ComponentConfig{}
               .with_label("Item 2")
               .with_size(ComponentSize{pixels(300), pixels(40)})
               .with_debug_name("item2"));

  h.layout_and_render();
  std::string xml = h.dump();

  CHECK(xml.find("name=\"column\"") != std::string::npos);
  CHECK(xml.find("name=\"item1\"") != std::string::npos);
  CHECK(xml.find("name=\"item2\"") != std::string::npos);
  CHECK(xml.find("text=\"Item 1\"") != std::string::npos);
  CHECK(xml.find("text=\"Item 2\"") != std::string::npos);

  // item2 should appear after item1 in the XML
  size_t pos1 = xml.find("name=\"item1\"");
  size_t pos2 = xml.find("name=\"item2\"");
  CHECK(pos1 < pos2);
}

// hstack row of buttons
TEST(imm_hstack_buttons) {
  ImmTestHarness h;

  auto row = imm::hstack(h.context(), imm::mk(h.root(), 0),
                          ComponentConfig{}
                              .with_size(ComponentSize{pixels(400), pixels(50)})
                              .with_debug_name("toolbar"));

  imm::button(h.context(), imm::mk(row.ent(), 0),
              ComponentConfig{}
                  .with_label("Save")
                  .with_size(ComponentSize{pixels(100), pixels(40)})
                  .with_debug_name("btn_save"));

  imm::button(h.context(), imm::mk(row.ent(), 1),
              ComponentConfig{}
                  .with_label("Cancel")
                  .with_size(ComponentSize{pixels(100), pixels(40)})
                  .with_debug_name("btn_cancel"));

  h.layout_and_render();
  std::string xml = h.dump();

  CHECK(xml.find("name=\"toolbar\"") != std::string::npos);
  CHECK(xml.find("name=\"btn_save\"") != std::string::npos);
  CHECK(xml.find("name=\"btn_cancel\"") != std::string::npos);
  CHECK(xml.find("text=\"Save\"") != std::string::npos);
  CHECK(xml.find("text=\"Cancel\"") != std::string::npos);
}

// Nested card layout: vstack > title + hstack > two columns
TEST(imm_nested_card_layout) {
  ImmTestHarness h;

  auto card = imm::vstack(h.context(), imm::mk(h.root(), 0),
                           ComponentConfig{}
                               .with_size(ComponentSize{pixels(600), pixels(400)})
                               .with_debug_name("card"));

  // Title bar
  imm::div(h.context(), imm::mk(card.ent(), 0),
           ComponentConfig{}
               .with_label("Settings")
               .with_size(ComponentSize{percent(1.0f), pixels(40)})
               .with_debug_name("title"));

  // Content row
  auto content = imm::hstack(h.context(), imm::mk(card.ent(), 1),
                              ComponentConfig{}
                                  .with_size(ComponentSize{percent(1.0f), pixels(300)})
                                  .with_debug_name("content"));

  // Left column
  auto left = imm::vstack(h.context(), imm::mk(content.ent(), 0),
                            ComponentConfig{}
                                .with_size(ComponentSize{pixels(200), percent(1.0f)})
                                .with_debug_name("left_col"));

  imm::div(h.context(), imm::mk(left.ent(), 0),
           ComponentConfig{}
               .with_label("Option A")
               .with_size(ComponentSize{percent(1.0f), pixels(30)})
               .with_debug_name("opt_a"));

  // Right column
  auto right = imm::vstack(h.context(), imm::mk(content.ent(), 1),
                             ComponentConfig{}
                                 .with_size(ComponentSize{pixels(200), percent(1.0f)})
                                 .with_debug_name("right_col"));

  imm::div(h.context(), imm::mk(right.ent(), 0),
           ComponentConfig{}
               .with_label("Option B")
               .with_size(ComponentSize{percent(1.0f), pixels(30)})
               .with_debug_name("opt_b"));

  h.layout_and_render();
  std::string xml = h.dump();

  // Verify the full hierarchy exists
  CHECK(xml.find("name=\"card\"") != std::string::npos);
  CHECK(xml.find("name=\"title\"") != std::string::npos);
  CHECK(xml.find("name=\"content\"") != std::string::npos);
  CHECK(xml.find("name=\"left_col\"") != std::string::npos);
  CHECK(xml.find("name=\"right_col\"") != std::string::npos);
  CHECK(xml.find("name=\"opt_a\"") != std::string::npos);
  CHECK(xml.find("name=\"opt_b\"") != std::string::npos);
  CHECK(xml.find("text=\"Settings\"") != std::string::npos);
  CHECK(xml.find("text=\"Option A\"") != std::string::npos);
  CHECK(xml.find("text=\"Option B\"") != std::string::npos);

  // Verify nesting: card should have closing tags (it has children)
  // Count </ui> tags — at least 5 (card, content, left_col, right_col, root)
  CHECK(count_occurrences(xml, "</ui>") >= 5);

  // Verify indentation: opt_a is nested 4 levels deep
  // root(0) > card(1) > content(2) > left_col(3) > opt_a(4)
  // But the immediate API may add extra wrapper nodes (label divs),
  // so just check that opt_a is indented more than card
  size_t card_indent = xml.find("name=\"card\"");
  size_t opt_a_indent = xml.find("name=\"opt_a\"");
  CHECK(card_indent < opt_a_indent);
}

// Hidden div emits hidden="true"
TEST(imm_hidden_div) {
  ImmTestHarness h;

  imm::div(h.context(), imm::mk(h.root(), 0),
           ComponentConfig{}
               .with_label("Hidden content")
               .with_size(ComponentSize{pixels(100), pixels(40)})
               .with_debug_name("hidden_div"));

  h.layout_and_render();

  // Manually hide the div after layout
  auto &entities = h.coll.get_entities();
  for (const auto &e : entities) {
    if (e && e->has<UIComponentDebug>() &&
        e->get<UIComponentDebug>().name() == "hidden_div") {
      e->get<UIComponent>().should_hide = true;
    }
  }

  std::string xml = h.dump();
  CHECK(xml.find("hidden=\"true\"") != std::string::npos);
}

// Scroll view emits scroll attributes
TEST(imm_scroll_view) {
  ImmTestHarness h;

  imm::div(h.context(), imm::mk(h.root(), 0),
           ComponentConfig{}
               .with_size(ComponentSize{pixels(200), pixels(100)})
               .with_debug_name("scrollable"));

  h.layout_and_render();

  // Add scroll state after layout
  auto &entities = h.coll.get_entities();
  for (const auto &e : entities) {
    if (e && e->has<UIComponentDebug>() &&
        e->get<UIComponentDebug>().name() == "scrollable") {
      auto &sv = e->addComponent<HasScrollView>();
      sv.scroll_offset = {0.0f, 42.7f};
    }
  }

  std::string xml = h.dump();
  CHECK(xml.find("scroll_x=\"0\"") != std::string::npos);
  CHECK(xml.find("scroll_y=\"43\"") != std::string::npos);
}

// Integer-rounded coordinates
TEST(imm_integer_coords) {
  ImmTestHarness h;

  imm::div(h.context(), imm::mk(h.root(), 0),
           ComponentConfig{}
               .with_size(ComponentSize{pixels(123), pixels(45)})
               .with_debug_name("precise"));

  h.layout_and_render();
  std::string xml = h.dump();

  // Coordinates should be integer-formatted (no decimal points in the values)
  CHECK(xml.find("name=\"precise\"") != std::string::npos);
  CHECK(xml.find("w=\"123\"") != std::string::npos);
  CHECK(xml.find("h=\"45\"") != std::string::npos);
}

// Self-closing tags for leaf nodes
TEST(imm_leaf_self_closing) {
  ImmTestHarness h;

  // A div with no label generates no children in the immediate API
  imm::div(h.context(), imm::mk(h.root(), 0),
           ComponentConfig{}
               .with_size(ComponentSize{pixels(100), pixels(50)})
               .with_debug_name("leaf"));

  h.layout_and_render();

  // Find the leaf node and dump just it
  auto &entities = h.coll.get_entities();
  Entity *leaf = nullptr;
  for (const auto &e : entities) {
    if (e && e->has<UIComponentDebug>() &&
        e->get<UIComponentDebug>().name() == "leaf") {
      leaf = e.get();
      break;
    }
  }
  CHECK(leaf != nullptr);
  if (leaf) {
    std::string xml = h.dump(*leaf);
    CHECK(xml.find("/>") != std::string::npos);
    CHECK(xml.find("</ui>") == std::string::npos);
  }
}

// ============================================================================
// Main
// ============================================================================

int main() {
  for (auto &entry : test_registry()) {
    printf("Running %s...\n", entry.name);
    entry.fn();
  }

  printf("\n%d / %d tests passed\n", tests_passed, tests_run);
  if (tests_passed != tests_run) {
    printf("SOME TESTS FAILED\n");
    return 1;
  }
  printf("ALL TESTS PASSED\n");
  return 0;
}
