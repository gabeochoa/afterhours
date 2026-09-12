#include "ui_test_harness.h"
#include <afterhours/src/plugins/ui/text_input/component.h>
#include <afterhours/src/plugins/ui/text_input/text_area.h>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

TEST(scopes_restore_and_reset_clipboards) {
    clipboard::MemoryProvider outer;
    {
        clipboard::ScopedProvider scope(outer);
        clipboard::set_text("outer");
        CHECK(clipboard::has_text());
        auto generation = outer.generation();
        clipboard::set_text("outer");
        CHECK(outer.generation() == generation + 1);
        clipboard::MemoryProvider inner;
        try {
            clipboard::ScopedProvider nested(inner);
            CHECK(!clipboard::has_text());
            clipboard::set_text("inner");
            throw 1;
        } catch (int) {}
        CHECK(clipboard::get_text() == "outer");
        outer.reset();
        CHECK(!clipboard::has_text());
        CHECK(outer.generation() == 0);
    }
    CHECK(clipboard::detail::provider == nullptr);
}

void check_widget(bool multiline) {
    clipboard::MemoryProvider memory;
    clipboard::ScopedProvider scope(memory);
    ui_test::ImmTestHarness h;
    std::string value = multiline ? "first\nsecond" : "first";
    auto emit = [&]() -> Entity & {
        h.begin_frame();
        auto ep = mk(h.root(), 0);
        auto config = ComponentConfig{}.with_size({pixels(300), pixels(100)});
        auto result = multiline ? imm::text_area(h.context(), ep, value, config)
                                : imm::text_input(h.context(), ep, value, config);
        h.layout_only();
        return result.ent();
    };
    auto &entity = emit();
    h.context().set_focus(entity.id);
    emit();
    h.context().last_action = ui_test::TestInputAction::TextSelectAll;
    emit();
    const auto copied = value;
    h.context().last_action = ui_test::TestInputAction::TextCopy;
    emit();
    CHECK(memory.generation() == 1);
    CHECK(clipboard::get_text() == copied);
    h.context().last_action = ui_test::TestInputAction::TextCopy;
    emit();
    CHECK(memory.generation() == 2);
    clipboard::set_text(multiline ? "new\nlines" : "replacement");
    h.context().last_action = ui_test::TestInputAction::TextPaste;
    emit();
    CHECK(value == (multiline ? "new\nlines" : "replacement"));
}

TEST(single_line_copy_paste_uses_scoped_provider) { check_widget(false); }
TEST(multiline_copy_paste_uses_scoped_provider) { check_widget(true); }

int main() { return ui_test::run_registered_tests("clipboard"); }
