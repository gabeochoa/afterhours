// e2e_unicode_type_test.cpp
// `type` accepts UTF-8 and delivers one Unicode code point per character, the
// same contract as GetCharPressed / sokol's char event.
//
// The injected queue stored `char`, so `type "é"` arrived as two negative
// bytes; text fields dropped them (key > 0 ends the read loop) and nothing
// international could be typed in a test. Built with raylib and
// AFTER_HOURS_ENABLE_E2E_TESTING, as text_area_test is, so the injected
// queue is what input::get_char_pressed actually reads.
#include "ui_test_harness.h"

#include <afterhours/src/plugins/ui/text_input/component.h>
#include <afterhours/src/plugins/ui/text_input/text_area.h>

#include <string>
#include <vector>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using namespace afterhours::testing;
using ui_test::ImmTestHarness;

namespace {

const std::string kSample = "héllo 日本 🎉 ok";

std::vector<int> drain() {
  std::vector<int> out;
  for (int c = test_input::get_char_pressed([] { return 0; }); c > 0;
       c = test_input::get_char_pressed([] { return 0; }))
    out.push_back(c);
  return out;
}

void type(const std::string &text) {
  PendingE2ECommand cmd;
  cmd.name = "type";
  cmd.args = {text};
  Entity dummy;
  HandleTypeCommand{}.for_each_with(dummy, cmd, 0.f);
  CHECK(cmd.is_consumed() && cmd.succeeded());
}

template <typename Emit>
std::string type_into(Emit emit, const char *field_name,
                      const std::string &typed) {
  ImmTestHarness h;
  std::string value;
  h.begin_frame();
  emit(h, value);
  h.layout_only();
  if (UIComponent *f = h.find(field_name)) h.context().focus_id = f->id;
  test_input::detail::test_mode = true;
  input_injector::reset_all();
  type(typed);
  h.begin_frame();
  emit(h, value);
  h.layout_only();
  test_input::reset_all();
  test_input::detail::test_mode = false;
  return value;
}

} // namespace

TEST(push_text_decodes_utf8_to_code_points) {
  test_input::detail::test_mode = true;
  test_input::reset_all();
  test_input::push_text("aé日🎉");
  CHECK((drain() == std::vector<int>{'a', 0xE9, 0x65E5, 0x1F389}));

  // Malformed input becomes U+FFFD per bad byte rather than vanishing or
  // swallowing what follows it.
  test_input::push_text("\xFF" "x");
  CHECK((drain() == std::vector<int>{0xFFFD, 'x'}));
  test_input::push_text("\xE6\x97");  // truncated 3-byte sequence
  CHECK((drain() == std::vector<int>{0xFFFD, 0xFFFD}));
  test_input::push_text("\xC0\xAF");  // overlong '/'
  CHECK((drain() == std::vector<int>{0xFFFD, 0xFFFD}));
  test_input::push_text("\xED\xA0\x80");  // UTF-16 surrogate
  CHECK((drain() == std::vector<int>{0xFFFD, 0xFFFD, 0xFFFD}));
  test_input::detail::test_mode = false;
}

TEST(type_command_queues_code_points_not_bytes) {
  test_input::detail::test_mode = true;
  test_input::reset_all();
  type("é");
  const auto got = drain();
  CHECK(got.size() == 1);
  CHECK(!got.empty() && got[0] == 0xE9);
  test_input::detail::test_mode = false;
}

TEST(type_reaches_a_text_input_intact) {
  const std::string value = type_into(
      [](ImmTestHarness &h, std::string &v) {
        imm::text_input(h.context(), mk(h.root(), 0), v,
                        ComponentConfig{}.with_size(
                            ComponentSize{pixels(400), pixels(40)}));
      },
      "text_input_field", kSample);
  CHECK(value == kSample);
  if (value != kSample) printf("      got '%s'\n", value.c_str());
}

TEST(type_reaches_a_text_area_intact) {
  const std::string value = type_into(
      [](ImmTestHarness &h, std::string &v) {
        imm::text_area(h.context(), mk(h.root(), 0), v,
                       ComponentConfig{}.with_size(
                           ComponentSize{pixels(400), pixels(200)}));
      },
      "text_area_field", kSample);
  CHECK(value == kSample);
  if (value != kSample) printf("      got '%s'\n", value.c_str());
}

int main() { return ui_test::run_registered_tests("E2E unicode type"); }
