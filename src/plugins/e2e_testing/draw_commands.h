// E2E Testing Framework - Assertions over what was DRAWN
//
// The layout tree says a widget exists and where. It does not say whether
// anything was painted, in what colour, or at all. Before this the only
// readback was a PNG, so "is the selected row's border drawn" meant
// screenshotting and profiling pixel columns, which is slow to write, breaks
// on any palette change, and silently measures the wrong element.
#pragma once

#include <format>
#include <string>

#include "../../capture.h"
#include "../../logging.h"
#include "pending_command.h"

namespace afterhours {
namespace testing {
namespace draw_commands {

// "#rrggbb" or "#rrggbbaa". Returns false on anything else, so a typo fails
// the assertion instead of silently matching black.
inline bool parse_hex_color(const std::string &s, ColorType &out) {
  if (s.size() != 7 && s.size() != 9)
    return false;
  if (s[0] != '#')
    return false;
  unsigned v[4] = {0, 0, 0, 255};
  for (size_t i = 0; i + 1 < s.size(); i += 2) {
    const std::string byte = s.substr(i + 1, 2);
    try {
      v[i / 2] = static_cast<unsigned>(std::stoul(byte, nullptr, 16));
    } catch (...) {
      return false;
    }
  }
  out = ColorType{static_cast<unsigned char>(v[0]),
                  static_cast<unsigned char>(v[1]),
                  static_cast<unsigned char>(v[2]),
                  static_cast<unsigned char>(v[3])};
  return true;
}

// What a script asked for. Everything except op is optional.
struct Filter {
  std::string op;
  bool has_color = false;
  ColorType color{};
  std::string text;
  bool has_text = false;

  bool matches(const capture::DrawnCall &c) const {
    if (!op.empty() && op != "any" && c.op != op)
      return false;
    if (has_text && c.text != text)
      return false;
    if (has_color && (c.color.r != color.r || c.color.g != color.g ||
                      c.color.b != color.b || c.color.a != color.a))
      return false;
    return true;
  }

  std::string describe() const {
    std::string d = op.empty() ? "any" : op;
    if (has_color)
      d += std::format(" color=#{:02x}{:02x}{:02x}{:02x}", color.r, color.g,
                       color.b, color.a);
    if (has_text)
      d += std::format(" text='{}'", text);
    return d;
  }
};

// Shared arg parsing: args[0] is the op, the rest are key=value.
inline bool build_filter(PendingE2ECommand &cmd, Filter &f, int &min_count) {
  f.op = cmd.args[0];
  min_count = 1;
  for (size_t i = 1; i < cmd.args.size(); i++) {
    const std::string &a = cmd.args[i];
    const size_t eq = a.find('=');
    if (eq == std::string::npos) {
      cmd.fail(std::format("malformed filter '{}', want key=value", a));
      return false;
    }
    const std::string key = a.substr(0, eq);
    const std::string val = a.substr(eq + 1);
    if (key == "color") {
      if (!parse_hex_color(val, f.color)) {
        cmd.fail(std::format("bad colour '{}', want #rrggbb or #rrggbbaa", val));
        return false;
      }
      f.has_color = true;
    } else if (key == "text") {
      f.text = val;
      f.has_text = true;
    } else if (key == "min_count") {
      min_count = std::atoi(val.c_str());
    } else {
      cmd.fail(std::format("unknown filter '{}', want color, text or min_count",
                           key));
      return false;
    }
  }
  return true;
}

// A summary of what WAS drawn, so a failure says more than "no".
inline std::string summarize(size_t limit = 12) {
  const auto &calls = capture::calls();
  std::string out = std::format("{} draws this frame", calls.size());
  size_t shown = 0;
  for (const auto &c : calls) {
    if (shown++ >= limit)
      break;
    out += std::format("\n  | {} #{:02x}{:02x}{:02x}{:02x} eid={}", c.op,
                       c.color.r, c.color.g, c.color.b, c.color.a, c.entity_id);
    if (!c.text.empty())
      out += std::format(" '{}'", c.text);
  }
  return out;
}

struct HandleExpectDrawnCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("expect_drawn")) return;
    if (!cmd.has_args(1)) {
      cmd.fail("expect_drawn requires: <op> [color=#rrggbb] [text=..] "
               "[min_count=N]");
      return;
    }
    Filter f;
    int min_count = 1;
    if (!build_filter(cmd, f, min_count))
      return;

    int found = 0;
    for (const auto &c : capture::calls())
      if (f.matches(c))
        found++;

    // Retry rather than fail: the frame carrying it may not have rendered yet.
    if (found < min_count) {
      cmd.retry();
      return;
    }
    cmd.consume();
  }
};

struct HandleExpectNotDrawnCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("expect_not_drawn")) return;
    if (!cmd.has_args(1)) {
      cmd.fail("expect_not_drawn requires: <op> [color=#rrggbb] [text=..]");
      return;
    }
    Filter f;
    int min_count = 1;
    if (!build_filter(cmd, f, min_count))
      return;

    // Absence needs a rendered frame to be absent FROM, or this passes before
    // anything has drawn, which is the way expect_no_text used to lie.
    if (capture::calls().empty()) {
      cmd.retry();
      return;
    }

    for (const auto &c : capture::calls()) {
      if (!f.matches(c)) continue;
      cmd.fail(std::format("expected NOT drawn but found {}: {}", f.describe(),
                           summarize()));
      return;
    }
    cmd.consume();
  }
};

struct HandleDumpDrawsCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("dump_draws")) return;
    const std::string name = cmd.has_args(1) ? cmd.args[0] : "draws";
    log_info("[E2E] dump_draws '{}': {}", name, summarize(10000));
    cmd.consume();
  }
};

} // namespace draw_commands
} // namespace testing
} // namespace afterhours
