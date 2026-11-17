#define AFTER_HOURS_ENABLE_E2E_TESTING
#include "ui_test_harness.h"
#include <afterhours/src/plugins/e2e_testing/runner.h>

using namespace afterhours;
using namespace afterhours::testing;

struct TypeFixture {
  EntityCollection collection;
  Entity &entity = collection.createEntity();

  TypeFixture() {
    EntityHelper::set_default_collection(&collection);
    collection.merge_entity_arrays();
    test_input::reset_all();
    test_input::detail::test_mode = true;
  }
  ~TypeFixture() {
    test_input::reset_all();
    test_input::detail::test_mode = false;
    EntityHelper::set_default_collection(nullptr);
  }
  PendingE2ECommand type(const std::string &text) {
    PendingE2ECommand command;
    command.name = "type";
    command.args = {text};
    HandleTypeCommand{}.for_each_with(entity, command, 0.f);
    return command;
  }
  std::vector<int> drain() {
    std::vector<int> out;
    while (int cp = test_input::get_char_pressed([] { return 0; }))
      out.push_back(cp);
    return out;
  }
};

TEST(multibyte_text_queues_codepoints) {
  TypeFixture f;
  auto command = f.type("caf\xC3\xA9 \xE2\x82\xAC\xF0\x9F\x99\x82!");
  CHECK(command.is_consumed());
  CHECK(command.error_message.empty());
  CHECK((f.drain() ==
         std::vector<int>{'c', 'a', 'f', 0xE9, ' ', 0x20AC, 0x1F642, '!'}));
}

TEST(invalid_utf8_fails_and_queues_nothing) {
  for (const char *bad : {"a\xA9", "a\xC3", "a\xC3(", "\xC0\xAF", "\xED\xA0\x80",
                          "\xF4\x90\x80\x80", "\xFF"}) {
    TypeFixture f;
    auto command = f.type(bad);
    CHECK(command.is_consumed());
    CHECK(!command.error_message.empty());
    CHECK(f.drain().empty());
  }
}

TEST(ascii_after_unicode_still_types) {
  TypeFixture f;
  CHECK(f.type("\xC3\xA9").error_message.empty());
  CHECK((f.drain() == std::vector<int>{0xE9}));
  CHECK(f.type("ok").error_message.empty());
  CHECK((f.drain() == std::vector<int>{'o', 'k'}));
}

int main() { return ui_test::run_registered_tests("E2E type command"); }
