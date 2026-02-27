// calendar_stress_test.cpp
// Google Calendar-like layout stress test for afterhours.
//
// Renders a week view with configurable meeting density, measuring:
//   - Entity creation time
//   - Autolayout solve time
//   - Full render time (drawing + text)
//   - Total frame time
//
// The calendar has: header bar, day columns, hour grid lines,
// and many overlapping meeting blocks with labels.
//
// Build:
//   make calendar_stress_test
//
// Run:
//   ./calendar_stress_test                # default: 7 days, 15 meetings/day
//   ./calendar_stress_test --days=5       # weekdays only
//   ./calendar_stress_test --meetings=30  # 30 meetings per day
//   ./calendar_stress_test --hours=12     # 12 visible hours (8am-8pm)
//   ./calendar_stress_test --frames=10    # render 10 frames then exit
//   ./calendar_stress_test --headless     # hidden window (still renders)
//   ./calendar_stress_test --layout-only  # no window, entity+layout only

#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include <afterhours/ah.h>
#include <afterhours/src/drawing_helpers.h>
#include <afterhours/src/plugins/autolayout.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>

using namespace afterhours;
using namespace afterhours::ui;

static constexpr int WINDOW_W = 1280;
static constexpr int WINDOW_H = 720;

// ── Timing ──

struct FrameTiming {
  double entity_ms = 0;
  double layout_ms = 0;
  double render_ms = 0;
  double total_ms = 0;
  int entity_count = 0;
};

static double now_ms() {
  return std::chrono::duration<double, std::milli>(
             std::chrono::high_resolution_clock::now().time_since_epoch())
      .count();
}

// ── Layout harness (same pattern as headless_validation) ──

struct CalendarLayout {
  std::vector<std::unique_ptr<Entity>> entities;
  window_manager::Resolution resolution{WINDOW_W, WINDOW_H};

  Entity &make_entity() {
    entities.push_back(std::make_unique<Entity>());
    return *entities.back();
  }

  Entity &make_ui(Size w, Size h) {
    Entity &e = make_entity();
    auto &ui = e.addComponent<UIComponent>(e.id);
    ui.set_desired_width(w);
    ui.set_desired_height(h);
    return e;
  }

  void add_child(Entity &parent, Entity &child) {
    parent.get<UIComponent>().add_child(child.id);
    child.get<UIComponent>().set_parent(parent.id);
  }

  void run(Entity &root) {
    EntityID max_id = 0;
    for (auto &e : entities) max_id = std::max(max_id, e->id);
    std::vector<Entity *> mapping(static_cast<size_t>(max_id) + 1, nullptr);
    for (auto &e : entities) mapping[e->id] = e.get();
    AutoLayout::autolayout(root.get<UIComponent>(), resolution, mapping, false);
  }

  static UIComponent &ui(Entity &e) { return e.get<UIComponent>(); }
};

// ── Meeting data ──

struct Meeting {
  std::string title;
  float start_hour;
  float duration_hours;
  raylib::Color color;
};

static const raylib::Color MEETING_COLORS[] = {
    {66, 133, 244, 220},   // Google blue
    {219, 68, 55, 220},    // Google red
    {244, 180, 0, 220},    // Google yellow
    {15, 157, 88, 220},    // Google green
    {171, 71, 188, 220},   // Purple
    {255, 112, 67, 220},   // Deep orange
    {0, 172, 193, 220},    // Cyan
    {124, 179, 66, 220},   // Light green
};
static constexpr int NUM_COLORS = sizeof(MEETING_COLORS) / sizeof(MEETING_COLORS[0]);

static const char *MEETING_TITLES[] = {
    "Team Standup",    "1:1 with Manager",  "Design Review",
    "Sprint Planning", "Code Review",       "Lunch",
    "All Hands",       "Customer Call",     "Architecture Sync",
    "Demo Prep",       "Retrospective",     "Interview",
    "Focus Time",      "Budget Review",     "Onboarding",
    "Brainstorm",      "QA Review",         "Deployment",
    "Training",        "Office Hours",      "Workshop",
    "Board Meeting",   "Product Sync",      "Incident Review",
    "Tech Spec Review","Hackathon",         "Team Social",
    "Performance Rev", "Roadmap Planning",  "Release Party",
};
static constexpr int NUM_TITLES = sizeof(MEETING_TITLES) / sizeof(MEETING_TITLES[0]);

