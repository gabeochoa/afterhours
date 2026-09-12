# Timing charts

Include `src/plugins/charts.h` for backend-independent point bounds and nearest-sample lookup. Include `src/plugins/ui/line_chart.h` for `ui::imm::line_chart`.

Pass owned `ChartSeries` values (name, points, color), optional unit and selected sample index, and an ordinary `ComponentConfig`. The widget retains its data through the draw callback. Mouse hover selects the nearest X sample in each series. Applications can expose keyboard sample controls through `selected_index`.

The plot shares axes across series, displays bounds and a legend, handles empty and constant series, and leaves gaps at nonfinite samples. X values should be ordered for connected lines. Use short series names for the compact legend. The widget uses theme text/grid colors and explicit series colors. The minimum drawing area is 120 by 90 pixels.

`wm --screen=chart_lab` demonstrates empty, single, constant, negative, multiple and live series, plus keyboard-accessible sample controls. The first release covers timing line plots; bars, histograms, pie charts and broader chart interactions remain future work.

`LineChartOptions::label_font_size` sets the legend, axis and hover-label text
size. It defaults to 12px. The plot reserves more space for larger labels.
