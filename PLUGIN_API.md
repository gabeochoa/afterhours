# Plugin API

Include `<afterhours/src/plugin.h>` for plugins, `<afterhours/src/ecs.h>` for
ECS types. `developer.h` still includes `plugin.h`, so old includes keep working.

## Write a plugin

A plugin is a config struct with `install(PluginContext &)`. No base class.
Config goes in fields, `install` wires singletons and systems, and
`PluginContext::use` runs it. Call it once at startup, in the order you want
systems to run.

```cpp
struct WeatherState : BaseComponent { float wind = 0.f; };

struct WeatherPlugin {
  float wind = 1.f;
  void install(PluginContext &ctx) const {
    ctx.ensure_singleton<WeatherState>(wind);
    ctx.add_system(Stage::Update, [w = wind](float) { /* use w */ });
  }
};

PluginContext(systems).use(WeatherPlugin{.wind = 2.f});
```

Check a plugin at compile time with
`static_assert(InstallablePlugin<WeatherPlugin>);`.

## Stage

Systems run by stage, in registration order inside a stage.

| Stage | Runs |
|---|---|
| `PreUpdate` | Update pass, first |
| `FixedUpdate` | Fixed tick only (`fixed_tick`) |
| `Update` | Update pass |
| `PostUpdate` | Update pass, last |
| `PreRender` | Render pass, first |
| `Render` | Render pass |
| `OverlayRender` | Render pass, last |

## PluginContext

| Method | What it does |
|---|---|
| `use(plugin)` | Calls `plugin.install(*this)`, returns `*this` so calls chain |
| `ensure_singleton<T>(args...)` | Returns the existing singleton component, or creates it on a permanent entity, registers it, and adds an `EnforceSingleton<T>` check. First call wins |
| `add_system(stage, system, group = {})` | Registers a `SystemBase` (or a `void(float)` callback) at a stage, optionally in a named group toggled with `SystemManager::set_group_enabled` |

Construct it as `PluginContext(systems)`, `PluginContext(systems, collection)`,
or `PluginContext(systems, host_entity)` when singletons should live on a
specific entity.

## Built-in plugins

Install any of these the same way, e.g.
`PluginContext(systems).use(TimerPlugin{}).use(ToastPlugin<Action>{});`.

`TimerPlugin`, `ToastPlugin`, `WindowManagerPlugin`, `InputPlugin`,
`LayeredInputPlugin`, `ModalPlugin`, `FilesPlugin`, `SettingsPlugin`,
`SoundPlugin`, `CameraPlugin`, `PathfindingPlugin`, `AnimationPlugin`,
`translation::TranslationPlugin`, `AppStatesPlugin`, `TriggerVolumePlugin`,
`TelemetryPlugin`, `RandomPlugin`, `CollisionPlugin`

Some are templated (`ToastPlugin<Action>`, `SettingsPlugin<Data>`,
`CollisionPlugin<Transform>`); pass the type your game already uses. Compile
the implementation files a plugin needs, such as `files.cpp` and the platform
dialog/watcher files — headers alone do not provide those symbols.

## Plugin or plain header?

A plugin owns state and systems: a singleton, something that ticks or renders,
setup that must run once. That gets an `install(PluginContext &)`.

Everything else is a plain header. Include it and call the functions:
`color` (`colors::`), `byte_cache`, `charts`, rect/math helpers, and the other
stateless utilities need no wrapper, no registration, and no singleton.

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

[Optional APIs](docs/plugins.md), [UI](src/plugins/ui/README.md), and
[profiling](docs/profiling.md) document their own contracts.

## Legacy static plugin interface

Older plugins inherit `developer::Plugin` and expose three static methods.
They still work and are unchanged; new plugins should use `install` above.

| Concept | Required static methods |
|---|---|
| `PluginCore` | `add_singleton_components(Entity&)`, `enforce_singletons(SystemManager&)`, `register_update_systems(SystemManager&)`, all returning void |
| `PluginWithRender` | PluginCore plus `register_render_systems(SystemManager&)` |
| `PluginTemplated<T, InputAction>` | Templated enforce_singletons and register_update_systems |

Use `static_assert(developer::PluginCore<MyPlugin>)` or `developer::plugin_ok<MyPlugin>`.
Concepts verify signatures, not lifecycle or registration order. See
`src/plugins/input_system.h`, `animation.h` and `window_manager.h` for examples.
