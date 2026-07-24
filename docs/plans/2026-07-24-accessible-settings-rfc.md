# RFC: Accessibility-First Settings Framework for afterhours

## Goal

Make the "right thing" the easiest thing for game teams:

- If a team defines settings through afterhours metadata, the generated settings UI is accessible by default.
- If a team uses `afterhours::ui::imm` settings helpers, they automatically get consistent navigation, control hints, affordance-correct widgets, value labels, dirty/apply behavior, and accessibility hooks.
- Teams can still customize visuals/flow, but cannot accidentally skip critical safety and accessibility behaviors unless they explicitly opt out.

This RFC targets high adoption with low per-game implementation cost.

---

## Why this is needed

Most games regress on the same settings issues:

- poor structure and discoverability
- unsafe display-mode changes with no revert countdown
- silent loss/discard of settings
- inaccessible controls and missing text/audio support
- accessibility features treated as a side menu instead of core personalization

The engine should provide these as defaults, not optional best practices.

---

## Non-goals

- Not prescribing one visual art style for settings screens.
- Not forcing one exact category list if a game has unique needs.
- Not implementing full screen-reader OS integration in v1 (menu TTS abstraction ships first; platform-specific backends can follow).

---

## Product principles (engine-level)

1. Accessibility is personalization for everyone, not a separate mode.
2. Settings navigation must work with default controls from launch.
3. Risky display changes must be reversible automatically.
4. One source of truth for settings metadata drives UI, persistence, and validation.
5. Teams should get compliant behavior by calling a few APIs, not writing custom menu logic.

---

## Proposed architecture

### 1) Typed settings schema as source of truth

Introduce a settings descriptor system:

```cpp
namespace afterhours::settings {

enum class Category {
  VideoGraphics,
  Audio,
  Controls,
  Gameplay,
  InterfaceHud,
  Accessibility,
  Language,
};

enum class ApplyMode {
  LivePreview,       // apply immediately as value changes
  ApplyRequired,     // staged until user confirms Apply
  RestartRequired,   // apply on restart; clearly labeled
};

enum class WidgetKind {
  Toggle,
  SliderInt,
  SliderFloat,
  Dropdown,
  Keybind,
  ActionButton,
};

struct SettingDescriptor {
  std::string id;
  Category category;
  WidgetKind widget;
  ApplyMode apply_mode;
  bool expose_in_quick_access = false;   // top-level surfaced options
  bool accessibility_critical = false;   // appears in first-launch setup

  std::string label;
  std::string description;

  // Bounds/options (only relevant fields used per widget)
  float min_value = 0.f;
  float max_value = 1.f;
  float step = 0.01f;
  std::vector<std::string> options;

  // Optional callbacks
  std::function<void(const SettingValue&)> on_preview;
  std::function<void(const SettingValue&)> on_apply;
  std::function<bool(const SettingValue&)> validate;
};

}
```

Impact:

- single registration model for setting behavior
- engine can auto-build UI and enforce safety rules
- consistent persistence and telemetry hooks

### 2) Accessible-by-default IM settings widgets

Add `afterhours::ui::imm::settings::*` wrappers (thin, opinionated wrappers over existing `imm` controls):

- `settings_section(...)`
- `settings_toggle(...)`
- `settings_slider_int(...)`
- `settings_slider_float(...)`
- `settings_dropdown(...)`
- `settings_keybind_row(...)`
- `settings_reset_button(...)`

These wrappers automatically include:

- label + short description text
- current value rendering beside sliders (`"60"`, `"80%"`, etc.)
- proper widget affordance and focus behavior
- minimum target size and spacing defaults
- optional "changed" marker when dirty
- per-widget hint text tokens for TTS narration

Teams can still pass `ComponentConfig` overrides for style, but behavior defaults remain intact.

### 3) Standard settings shell + navigation contract

Provide a reusable screen scaffold:

- left nav categories: Video/Graphics, Audio, Controls, Gameplay, Interface/HUD, Accessibility, Language
- per-category panel
- top "Quick Access" cluster for frequently changed values:
  - brightness
  - master volume
  - look sensitivity
  - resolution/display mode
- bottom control hint bar:
  - Select / Back / Apply / Reset

Input contract (default mapping through layered input):

