#include <cassert>
#include <iostream>

#include "../../../src/plugins/command_history.h"

using namespace afterhours;

// Simple game state for testing
struct GameState {
    int health = 100;
    int gold = 50;
    std::string player_name = "Hero";
};

// Custom command class for health changes
struct SetHealthCommand : public Command<GameState> {
    int old_health;
    int new_health;

    SetHealthCommand(int target_health) : old_health(0), new_health(target_health) {}

    void execute(GameState& state) override {
        old_health = state.health;
        state.health = new_health;
    }

    void undo(GameState& state) override {
        state.health = old_health;
    }

    std::string description() const override {
        return "Set health to " + std::to_string(new_health);
    }
};

// Command that can merge (for testing merge functionality)
struct AddGoldCommand : public Command<GameState> {
    int amount;

    AddGoldCommand(int amt) : amount(amt) {}

    void execute(GameState& state) override {
        state.gold += amount;
    }

    void undo(GameState& state) override {
        state.gold -= amount;
    }

    std::string description() const override {
        return "Add " + std::to_string(amount) + " gold";
    }

    bool can_merge_with(const Command<GameState>& other) const override {
        // Can merge with other AddGoldCommands
        return dynamic_cast<const AddGoldCommand*>(&other) != nullptr;
    }

    void merge_with(Command<GameState>& other) override {
        auto* other_gold = dynamic_cast<AddGoldCommand*>(&other);
        if (other_gold) {
            amount += other_gold->amount;
        }
    }
};

int main() {
    std::cout << "=== Command History Plugin Example ===" << std::endl;

    // Test 1: Create command history and game state
    std::cout << "\n1. Creating command history and state:" << std::endl;
    CommandHistory<GameState> history;
    GameState state;
    std::cout << "  - Initial: health=" << state.health << ", gold=" << state.gold << std::endl;
    assert(state.health == 100);
    assert(state.gold == 50);

    // Test 2: Execute a custom command
    std::cout << "\n2. Executing custom SetHealth command:" << std::endl;
    history.execute(std::make_unique<SetHealthCommand>(75), state);
    std::cout << "  - After SetHealth(75): health=" << state.health << std::endl;
    assert(state.health == 75);
    assert(history.can_undo());
    assert(!history.can_redo());

    // Test 3: Undo the command
    std::cout << "\n3. Undoing command:" << std::endl;
    std::cout << "  - Next undo: " << history.next_undo_description() << std::endl;
    bool undone = history.undo(state);
    std::cout << "  - After undo: health=" << state.health << std::endl;
    assert(undone);
    assert(state.health == 100);
    assert(history.can_redo());

    // Test 4: Redo the command
    std::cout << "\n4. Redoing command:" << std::endl;
    std::cout << "  - Next redo: " << history.next_redo_description() << std::endl;
    bool redone = history.redo(state);
    std::cout << "  - After redo: health=" << state.health << std::endl;
    assert(redone);
    assert(state.health == 75);

    // Test 5: Lambda commands using make_command
    std::cout << "\n5. Using lambda command (make_command):" << std::endl;
    std::string old_name = state.player_name;
    history.execute(
        make_command<GameState>(
            [](GameState& s) { s.player_name = "Champion"; },
            [old_name](GameState& s) { s.player_name = old_name; },
            "Rename player to Champion"
        ),
        state
    );
    std::cout << "  - After rename: player_name=" << state.player_name << std::endl;
    assert(state.player_name == "Champion");

    // Test 6: Undo lambda command
    std::cout << "\n6. Undoing lambda command:" << std::endl;
    history.undo(state);
    std::cout << "  - After undo: player_name=" << state.player_name << std::endl;
    assert(state.player_name == "Hero");

    // Test 7: Command merging
    std::cout << "\n7. Testing command merging:" << std::endl;
    std::cout << "  - Initial gold: " << state.gold << std::endl;

    // Execute multiple gold additions - they should merge
    history.execute(std::make_unique<AddGoldCommand>(10), state);
    std::cout << "  - After +10 gold: " << state.gold << " (undo stack: " << history.undo_count() << ")" << std::endl;

    history.execute(std::make_unique<AddGoldCommand>(5), state);
    std::cout << "  - After +5 gold (merged): " << state.gold << " (undo stack: " << history.undo_count() << ")" << std::endl;

    history.execute(std::make_unique<AddGoldCommand>(15), state);
    std::cout << "  - After +15 gold (merged): " << state.gold << " (undo stack: " << history.undo_count() << ")" << std::endl;

    assert(state.gold == 80); // 50 + 10 + 5 + 15

    // Single undo should revert all merged gold additions
    std::cout << "  - Undoing merged command..." << std::endl;
    history.undo(state);
    std::cout << "  - After single undo: gold=" << state.gold << std::endl;
    // The merged command should undo all 30 gold at once
    assert(state.gold == 50);

    // Test 8: History limits
    std::cout << "\n8. Testing history limits:" << std::endl;
    CommandHistory<GameState> limited_history(3);
    std::cout << "  - Created history with max_depth=3" << std::endl;

    for (int i = 1; i <= 5; i++) {
        limited_history.execute(std::make_unique<SetHealthCommand>(i * 10), state);
    }
    std::cout << "  - Executed 5 commands, undo stack size: " << limited_history.undo_count() << std::endl;
    assert(limited_history.undo_count() == 3);

    // Test 9: Clear history
    std::cout << "\n9. Clearing history:" << std::endl;
    std::cout << "  - Before clear: undo=" << history.undo_count() << ", redo=" << history.redo_count() << std::endl;
    history.clear();
    std::cout << "  - After clear: undo=" << history.undo_count() << ", redo=" << history.redo_count() << std::endl;
    assert(!history.can_undo());
    assert(!history.can_redo());

    // Test 10: Empty undo/redo returns false
    std::cout << "\n10. Edge cases:" << std::endl;
    CommandHistory<GameState> empty_history;
    bool result = empty_history.undo(state);
    std::cout << "  - Undo on empty history: " << (result ? "true" : "false") << std::endl;
    assert(!result);

    result = empty_history.redo(state);
    std::cout << "  - Redo on empty history: " << (result ? "true" : "false") << std::endl;
    assert(!result);

    std::cout << "\n=== All command history tests passed! ===" << std::endl;
    return 0;
}
