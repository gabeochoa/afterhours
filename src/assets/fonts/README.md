# Default UI font

`Roboto-Regular.ttf` is the library's default UI face: what
`afterhours::get_default_font()` returns on the raylib backend when the app
has not called `afterhours::set_default_font()`. It replaces raylib's
built-in pixel font.

- Roboto Regular, Version 2.000980; 2014, Copyright The Roboto Project
  Authors. Apache License, Version 2.0: see `LICENSE-Roboto.txt`.
  License URL recorded in the font: <http://www.apache.org/licenses/LICENSE-2.0>.
  SHA-256 of the shipped file:
  `bde8a188e37aa936b167aecc5e5a3da40262f6e51fd54c584f2cf2b6b99d96ca`.
  The file is shipped unmodified (full font, no subsetting).

## Loading

The lazy loader lives in `src/backends/raylib/font_helper.h`
(`detail::bundled_default_font()`). It runs once on the first
`get_default_font()` call and loads the shipped file from memory via the
existing `load_font_from_memory` path. Apps should init the window first
(e.g. `graphics::init`) so the atlas gets a GL context; a font loaded
without one measures by estimate until the next `get_default_font()` call
after a context exists, which reloads it once with a real atlas.

The file is located, in order:

1. The compile-time override `-DAFTERHOURS_ROBOTO_FONT_PATH="/abs/path/Roboto-Regular.ttf"`,
   in the same style as the `FONT_PATH` / `AFTERHOURS_TEST_FIXTURES`
   conventions used by the examples and tests.
2. Relative to the headers: `src/assets/fonts/Roboto-Regular.ttf` under the
   checkout that provided `font_helper.h`. This travels with a vendored
   copy of `src/`.

If the file cannot be found or rasterized, `get_default_font()` falls back
to `raylib::GetFontDefault()`, so an app that copies only the headers (or a
consumer outside the checkout) keeps working with raylib's pixel font.

## Backends

- raylib: the bundled face is the default (this directory).
- sokol: unchanged. `get_default_font()` there returns only an explicit
  `set_default_font()` override; sokol apps pick their face by loading it.
- none / psx: unchanged. Those backends have no rasterizer, so there is no
  face to swap; text measure stays estimated.

## Capture builds

For screenshot/capture builds (e.g. `examples/imm_visual_check`), build with
the usual flags (`RAYLIB_PREFIX=$HOME/homebrew BREW_PREFIX=$HOME/homebrew`)
and init the window before setting up the UI, which `ui::run` and the
examples already do. No extra flag is needed: the font file is found via
the header-relative path. To pin it explicitly, add
`-DAFTERHOURS_ROBOTO_FONT_PATH="..."` to the target's flags.
