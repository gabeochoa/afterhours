#include "../../../../ah.h"
#include "../../../../src/plugins/e2e_testing/e2e_testing.h"
#include "../../../../src/plugins/e2e_testing/harness.h"

using namespace afterhours;
using namespace afterhours::testing;

struct Game {
    bool playing = false;
    int loading_frames = 0;
    int enemies = 0;
};

struct MenuCommands : System<PendingE2ECommand> {
    Game &game;
    explicit MenuCommands(Game &g) : game(g) {}
    void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
        if (cmd.is_consumed() || !cmd.is("start_game")) return;
        game.loading_frames = 10;
        cmd.consume();
    }
};

struct GameplayCommands : System<PendingE2ECommand> {
    Game &game;
    explicit GameplayCommands(Game &g) : game(g) {}
    void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
        if (cmd.is_consumed()) return;
        if (cmd.is("spawn_enemy")) {
            ++game.enemies;
            cmd.consume();
        } else if (cmd.is("expect_enemies")) {
            if (game.enemies == cmd.arg_as<int>(0))
                cmd.consume();
            else
                cmd.fail(std::format("{} enemies", game.enemies));
        }
    }
};

int main(int argc, char *argv[]) {
    const E2EArgs args = parse_e2e_args(argc, argv);
    Game game;

    SystemManager pack;
    register_builtin_handlers(pack);
    register_unknown_handler(pack, {"spawn_enemy", "expect_enemies"});
    register_cleanup(pack);

    SystemManager menu;
    menu.register_update_system(std::make_unique<MenuCommands>(game));
    register_shared_handlers(menu, pack);

    SystemManager gameplay;
    gameplay.register_update_system(std::make_unique<GameplayCommands>(game));
    register_shared_handlers(gameplay, pack);

    E2ERunner runner;
    configure_runner(runner, args);
    runner.load_script(args.script_path);
    while (!runner.is_finished()) {
        runner.tick(1.f / 60.f);
        if (game.loading_frames > 0 && --game.loading_frames == 0)
            game.playing = true;
        (game.playing ? gameplay : menu).run(1.f / 60.f);
    }
    runner.print_results();
    return runner.has_failed() ? 1 : 0;
}
