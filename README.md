# afterhours


an ecs framework based on the one used in https://github.com/gabeochoa/pharmasea

check the examples folder for how to use it :) 


## compiler options: 

AFTER_HOURS_MAX_COMPONENTS
- sets how big the bitset is for components, defaults to 128

AFTER_HOURS_USE_RAYLIB
- some of the plugins use raylib functions (like raylib::SetWindowSize or raylib::IsKeyPressed). Not used for main library 

AFTER_HOURS_INCLUDE_DERIVED_CHILDREN
- Allows access to for_each_with_derived which will return all entities which match a component or a components children (TODO add an example) 

AFTER_HOURS_REPLACE_LOGGING
- if you want the library to log, implement the four functions and define this

AFTER_HOURS_REPLACE_VALIDATE
- same as logging but assert + log_error

AFTER_HOURS_DEBUG
- enables some debug logging

AFTER_HOURS_INPUT_VALIDATION_<>
- for UI plugin, validates that you have mapped input actions used by the plugin
- add ASSERT or LOG_ONLY to enable validation
- or NONE (default) to disable it

AFTER_HOURS_ENTITY_ALLOC_DEBUG
- turns on log_warn whenever Entities deallocates (and theoretically allocates but unlikely) 


## Plugins

Plugins are basically just helpful things I added to the library so i can help them in all my projects. They arent needed at all and dont provide examples (yet). All plugins (and any you make i hope) should implement the functions mentioned in developer.h 


### input (requires raylib)
a way to collect user input and map it to specific actions 

Components: 
- InputCollector => where all the action data goes
- ProvidesMaxGamepadID => the total gamepads connected
- ProvidesInputMapping => Stores the mapping from Keys => Actions

Update Systems: 
- InputSystem => does all the heavy lifting

Render Systems: 
- RenderConnectedGamepads => renders the number of gamepads connected


### window_manager (desires raylib)
gives access to resolution and window related functions

Components: 
- ProvidesTargetFPS
- ProvidesCurrentResolution
- ProvidesAvailableWindowResolutions

Update Systems: 
- CollectCurrentResolution => runs when ProvidesCurrentResolution.should_refetch = true
- CollectAvailableResolutions => runs when ProvidesAvailableWindowResolutions.should_refetch = true

Render Systems: 
- :)


### ui (requires magic_enum, desires raylib)
gives access to some UI components 

Components: 
- there are a bunch, but you dont use them directly

Update Systems: 
- register_before_ui_updates() => do this before you run div() or button()
- register_after_ui_updates() => do this after all your ui is run

Render Systems: 
- register_render_systems() => does both UI and debug rendering

UI Elements:
- div
- button
- slider
- dropdown 
- checkbox
- // TODO add more 

### texture manager (desires raylib)
sprite rendering

Components: 
- HasSpritesheet 
- HasSprite
- HasAnimation

Update Systems: 
- AnimationUpdateCurrentFrame

Render Systems: 
- RenderSprites
- RenderAnimation


### animation 

- types: `afterhours::animation::EasingType`, `AnimSegment`, `AnimTrack`
- api (templated by your enum key type):
  - `animation::AnimationManager<Key>`: holds tracks
  - `animation::manager<Key>()`: singleton manager accessor
  - `animation::anim(Key key)`: fluent handle with `.from()`, `.to()`, `.sequence()`, `.hold()`, `.on_complete()`, `.on_change()`, `.on_step()`
  - `animation::one_shot(Key key, Fn)` and `animation::one_shot(Enum base, size_t index, Fn)`: run an animation once per key (or per composite key)
  - `AnimHandle::loop_sequence(segments)`: repeat a sequence forever (calls `.sequence` again on complete)
  - `animation::register_update_systems<Key>(SystemManager&)`: updates manager each frame

example:
```cpp
// define your keys (app side)
enum struct UIKey : size_t { MapShuffle };

// wire update
afterhours::animation::register_update_systems<UIKey>(systems);

// start animation
afterhours::animation::anim(UIKey::MapShuffle)
  .from(0.0f)
  .sequence({ { .to_value = 8.f, .duration = 0.45f, .easing = afterhours::animation::animation::EasingType::Linear },
              { .to_value = 5.f, .duration = 0.55f, .easing = afterhours::animation::animation::EasingType::EaseOutQuad } })
  .hold(0.5f)
  .on_step(1.0f, [](int step){ /* called when value crosses multiples of 1.0 */ })
  .on_complete([]{ /* done */ });

// read value in UI
auto v = afterhours::animation::manager<UIKey>().get_value(UIKey::MapShuffle);
```

one-shot usage:
```cpp
// runs once per key
afterhours::animation::one_shot(UIKey::IntroReveal, [](auto h){
  h.from(0.0f).to(1.0f, 0.3f, afterhours::animation::EasingType::EaseOutQuad);
});

// runs once per composite key (enum+index)
afterhours::animation::one_shot(UIKey::MapCard, i, [i](auto h){
  const float delay = 0.05f * static_cast<float>(i);
  h.from(0.0f)
   .sequence({
     { .to_value = 0.0f, .duration = delay,  .easing = afterhours::animation::EasingType::Hold },
     { .to_value = 1.0f, .duration = 0.25f, .easing = afterhours::animation::EasingType::EaseOutQuad },
   });
});
```

looping usage:
```cpp
afterhours::animation::anim(UIKey::Spinner)
  .from(0.0f)
  .loop_sequence({
    { .to_value = 1.0f, .duration = 0.5f, .easing = afterhours::animation::EasingType::Linear },
    { .to_value = 0.0f, .duration = 0.5f, .easing = afterhours::animation::EasingType::Linear },
  });
```



examples in other repos:
- https://github.com/gabeochoa/kart-afterhours/
- https://github.com/gabeochoa/tetr-afterhours/
- https://github.com/gabeochoa/wm-afterhours/
- https://github.com/gabeochoa/ui-afterhours/
- https://github.com/gabeochoa/pong-afterhours/
