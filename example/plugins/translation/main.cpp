#include <cassert>
#include <iostream>

#define AFTER_HOURS_ENTITY_HELPER
#define AFTER_HOURS_SYSTEM
#include "../../../ah.h"
#include "../../../src/plugins/translation.h"

using namespace afterhours;
using namespace afterhours::translation;

// Define string keys for translation
enum struct StringKey {
    Hello,
    Goodbye,
    Welcome,
    PlayerHealth,
};

// Define parameter keys for formatted strings
enum struct ParamKey {
    Name,
    Amount,
};

// Dummy font enum (not used but required by template)
enum struct FontID { Default };

// Font name getter (not used but required by template)
std::string get_font_name(FontID) { return "Default"; }

// Create our translation plugin type
using TranslationPlugin = translation_plugin<StringKey, ParamKey, FontID, decltype(&get_font_name)>;

int main() {
    std::cout << "=== Translation Plugin Example ===" << std::endl;

    // Test 1: Set up translation data
    std::cout << "\n1. Setting up translations:" << std::endl;

    TranslationPlugin::LanguageMap translations;

    // English translations
    translations[Language::English][StringKey::Hello] =
        TranslationPlugin::TranslatableStringType("Hello!", "Greeting");
    translations[Language::English][StringKey::Goodbye] =
        TranslationPlugin::TranslatableStringType("Goodbye!", "Farewell");
    translations[Language::English][StringKey::Welcome] =
        TranslationPlugin::TranslatableStringType("Welcome, {name}!", "Welcome with name");
    translations[Language::English][StringKey::PlayerHealth] =
        TranslationPlugin::TranslatableStringType("Health: {amount}", "Player health display");

    // Korean translations
    translations[Language::Korean][StringKey::Hello] =
        TranslationPlugin::TranslatableStringType("안녕하세요!", "Greeting");
    translations[Language::Korean][StringKey::Goodbye] =
        TranslationPlugin::TranslatableStringType("안녕히 가세요!", "Farewell");
    translations[Language::Korean][StringKey::Welcome] =
        TranslationPlugin::TranslatableStringType("{name}님 환영합니다!", "Welcome with name");
    translations[Language::Korean][StringKey::PlayerHealth] =
        TranslationPlugin::TranslatableStringType("체력: {amount}", "Player health display");

    // Japanese translations
    translations[Language::Japanese][StringKey::Hello] =
        TranslationPlugin::TranslatableStringType("こんにちは!", "Greeting");
    translations[Language::Japanese][StringKey::Goodbye] =
        TranslationPlugin::TranslatableStringType("さようなら!", "Farewell");
    translations[Language::Japanese][StringKey::Welcome] =
        TranslationPlugin::TranslatableStringType("{name}さん、ようこそ!", "Welcome with name");
    translations[Language::Japanese][StringKey::PlayerHealth] =
        TranslationPlugin::TranslatableStringType("体力: {amount}", "Player health display");

    std::cout << "  - Added translations for 3 languages" << std::endl;
    std::cout << "  - English, Korean, Japanese" << std::endl;

    // Test 2: Initialize translation plugin
    std::cout << "\n2. Initializing translation plugin:" << std::endl;

    // Create singleton entity for translation
    Entity& translationEntity = EntityHelper::createEntity();

    std::map<ParamKey, std::string> param_name_map = {
        {ParamKey::Name, "name"},
        {ParamKey::Amount, "amount"},
    };

    TranslationPlugin::add_singleton_components(
        translationEntity,
        translations,
        Language::English,
        param_name_map
    );

    std::cout << "  - Translation singleton initialized" << std::endl;

    // Test 3: Get strings in English
    std::cout << "\n3. Getting strings (English):" << std::endl;
    std::string hello = TranslationPlugin::get_string(StringKey::Hello);
    std::string goodbye = TranslationPlugin::get_string(StringKey::Goodbye);
    std::cout << "  - Hello: " << hello << std::endl;
    std::cout << "  - Goodbye: " << goodbye << std::endl;
    assert(hello == "Hello!");
    assert(goodbye == "Goodbye!");

    // Test 4: Switch to Korean
    std::cout << "\n4. Switching to Korean:" << std::endl;
    TranslationPlugin::set_language(Language::Korean);
    Language current = TranslationPlugin::get_language();
    assert(current == Language::Korean);

    hello = TranslationPlugin::get_string(StringKey::Hello);
    goodbye = TranslationPlugin::get_string(StringKey::Goodbye);
    std::cout << "  - Hello: " << hello << std::endl;
    std::cout << "  - Goodbye: " << goodbye << std::endl;
    assert(hello == "안녕하세요!");

    // Test 5: Switch to Japanese
    std::cout << "\n5. Switching to Japanese:" << std::endl;
    TranslationPlugin::set_language(Language::Japanese);

    hello = TranslationPlugin::get_string(StringKey::Hello);
    goodbye = TranslationPlugin::get_string(StringKey::Goodbye);
    std::cout << "  - Hello: " << hello << std::endl;
    std::cout << "  - Goodbye: " << goodbye << std::endl;
    assert(hello == "こんにちは!");

    // Test 6: Available languages
    std::cout << "\n6. Getting available languages:" << std::endl;
    auto languages = TranslationPlugin::get_available_languages();
    std::cout << "  - Available languages: ";
    for (size_t i = 0; i < languages.size(); i++) {
        std::cout << languages[i];
        if (i < languages.size() - 1) std::cout << ", ";
    }
    std::cout << std::endl;
    assert(languages.size() == 3);

    // Test 7: Language index
    std::cout << "\n7. Language indices:" << std::endl;
    size_t eng_idx = TranslationPlugin::get_language_index(Language::English);
    size_t kor_idx = TranslationPlugin::get_language_index(Language::Korean);
    size_t jpn_idx = TranslationPlugin::get_language_index(Language::Japanese);
    std::cout << "  - English index: " << eng_idx << std::endl;
    std::cout << "  - Korean index: " << kor_idx << std::endl;
    std::cout << "  - Japanese index: " << jpn_idx << std::endl;

    // Test 8: TranslatableString features
    std::cout << "\n8. TranslatableString features:" << std::endl;
    auto ts = TranslationPlugin::TranslatableStringType("Test string", "Description");
    std::cout << "  - Content: " << ts.str() << std::endl;
    std::cout << "  - Description: " << ts.get_description() << std::endl;
    std::cout << "  - Size: " << ts.size() << std::endl;
    std::cout << "  - Empty: " << (ts.empty() ? "yes" : "no") << std::endl;
    assert(ts.str() == "Test string");
    assert(!ts.empty());

    // Test 9: Non-translatable string
    std::cout << "\n9. Non-translatable string:" << std::endl;
    auto no_translate = TranslationPlugin::TranslatableStringType("Do not translate", true);
    std::cout << "  - Skip translate: " << (no_translate.skip_translate() ? "yes" : "no") << std::endl;
    assert(no_translate.skip_translate());

    // Cleanup
    EntityHelper::cleanup();

    std::cout << "\n=== All translation tests passed! ===" << std::endl;
    return 0;
}
