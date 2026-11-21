// file_drop_test.cpp
// Injected drops (the call backends make on a real OS drop) deliver their
// paths in order when enabled, and nothing when disabled. Real OS drag
// delivery itself needs a human dragging from Finder/Explorer.

#include <afterhours/src/file_drop.h>

#include <cstdio>
#include <string>
#include <vector>

static int failures = 0;
static void check(bool ok, const char *name) {
  printf("%s: %s\n", name, ok ? "ok" : "FAIL");
  if (!ok)
    failures++;
}

int main() {
  using namespace afterhours::file_drop;

  clear();
  push_dropped_files({"/tmp/a.png", "/tmp/b.png"});
  check(take_dropped_files().empty(), "disabled drop delivers nothing");
  check(!enabled(), "disabled by default");

  set_enabled(true);
  push_dropped_files({"/tmp/a.png", "/tmp/b.png", "/tmp/c.txt"});
  push_dropped_files({"/tmp/d.png"});
  const std::vector<std::string> expected = {"/tmp/a.png", "/tmp/b.png",
                                             "/tmp/c.txt", "/tmp/d.png"};
  check(take_dropped_files() == expected, "injected drops keep path order");
  check(take_dropped_files().empty(), "take drains the queue");

  push_dropped_files({});
  check(take_dropped_files().empty(), "empty drop queues nothing");

  set_enabled(false);
  push_dropped_files({"/tmp/a.png"});
  check(take_dropped_files().empty(), "disabling drops delivery again");

  clear();
  return failures == 0 ? 0 : 1;
}