static std::vector<Meeting> generate_meetings(int count, int day_seed,
                                              int visible_hours,
                                              float start_hour) {
  std::vector<Meeting> meetings;
  meetings.reserve(count);
  unsigned seed = static_cast<unsigned>(day_seed * 1337 + 42);
  for (int i = 0; i < count; ++i) {
    seed = seed * 1103515245 + 12345;
    float hour_offset = static_cast<float>((seed >> 4) % static_cast<unsigned>(visible_hours * 4)) / 4.0f;
    seed = seed * 1103515245 + 12345;
    float dur = 0.25f + static_cast<float>((seed >> 4) % 8) * 0.25f;

    float s = start_hour + hour_offset;
    if (s + dur > start_hour + static_cast<float>(visible_hours))
      dur = start_hour + static_cast<float>(visible_hours) - s;
    if (dur < 0.25f) dur = 0.25f;

    seed = seed * 1103515245 + 12345;
    int title_idx = static_cast<int>((seed >> 4) % NUM_TITLES);
    seed = seed * 1103515245 + 12345;
    int color_idx = static_cast<int>((seed >> 4) % NUM_COLORS);

    meetings.push_back({MEETING_TITLES[title_idx], s, dur, MEETING_COLORS[color_idx]});
  }
  return meetings;
}

// ── Build + render the calendar ──

