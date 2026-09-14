# Sokol web example

`main.cpp` runs bouncing ECS entities with `graphics::run`. `sokol_impl.cc` is the
single SOKOL_IMPL/Fontstash translation unit. Build selects SOKOL_METAL on macOS or
SOKOL_GLES3 on web; AFTER_HOURS_USE_METAL selects the Sokol adapter in both cases.

From this directory, `make` builds the macOS `demo`; `make web` produces
index.html/js/wasm. Install and activate the Emscripten SDK and source its
emsdk_env.sh first. `make serve` serves http://localhost:8000/index.html;
WebAssembly needs HTTP, not file URLs.

The Makefile enables WebGL2/GLES3. Sokol drives requestAnimationFrame; do not add
another Emscripten main loop. The stock shell supplies canvas id `canvas`.
Raylib applications instead use `tools/web.mk`; see the root
[web setup](../../README.md#web-emscripten--raylib) for audio memory and fullscreen requirements.
