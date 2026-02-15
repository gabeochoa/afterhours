#pragma once

#include <functional>
#include <string>
#include <unordered_set>
#include <vector>

#include "component_init.h"
#include "element_result.h"
#include "entity_management.h"
#include "imm_components.h"

namespace afterhours {

namespace ui {

namespace imm {

template <typename T> struct TreeNode {
  T data;
  std::vector<TreeNode<T>> children;
  bool is_leaf = false;
};

struct HasTreeViewState : BaseComponent {
  std::unordered_set<std::string> expanded_nodes;
  std::string selected_node_id;
  bool changed_since = false;

  bool is_expanded(const std::string &id) const {
    return expanded_nodes.contains(id);
  }

  void toggle_expanded(const std::string &id) {
    if (expanded_nodes.contains(id)) {
      expanded_nodes.erase(id);
    } else {
      expanded_nodes.insert(id);
    }
  }
};

template <typename T> struct TreeViewConfig {
  float indent_width = 20.0f;
  float row_height = 28.0f;
  std::function<std::string(const T &)> get_label = nullptr;
  std::function<std::string(const T &)> get_id = nullptr;
  std::function<bool(const T &)> is_expandable = nullptr;
};

namespace detail {

template <typename T>
void render_tree_node(HasUIContext auto &ctx, Entity &scroll_entity,
                      const TreeNode<T> &node, HasTreeViewState &state,
                      const TreeViewConfig<T> &view_config,
                      const ComponentConfig &base_config, int depth,
                      int &child_index) {
  std::string node_id = view_config.get_id(node.data);
  std::string label = view_config.get_label(node.data);
  bool expandable =
      view_config.is_expandable ? view_config.is_expandable(node.data)
                                : !node.children.empty();
  bool expanded = state.is_expanded(node_id);
  bool is_selected = (state.selected_node_id == node_id);

  float indent_px = static_cast<float>(depth) * view_config.indent_width;

  // Row button for this node
  std::string arrow = expandable ? (expanded ? "v " : "> ") : "  ";
  std::string row_label = arrow + label;

  auto row_config =
      ComponentConfig::inherit_from(base_config, "tree_row")
          .with_size(ComponentSize{percent(1.0f),
                                   pixels(view_config.row_height)})
          .with_flex_direction(FlexDirection::Row)
          .with_align_items(AlignItems::Center)
          .with_no_wrap()
          .with_padding(Padding::Left(pixels(indent_px)))
          .with_label(row_label)
          .with_alignment(TextAlignment::Left);

  if (is_selected) {
    row_config.with_color_usage(Theme::Usage::Primary);
  } else {
    row_config.with_custom_background(colors::transparent());
  }

  if (button(ctx, mk(scroll_entity, child_index), row_config)) {
    if (expandable) {
      state.toggle_expanded(node_id);
    }
    state.selected_node_id = node_id;
    state.changed_since = true;
  }
  child_index++;

  // Render children if expanded
  if (expanded && expandable) {
    for (const auto &child : node.children) {
      render_tree_node(ctx, scroll_entity, child, state, view_config,
                       base_config, depth + 1, child_index);
    }
  }
}

} // namespace detail

template <typename T>
ElementResult tree_view(HasUIContext auto &ctx, EntityParent ep_pair,
                        const std::vector<TreeNode<T>> &roots,
                        const TreeViewConfig<T> &view_config,
                        ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);

  // Default size: expand to fill parent
  if (config.size.is_default) {
    config.with_size(ComponentSize{expand(), expand()});
  }

  config.with_flex_direction(FlexDirection::Column);
  init_component(ctx, ep_pair, config, ComponentType::Div, false, "tree_view");

  auto &state =
      init_state<HasTreeViewState>(entity, [&](auto &) {});

  // Scrollable container
  auto scroll_config =
      ComponentConfig::inherit_from(config, "tree_scroll")
          .with_size(ComponentSize{percent(1.0f), expand()})
          .with_overflow(Overflow::Scroll, Axis::Y)
          .with_flex_direction(FlexDirection::Column)
          .with_custom_background(colors::transparent());

  auto scroll_pair = mk(entity);
  auto [scroll_entity, scroll_parent] = deref(scroll_pair);
  div(ctx, scroll_pair, scroll_config);

  // Render all root nodes
  int child_index = 0;
  for (const auto &root : roots) {
    detail::render_tree_node(ctx, scroll_entity, root, state, view_config,
                             config, 0, child_index);
  }

  bool changed = state.changed_since;
  state.changed_since = false;
  return ElementResult{changed, entity};
}

} // namespace imm

} // namespace ui

} // namespace afterhours