static const char *DAY_NAMES[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

struct CalendarBuildResult {
  CalendarLayout cal;
  std::vector<std::vector<Meeting>> all_meetings;
  EntityID root_id;
  EntityID header_id;
  EntityID body_id;
  EntityID gutter_id;
};

static CalendarBuildResult build_calendar(int num_days, int meetings_per_day,
                                          int visible_hours, float start_hour) {
  CalendarBuildResult result;
  auto &cal = result.cal;

  auto &root = cal.make_ui(pixels(WINDOW_W), pixels(WINDOW_H));
  cal.ui(root).set_flex_direction(FlexDirection::Column);
  result.root_id = root.id;

  auto &header = cal.make_ui(percent(1.0f), pixels(50));
  cal.ui(header).set_flex_direction(FlexDirection::Row);
  cal.add_child(root, header);
  result.header_id = header.id;

  auto &gutter_label = cal.make_ui(pixels(60), percent(1.0f));
  cal.add_child(header, gutter_label);

  for (int d = 0; d < num_days; ++d) {
    auto &day_hdr = cal.make_ui(expand(1), percent(1.0f));
    day_hdr.addComponent<HasLabel>(DAY_NAMES[d % 7]);
    cal.add_child(header, day_hdr);
  }

  auto &body = cal.make_ui(percent(1.0f), expand(1));
  cal.ui(body).set_flex_direction(FlexDirection::Row);
  cal.add_child(root, body);
  result.body_id = body.id;

  auto &gutter = cal.make_ui(pixels(60), percent(1.0f));
  cal.ui(gutter).set_flex_direction(FlexDirection::Column);
  cal.add_child(body, gutter);
  result.gutter_id = gutter.id;

  for (int h = 0; h < visible_hours; ++h) {
    auto &hour_label = cal.make_ui(percent(1.0f), expand(1));
    int display_hour = static_cast<int>(start_hour) + h;
    std::string label = fmt::format("{}:00", display_hour);
    hour_label.addComponent<HasLabel>(label);
    cal.add_child(gutter, hour_label);
  }

  for (int d = 0; d < num_days; ++d) {
    result.all_meetings.push_back(
        generate_meetings(meetings_per_day, d, visible_hours, start_hour));

    auto &day_col = cal.make_ui(expand(1), percent(1.0f));
    cal.ui(day_col).set_desired_padding(pixels(1), Axis::left);
    cal.ui(day_col).set_desired_padding(pixels(1), Axis::right);
    cal.add_child(body, day_col);

    for (int m = 0; m < meetings_per_day; ++m) {
      const Meeting &mtg = result.all_meetings.back()[m];
      float top_pct = (mtg.start_hour - start_hour) /
                      static_cast<float>(visible_hours);
      float height_pct = mtg.duration_hours / static_cast<float>(visible_hours);

      auto &block = cal.make_ui(percent(0.95f), percent(height_pct));
      cal.ui(block).absolute = true;
      cal.ui(block).set_desired_margin(percent(top_pct), Axis::top);
      cal.ui(block).set_desired_margin(percent(0.025f), Axis::left);
      block.addComponent<HasLabel>(mtg.title);
      cal.add_child(day_col, block);
    }
  }

  return result;
}

static void render_calendar(CalendarBuildResult &b, int num_days) {
  auto &cal = b.cal;
  auto &all_meetings = b.all_meetings;

  auto find_entity = [&](EntityID id) -> Entity & {
    return *cal.entities[static_cast<size_t>(id)];
  };

  raylib::ClearBackground(raylib::Color{255, 255, 255, 255});

  {
    auto r = cal.ui(find_entity(b.header_id)).rect();
    draw_rectangle(r, raylib::Color{245, 245, 245, 255});
    draw_rectangle(
        raylib::Rectangle{r.x, r.y + r.height - 1, r.width, 1},
        raylib::Color{218, 220, 224, 255});
  }

  for (EntityID child_id : cal.ui(find_entity(b.header_id)).children) {
    Entity &ep = find_entity(child_id);
    if (!ep.has<HasLabel>()) continue;
    auto rect = cal.ui(ep).rect();
    draw_text(ep.get<HasLabel>().label.c_str(),
              rect.x + rect.width / 2.f - 15.f,
              rect.y + rect.height / 2.f - 8.f, 16,
              raylib::Color{60, 64, 67, 255});
    draw_rectangle(
        raylib::Rectangle{rect.x, rect.y, 1, rect.height},
        raylib::Color{218, 220, 224, 255});
  }

  auto body_rect = cal.ui(find_entity(b.body_id)).rect();
  float body_x = body_rect.x + 60;
  float body_w = body_rect.width - 60;
  for (EntityID child_id : cal.ui(find_entity(b.gutter_id)).children) {
    Entity &ep = find_entity(child_id);
    if (!ep.has<HasLabel>()) continue;
    auto rect = cal.ui(ep).rect();
    draw_text(ep.get<HasLabel>().label.c_str(),
              rect.x + 5, rect.y + 2, 11,
              raylib::Color{112, 117, 122, 255});
    draw_rectangle(
        raylib::Rectangle{body_x, rect.y, body_w, 1},
        raylib::Color{218, 220, 224, 80});
  }

  for (EntityID child_id : cal.ui(find_entity(b.body_id)).children) {
    auto rect = cal.ui(find_entity(child_id)).rect();
    draw_rectangle(
        raylib::Rectangle{rect.x, rect.y, 1, rect.height},
        raylib::Color{218, 220, 224, 128});
  }

  int meeting_day = 0;
  for (EntityID day_col_id : cal.ui(find_entity(b.body_id)).children) {
    if (meeting_day == 0) { meeting_day++; continue; }
    int day_idx = meeting_day - 1;
    if (day_idx >= num_days) break;

    int mi = 0;
    for (EntityID block_id : cal.ui(find_entity(day_col_id)).children) {
      Entity &bp = find_entity(block_id);
      auto rect = cal.ui(bp).rect();
      if (rect.height < 2) { mi++; continue; }

      const Meeting &mtg = all_meetings[day_idx][mi];

      draw_rectangle_rounded(rect, 0.15f, 4, mtg.color,
                             std::bitset<4>("1111"));

      draw_rectangle(
          raylib::Rectangle{rect.x, rect.y + 2, 3, rect.height - 4},
          raylib::Color{
              static_cast<unsigned char>(std::max(0, mtg.color.r - 40)),
              static_cast<unsigned char>(std::max(0, mtg.color.g - 40)),
              static_cast<unsigned char>(std::max(0, mtg.color.b - 40)),
              255});

      if (rect.height > 14) {
        float font_size = std::min(12.f, rect.height - 4.f);
        draw_text(mtg.title.c_str(), rect.x + 7, rect.y + 3,
                  static_cast<int>(font_size),
                  raylib::Color{255, 255, 255, 255});
      }

      if (rect.height > 28) {
        int sh = static_cast<int>(mtg.start_hour);
        int sm = static_cast<int>((mtg.start_hour - sh) * 60);
        float end = mtg.start_hour + mtg.duration_hours;
        int eh = static_cast<int>(end);
        int em = static_cast<int>((end - eh) * 60);
        auto time_str = fmt::format("{:d}:{:02d}-{:d}:{:02d}", sh, sm, eh, em);
        draw_text(time_str.c_str(), rect.x + 7, rect.y + 17, 10,
                  raylib::Color{255, 255, 255, 180});
      }
      mi++;
      if (mi >= static_cast<int>(all_meetings[day_idx].size())) break;
    }
    meeting_day++;
  }

  {
    int visible_hours = static_cast<int>(body_rect.height);
    (void)visible_hours;
    float now_pct = 0.4f;
    float y = body_rect.y + now_pct * body_rect.height;
    draw_rectangle(
        raylib::Rectangle{body_x - 5, y - 1, body_w + 5, 2},
        raylib::Color{234, 67, 53, 200});
    draw_circle(body_x - 5, y, 5, raylib::Color{234, 67, 53, 255});
  }
}

static FrameTiming run_frame(int num_days, int meetings_per_day,
                             int visible_hours, float start_hour,
                             bool layout_only) {
  FrameTiming timing;
  double t0 = now_ms();

  double t_entity_start = now_ms();
  ENTITY_ID_GEN.store(0);
  auto built = build_calendar(num_days, meetings_per_day,
                              visible_hours, start_hour);
  timing.entity_count = static_cast<int>(built.cal.entities.size());
  timing.entity_ms = now_ms() - t_entity_start;

  double t_layout_start = now_ms();
  built.cal.run(*built.cal.entities[static_cast<size_t>(built.root_id)]);
  timing.layout_ms = now_ms() - t_layout_start;

  if (!layout_only) {
    double t_render_start = now_ms();
    render_calendar(built, num_days);
    timing.render_ms = now_ms() - t_render_start;
  }

  timing.total_ms = now_ms() - t0;
  return timing;
}

// ── Main ──

int main(int argc, char *argv[]) {
  int num_days = 7;
  int meetings_per_day = 15;
  int visible_hours = 14;
  float start_hour = 7.0f;
  int num_frames = 60;
  bool headless = false;
  bool layout_only = false;

  for (int i = 1; i < argc; ++i) {
    if (strncmp(argv[i], "--days=", 7) == 0)
      num_days = atoi(argv[i] + 7);
    else if (strncmp(argv[i], "--meetings=", 11) == 0)
      meetings_per_day = atoi(argv[i] + 11);
    else if (strncmp(argv[i], "--hours=", 8) == 0)
      visible_hours = atoi(argv[i] + 8);
    else if (strncmp(argv[i], "--frames=", 9) == 0)
      num_frames = atoi(argv[i] + 9);
    else if (strcmp(argv[i], "--headless") == 0)
      headless = true;
    else if (strcmp(argv[i], "--layout-only") == 0)
      layout_only = true;
  }

  const char *mode_str = layout_only ? "layout-only" :
                         headless    ? "headless" : "windowed";

  int total_meetings = num_days * meetings_per_day;
  printf("Calendar Stress Test\n");
  printf("====================\n");
  printf("  Days: %d, Meetings/day: %d, Total meetings: %d\n",
         num_days, meetings_per_day, total_meetings);
  printf("  Visible hours: %d (%.0f:00 - %.0f:00)\n",
         visible_hours, start_hour, start_hour + visible_hours);
  printf("  Frames: %d, Mode: %s\n\n", num_frames, mode_str);

  if (!layout_only) {
    if (headless) {
      raylib::SetConfigFlags(raylib::FLAG_WINDOW_HIDDEN);
    }
    raylib::SetTraceLogLevel(raylib::LOG_WARNING);
    raylib::InitWindow(WINDOW_W, WINDOW_H, "Calendar Stress Test");
    raylib::SetTargetFPS(0);
  }

  std::vector<FrameTiming> timings;
  timings.reserve(num_frames);

  for (int frame = 0; frame < num_frames; ++frame) {
    if (!layout_only) raylib::BeginDrawing();

    FrameTiming ft = run_frame(num_days, meetings_per_day,
                               visible_hours, start_hour, layout_only);
    timings.push_back(ft);

    if (!layout_only) {
      raylib::EndDrawing();
      if (frame == 0) {
        raylib::TakeScreenshot("calendar_stress_output.png");
      }
    }
  }

  if (!layout_only) raylib::CloseWindow();

  // ── Report ──
  if (layout_only) {
    printf("%-8s %10s %10s %10s %8s\n",
           "Frame", "Entity", "Layout", "Total", "Ents");
    printf("--------------------------------------------------\n");
  } else {
    printf("%-8s %10s %10s %10s %10s %8s\n",
           "Frame", "Entity", "Layout", "Render", "Total", "Ents");
    printf("--------------------------------------------------------------\n");
  }

  double sum_entity = 0, sum_layout = 0, sum_render = 0, sum_total = 0;
  double max_entity = 0, max_layout = 0, max_render = 0, max_total = 0;

  int start_frame = timings.size() > 1 ? 1 : 0;

  for (size_t i = start_frame; i < timings.size(); ++i) {
    auto &ft = timings[i];
    sum_entity += ft.entity_ms;
    sum_layout += ft.layout_ms;
    sum_render += ft.render_ms;
    sum_total += ft.total_ms;
    max_entity = std::max(max_entity, ft.entity_ms);
    max_layout = std::max(max_layout, ft.layout_ms);
    max_render = std::max(max_render, ft.render_ms);
    max_total = std::max(max_total, ft.total_ms);
  }

  for (size_t i = 0; i < timings.size(); ++i) {
    if (i < 5 || i >= timings.size() - 3) {
      auto &ft = timings[i];
      if (layout_only) {
        printf("%-8zu %9.2fms %9.2fms %9.2fms %8d\n",
               i, ft.entity_ms, ft.layout_ms, ft.total_ms, ft.entity_count);
      } else {
        printf("%-8zu %9.2fms %9.2fms %9.2fms %9.2fms %8d\n",
               i, ft.entity_ms, ft.layout_ms, ft.render_ms, ft.total_ms,
               ft.entity_count);
      }
    } else if (i == 5) {
      printf("  ...    (frames %zu-%zu omitted)\n", i, timings.size() - 4);
    }
  }

  int count = static_cast<int>(timings.size()) - start_frame;
  if (count > 0) {
    printf("\n");
    if (layout_only) {
      printf("%-8s %9.2fms %9.2fms %9.2fms\n", "AVG",
             sum_entity / count, sum_layout / count, sum_total / count);
      printf("%-8s %9.2fms %9.2fms %9.2fms\n", "MAX",
             max_entity, max_layout, max_total);
    } else {
      printf("%-8s %9.2fms %9.2fms %9.2fms %9.2fms\n", "AVG",
             sum_entity / count, sum_layout / count,
             sum_render / count, sum_total / count);
      printf("%-8s %9.2fms %9.2fms %9.2fms %9.2fms\n", "MAX",
             max_entity, max_layout, max_render, max_total);
    }

    double avg_total = sum_total / count;
    double fps = avg_total > 0 ? 1000.0 / avg_total : 999.0;
    printf("\n  Estimated FPS (%s): %.1f\n",
           layout_only ? "entity+layout" : "entity+layout+render", fps);
    printf("  Budget @ 60fps: %.1fms available, %.1fms used (%.0f%%)\n",
           16.67, avg_total, (avg_total / 16.67) * 100.0);

    if (timings[0].entity_count > 0) {
      printf("  Entity count: %d\n", timings[0].entity_count);
      printf("  Layout cost per entity: %.3fms\n",
             (sum_layout / count) / timings[0].entity_count);
    }
  }

  printf("\n");
  return 0;
}
