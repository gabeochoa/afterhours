#include <cassert>
#include <cstdio>
#include <string>

#include "../../../../ah.h"
#include "../../../../src/plugins/ui.h"
#include "../../../../src/plugins/ui/text_input/utils.h"

int main(int, char **) {
  using namespace afterhours;

  const std::string coder =
      "\xF0\x9F\x91\xA9\xF0\x9F\x8F\xBD\xE2\x80\x8D\xF0\x9F\x92\xBB";
  const std::string text = "hi " + coder + " Priya";

  std::printf("clusters in \"%s\":", text.c_str());
  for (size_t i = 0; i < text.size(); i = ui::next_grapheme_end(text, i))
    std::printf(" [%zu,%zu)", i, ui::next_grapheme_end(text, i));
  std::printf("\n");

  text_input::HasTextInputState field;
  field.storage.insert(0, text);
  field.cursor_position = 3 + coder.size();

  text_input::move_cursor_left(field);
  std::printf("left from after the emoji lands at byte %zu\n",
              field.cursor_position);
  assert(field.cursor_position == 3);

  text_input::move_cursor_right(field);
  text_input::delete_before_cursor(field);
  std::printf("backspace leaves \"%s\"\n", field.text().c_str());
  assert(field.text() == "hi  Priya");

  std::printf("grapheme_editing: ok\n");
  return 0;
}
