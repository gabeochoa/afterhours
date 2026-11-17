// tree_view_test.cpp
// Regression tests for the imm::tree_view widget.
//
// Covers: a default TreeViewConfig leaves get_id/get_label empty, and the
// widget used to invoke them unchecked, throwing std::bad_function_call
// mid-frame. It must log an error once and render nothing instead.

#include <string>
#include <vector>

#include "ui_test_harness.h"
#include <afterhours/src/plugins/ui/tree_view.h>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

namespace {
std::string captured_errors;
void capture_error(const char *level, const char *message) {
  if (level && message &&
      std::string_view(level).find("ERROR") != std::string_view::npos) {
    captured_errors += message;
    captured_errors += '\n';
  }
}
} // namespace

// Missing required callbacks must not throw; the widget bails with an error.
TEST(tree_view_missing_callbacks_do_not_throw) {
  const std::vector<TreeNode<std::string>> roots = {{"root", {}, true}};

  for (int missing = 0; missing < 3; missing++) {
    ImmTestHarness h;
    TreeViewConfig<std::string> cfg;
    if (missing != 1)
      cfg.get_id = [](const std::string &s) { return s; };
    if (missing != 2)
      cfg.get_label = [](const std::string &s) { return s; };
    // missing==0 leaves both empty, 1 leaves get_id empty, 2 get_label.
    if (missing == 0)
      cfg.get_id = nullptr, cfg.get_label = nullptr;

    captured_errors.clear();
    const auto previous_sink = log_sink_fn;
    log_sink_fn = capture_error;
    bool threw = false;
    try {
      tree_view(h.context(), mk(h.root(), 0), roots, cfg);
      h.layout_only();
    } catch (...) {
      threw = true;
    }
    log_sink_fn = previous_sink;

    CHECK(!threw);
    if (missing == 0)
      CHECK(captured_errors.find("tree_view") != std::string::npos);
    CHECK(h.find("tree_row_label") == nullptr);
  }
}

// A complete config still renders rows.
TEST(tree_view_valid_config_renders_rows) {
  ImmTestHarness h;
  const std::vector<TreeNode<std::string>> roots = {{"root", {}, true}};
  TreeViewConfig<std::string> cfg;
  cfg.get_id = [](const std::string &s) { return s; };
  cfg.get_label = [](const std::string &s) { return s; };

  tree_view(h.context(), mk(h.root(), 0), roots, cfg);
  h.layout_only();

  UIComponent *label = h.find("tree_row_label");
  CHECK(label != nullptr);
  if (label) {
    auto ent = UICollectionHolder::getEntityForID(label->id);
    CHECK(ent->get<HasLabel>().label.find("root") != std::string::npos);
  }
}

TEST(tree_view_render_row_builds_each_visible_row) {
  ImmTestHarness h;
  const std::vector<TreeNode<std::string>> roots = {
      {"src", {{"main.cpp", {}, true}, {"util.h", {}, true}}, false},
      {"README", {}, true}};
  TreeViewConfig<std::string> cfg;
  cfg.get_id = [](const std::string &s) { return s; };
  cfg.get_label = [](const std::string &s) { return s; };

  std::vector<std::pair<std::string, TreeRowInfo>> seen;
  cfg.render_row = [&](Entity &row, const std::string &name,
                       const TreeRowInfo &info) {
    seen.emplace_back(name, info);
    div(h.context(), mk(row, 0),
        ComponentConfig{}
            .with_size(ComponentSize{expand(), percent(1.f)})
            .with_label(name + (info.expandable ? "/" : ""))
            .with_debug_name("custom_" + name));
  };

  const auto build = [&] {
    return tree_view(h.context(), mk(h.root(), 0), roots, cfg);
  };
  auto &state = build().ent().get<HasTreeViewState>();
  state.expanded_nodes.insert("src");
  state.selected_node_id = "util.h";
  h.layout_only();
  h.begin_frame();
  seen.clear();
  build();
  h.layout_only();

  CHECK(seen.size() == 4);
  if (seen.size() == 4) {
    CHECK(seen[0].first == "src" && seen[0].second.depth == 0 &&
          seen[0].second.expandable && seen[0].second.expanded);
    CHECK(seen[1].first == "main.cpp" && seen[1].second.depth == 1 &&
          !seen[1].second.expandable && !seen[1].second.selected);
    CHECK(seen[2].first == "util.h" && seen[2].second.selected);
    CHECK(seen[3].first == "README" && seen[3].second.depth == 0);
  }
  CHECK(h.find("tree_row_label") == nullptr);

  UIComponent *custom = h.find("custom_main.cpp");
  UIComponent *custom_root = h.find("custom_src");
  CHECK(custom != nullptr && custom_root != nullptr);
  if (custom && custom_root) {
    CHECK(UICollectionHolder::getEntityForID(custom->parent)
              ->get<UIComponentDebug>()
              .name() == "tree_row");
    CHECK_APPROX(custom->rect().x - custom_root->rect().x, cfg.indent_width);
  }
}

int main() { return ui_test::run_registered_tests("tree_view tests"); }