- same directional navigation everywhere
- same confirm/cancel everywhere
- menu confirm/cancel are independent from gameplay actions
- control hints visible at all times on settings screens

### 4) Transaction model: Live Preview + Explicit Apply

Engine `SettingsTransactionManager` behavior:

- `LivePreview` fields write preview state instantly (brightness, gamma, HUD scale, audio volumes).
- `ApplyRequired` fields are staged in a pending buffer (resolution, display mode, potentially language/font atlas changes).
- user leaving with dirty state triggers discard prompt, never silent discard.
- apply writes to runtime + disk atomically.

Core API:

```cpp
namespace afterhours::settings {

struct PendingStateSummary {
  bool has_live_preview_changes = false;
  bool has_apply_required_changes = false;
  bool has_restart_required_changes = false;
};

class TransactionManager {
public:
  void set_preview(const std::string& id, const SettingValue& value);
  void stage(const std::string& id, const SettingValue& value);
  PendingStateSummary summary() const;

  bool apply_all();     // commit + persist
  void discard_all();   // restore baseline for staged values
};

}
```

### 5) Safe display-mode apply with revert countdown

For risky display settings (`resolution`, `display_mode`, `refresh_rate`):

1. apply candidate display mode
2. show blocking confirm dialog with countdown (default 15s)
3. if confirmed, commit to disk
4. if timeout/cancel/error, auto-revert previous display mode

Engine component:

- `DisplaySafetyGuard` with built-in timer, rollback snapshot, and confirm dialog helper

This prevents players getting locked out by black screen/bad mode.

### 6) Persistence contract

Introduce centralized persistence API:

- `settings::load_from_disk()`
- `settings::save_to_disk_atomic()`

Requirements:

- atomic write (temp file + replace)
- save on successful Apply
- crash-safe: no in-memory-only final states
- per-setting schema version for migration

### 7) Accessibility features as first-class settings set

Ship a starter built-in accessibility setting pack games can enable:

Visual:

- text scale
- subtitle size
- subtitle background opacity
- colorblind mode (off/protan/deutan/tritan)
- colorblind intensity
- high contrast mode
- HUD scale
- HUD color preset
- reduce flashing
- reduce motion

Motor:

- remap all inputs (kb/mouse/gamepad)
- hold vs toggle (aim/crouch/sprint/interact)
- per-axis sensitivity
- haptics intensity + toggle
- QTE assist mode
- game speed scale

Hearing:

- independent volume sliders (master/music/sfx/voice/ambient)
- mono audio
- visualize directional audio cues

Cognitive:

- hint frequency
- objective reminder cadence
- interactive object highlight
- puzzle skip / encounter skip toggles (if game supports)
- time pressure reduction toggle

Difficulty decoupling:

- combat difficulty
- puzzle difficulty
- resource scarcity
- enemy perception
- aim assist strength

### 8) Accessibility entry points from launch

Add `AccessibilityBootstrapFlow` helper:

- optional first-launch prompt: "Set up accessibility now?"
- guaranteed default-controls path to Accessibility settings
- works from both main menu and pause menu
- supports launching directly into accessibility section

### 9) Menu text-to-speech abstraction

Define engine interface:

```cpp
namespace afterhours::accessibility {

class IMenuTTS {
public:
  virtual ~IMenuTTS() = default;
  virtual void speak(const std::string& text, bool interrupt) = 0;
  virtual void stop() = 0;
  virtual bool available() const = 0;
};

}
```

Behavior defaults:

- read focused setting label + current value + description
- read dialog prompts (apply/revert/discard)
- queue control hint summaries on screen change

Games can ship:

- no-op backend
- platform backend
- custom middleware backend

### 10) Authoring/QA guardrails

Add validation pass `validate_settings_accessibility()`:

- warns if no Accessibility category exists
- warns if critical sliders hide value label
- warns if settings menu missing from pause or main menu registration
- warns if risky display settings lack revert countdown
- warns if categories are duplicated/empty

Optional strict mode (CI failure) for first-party standards.

### 11) Pointer and mouse-only accessibility contract

Mouse-only interaction is a valid accessibility mode for many players, but must
never be the only path. Engine defaults should preserve one-handed
mouse-friendly play while guaranteeing keyboard/switch-equivalent operation.

