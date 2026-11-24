# afterhours gaps

Friction hit while building on afterhours in other repos (cartographer so
far). Each item is something the game hand-rolled that afterhours could
provide once. Filed by agents as they run into them, at the owner's ask
(Nov 20 2026); not commitments, just the list.

## Audio

- No audio plugin. There is no sound/music channel management, no pooling,
  no 3D placement (volume falloff + pan from a listener), no loop
  start/stop with fades, and no settings-bus routing. Cartographer
  hand-rolls all of it three times over: the birdsong pool and
  `place_call` falloff/pan in `src/bird_audio.cpp`, the settings preview
  cards in `src/journal_systems.cpp`, and per-trigger `Music` stream
  channels in `src/sound_triggers.cpp`. A small audio plugin (load and
  pool sounds/streams, per-bus volumes, `place(sound, position, listener)`,
  loop fade in/out) would delete most of that code.
- No fake/recording audio backend for the e2e plugin. Tests cannot assert
  "this sound played at this volume" without the game adding its own
  tracking state for e2e to read (`BirdAudio.songs`, `SoundTrigger.plays`,
  a `Footsteps` counter component). A recording backend that logs play
  calls (name, volume, pan, time) in test mode, plus a built-in
  `expect_sound_played` e2e command, would remove that per-game state.
  Related: "audio off in headless runs" is a game-side global
  (`AUDIO_DISABLED` in cartographer's `main.cpp`); the e2e plugin could
  own that switch.

## Gameplay primitives

- No trigger volume / zone helper. "Is the player inside this box, did
  they just enter/leave, fade something while inside" is written from
  scratch per use: cartographer has a submission zone, tutorial rings,
  pond zones and now `SoundTrigger` areas, all with their own AABB checks.
  A `Zone` component plus a system emitting enter/exit/inside state (and
  an optional smoothed 0..1 presence value) would cover all four.
- Singleton access helpers live in the game (`has_singleton<T>` /
  `get_singleton<T>` in cartographer's `cart_system.h` wrap
  `EntityHelper::get_singleton_cmp_enforce`). These two one-liners are
  needed by almost every system; they belong in afterhours core next to
  `EntityHelper`.

## E2E testing

- The `screenshot` command captures the render texture from before the
  current frame, and plain `wait` skips rendering except its last frame,
  so "teleport, aim, screenshot" can quietly save a stale frame (hit this
  shooting review screenshots for the gym sfx wing: two shots came back
  as identical empty sky). A screenshot variant that forces one fresh
  render before capturing would make review shots reliable.
