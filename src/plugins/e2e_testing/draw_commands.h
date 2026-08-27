// E2E Testing Framework - Assertions over what was DRAWN
//
// The layout tree says a widget exists and where. It does not say whether
// anything was painted, in what colour, or at all. Before this the only
// readback was a PNG, so "is the selected row's border drawn" meant
// screenshotting and profiling pixel columns, which is slow to write, breaks
// on any palette change, and silently measures the wrong element.
#pragma once

#include <cmath>
#include <cstdlib>
#include <format>
#include <vector>
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

// A position assertion, in whichever axes the caller named.
struct Position {
  bool has_x = false, has_y = false, has_w = false, has_h = false;
  float x = 0, y = 0, w = 0, h = 0;
  float tol = 1.f;
  int min_count = 1;

  bool any() const { return has_x || has_y || has_w || has_h; }

  bool near(float a, float b) const { return std::fabs(a - b) <= tol; }

  bool matches(const RectangleType &r) const {
    if (has_x && !near(r.x, x)) return false;
    if (has_y && !near(r.y, y)) return false;
    if (has_w && !near(r.width, w)) return false;
    if (has_h && !near(r.height, h)) return false;
    return true;
  }

  std::string describe() const {
    std::string d = "at";
    if (has_x) d += std::format(" x={:.0f}", x);
    if (has_y) d += std::format(" y={:.0f}", y);
    if (has_w) d += std::format(" w={:.0f}", w);
    if (has_h) d += std::format(" h={:.0f}", h);
    return d + std::format(" (tol {:.0f})", tol);
  }
};

// Shared arg parsing: args[0] is the op, the rest are key=value.
inline bool build_filter(PendingE2ECommand &cmd, Filter &f, Position &pos) {
  f.op = cmd.args[0];
  for (size_t i = 1; i < cmd.args.size(); i++) {
    const std::string &a = cmd.args[i];
    const size_t eq = a.find('=');
    if (eq == std::string::npos) {
      cmd.fail(std::format("malformed filter '{}', want key=value", a));
      return false;
    }
    const std::string key = a.substr(0, eq);
    std::string val = a.substr(eq + 1);
    // The runner splits on whitespace, so text="Click Me!" arrives in pieces.
    // Rejoin until the closing quote, or a label with a space is unmatchable.
    if (val.size() >= 1 && val.front() == '"') {
      val.erase(0, 1);
      while (!(val.size() && val.back() == '"') && i + 1 < cmd.args.size())
        val += " " + cmd.args[++i];
      if (val.size() && val.back() == '"')
        val.pop_back();
    }
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
      pos.min_count = std::atoi(val.c_str());
    } else if (key == "x") {
      pos.has_x = true; pos.x = std::strtof(val.c_str(), nullptr);
    } else if (key == "y") {
      pos.has_y = true; pos.y = std::strtof(val.c_str(), nullptr);
    } else if (key == "w") {
      pos.has_w = true; pos.w = std::strtof(val.c_str(), nullptr);
    } else if (key == "h") {
      pos.has_h = true; pos.h = std::strtof(val.c_str(), nullptr);
    } else if (key == "tol") {
      pos.tol = std::strtof(val.c_str(), nullptr);
    } else {
      cmd.fail(std::format(
          "unknown filter '{}', want color, text, min_count, x, y, w, h or tol",
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
    out += std::format("\n  | {} ({:.0f},{:.0f}) {:.0f}x{:.0f} "
                       "#{:02x}{:02x}{:02x}{:02x} eid={}",
                       c.op, c.rect.x, c.rect.y, c.rect.width, c.rect.height,
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
    Position pos;
    if (!build_filter(cmd, f, pos))
      return;

    int found = 0;
    for (const auto &c : capture::calls())
      if (f.matches(c))
        found++;

    // Retry rather than fail: the frame carrying it may not have rendered yet.
    if (found < pos.min_count) {
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
    Position pos;
    if (!build_filter(cmd, f, pos))
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

// Where a draw landed, and if it did not, where it actually is. hanabi bisected
// geometry back out of a PNG for want of this.
struct HandleExpectDrawnAtCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("expect_drawn_at")) return;
    if (!cmd.has_args(2)) {
      cmd.fail("expect_drawn_at requires: <op> x=N y=N [text=..] [w=N] [h=N] "
               "[tol=N]");
      return;
    }
    Filter f;
    Position want;
    if (!build_filter(cmd, f, want))
      return;
    if (!want.any()) {
      cmd.fail("expect_drawn_at needs at least one of x, y, w, h");
      return;
    }

    std::vector<const capture::DrawnCall *> candidates;
    for (const auto &c : capture::calls())
      if (f.matches(c))
        candidates.push_back(&c);

    // Nothing of that shape yet: it may not have rendered, so wait.
    if (candidates.empty()) {
      cmd.retry();
      return;
    }

    for (const auto *c : candidates)
      if (want.matches(c->rect)) {
        cmd.consume();
        return;
      }

    // It IS drawn, just not there. That is a real failure, and the useful
    // part is saying where it landed instead.
    std::string got;
    for (size_t i = 0; i < candidates.size() && i < 6; i++)
      got += std::format("\n  | at ({:.0f},{:.0f}) {:.0f}x{:.0f}",
                         candidates[i]->rect.x, candidates[i]->rect.y,
                         candidates[i]->rect.width, candidates[i]->rect.height);
    cmd.fail(std::format("{} drawn, but not {}. {} found:{}", f.describe(),
                         want.describe(), candidates.size(), got));
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
