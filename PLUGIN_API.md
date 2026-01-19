# Plugin API Documentation

This document defines the public API boundary for afterhours plugins. Plugins must only use the public APIs documented here and must not access private internals.

## Plugin Concepts (C++20)

Afterhours provides C++20 concepts to enforce plugin interface requirements at compile-time. These concepts are defined in `developer.h`.

### PluginCore

The core concept that all plugins must satisfy. Requires three static methods:

```cpp
template <typename T>
concept PluginCore = requires(Entity &e, SystemManager &sm) {
  { T::add_singleton_components(e) } -> std::same_as<void>;
  { T::enforce_singletons(sm) } -> std::same_as<void>;
  { T::register_update_systems(sm) } -> std::same_as<void>;
};
```

### PluginWithRender

For plugins that also provide render systems:

```cpp
template <typename T>
concept PluginWithRender = PluginCore<T> && requires(SystemManager &sm) {
  { T::register_render_systems(sm) } -> std::same_as<void>;
};
```

### PluginTemplated

For plugins with templated registration methods (like `modal`, `toast`):

```cpp
template <typename T, typename InputAction>
concept PluginTemplated = requires(SystemManager &sm) {
  { T::template enforce_singletons<InputAction>(sm) } -> std::same_as<void>;
  { T::template register_update_systems<InputAction>(sm) } -> std::same_as<void>;
};
```

### Using Concepts

Add a static_assert at the end of your plugin to verify it satisfies the concept:

```cpp
struct my_plugin : developer::Plugin {
  static void add_singleton_components(Entity &entity);
  static void enforce_singletons(SystemManager &sm);
  static void register_update_systems(SystemManager &sm);
};

// Compile-time verification
static_assert(developer::PluginCore<my_plugin>,
              "my_plugin must implement the core plugin interface");
```

You can also use the helper variable template for cleaner assertions:

```cpp
static_assert(developer::plugin_ok<my_plugin>);
```

## Plugin Categories

Plugins in afterhours fall into different categories:

| Category | Description | Examples |
|----------|-------------|----------|
| **Full Plugins** | Implement `PluginCore` concept | `timer`, `camera`, `input`, `pathfinding` |
| **Templated Plugins** | Have templated registration methods | `modal`, `toast`, `animation`, `settings` |
| **Utility Namespaces** | Not true plugins, just helper functions | `clipboard`, `colors` |
| **Core Algorithms** | Internal implementation details | `autolayout` |

## Public Headers

Plugins may include the following headers:

- `core/base_component.h` - Base component interface
- `core/entity.h` - Entity class and component access
- `core/entity_query.h` - EntityQuery for querying entities
- `core/system.h` - System base class
- `ecs.h` - **Recommended**: Single include that provides all core ECS functionality
- `developer.h` - Plugin interface and developer utilities

**Recommended approach**: Include `ecs.h` instead of individual core headers for simplicity.

## Allowed EntityHelper Public Methods

Plugins may use the following public static methods from `EntityHelper`:

### Entity Creation
- `EntityHelper::createEntity()` - Create a temporary entity
- `EntityHelper::createPermanentEntity()` - Create a permanent entity
- `EntityHelper::createEntityWithOptions(const CreationOptions &options)` - Create entity with options

### Singleton Management
- `EntityHelper::registerSingleton<Component>(Entity &ent)` - Register a singleton component
- `EntityHelper::get_singleton<Component>()` - Get singleton entity by component type
- `EntityHelper::get_singleton_cmp<Component>()` - Get singleton component pointer

### Entity Access
- `EntityHelper::getEntityForID(const EntityID id)` - Get entity by ID (returns OptEntity)
- `EntityHelper::getEntityForIDEnforce(const EntityID id)` - Get entity by ID (throws if not found)
- `EntityHelper::getEntityAsSharedPtr(const Entity &entity)` - Get shared pointer to entity
- `EntityHelper::getEntityAsSharedPtr(const OptEntity entity)` - Get shared pointer from OptEntity

### Entity Management
- `EntityHelper::merge_entity_arrays()` - Merge temporary entities into main entity array
- `EntityHelper::cleanup()` - Remove entities marked for cleanup
- `EntityHelper::delete_all_entities(const bool include_permanent)` - Delete all entities
- `EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL()` - Delete absolutely all entities
- `EntityHelper::markIDForCleanup(const int e_id)` - Mark entity for cleanup by ID

### Iteration
- `EntityHelper::forEachEntity(const std::function<ForEachFlow(Entity &)> &cb)` - Iterate over all entities

### Internal Access (Use with Caution)
- `EntityHelper::get_temp()` - Get temporary entities vector (for advanced use cases)
- `EntityHelper::get_entities()` - Get read-only entities vector
- `EntityHelper::get_entities_for_mod()` - Get modifiable entities vector (use sparingly)
- `EntityHelper::reserve_temp_space()` - Reserve space for temporary entities

