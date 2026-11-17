#include "../../../../ah.h"
#include "../../../../src/plugins/files.h"

using namespace afterhours;

int main() {
    fs::path root;
    {
        files::TestRoots roots;
        root = roots.root;
        files::init("files_test_roots_example");

        files::write_string_atomic(files::get_save_path() / "slot1.json",
                                   R"({"level": 3})");
        files::write_string_atomic(files::get_config_path() / "settings.json",
                                   R"({"volume": 0.5})");
        files::write_string_atomic(roots.temp / "scratch.txt", "draft");

        log_info("save:   {}", files::get_save_path().string());
        log_info("config: {}", files::get_config_path().string());
        log_info("temp:   {}", roots.temp.string());
        if (files::read_string(roots.save / "slot1.json") != R"({"level": 3})")
            return 1;
    }
    log_info("after the scope: save {}, {} deleted: {}",
             files::get_save_path().string(), root.string(), !fs::exists(root));
    return fs::exists(root) ? 1 : 0;
}
