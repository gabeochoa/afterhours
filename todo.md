# TODO List

This document contains all TODO, FIXME, HACK, XXX, and NOTE comments found in the codebase.

## Core System TODOs

### Entity Query System (`src/entity_query.h`)
- **Line 38**: TODO i would love to just have an api like
- **Line 117**: TODO add support for converting Entities to other Entities
- **Line 218**: TODO this allocates (and deallocates) the entire list of entities
- **Line 223**: TODO Created entities are not available in queries until the next system
- **Line 320**: TODO :SPEED: if there is only one item no need to sort
- **Line 321**: TODO :SPEED: if we are doing gen_first() then partial sort?

### System Framework (`src/system.h`)
- **Line 48**: TODO I would like to support the ability to add Not<> Queries to the systesm
- **Line 158**: TODO - one issue is that if you write a system that could be const

### Entity Management (`src/entity.h`)
- **Line 234**: TODO look into switching this to using functional monadic stuff

### Entity Helper (`src/entity_helper.h`)
- **Line 31**: TODO spelling
- **Line 206**: TODO exists as a conversion for things that need shared_ptr right now

### Base Component (`src/base_component.h`)
- **Line 23**: TODO this doesnt work for some reason

### Developer Tools (`src/developer.h`)
- **Line 58**: TODO move into a dedicated file?

### Logging System (`src/logging.h`)
- **Line 4**: TODO eventually implement these
- **Line 5**: TODO move to a log.h file and include them in the other parts of the library

## Plugin TODOs

### Texture Manager (`src/plugins/texture_manager.h`)
- **Line 51**: TODO stolen from transform...
- **Line 87**: TODO add support for InnerLeft, InnerRight

### Input System (`src/plugins/input_system.h`)
- **Line 113**: TODO for now just use the xbox ones
- **Line 149**: TODO add icon for mac?
- **Line 359**: TODO figure out why this is the same as KEY_R
- **Line 376**: TODO good luck ;)
- **Line 442**: TODO consider making the deadzone configurable?
- **Line 514**: TODO replace with a singletone query
- **Line 523**: TODO i would like to move this out of input namespace

### UI System

#### Theme (`src/plugins/ui/theme.h`)
- **Line 56**: TODO find a better error color

#### Rendering (`src/plugins/ui/rendering.h`)
- **Line 36**: TODO add some caching here?
- **Line 296**: TODO do we need another color for this?
- **Line 337**: NOTE: i dont think we need this TODO
- **Line 345-346**: TODO why if we add these to the filter, this doesnt return any entities?

#### UI Context (`src/plugins/ui/context.h`)
- **Line 34**: TODO move to input system
- **Line 53**: TODO: Add styling defaults back when circular dependency is resolved
- **Line 115**: TODO How do we handle something that wants to use

#### UI Systems (`src/plugins/ui/systems.h`)
- **Line 27**: TODO this should live inside input_system
- **Line 176**: TODO i like this but for Tags, i wish

#### UI Providers (`src/plugins/ui/providers.h`)
- **Line 88**: TODO see message above

#### Immediate Mode Components (`src/plugins/ui/immediate/imm_components.h`)
- **Line 127-129**: TODO the focus ring is not correct because the actual clickable area is the checkbox_no_label element, not the checkbox element.
- **Line 152-153**: TODO - if the user wants to mess with the corners, how can we merge these
- **Line 507-509**: TODO This works great but we need a way to close the dropdown when you leave without selecting anything

#### Component Config (`src/plugins/ui/immediate/component_config.h`)
- **Line 50**: TODO should everything be inheritable?

### Auto Layout (`src/plugins/autolayout.h`)
- **Line 367**: TODO do you think this should be 5 and 5 or 10 and 10?
- **Line 995**: TODO does this need to be a setting? is this generally
- **Line 1075**: TODO idk if we should do this for all non 1.f children?
- **Line 1102**: TODO support flexing
- **Line 1119**: TODO Dont worry about absolute positioned children
- **Line 1182**: TODO we cannot enforce the assert below in the case of wrapping

## Example Code TODOs

### UI Component Example (`example/ui_component/main.cpp`)
- **Line 33**: TODO what happens if we pass void?

### Custom Queries Example (`example/custom_queries/main.cpp`)
- **Line 39**: TODO mess around with the right epsilon here

### Tests Example (`example/tests/main.cpp`)
- **Line 79**: TODO
- **Line 84**: TODO

### UI Layout Example (`example/ui_layout/main.cpp`)
- **Line 168**: TODO figure out how to update this
- **Line 805**: TODO should the child bounds go outside its parent bounds?

## Documentation TODOs

### README.md
- **Line 17**: Allows access to for_each_with_derived which will return all entities which match a component or a components children (TODO add an example)
- **Line 92**: // TODO add more

## Notes and Other Comments

### Bitwise Operations (`src/bitwise.h`)
- **Line 44**: Note: we dont use this one, cause we use the & for bool

### UI Utilities (`src/plugins/ui/utilities.h`)
- **Line 56**: NOTE: i tried to write this as a constexpr function but

### Input System Notes (`src/plugins/input_system.h`)
- **Line 438**: Note: this one is a bit more complex because we have to check if you
- **Line 441**: Note: The 0.25 is how big the deadzone is

### Entity Helper Notes (`src/entity_helper.h`)
- **Line 143**: note: I feel like we dont need to give this ability

### Auto Layout Notes (`src/plugins/autolayout.h`)
- **Line 1019**: Note, we dont early return when empty, because

---

**Total TODOs Found**: 45
**Total Notes Found**: 6
**Last Updated**: $(date)