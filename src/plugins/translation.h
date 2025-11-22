#pragma once

#include "../core/base_component.h"
#include "../core/entity_helper.h"
#include "../core/entity_query.h"
#include "../developer.h"
#include "../logging.h"
#include <cstddef>
#include <fmt/args.h>
#include <fmt/format.h>
#include <functional>
#include <magic_enum/magic_enum.hpp>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace afterhours {
namespace translation {

enum struct Language { English, Korean, Japanese };

template <typename ParamEnum> class TranslatableString {
public:
  static constexpr int MAX_LENGTH = 100;

  explicit TranslatableString() {}
  explicit TranslatableString(const std::string &s) : content(s) {}
  explicit TranslatableString(const std::string &s, const std::string &desc)
      : content(s), description(desc) {}
  explicit TranslatableString(const char *s, const char *desc)
      : content(s), description(desc) {}
  explicit TranslatableString(const std::string &s, bool ignore_translate)
      : content(s), no_translate(ignore_translate) {}

  [[nodiscard]] bool skip_translate() const { return no_translate; }
  [[nodiscard]] bool empty() const { return content.empty(); }
  [[nodiscard]] const char *debug() const { return content.c_str(); }
  [[nodiscard]] const char *underlying_TL_ONLY() const {
    return content.c_str();
  }

  [[nodiscard]] const std::string &str() const { return content; }
  [[nodiscard]] const std::string &get_description() const {
    return description;
  }

  [[nodiscard]] const std::string &get_text() const { return content; }
  [[nodiscard]] size_t size() const { return content.size(); }
  void resize(size_t len) { content.resize(len); }

  template <typename ParamNameMap>
  auto &set_param(const ParamEnum &param, const std::string &arg,
                  const ParamNameMap &param_map) {
    if (!formatted)
      formatted = true;
    params[param] = arg;
    param_name_map = &param_map;
    return *this;
  }

  template <typename T, typename ParamNameMap>
  auto &set_param(const ParamEnum &param, const T &arg,
                  const ParamNameMap &param_map) {
    return set_param(param, fmt::format("{}", arg), param_map);
  }

  template <typename ParamNameMap>
  auto &set_param(const ParamEnum &param, const TranslatableString &arg,
                  const ParamNameMap &param_map) {
    return set_param(param, fmt::format("{}", arg.underlying_TL_ONLY()),
                     param_map);
  }

  [[nodiscard]] bool is_formatted() const { return formatted; }

  template <typename ParamNameMap>
  fmt::dynamic_format_arg_store<fmt::format_context>
  get_params(const ParamNameMap &param_map) const {
    fmt::dynamic_format_arg_store<fmt::format_context> store;
    for (const auto &kv : params) {
      if (param_map.contains(kv.first)) {
        const char *param_name = param_map.at(kv.first).c_str();
        store.push_back(fmt::arg(param_name, kv.second));
      }
    }
    return store;
  }

  template <typename ParamNameMap> operator std::string() const {
    if (skip_translate())
      return underlying_TL_ONLY();
    if (is_formatted() && param_name_map) {
      const ParamNameMap *map_ptr =
          reinterpret_cast<const ParamNameMap *>(param_name_map);
      return fmt::vformat(underlying_TL_ONLY(), get_params(*map_ptr));
    }
    return underlying_TL_ONLY();
  }

private:
  std::string content;
  std::string description;
  std::map<ParamEnum, std::string> params;
  bool formatted = false;
  bool no_translate = false;
  const void *param_name_map = nullptr;
};

template <typename StringKeyEnum, typename ParamEnum, typename FontIDEnum,
          typename FontNameGetter>
struct translation_plugin : developer::Plugin {

  using TranslatableStringType = TranslatableString<ParamEnum>;
  using TranslationMap = std::map<StringKeyEnum, TranslatableStringType>;
  using LanguageMap = std::map<Language, TranslationMap>;

  struct ProvidesTranslation : BaseComponent {
    Language current_language = Language::English;
    LanguageMap translations;
    std::map<ParamEnum, std::string> param_name_map;

    ProvidesTranslation() = default;
    ProvidesTranslation(const LanguageMap &trans, const Language &lang,
                        const std::map<ParamEnum, std::string> &param_map)
        : current_language(lang), translations(trans),
          param_name_map(param_map) {}

    const TranslationMap &get_translations_for_language(Language lang) const {
      auto it = translations.find(lang);
      if (it != translations.end()) {
        return it->second;
      }
      return translations.at(Language::English);
    }

    std::string get_string(StringKeyEnum key) const {
      const auto &trans_map = get_translations_for_language(current_language);
      auto it = trans_map.find(key);
      if (it != trans_map.end()) {
        return it->second.get_text();
      }
      log_warn("Translation not found for key: {}", magic_enum::enum_name(key));
      return "MISSING_TRANSLATION";
    }

    TranslatableStringType get_translatable_string(StringKeyEnum key) const {
      const auto &trans_map = get_translations_for_language(current_language);
      auto it = trans_map.find(key);
      if (it != trans_map.end()) {
        return TranslatableStringType(it->second.get_text(),
                                      it->second.get_description());
      }
      log_warn("Translation not found for key: {}", magic_enum::enum_name(key));
      return TranslatableStringType("MISSING_TRANSLATION", true);
    }
  };

  static void add_singleton_components(
      Entity &entity, const LanguageMap &translations,
      const Language &default_language = Language::English,
      const std::map<ParamEnum, std::string> &param_name_map = {}) {
    entity.addComponent<ProvidesTranslation>(translations, default_language,
                                             param_name_map);
    EntityHelper::registerSingleton<ProvidesTranslation>(entity);
  }