Input abstraction requirement:

- gameplay systems consume actions (`Activate`, `Cancel`, `CycleNext`,
  `CyclePrev`, `ContextAction`) instead of raw pointer coordinates + click type
- pointer, keyboard, switch scanning, dwell-click, and voice can map into the
  same action channel

Built-in behaviors for pointer-heavy games:

- `InteractableRegistry` for all clickable hotspots with stable ids + labels
- focus/cycle mode (`Tab`/arrow) to move between hotspots without pointing
- `Enter`/`Confirm` activates currently focused hotspot
- optional snap-to-nearest-interactable cursor assist
- minimum target size and spacing defaults for interactables
- no mandatory double-click or rapid click patterns in default widgets

Drag accessibility:

- every drag interaction must expose an alternate flow:
  - click-pick + click-drop, or
  - select source then select destination
- engine helper can auto-generate drag alternatives for registered drag targets

Suggested plugin/API additions:

```cpp
namespace afterhours::accessibility {

struct PointerAssistConfig {
  bool hotspot_cycle_enabled = true;
  bool snap_to_interactable = false;
  bool sticky_targets = false;
  bool dwell_activation_enabled = false;
  float dwell_seconds = 0.8f;
  float cursor_scale = 1.0f;
  float cursor_speed_multiplier = 1.0f;
};

void register_interactable(const InteractableDescriptor& d);
void set_pointer_assist_config(const PointerAssistConfig& cfg);
void focus_next_interactable();
void focus_prev_interactable();
bool activate_focused_interactable();

}
```

Validation additions:

- warn when clickable elements are not keyboard-focusable
- warn when actions are pointer-only with no keyboard/switch route
- warn when drag has no alternate activation path
- warn when targets fail minimum size/spacing thresholds

This keeps "mouse-only" as a supported comfort mode while ensuring the game is
not "mouse-required."

### 12) Localization-first settings UI contract

Settings screens are the highest-risk UI for localization regressions because
they are dense, label-heavy, and commonly built with fixed-width controls.
afterhours should treat localization constraints as core layout requirements.

Layout and string model requirements:

- plan for at least +30% text expansion beyond English for all settings labels
  and action text
- primary setting labels must never be ellipsized; they must wrap or expand
- avoid fixed-width label boxes for setting names; allow content-driven width or
  multi-line rows
- do not compose runtime sentences via string concatenation
  (`"Set " + setting + " to " + value"`); use localized templates with named
  placeholders
- pluralization must use locale-aware message formatting (ICU-style rules or
  equivalent engine support)

RTL requirements (engineering, not just content):

- support full layout mirroring for RTL locales (Arabic/Hebrew)
- mirror directional iconography and navigation affordances (back arrows, row
  flow, panel docking)
- mirror value direction where semantically appropriate (for example slider fill
  direction and increment/decrement affordances)

Font and glyph coverage policy:

- define locale/script fallback chains (Latin, Cyrillic, CJK, Arabic, etc.)
- automatically load script-appropriate fallback fonts when selected locale
  lacks glyphs in the primary face
- expose locale-tunable typography metrics (line height, baseline offsets,
  minimum readable sizes) because CJK and Arabic often need different defaults
- disallow shipping locale bundles with missing glyph coverage in CI validation

String externalization and assets:

- all user-facing strings must be externalized (no hardcoded UI copy in C++)
- avoid text baked into textures/images for core menu affordances
- where image text is unavoidable, require per-locale variants and fallback
  logic

Locale formatting:

- all numeric display (decimals, grouping), units, and time/date formatting must
  be locale-aware
- setting value renderers (`sensitivity`, timers, percentages, resolution
  labels) should pass through a locale formatting layer, not raw `std::to_string`

Pseudolocalization workflow:

- ship a built-in pseudolocale mode for development and CI
- pseudolocale should:
  - inject accented/extended glyphs
  - expand strings (padding/stretch) to simulate translation growth
  - optionally force RTL simulation mode
- add a screenshot/layout validation pass for key settings screens under
  pseudolocale

Language picker fail-safe:

- language selection must be reachable before reading complex menu text
- language options should render self-identifying endonyms (for example
  `Español`, `Deutsch`, `日本語`) rather than only English exonyms
