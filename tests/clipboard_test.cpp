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

TEST(masked_single_line_copies_and_cuts_by_default) {
    clipboard::MemoryProvider memory;
    clipboard::ScopedProvider scope(memory);
    clipboard::set_text("sentinel");
    CHECK(memory.generation() == 1);

    ui_test::ImmTestHarness h;
    std::string value = "secret";
    auto emit = [&]() -> Entity & {
        h.begin_frame();
        auto result = imm::text_input(
            h.context(), mk(h.root(), 0), value,
            ComponentConfig{}.with_size({pixels(300), pixels(100)}).with_mask_char('*'));
        h.layout_only();
        return result.ent();
    };

    auto &entity = emit();
    h.context().set_focus(entity.id);
    emit();
    h.context().last_action = ui_test::TestInputAction::TextSelectAll;
    emit();

    // Masking is display-only: clipboard export works unless the field
    // explicitly disables it.
    h.context().last_action = ui_test::TestInputAction::TextCopy;
    emit();
    CHECK(memory.generation() == 2);
    CHECK(clipboard::get_text() == "secret");

    h.context().last_action = ui_test::TestInputAction::TextCut;
    emit();
    CHECK(memory.generation() == 3);
    CHECK(clipboard::get_text() == "secret");
    CHECK(value.empty());
}

TEST(single_line_clipboard_operations_can_be_disabled) {
    clipboard::MemoryProvider memory;
    clipboard::ScopedProvider scope(memory);
    clipboard::set_text("sentinel");
    CHECK(memory.generation() == 1);

    ui_test::ImmTestHarness h;
    std::string value = "secret";
    bool paste_disabled = false;
    auto emit = [&]() -> Entity & {
        h.begin_frame();
        auto result = imm::text_input(
            h.context(), mk(h.root(), 0), value,
            ComponentConfig{}
                .with_size({pixels(300), pixels(100)})
                .with_mask_char('*')
                .with_copy_disabled()
                .with_cut_disabled()
                .with_paste_disabled(paste_disabled));
        h.layout_only();
        return result.ent();
    };

    auto &entity = emit();
    h.context().set_focus(entity.id);
    emit();
    h.context().last_action = ui_test::TestInputAction::TextSelectAll;
    emit();

    h.context().last_action = ui_test::TestInputAction::TextCopy;
    emit();
    CHECK(memory.generation() == 1);
    CHECK(clipboard::get_text() == "sentinel");

    h.context().last_action = ui_test::TestInputAction::TextCut;
    emit();
    CHECK(memory.generation() == 1);
    CHECK(clipboard::get_text() == "sentinel");
    CHECK(value == "secret");

    clipboard::set_text("replacement");
    CHECK(memory.generation() == 2);
    h.context().last_action = ui_test::TestInputAction::TextPaste;
    emit();
    CHECK(value == "replacement");

    paste_disabled = true;
    h.context().last_action = ui_test::TestInputAction::TextSelectAll;
    emit();
    clipboard::set_text("blocked");
    CHECK(memory.generation() == 3);
    h.context().last_action = ui_test::TestInputAction::TextPaste;
    emit();
    CHECK(value == "replacement");
}

TEST(multiline_clipboard_operations_can_be_disabled) {
    clipboard::MemoryProvider memory;
    clipboard::ScopedProvider scope(memory);
    clipboard::set_text("sentinel");
    CHECK(memory.generation() == 1);

    ui_test::ImmTestHarness h;
    std::string value = "first\nsecond";
    bool paste_disabled = false;
    auto emit = [&]() -> Entity & {
        h.begin_frame();
        auto result = imm::text_area(
            h.context(), mk(h.root(), 0), value,
            ComponentConfig{}
                .with_size({pixels(300), pixels(100)})
                .with_copy_disabled()
                .with_cut_disabled()
                .with_paste_disabled(paste_disabled));
        h.layout_only();
        return result.ent();
    };

    auto &entity = emit();
    h.context().set_focus(entity.id);
    emit();
    h.context().last_action = ui_test::TestInputAction::TextSelectAll;
    emit();

    h.context().last_action = ui_test::TestInputAction::TextCopy;
    emit();
    CHECK(memory.generation() == 1);
    CHECK(clipboard::get_text() == "sentinel");

    h.context().last_action = ui_test::TestInputAction::TextCut;
    emit();
    CHECK(memory.generation() == 1);
    CHECK(clipboard::get_text() == "sentinel");
    CHECK(value == "first\nsecond");

    clipboard::set_text("new\nlines");
    CHECK(memory.generation() == 2);
    h.context().last_action = ui_test::TestInputAction::TextPaste;
    emit();
    CHECK(value == "new\nlines");

    paste_disabled = true;
    h.context().last_action = ui_test::TestInputAction::TextSelectAll;
    emit();
    clipboard::set_text("blocked");
    CHECK(memory.generation() == 3);
    h.context().last_action = ui_test::TestInputAction::TextPaste;
    emit();
    CHECK(value == "new\nlines");
}

int main() { return ui_test::run_registered_tests("clipboard"); }
