#pragma once

// A grid with explicit rows and columns.
//
// Flex gets you a row of things or a column of things. What it does not give
// you is a column that lines up across rows: today that is done by handing
// every cell the same fixed width, which is the thing that keeps breaking at
// other resolutions. Here the column tracks are declared once and every row
// uses them.
//
//   auto tbl = grid(ctx, mk(parent),
//                   GridConfig{}.with_rows(3).with_cols(4)
//                       .with_cell_border(BorderWeight::Thin, line_color));
//   grid_cell(ctx, tbl, r, c, ComponentConfig{}.with_label(text));

#include <vector>

#include "../../developer.h"
#include "component_config.h"
#include "imm_components.h"

namespace afterhours {
namespace ui {
namespace imm {

enum struct BorderWeight { None, Thin, Medium, Thick };

inline float border_px(BorderWeight w) {
  switch (w) {
  case BorderWeight::None:
    return 0.f;
  case BorderWeight::Thin:
    return 1.f;
  case BorderWeight::Medium:
    return 2.f;
  case BorderWeight::Thick:
    return 4.f;
  }
  return 0.f;
}

struct GridConfig {
  int rows = 0;
  int cols = 0;
  // Empty means equal columns. Otherwise one Size per column, so a table can
  // have a narrow gutter and a wide body without measuring anything.
  std::vector<Size> col_widths;
  Size row_height = pixels(32.f);
  Size gap{};
  BorderWeight cell_border = BorderWeight::None;
  ColorType cell_border_color{};

  GridConfig &with_rows(int r) {
    rows = r;
    return *this;
  }
  GridConfig &with_cols(int c) {
    cols = c;
    return *this;
  }
  GridConfig &with_col_widths(std::vector<Size> w) {
    col_widths = std::move(w);
    return *this;
  }
  GridConfig &with_row_height(Size h) {
    row_height = h;
    return *this;
  }
  GridConfig &with_gap(Size g) {
    gap = g;
    return *this;
  }
  GridConfig &with_cell_border(BorderWeight w, ColorType c) {
    cell_border = w;
    cell_border_color = c;
    return *this;
  }
};

// The row entities, so grid_cell can find the one it belongs in. Also carries
// the track sizes, so a cell does not have to be told its own width.
struct HasGridRows : BaseComponent {
  std::vector<EntityID> rows;
  std::vector<Size> col_widths;
  int cols = 0;
  BorderWeight cell_border = BorderWeight::None;
  ColorType cell_border_color{};
};

inline ElementResult grid(HasUIContext auto &ctx, EntityParent ep_pair,
                          const GridConfig &grid_config,
                          ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);

  if (grid_config.rows <= 0 || grid_config.cols <= 0)
    return ElementResult{false, entity};

  auto container =
      vstack(ctx, ep_pair,
             ComponentConfig(config).with_gap(grid_config.gap).with_no_wrap());

  auto &state = entity.template addComponentIfMissing<HasGridRows>();
  state.rows.clear();
  state.cols = grid_config.cols;
  state.col_widths = grid_config.col_widths;
  state.cell_border = grid_config.cell_border;
  state.cell_border_color = grid_config.cell_border_color;

  for (int r = 0; r < grid_config.rows; r++) {
    auto row = hstack(ctx, mk(container.ent(), r),
                      ComponentConfig{}
                          .with_size(ComponentSize{percent(1.f),
                                                   grid_config.row_height})
                          .with_gap(grid_config.gap)
                          .with_no_wrap()
                          .with_debug_name("grid_row"));
    state.rows.push_back(row.ent().id);
  }

  return ElementResult{true, entity};
}

// Placed by coordinate rather than by call order, so a caller can skip a cell
// or fill them out of order without the grid drifting.
inline ElementResult grid_cell(HasUIContext auto &ctx, ElementResult grid_elem,
                               int row, int col,
                               ComponentConfig config = ComponentConfig(),
                               int col_span = 1) {
  Entity &grid_entity = grid_elem.ent();
  if (!grid_entity.template has<HasGridRows>())
    return ElementResult{false, grid_entity};

  auto &state = grid_entity.template get<HasGridRows>();
  if (row < 0 || row >= static_cast<int>(state.rows.size()) || col < 0 ||
      col >= state.cols)
    return ElementResult{false, grid_entity};

  OptEntity row_opt = UICollectionHolder::getEntityForID(state.rows[(size_t)row]);
  if (!row_opt.valid())
    return ElementResult{false, grid_entity};

  const int span = std::max(1, std::min(col_span, state.cols - col));

  // A spanning cell takes its own track plus the ones it covers, so the rest
  // of the row still lines up with every other row.
  const auto track_size = [&]() -> Size {
    // Equal columns: expand() with a weight, so span n is n times as wide.
    if (state.col_widths.empty())
      return Size{Dim::Expand, static_cast<float>(span), 1.f};
    float total = 0.f;
    for (int i = col; i < col + span && i < (int)state.col_widths.size(); i++)
      total += state.col_widths[(size_t)i].value;
    return Size{state.col_widths[(size_t)col].dim, total, 1.f};
  };
  const ComponentSize size{track_size(), percent(1.f)};

  ComponentConfig cell = std::move(config);
  cell = cell.with_size(size);
  if (state.cell_border != BorderWeight::None && !cell.has_border())
    cell = cell.with_border(state.cell_border_color,
                            border_px(state.cell_border));
  // Square by default. Adjacent rounded cells read as a row of separate pills
  // rather than a table; a caller who wants rounded can still say so.
  if (!cell.roundness.has_value() && !cell.corner_radius.has_value())
    cell = cell.with_roundness(0.f);
  if (cell.debug_name.empty())
    cell = cell.with_debug_name("grid_cell");

  return div(ctx, mk(row_opt.asE(), col), cell);
}

} // namespace imm
} // namespace ui
} // namespace afterhours
