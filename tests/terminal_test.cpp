#include <afterhours/src/plugins/terminal/console.h>
#include <cassert>
#include <limits>

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
    assert((objects.complete("object ag") == std::vector<std::string>{"object \"again\""}));
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

  Console disabled(0, 0);
  disabled.execute("help");
  assert(disabled.output().empty() && disabled.history().empty());
}