## Forbidden Private Access

Plugins must **NOT** directly access the following private members:

- `EntityHelper::singletonMap` - Use `get_singleton<Component>()` instead
- `EntityHelper::entities_DO_NOT_USE` - Use `get_entities()` or `EntityQuery` instead
- `EntityHelper::permanant_ids` - Use `createPermanentEntity()` instead
- `EntityHelper::temp_entities` - Use `get_temp()` if needed, or `createEntity()` for creation

## ECS Macros

Plugins may use the following ECS macros:

- `SINGLETON_FWD(Type)` - Forward declaration for singleton
- `SINGLETON(Type)` - Define singleton getter method
- `SINGLETON_PARAM(Type, ParamType)` - Parameterized singleton

## How to Write a Plugin

### Step 1: Define Your Plugin Structure

Create a struct that inherits from `developer::Plugin`:

```cpp
#include "../ecs.h"
#include "../developer.h"

namespace afterhours {

struct my_plugin : developer::Plugin {
  // Your plugin components and systems go here
  
  static void add_singleton_components(Entity &entity);
  static void enforce_singletons(SystemManager &sm);
  static void register_update_systems(SystemManager &sm);
};

// Verify the plugin satisfies the concept
static_assert(developer::PluginCore<my_plugin>,
              "my_plugin must implement the core plugin interface");

} // namespace afterhours
```

### Step 2: Implement add_singleton_components

This method is called during initialization to add singleton components to the manager entity:

```cpp
static void add_singleton_components(Entity &entity) {
  entity.addComponent<MySingletonComponent>();
  EntityHelper::registerSingleton<MySingletonComponent>(entity);
}
```

### Step 3: Implement enforce_singletons

This method registers systems that enforce singleton constraints:

```cpp
static void enforce_singletons(SystemManager &sm) {
  sm.register_update_system(
    std::make_unique<developer::EnforceSingleton<MySingletonComponent>>());
}
```

### Step 4: Implement register_update_systems

This method registers all update systems for your plugin:

```cpp
static void register_update_systems(SystemManager &sm) {
  sm.register_update_system(std::make_unique<MySystem>());
}
```

### Step 5: Use Public APIs Only

- Use `EntityHelper::createEntity()` to create entities
- Use `EntityHelper::get_singleton<Component>()` to access singletons
- Use `EntityQuery` to query entities
- Include `ecs.h` instead of individual core headers

## Example Plugin

See existing plugins in `src/plugins/` for reference:
- `input_system.h` - Input handling plugin
- `animation.h` - Animation plugin
- `window_manager.h` - Window management plugin
- `ui/` - UI system plugin

## Common Patterns

### Creating Entities
```cpp
Entity &entity = EntityHelper::createEntity();
entity.addComponent<MyComponent>(arg1, arg2);
```

### Querying Entities
```cpp
auto results = EntityQuery()
  .whereHasComponent<MyComponent>()
  .whereLambda([](const Entity &e) { return e.id > 100; })
  .gen();
```

### Accessing Singletons
```cpp
auto &singleton = EntityHelper::get_singleton<MySingleton>();
auto *component = EntityHelper::get_singleton_cmp<MySingleton>();
```

## Anti-Patterns to Avoid

❌ **Don't** include `core/entity_helper.h` directly - use `ecs.h` instead
❌ **Don't** access `EntityHelper::singletonMap` directly
❌ **Don't** access `EntityHelper::entities_DO_NOT_USE` directly
❌ **Don't** access `EntityHelper::permanant_ids` directly
❌ **Don't** access `EntityHelper::temp_entities` directly

✅ **Do** use `EntityHelper::get_singleton<Component>()` for singletons
✅ **Do** use `EntityQuery` for querying entities
✅ **Do** use `EntityHelper::createEntity()` for entity creation
✅ **Do** include `ecs.h` for ECS functionality
✅ **Do** add `static_assert(developer::PluginCore<YourPlugin>, ...)` to verify your plugin

## Templated Plugin Pattern

Some plugins require a template parameter (typically `InputAction`) for their registration methods. These plugins provide:

1. **Non-templated stubs** for `PluginCore` concept compatibility
2. **Templated versions** for actual functionality

Example usage:

```cpp
// For templated plugins like modal, toast, animation:

// Non-templated (satisfies PluginCore, but doesn't do anything useful)
modal::register_update_systems(sm);  // No-op

// Templated (actual functionality)
modal::register_update_systems<MyInputAction>(sm);  // Registers real systems
toast::register_layout_systems<MyInputAction>(sm);
animation::register_update_systems<MyAnimKey>(sm);
```

## Plugins Requiring External Compilation

Some plugins have implementations in `.cpp` files that must be compiled:

- `settings.cpp` - Settings persistence
- `files.cpp` - File system utilities

If you see linker errors about missing symbols from these plugins, ensure you compile the corresponding `.cpp` file.
