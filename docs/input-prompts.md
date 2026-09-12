# Input prompts

Include `src/plugins/input_prompts.h`. Device activity belongs to the normal
`InputCollector`: both the standard and layered collection systems update it
while checking bindings, before alternatives are reduced to one action.
Prompts read `collector.device_activity.preferred()`; they do not poll inputs
or maintain a second input history. Activity uses the same deadzone-filtered
axis samples as gameplay. Axis identity is the physical axis and controller,
so two bindings for one action cannot hide a held axis from device tracking.

Mapped key/button presses, new stick excursions and mouse clicks select the
device. Incidental mouse motion and a held stick do not repeatedly switch
prompts. Keyboard or mouse clicks win simultaneous activity. Unmapped keyboard
or controller inputs do not affect prompts.

Pass `prompt_for(mapping, action, collector, preference)` to read the current
layer and bindings. `DevicePreference::pinned` optionally overrides presentation;
reset it to resume automatic selection. It never disables device input.
An unbound device returns `std::nullopt`. The result retains the typed binding
for artwork or glyphs.

English text is the default. `format_binding(binding, key_name)` accepts an
app-owned key-name lookup, for example to show Return instead of Enter.
`prompt_for` accepts an optional `BindingFormatter` for the entire binding.
Use this for localization so modifier names, ordering and separators can follow
the locale. The library does not impose a translation catalog or global mutable
label map. Default gamepad labels describe button positions without assuming a
controller brand. Persistence remains in a separate optional header.
