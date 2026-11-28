#include <cstdio>
#include <string>
#include <vector>

#define AFTER_HOURS_EXAMPLE_UI
#include "../../../example_helper.h"
#include "../../../../src/plugins/ui/tree_view.h"

using namespace afterhours;
using namespace afterhours::examples;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

struct File {
  std::string name;
  int bytes = 0;
};

const std::vector<TreeNode<File>> project = {
    {{"src"},
     {{{"main.cpp", 2048}, {}, true}, {{"tree_view.h", 6100}, {}, true}},
     false},
    {{"README.md", 512}, {}, true},
};

int rows_drawn = 0;

struct Browser : System<DefaultUIContext> {
  void for_each_with(Entity &entity, DefaultUIContext &ctx, float) override {
    TreeViewConfig<File> config;
    config.get_id = [](const File &f) { return f.name; };
    config.get_label = [](const File &f) { return f.name; };
    config.render_row = [&ctx](Entity &row, const File &f,
                               const TreeRowInfo &info) {
      const std::string mark =
          info.expandable ? (info.expanded ? "- " : "+ ") : "  ";
      div(ctx, mk(row, 0),
          ComponentConfig{}
              .with_size(ComponentSize{expand(), percent(1.f)})
              .with_label(mark + f.name)
              .with_alignment(TextAlignment::Left)
              .with_skip_tabbing(true));
      if (!info.expandable)
        div(ctx, mk(row, 1),
            ComponentConfig{}
                .with_size(ComponentSize{pixels(80), percent(1.f)})
                .with_label(std::to_string(f.bytes) + " B")
                .with_alignment(TextAlignment::Right)
                .with_skip_tabbing(true));
      printf("%*s%s%s\n", info.depth * 2, "", mark.c_str(), f.name.c_str());
      ++rows_drawn;
    };

    auto tree = tree_view(ctx, mk(entity, 0), project, config,
                          ComponentConfig{}.with_size(
                              ComponentSize{pixels(320), pixels(200)}));
    tree.ent().get<HasTreeViewState>().expanded_nodes.insert("src");
  }
};

int main(int, char **) {
  Checks checks;
  auto app = make_logic_ui<Browser>({1280, 720}, std::make_unique<Browser>());
  app.run(1);
  rows_drawn = 0;
  app.run(1);
  checks.check(rows_drawn == 4, "expanded tree draws all 4 rows");
  return checks.finish("tree_view_rows");
}