  static void enforce_singletons(SystemManager &sm) {
    sm.register_update_system(
        std::make_unique<developer::EnforceSingleton<ProvidesTranslation>>());
  }

  static void register_update_systems(SystemManager &sm) {}

  static std::string get_string(StringKeyEnum key) {
    auto *provides = EntityHelper::get_singleton_cmp<ProvidesTranslation>();
    if (!provides) {
      log_error("Translation plugin not initialized");
      return "MISSING_TRANSLATION";
    }
    return provides->get_string(key);
  }

  static TranslatableStringType get_translatable_string(StringKeyEnum key) {
    auto *provides = EntityHelper::get_singleton_cmp<ProvidesTranslation>();
    if (!provides) {
      log_error("Translation plugin not initialized");
      return TranslatableStringType("MISSING_TRANSLATION", true);
    }
    return provides->get_translatable_string(key);
  }

  static void set_language(Language language) {
    auto *provides = EntityHelper::get_singleton_cmp<ProvidesTranslation>();
    if (!provides) {
      log_error("Translation plugin not initialized");
      return;
    }
    provides->current_language = language;
    std::string lang_name = get_language_name(language);
    log_trace("Language set to: {}", lang_name.c_str());
  }

  static Language get_language() {
    auto *provides = EntityHelper::get_singleton_cmp<ProvidesTranslation>();
    if (!provides) {
      return Language::English;
    }
    return provides->current_language;
  }

  using FontIDMapper = std::function<FontIDEnum(Language)>;

  static FontIDEnum get_font_for_language(Language language,
                                          FontIDMapper mapper) {
    return mapper(language);
  }

  static std::string get_language_name(Language language) {
    return std::string(magic_enum::enum_name(language));
  }

  static std::vector<std::string> get_available_languages() {
    std::vector<std::string> languages;
    auto enum_values = magic_enum::enum_values<Language>();
    for (auto lang : enum_values) {
      languages.push_back(std::string(magic_enum::enum_name(lang)));
    }
    return languages;
  }

  static size_t get_language_index(Language language) {
    auto index = magic_enum::enum_index(language);
    return index.value_or(0);
  }

  template <typename FontManager>
  static void
  load_cjk_fonts(FontManager &font_manager, const std::string &font_file,
                 FontNameGetter get_font_name, FontIDMapper font_id_mapper) {
    auto *provides = EntityHelper::get_singleton_cmp<ProvidesTranslation>();
    if (!provides) {
      log_error("Translation plugin not initialized");
      return;
    }

    std::set<int> all_codepoints;

    for (char c = 'A'; c <= 'Z'; ++c) {
      all_codepoints.insert(static_cast<int>(c));
    }
    for (char c = 'a'; c <= 'z'; ++c) {
      all_codepoints.insert(static_cast<int>(c));
    }
    for (char c = '0'; c <= '9'; ++c) {
      all_codepoints.insert(static_cast<int>(c));
    }

    const char punctuation[] = ".,!?;:()[]{}\"'`~@#$%^&*+-=_|\\/<>";
    for (char c : punctuation) {
      all_codepoints.insert(static_cast<int>(c));
    }

    for (const auto &lang : {Language::Korean, Language::Japanese}) {
      const auto &translations = provides->get_translations_for_language(lang);

      std::string all_chars;
      for (const auto &[key, value] : translations) {
        all_chars += value.get_text();
      }

      size_t pos = 0;
      while (pos < all_chars.length()) {
        const unsigned char *bytes =
            reinterpret_cast<const unsigned char *>(all_chars.c_str() + pos);
        int codepoint = 0;
        int bytes_consumed = 0;

        if (bytes[0] < 0x80) {
          codepoint = bytes[0];
          bytes_consumed = 1;
        } else if ((bytes[0] & 0xE0) == 0xC0 && pos + 1 < all_chars.length()) {
          codepoint = ((bytes[0] & 0x1F) << 6) | (bytes[1] & 0x3F);
          bytes_consumed = 2;
        } else if ((bytes[0] & 0xF0) == 0xE0 && pos + 2 < all_chars.length()) {
          codepoint = ((bytes[0] & 0x0F) << 12) | ((bytes[1] & 0x3F) << 6) |
                      (bytes[2] & 0x3F);
          bytes_consumed = 3;
        }

        if (bytes_consumed > 0 && codepoint > 0) {
          all_codepoints.insert(codepoint);
          pos += bytes_consumed;
        } else {
          pos++;
        }
      }
    }

    if (!all_codepoints.empty()) {
      std::vector<int> codepoints(all_codepoints.begin(), all_codepoints.end());

      if (!font_file.empty() && codepoints.size() > 0) {
        FontIDEnum korean_font_id = font_id_mapper(Language::Korean);
        FontIDEnum japanese_font_id = font_id_mapper(Language::Japanese);
        std::string korean_font_name = get_font_name(korean_font_id);
        std::string japanese_font_name = get_font_name(japanese_font_id);

        font_manager.load_font_with_codepoints(
            korean_font_name, font_file.c_str(), codepoints.data(),
            static_cast<int>(codepoints.size()));

        font_manager.load_font_with_codepoints(
            japanese_font_name, font_file.c_str(), codepoints.data(),
            static_cast<int>(codepoints.size()));

        log_trace("Loaded CJK fonts with {} total codepoints for Korean and "
                  "Japanese support",
                  codepoints.size());
      } else {
        log_warn("Cannot load CJK fonts: font_file is empty or no codepoints");
      }
    }
  }
};

} // namespace translation
} // namespace afterhours
