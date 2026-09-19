#include <afterhours/src/plugins/terminal/autocomplete.h>
#include <cassert>
#include <limits>
#include <stdexcept>

using namespace afterhours::terminal;

struct ObjectCommand : CommandBase {
  Console &console;
  bool &destroyed;
  int calls = 0;

  ObjectCommand(Console &owner, bool &was_destroyed)
      : console(owner), destroyed(was_destroyed) {}
  ~ObjectCommand() override { destroyed = true; }
  std::string_view name() const override { return "object"; }
  std::string_view help() const override { return "Count calls or remove this command"; }
  std::vector<std::string> completions() const override { return {"again", "remove"}; }
  Result run(Arguments args) override {
    if (args.size() != 1) return {"Usage: object again|remove", false};
    if (args[0] == "remove") console.remove_command("object");
    assert(!destroyed);
    return {std::to_string(++calls)};
  }
};

int main() {
  {
    Console console;
    bool ready = false;
    int calls = 0;
    console.add_command({"save", "Save the document", [&](Arguments) {
      ++calls;
      ready = false;
      return Result{"saved"};
    }, {}, {}, "save [path]", [&]() -> std::optional<std::string> {
      if (!ready) return "Open a document first";
      return {};
    }});
    assert(console.command_usage("save") == "save [path]");
    assert(console.command_usage("help") == "help [command]");
    assert(console.command_usage("clear") == "clear");
    assert(console.command_usage("missing").empty());
    assert(console.command_unavailable_reason("missing"));
    assert(!console.command_unavailable_reason("help"));
    assert(console.complete("sa") == std::vector<std::string>{"save"});
    assert(console.execute("help save").text ==
        "save - Save the document\nUsage: save [path]\nUnavailable: Open a document first");
    assert(!console.execute("save").success && calls == 0);
    assert(console.output().back().text == "Unavailable: Open a document first");
    ready = true;
    assert(!console.command_unavailable_reason("save"));
    assert(console.execute("save").success && calls == 1);
    ready = true;
    console.enqueue("save");
    ready = false;
    console.drain();
    assert(calls == 1 && !console.output().back().success);
    console.enqueue("save");
    ready = true;
    console.drain();
    assert(calls == 2 && console.output().back().success);
    ready = true;
    console.enqueue("save");
    console.enqueue("save");
    console.drain();
    assert(calls == 3 && !console.output().back().success);

    struct Reset : CommandBase {
      int &value;
      explicit Reset(int &target) : value(target) {}
      std::string_view name() const override { return "reset"; }
      std::string_view help() const override { return "Reset the value"; }
      std::string_view usage() const override { return "reset"; }
      std::optional<std::string> unavailable_reason() const override {
        if (value == 0) return "Already zero";
        return {};
      }
      Result run(Arguments) override { value = 0; return {"reset"}; }
    };
    int value = 0;
    console.add_command(std::make_unique<Reset>(value));
    assert(console.command_usage("reset") == "reset");
    assert(!console.execute("reset").success);
    value = 5;
    assert(console.execute("reset").success && value == 0);
    assert(!console.execute("reset").success);
    console.add_command({"empty", "", [](Arguments) { assert(false); return Result{}; },
        {}, {}, {}, [] { return std::optional<std::string>{""}; }});
    assert(console.execute("empty").text == "Unavailable: Command unavailable");
  }

  {
    Console queued;
    queued.execution = Execution::Queued;
    std::vector<std::string> received;
    queued.add_command({"save", "", [&](Arguments args) {
      received.assign(args.begin(), args.end());
      return Result{"saved"};
    }});
    queued.input = R"(save "two words" "" last)";
    queued.submit();
    queued.input = "changed";
    assert(received.empty() && queued.pending_count() == 1);
    assert(queued.output().size() == 1 && queued.history().size() == 1);
    queued.drain();
    assert((received == std::vector<std::string>{"two words", "", "last"}));
    assert(queued.pending_count() == 0 && queued.output().back().text == "saved");
    assert(queued.output().size() == 2 && queued.history().size() == 1);
    queued.drain();
    assert(queued.output().size() == 2);
    assert(queued.execute("save immediate").text == "saved");
    assert(received.front() == "immediate" && queued.pending_count() == 0);
    assert(!queued.enqueue("   "));
    assert(!queued.enqueue("save \"unfinished"));
    assert(!queued.output().back().success && queued.pending_count() == 0);
    queued.enqueue("save removed");
    queued.remove_command("save");
    queued.drain();
    assert(!queued.output().back().success && received.front() == "immediate");
    queued.enqueue("missing");
    queued.enqueue("clear");
    queued.enqueue("help");
    queued.drain();
    assert(queued.output().size() == 2);
    assert(queued.output().front().text == "clear - Clear output");
  }
  {
    Console queued;
    std::vector<std::string> order;
    queued.add_command({"step", "", [&](Arguments args) {
      order.push_back(args[0]);
      if (args[0] == "first") {
        queued.enqueue("step later");
        queued.drain();
      }
      return Result{args[0]};
    }});
    queued.enqueue("step first");
    queued.enqueue("step second");
    queued.drain();
    assert((order == std::vector<std::string>{"first", "second"}));
    assert(queued.pending_count() == 1 && queued.output().back().text == "second");
    queued.drain();
    assert(order.back() == "later" && queued.pending_count() == 0);
    queued.add_command({"fail", "", [](Arguments) -> Result {
      throw std::runtime_error("failed");
    }});
    queued.enqueue("fail");
    queued.enqueue("step recovery");
    bool threw = false;
    try { queued.drain(); }
    catch (const std::runtime_error &) { threw = true; }
    assert(threw && queued.pending_count() == 1);
    queued.drain();
    assert(order.back() == "recovery" && queued.pending_count() == 0);
  }

  {
    std::vector<std::string> words{"-42", "1.25", "2e3", "0", "255"};
    Arguments args(words);
    assert(args.get<int>(0).value() == -42);
    assert(args.get<double>(1).value() == 1.25);
    assert(args.get<float>(2).value() == 2000.f);
    assert(args.get<unsigned>(3).value() == 0);
    assert(args.get<unsigned char>(4).value() == 255);
    assert(args.get<int>(5).error() == ArgumentError::Missing);
    assert(args.get<int>(5, 7).value() == 7);
    assert(args.get<int>(0, 7).value() == -42);
    assert(args.get<unsigned>(0).error() == ArgumentError::Invalid);
    assert(args.get<int>(1).error() == ArgumentError::Invalid);
    assert(args.get<int>(1, 7).error() == ArgumentError::Invalid);
    assert(args.subspan(1).get<double>(0).value() == 1.25);
    assert(Arguments{}.get<int>(0).error() == ArgumentError::Missing);
    assert(Arguments{}.get<double>(0, 0.5).value() == 0.5);

    for (const std::string word : {"", "12oops", " 12", "12 ", "+12", "0x10", "1,5"}) {
      words = {word};
      const Arguments invalid(words);
      assert(invalid.get<int>(0).error() == ArgumentError::Invalid);
      assert(invalid.get<double>(0).error() == ArgumentError::Invalid);
      assert(invalid.get<int>(0, 3).error() == ArgumentError::Invalid);
    }
    for (const std::string word : {"nan", "inf", "-inf", "1e9999", "1e-9999", "1e"}) {
      words = {word};
      assert(Arguments(words).get<double>(0).error() == ArgumentError::Invalid);
    }
    words = {std::to_string(std::numeric_limits<int>::min()),
             std::to_string(std::numeric_limits<int>::max()),
             std::to_string(std::numeric_limits<unsigned long long>::max()),
             "9999999999999999999999999", "256"};
    const Arguments limits(words);
    assert(limits.get<int>(0).value() == std::numeric_limits<int>::min());
    assert(limits.get<int>(1).value() == std::numeric_limits<int>::max());
    assert(limits.get<unsigned long long>(2).value() == std::numeric_limits<unsigned long long>::max());
    assert(limits.get<long long>(2).error() == ArgumentError::Invalid);
    assert(limits.get<unsigned long long>(3).error() == ArgumentError::Invalid);
    assert(limits.get<unsigned char>(4).error() == ArgumentError::Invalid);

    Console numeric;
    numeric.add_command({"number", "Read a number", [](Arguments input) {
      const auto value = input.get<double>(0);
      if (!value) return Result{value.error() == ArgumentError::Missing ? "Missing" : "Invalid", false};
      return Result{std::to_string(*value)};
    }});
    assert(numeric.execute("number -2.5").text == "-2.500000");
    assert(numeric.execute("number \"2.5\"").text == "2.500000");
    assert(numeric.execute("number").text == "Missing");
    assert(numeric.execute("number \"\"").text == "Invalid");
    assert(numeric.execute("number 2.5oops").text == "Invalid");
    assert(!numeric.execute("number nan").success);
  }

  {
    bool destroyed = false;
    bool rejected_destroyed = false;
    Console objects;
    assert(!objects.add_command(std::unique_ptr<CommandBase>{}));
    assert(objects.add_command(std::make_unique<ObjectCommand>(objects, destroyed)));
    assert(!objects.add_command(std::make_unique<ObjectCommand>(objects, rejected_destroyed)));
    assert(rejected_destroyed && !destroyed);
    assert(objects.execute("object again").text == "1");
    assert(objects.execute("object again").text == "2");
    assert(!objects.execute("object").success);
    assert(objects.execute("help object").text == "object - Count calls or remove this command");
    assert((objects.complete("object ag") == std::vector<std::string>{"object again"}));
    assert(objects.execute("object remove").text == "3");
    assert(destroyed);
    assert(!objects.execute("object again").success);
    destroyed = false;
    assert(objects.add_command(std::make_unique<ObjectCommand>(objects, destroyed)));
    assert(objects.remove_command("object"));
    assert(destroyed);
  }

  Console console(6, 3);
  int calls = 0;
  assert(console.add_command({"echo", "echo <text>", [&](Arguments args) {
    ++calls;
    return Result{args.empty() ? "empty" : args[0]};
  }, {"one", "two words"}}));
  assert(!console.add_command({"echo", "duplicate", [](Arguments) { return Result{}; }}));
  assert(!console.add_command({"help", "reserved", [](Arguments) { return Result{}; }}));
  assert(!console.add_command({"bad name", "", [](Arguments) { return Result{}; }}));
  assert(!console.add_command({"missing", "", {}}));
  assert(console.execute(" \t").success && calls == 0);
  assert(console.execute("echo \"two words\"").text == "two words");
  assert(console.execute("echo 'another word'").text == "another word");
  assert(console.execute("echo \"\"").text.empty());
  assert(calls == 3);
  assert(!console.execute("echo \"unfinished").success && calls == 3);
  assert(!console.execute("missing").success);
  assert(console.execute("help echo").text == "echo - echo <text>");
  assert(!console.execute("help missing").success);
  assert(!console.execute("help echo extra").success);
  assert(console.output().size() == 6);
  assert(console.history().size() == 3);
  assert((detail::parse(R"(echo "a\"b" C:\temp "\\server")").words ==
          std::vector<std::string>{"echo", "a\"b", "C:\\temp", "\\server"}));
  assert((console.complete("ec") == std::vector<std::string>{"echo"}));
  assert(console.complete("nothing").empty());
  assert((console.complete("echo tw") == std::vector<std::string>{"echo \"two words\""}));
  assert(console.execute(console.complete("echo tw")[0]).text == "two words");
  assert(console.add_command({"self_remove", "", [&](Arguments) {
    console.remove_command("self_remove");
    return Result{"removed"};
  }}));
  assert(console.execute("self_remove").text == "removed");
  assert(!console.execute("self_remove").success);
  console.print({"one\ntwo\nthree"});
  assert(console.output().back().text == "three");
  assert(!console.execute("clear extra").success);
  assert(console.execute("clear").success && console.output().empty());

  assert(console.command_help("echo") == "echo <text>");
  assert(console.command_help("help") == "List commands or show help");
  assert(console.command_help("clear") == "Clear output");
  assert(console.command_help("missing").empty());

  Console other;
  other.previous();
  other.next();
  assert(other.input.empty());
  other.execute("help");
  other.execute("clear");
  other.input = "draft";
  other.previous();
  assert(other.input == "clear");
  other.previous();
  other.previous();
  assert(other.input == "help");
  other.next();
  other.next();
  assert(other.input == "draft");
  other.input = "he";
  other.complete_input();
  assert(other.input == "help ");
  other.submit();
  assert(other.input.empty() && !other.output().empty());
  other.next();
  assert(other.input.empty());

  {
    Console completing;
    completing.add_command({"choose", "", [](Arguments args) { return Result{args[0]}; },
                            {"two words", "two words", "two more", "it's fine", "a\"b"}});
    assert((completing.complete("  ch") == std::vector<std::string>{"choose"}));
    assert(completing.complete("choose \"two").size() == 2);
    assert(completing.complete("choose 'two").size() == 2);
    assert(completing.complete("choose two more").empty());
    assert(completing.complete("choose 'two words' ").empty());
    assert((completing.complete("help ch") == std::vector<std::string>{"help choose"}));
    for (const auto &line : completing.complete("choose ")) assert(completing.execute(line).success);

    detail::Autocomplete suggestions;
    suggestions.refresh(completing);
    assert(suggestions.matches.empty());
    completing.input = "ch";
    suggestions.refresh(completing);
    assert(suggestions.matches.size() == 1);
    const auto output_size = completing.output().size();
    suggestions.accept(completing);
    assert(completing.input == "choose ");
    assert(completing.output().size() == output_size);
    assert(suggestions.matches.size() == 4);
    suggestions.refresh(completing);
    assert(suggestions.matches.size() == 4);
    completing.input = "choose 'two";
    suggestions.refresh(completing);
    assert(suggestions.matches.size() == 2);
    suggestions.move(false);
    assert(suggestions.selected == 1);
    suggestions.move(true);
    assert(suggestions.selected == 0);
    suggestions.dismiss(completing);
    suggestions.refresh(completing);
    assert(suggestions.matches.empty());
    suggestions.refresh(completing, true);
    assert(suggestions.matches.size() == 2);
    completing.input = "choose 'two words'";
    suggestions.refresh(completing);
    assert(suggestions.matches.empty());
    completing.input = "new";
    suggestions.refresh(completing);
    assert(suggestions.matches.empty());
    completing.add_command({"new_command", "", [](Arguments) { return Result{}; }});
    suggestions.refresh(completing);
    assert(suggestions.matches.size() == 1);
    completing.remove_command("new_command");
    suggestions.refresh(completing);
    assert(suggestions.matches.empty());
    completing.input.clear();
    suggestions.refresh(completing, true);
    assert(suggestions.matches.size() == 3);
    completing.add_command({"choose_more", "", [](Arguments) { return Result{}; }});
    completing.input = "choose";
    suggestions.refresh(completing);
    assert(suggestions.matches.empty());
    suggestions.refresh(completing, true);
    assert(suggestions.matches.size() == 2);
  }

  {
    Console live;
    std::vector<std::string> values{"two words", "loft"};
    size_t argument_index = 99;
    std::string prefix;
    std::string line;
    std::vector<std::string> arguments;
    std::vector<std::string> executed;
    int provider_calls = 0;
    live.add_command({"set", "", [&](Arguments args) {
      executed.assign(args.begin(), args.end());
      return Result{};
    }, {}, [&](const CompletionRequest &request) -> std::vector<std::string> {
      ++provider_calls;
      argument_index = request.argument_index;
      prefix = request.prefix;
      line = request.line;
      arguments.assign(request.arguments.begin(), request.arguments.end());
      if (argument_index == 0) return values;
      if (argument_index == 1 && request.arguments[0] == "two words")
        return {"on", "off", "off", "unrelated"};
      if (argument_index == 2) return {"x y", "a\"b", "C:\\temp", ""};
      return {};
    }});
    assert(live.complete("set ").size() == 2);
    assert(argument_index == 0 && prefix.empty() && arguments.empty());
    const auto second = live.complete("set 'two words' o");
    assert(second.size() == 2);
    assert(argument_index == 1 && prefix == "o" && line == "set 'two words' o");
    assert((arguments == std::vector<std::string>{"two words", "o"}));
    live.execute(second.front());
    assert((executed == std::vector<std::string>{"two words", "off"}));
    assert(live.complete("set 'two words' \"o").size() == 2);
    assert(live.complete("set 'two words'   ").size() == 3);
    assert(argument_index == 1 && prefix.empty());
    assert((arguments == std::vector<std::string>{"two words"}));
    assert(live.complete("set 'two ").size() == 1);
    assert(argument_index == 0 && prefix == "two ");
    const auto third = live.complete("set 'two words' on ");
    assert(argument_index == 2 && third.size() == 4);
    for (const auto &candidate : third) {
      live.execute(candidate);
      assert(executed.size() == 3 && executed[0] == "two words" && executed[1] == "on");
    }
    assert(live.complete("set 'two words' on \"a\\\"").size() == 1);
    assert(prefix == "a\"");
    assert(live.complete("help set extra").empty());

    detail::Autocomplete suggestions;
    live.input = "se";
    suggestions.refresh(live);
    suggestions.accept(live);
    assert(live.input == "set " && suggestions.matches.size() == 2);
    suggestions.move(true);
    suggestions.accept(live);
    assert(argument_index == 1 && suggestions.matches.size() == 3);
    suggestions.accept(live);
    assert(argument_index == 2 && suggestions.matches.size() == 4);
    suggestions.accept(live);
    assert(argument_index == 3 && suggestions.matches.empty());
    assert((detail::parse(live.input).words == std::vector<std::string>{"set", "two words", "off", ""}));
    live.input = "set l";
    provider_calls = 0;
    suggestions.refresh(live);
    assert(provider_calls == 1 && suggestions.matches.size() == 1);
    for (int i = 0; i < 120; ++i) suggestions.refresh(live);
    assert(provider_calls == 1);
    values.push_back("library");
    live.invalidate_completions();
    suggestions.refresh(live);
    assert(provider_calls == 2 && suggestions.matches.size() == 2);
    values.clear();
    suggestions.refresh(live, true);
    assert(provider_calls == 3 && suggestions.matches.empty());

    struct LiveCommand : CommandBase {
      std::vector<std::string> &values;
      explicit LiveCommand(std::vector<std::string> &source) : values(source) {}
      std::string_view name() const override { return "live"; }
      std::string_view help() const override { return "Live values"; }
      Result run(Arguments) override { return {}; }
      std::vector<std::string> complete(const CompletionRequest &request) const override {
        if (request.argument_index != 1) return {};
        return values;
      }
    };
    live.add_command(std::make_unique<LiveCommand>(values));
    assert(live.complete("live first ").empty());
    values.push_back("new value");
    assert((live.complete("live first n") == std::vector<std::string>{"live first \"new value\""}));
    live.add_command({"remove", "", [](Arguments) { return Result{}; }, {},
        [&](const CompletionRequest &) {
          live.remove_command("remove");
          return std::vector<std::string>{"done"};
        }});
    assert(live.complete("remove ").size() == 1);
    assert(live.complete("remove ").empty());
  }

  {
    Console quoting;
    std::vector<std::string> options{"amber", "-2.5", "path/to/file", "two words", "can't",
                                     "a\"b", "C:\\temp", "x\ty", "", "line\nbreak"};
    std::vector<std::string> received;
    quoting.add_command({"quote", "", [&](Arguments args) {
      assert(args.size() == 1);
      received.push_back(args[0]);
      return Result{};
    }, options});
    const auto matches = quoting.complete("quote ");
    assert(std::find(matches.begin(), matches.end(), "quote amber") != matches.end());
    assert(std::find(matches.begin(), matches.end(), "quote -2.5") != matches.end());
    assert(std::find(matches.begin(), matches.end(), "quote path/to/file") != matches.end());
    assert(std::find(matches.begin(), matches.end(), "quote \"two words\"") != matches.end());
    for (const auto &match : matches) assert(quoting.execute(match).success);
    std::sort(options.begin(), options.end());
    std::sort(received.begin(), received.end());
    assert(received == options);
  }

  Console disabled(0, 0);
  disabled.execute("help");
  assert(disabled.output().empty() && disabled.history().empty());
}