- keep language selection reachable from first-launch setup and settings quick
  access

Suggested engine surface:

```cpp
namespace afterhours::i18n {

struct LocaleLayoutPolicy {
  bool is_rtl = false;
  float text_expansion_budget = 1.3f;
  bool mirror_directional_ui = true;
};

void set_locale(std::string locale_id);
const LocaleLayoutPolicy& layout_policy();
std::string tr(std::string key);                 // localized lookup
std::string trf(std::string key, NamedArgs args); // templated format
std::string format_number(double value);
std::string format_percent(double value);
std::string format_unit(std::string unit_key, double value);

}
```

Validation additions:

- warn/fail if primary settings labels ellipsize in any enabled locale
- warn/fail for untranslated keys in settings categories
- warn/fail when locale glyph coverage is incomplete for active fonts
- warn/fail when RTL locale uses non-mirrored settings scaffold layout

---

## "If you use IMM, it is automatically accessible"

This is the adoption headline and should be true in practice by providing:

1. `imm::settings::*` controls that include accessible defaults.
2. `SettingsScreenScaffold` that enforces consistent nav + hints + structure.
3. `SettingsTransactionManager` that enforces apply/discard safety.
4. `DisplaySafetyGuard` that enforces rollback countdown.
5. `AccessibilityBootstrapFlow` that makes accessibility reachable from launch.

Minimum integration example:

```cpp
afterhours::settings::register_defaults(); // built-in categories + descriptors
afterhours::settings::register_game_overrides(my_descriptors);

afterhours::settings::mount_main_menu_entry();
afterhours::settings::mount_pause_menu_entry();

afterhours::settings::render_screen(ctx); // uses imm::settings wrappers
```

If teams only do this, they still get compliant baseline behavior.

---

## Rollout plan

### Phase 1: Core schema + transaction + persistence

- `SettingDescriptor`
- `TransactionManager`
- atomic settings save/load
- dirty/discard prompt flow

### Phase 2: IM wrappers + scaffold

- `imm::settings::*` wrappers
- category shell + control hint bar
- quick access surface

### Phase 3: display safety + accessibility bootstrap + TTS interface

- `DisplaySafetyGuard`
- first-launch accessibility prompt/pathing
- menu TTS interface + no-op backend + one reference backend

### Phase 4: built-in accessibility pack + validators

- built-in descriptor bundle
- validation warnings + strict mode
- migration docs and examples

---

## Adoption model for game teams

Lowest-friction path:

1. register descriptors
2. call scaffold renderer
3. wire menu entry in main + pause
4. enable display safety guard

Advanced teams can:

- override per-category layout
- add custom widgets
- plug in custom TTS backend
- extend validator rules

---

## Risks and mitigations

- **Risk:** Teams bypass scaffold and hand-roll menus.
  - **Mitigation:** Make scaffold visually good by default and easier than custom code.
- **Risk:** Too rigid for atypical games.
  - **Mitigation:** Keep descriptor + wrapper APIs composable and override-friendly.
- **Risk:** TTS support quality differs by platform.
  - **Mitigation:** standardize interface now; ship platform adapters incrementally.
- **Risk:** Existing projects have incompatible settings storage.
  - **Mitigation:** provide migration adapter to load old keys and map into descriptor ids.

---

## Success metrics

- Integration time for baseline accessible settings under 1 day for an existing afterhours game.
- >80% of settings UI created through `imm::settings::*` in adopting projects.
- Zero reports of unrecoverable display settings after mode change.
- All shipping games expose accessibility from both main and pause paths.

---

## Open questions

1. Should first-launch accessibility prompt be default-on or opt-in?
2. Which platform backend should be first for menu TTS reference implementation?
3. Should `RestartRequired` settings auto-queue a "restart recommended" banner globally?

---

## References

- [Game Accessibility Guidelines](https://gameaccessibilityguidelines.com/)
- [Xbox Accessibility Guidelines (XAGs)](https://learn.microsoft.com/en-us/gaming/accessibility/)
- [The Last of Us Part II accessibility talks and postmortems](https://www.naughtydog.com/blog/the_last_of_us_part_ii_accessibility_features_and_settings)
