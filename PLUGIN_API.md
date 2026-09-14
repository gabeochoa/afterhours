# Plugin API

Use `src/ecs.h` for ECS types and `src/developer.h` for plugin concepts. Utility
namespaces need no installation; stateful plugins expose their registration/lifetime.
Implement public methods and avoid private collection members.

| Concept | Required static methods |
|---|---|
| `PluginCore` | `add_singleton_components(Entity&)`, `enforce_singletons(SystemManager&)`, `register_update_systems(SystemManager&)`, all returning void |
| `PluginWithRender` | PluginCore plus `register_render_systems(SystemManager&)` |
| `PluginTemplated<T, InputAction>` | Templated enforce_singletons and register_update_systems |

Use `static_assert(developer::PluginCore<MyPlugin>)` or `developer::plugin_ok<MyPlugin>`.
Concepts verify signatures, not lifecycle or registration order. Templated plugins
must use their real typed registration; compatibility no-op overloads do no work.

## EntityHelper operations

### Entity Creation

- `EntityHelper::createEntity()`: Create a temporary entity
- `EntityHelper::createPermanentEntity()`: Create a permanent entity
- `EntityHelper::createEntityWithOptions(const CreationOptions &options)`: Create entity with options

### Singleton Management

- `EntityHelper::registerSingleton<Component>(Entity &ent)`: Register a singleton component
- `EntityHelper::get_singleton<Component>()`: Get singleton entity by component type
- `EntityHelper::get_singleton_cmp<Component>()`: Get singleton component pointer

### Entity Access

- `EntityHelper::getEntityForID(const EntityID id)`: Get entity by ID (returns OptEntity)
- `EntityHelper::getEntityForIDEnforce(const EntityID id)`: Get entity by ID (throws if not found)
- `EntityHelper::getEntityAsSharedPtr(const Entity &entity)`: Get shared pointer to entity
- `EntityHelper::getEntityAsSharedPtr(const OptEntity entity)`: Get shared pointer from OptEntity

### Entity Management

- `EntityHelper::merge_entity_arrays()`: Merge temporary entities into main entity array
- `EntityHelper::cleanup()`: Remove entities marked for cleanup
- `EntityHelper::delete_all_entities(const bool include_permanent)`: Delete all entities
- `EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL()`: Delete absolutely all entities
- `EntityHelper::markIDForCleanup(const int e_id)`: Mark entity for cleanup by ID

### Iteration

- `EntityHelper::forEachEntity(const std::function<ForEachFlow(Entity &)> &cb)`: Iterate over all entities

### Internal Access (Use with Caution)

- `EntityHelper::get_temp()`: Get temporary entities vector (for advanced use cases)
- `EntityHelper::get_entities()`: Get read-only entities vector
- `EntityHelper::get_entities_for_mod()`: Get modifiable entities vector (use sparingly)
- `EntityHelper::reserve_temp_space()`: Reserve space for temporary entities

Use singleton accessors instead of `singletonMap`, queries instead of
`entities_DO_NOT_USE`, `createPermanentEntity` instead of `permanent_ids`, and public
creation/access APIs instead of `temp_entities`. Macros `SINGLETON_FWD`, `SINGLETON`
and `SINGLETON_PARAM` remain available.

Construct singleton state, register it, install EnforceSingleton checks, then register
update/render systems in the application's intended order. State must outlive systems.
See `src/plugins/input_system.h`, `animation.h` and `window_manager.h` for real examples.
Compile implementation files required by selected plugins, such as `files.cpp` and
platform dialog/watcher files; headers alone do not provide those symbols.

[Optional APIs](docs/plugins.md), [UI](src/plugins/ui/README.md), and
[profiling](docs/profiling.md) document their own contracts. Proposed extraction and
registration changes are in [the roadmap](docs/roadmap.md), not this public contract.
