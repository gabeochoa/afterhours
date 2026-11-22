#pragma once

#include <functional>
#include <map>
#include <string>

#include "pending_command.h"

namespace afterhours {
namespace testing {
namespace state_commands {

struct StateSource {
    std::function<std::string()> get;
    std::function<void(const std::string &)> set;
};

inline std::map<std::string, StateSource> &states() {
    static std::map<std::string, StateSource> s;
    return s;
}

inline void clear_states() { states().clear(); }

inline void register_state(std::string name, std::function<std::string()> get,
                           std::function<void(const std::string &)> set) {
    states()[std::move(name)] = StateSource{std::move(get), std::move(set)};
}

inline void register_state(std::string name, std::function<std::string()> get) {
    states()[std::move(name)] = StateSource{std::move(get), nullptr};
}

inline void register_state_int(std::string name, std::function<int()> get,
                               std::function<void(int)> set) {
    states()[std::move(name)] = StateSource{
        [get = std::move(get)] { return std::to_string(get()); },
        [set = std::move(set)](const std::string &v) { set(std::stoi(v)); }};
}

inline std::string known_names() {
    std::string out;
    for (const auto &kv : states()) {
        if (!out.empty()) out += ", ";
        out += kv.first;
    }
    return out;
}

struct HandleExpectStateCommand : System<PendingE2ECommand> {
    void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
        if (cmd.is_consumed() || !cmd.is("expect_state")) return;
        if (!cmd.has_args(2)) {
            cmd.fail("expect_state requires: <name> <value>");
            return;
        }
        const std::string &name = cmd.args[0];
        auto it = states().find(name);
        if (it == states().end()) {
            cmd.fail(std::format("expect_state: unknown state '{}' (registered: {})",
                                 name, known_names()));
            return;
        }
        const std::string got = it->second.get();
        cmd.expect(got == cmd.args[1],
                   std::format("expect_state: state '{}' is '{}', expected '{}'",
                               name, got, cmd.args[1]));
    }
};

struct HandleSetStateCommand : System<PendingE2ECommand> {
    void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
        if (cmd.is_consumed() || !cmd.is("set_state")) return;
        if (!cmd.has_args(2)) {
            cmd.fail("set_state requires: <name> <value>");
            return;
        }
        const std::string &name = cmd.args[0];
        auto it = states().find(name);
        if (it == states().end()) {
            cmd.fail(std::format("set_state: unknown state '{}' (registered: {})",
                                 name, known_names()));
            return;
        }
        if (!it->second.set) {
            cmd.fail(std::format("set_state: state '{}' is read-only (no setter registered)", name));
            return;
        }
        it->second.set(cmd.args[1]);
        cmd.consume();
    }
};

inline void register_state_commands(SystemManager &sm) {
    sm.register_update_system(std::make_unique<HandleExpectStateCommand>());
    sm.register_update_system(std::make_unique<HandleSetStateCommand>());
}

}  // namespace state_commands
}  // namespace testing
}  // namespace afterhours
