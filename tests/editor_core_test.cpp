#include <cstdio>
#include <string>

#include <afterhours/src/plugins/editor_core.h>

using namespace afterhours;

static int checks_run = 0, checks_passed = 0;
static void check(bool cond, const std::string &what) {
  checks_run++;
  if (cond) checks_passed++;
  else fprintf(stderr, "  FAIL: %s\n", what.c_str());
}

struct World { std::string log; };
using Ed = editor::EditorCore<World>;

static auto add(char c) {
  return make_command<World>(
      [c](World &w) { w.log += c; },
      [](World &w) { w.log.pop_back(); }, std::string("add ") + c);
}

int main() {
  {
    Ed ed; World w;
    check(!ed.dirty() && !ed.primary(), "starts clean, no primary");
    ed.select(3); ed.select(5, true);
    check(ed.is_selected(3) && ed.is_selected(5) && ed.selection_size() == 2,
          "additive select keeps both");
    check(ed.primary() == 5, "primary is last selected");
    check(!ed.dirty(), "selection does not dirty");
    ed.select(7);
    check(ed.selection_size() == 1 && ed.primary() == 7, "plain select replaces");
    ed.toggle(7);
    check(ed.selection_size() == 0 && !ed.primary(), "toggle off clears primary");
    ed.select(1); ed.select(2, true); ed.deselect(2);
    check(ed.primary() == 1, "primary falls back to a remaining id");
    ed.clear_selection();
    check(ed.selection_size() == 0, "clear_selection empties");
    (void)w;
  }
  {
    Ed ed(2); World w;
    ed.execute(add('a'), w); ed.execute(add('b'), w);
    check(w.log == "ab" && ed.dirty() && ed.undo_label() == "add b", "do a,b");
    check(ed.undo(w) && w.log == "a", "undo b first");
    check(ed.redo(w) && w.log == "ab", "redo b");
    check(ed.undo(w) && ed.undo(w) && w.log.empty(), "undo both");
    check(ed.redo(w) && w.log == "a", "redo a");
    ed.execute(add('c'), w);
    check(w.log == "ac" && !ed.can_redo(), "new edit truncates redo");
    ed.execute(add('d'), w);
    check(ed.history.undo_count() == 2, "history bounded at depth 2");
    check(ed.undo(w) && ed.undo(w) && !ed.undo(w), "only 2 undos kept");
  }
  {
    Ed ed; World w; int saves = 0;
    ed.execute(add('x'), w);
    ed.mark_saved();
    check(!ed.dirty(), "mark_saved clears dirty");
    check(ed.undo(w) && ed.dirty(), "undo past save point dirties");
    check(ed.redo(w) && !ed.dirty(), "redo back to save point is clean");
    ed.on_save([&] { saves++; return true; });
    ed.execute(add('y'), w);
    check(ed.save() && saves == 1 && !ed.dirty(), "save hook fires, dirty clears");
    ed.on_save([] { return false; });
    ed.execute(add('z'), w);
    check(!ed.save() && ed.dirty(), "vetoed save keeps dirty");
    ed.mark_dirty();
    check(ed.dirty(), "mark_dirty flags an uncommanded edit");
  }
  {
    Ed ed;
    ed.set_pick_callback([] { return std::optional<long long>(42); });
    check(ed.pick() == 42 && ed.primary() == 42, "pick selects callback hit");
    auto miss = [] { return std::optional<long long>(); };
    check(!ed.pick_with(miss) && ed.selection_size() == 0, "miss clears selection");
    ed.select(9);
    check(!ed.pick_with(miss, true) && ed.is_selected(9), "additive miss keeps selection");
  }
  printf("editor_core_test: %d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
