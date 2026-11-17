#include <algorithm>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "../../../../src/plugins/translation.h"

using namespace afterhours;
using namespace afterhours::translation;

namespace {
enum class Key { Hello };
enum class Param { Count };
enum class FontID { Default };
struct FontNameGetter {
  std::string operator()(FontID) const { return "default"; }
};
using Plugin = translation_plugin<Key, Param, FontID, FontNameGetter>;
using TS = Plugin::TranslatableStringType;

struct MockFontManager {
  std::vector<int> captured;
  int calls = 0;
  void load_font_with_codepoints(const std::string &, const char *,
                                 const int *codepoints, int count) {
    calls++;
    captured.assign(codepoints, codepoints + count);
  }
  bool contains(int cp) const {
    return std::find(captured.begin(), captured.end(), cp) != captured.end();
  }
};
} // namespace

int main() {
  Plugin::LanguageMap translations;
  const std::string korean_text = std::string("A") + "\xC3\xA9" +
                                  "\xE4\xB8\xAD" + "\xF0\x9F\x98\x80" +
                                  "\xF0\xA0\x80\x80";
  translations[Language::Korean][Key::Hello] = TS(korean_text);
  translations[Language::Japanese][Key::Hello] =
      TS(std::string("\xF0\x9F\x8E\x89"));

  Entity &entity = EntityHelper::createEntity();
  const std::map<Param, std::string> param_map{{Param::Count, "count"}};
  Plugin::add_singleton_components(entity, translations, Language::English,
                                   param_map);

  MockFontManager fonts;
  Plugin::load_cjk_fonts(fonts, std::string("font.ttf"), FontNameGetter{},
                         [](Language) { return FontID::Default; });

  const bool ok = fonts.calls == 2 && fonts.contains(0x41) &&
                  fonts.contains(0xE9) && fonts.contains(0x4E2D) &&
                  fonts.contains(0x1F600) && fonts.contains(0x20000) &&
                  fonts.contains(0x1F389);
  std::printf("translation_4byte: calls=%d codepoints=%zu emoji=%s "
              "cjk_ext_b=%s\n",
              fonts.calls, fonts.captured.size(),
              fonts.contains(0x1F600) ? "yes" : "no",
              fonts.contains(0x20000) ? "yes" : "no");
  EntityHelper::cleanup();
  return ok ? 0 : 1;
}
