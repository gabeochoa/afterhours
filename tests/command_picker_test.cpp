#include <afterhours/src/plugins/command_picker/picker.h>
#include <cassert>

using namespace afterhours;

int main() {
  command_picker::Picker picker{{
      {"color green", "Green accent", "Appearance", ""},
      {"reset", "Reset counter", "Counter", ""},
      {"increment", "Increase counter", "Counter", ""}}};
  assert(picker.refresh() && picker.count() == 3);
  assert(!picker.refresh());
  picker.query = "GrN";
  picker.refresh();
  assert(picker.count() == 1 && picker.entry(0).command == "color green");
  picker.query = "counter";
  picker.refresh();
  assert(picker.count() == 2);
  picker.move(false);
  assert(picker.selected() == 1);
  picker.move(true);
  assert(picker.selected() == 0);
  picker.query = "no match";
  picker.refresh();
  picker.move(true);
  assert(picker.count() == 0);

  terminal::Console commands;
  int counter = 0;
  commands.input = "keep terminal draft";
  commands.add_command({"increment", "", [&](terminal::Arguments) {
    ++counter;
    return terminal::Result{"incremented"};
  }});
  commands.add_command({"reset", "", [&](terminal::Arguments) {
    counter = 0;
    return terminal::Result{"reset"};
  }, {}, {}, "reset", [&]() -> std::optional<std::string> {
    if (!counter) return "Already zero";
    return {};
  }});
  picker.query = "reset";
  assert(!picker.activate(commands) && counter == 0);
  assert(picker.result.text == "Unavailable: Already zero");
  picker.query = "increase";
  assert(picker.activate(commands) && counter == 1);
  assert(commands.input == "keep terminal draft");
  picker.query = "reset";
  commands.execution = terminal::Execution::Queued;
  assert(picker.activate(commands) && counter == 1 && commands.pending_count() == 1);
  counter = 0;
  commands.drain();
  assert(!commands.output().back().success);
  picker.query = "increase";
  assert(picker.activate(commands) && counter == 0);
  commands.drain();
  assert(counter == 1);
  commands.remove_command("increment");
  assert(!picker.activate(commands));
  assert(picker.result.text == "Unavailable: Unknown command: increment");

  picker.set_entries({{"reset", "", "", ""}});
  picker.query.clear();
  picker.refresh();
  assert(picker.count() == 1 && picker.entry(0).label == "reset");
  picker.set_entries({});
  assert(!picker.activate(commands) && picker.count() == 0);
  picker.set_entries({{"broken \"", "Broken", "", ""}});
  assert(!picker.activate(commands) && picker.result.text == "Unclosed quote");

  std::vector<std::string> args;
  commands.add_command({"color", "", [&](terminal::Arguments values) {
    args.assign(values.begin(), values.end());
    return terminal::Result{};
  }});
  picker.set_entries({{"color 'two words'", "Preset", "", ""}});
  assert(picker.activate(commands));
  commands.drain();
  assert(args == std::vector<std::string>{"two words"});
}
