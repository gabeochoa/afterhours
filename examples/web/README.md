# afterhours web example (sokol backend)

A small graphical afterhours demo — ECS entities bouncing around the window —
that builds for the web (WebAssembly + WebGL2) using the **sokol** backend
(`AFTER_HOURS_USE_METAL`). Sokol targets WebGL natively, so the web build is a
single `emcc` command with no separate library to cross-compile.

The same source builds natively on macOS (where sokol uses Metal) for quick
local testing.

## How it's wired

- `main.cpp` — the app. Selects the sokol backend, sets up an ECS
  `MovementSystem`, and hands `init`/`frame` callbacks to
  `afterhours::graphics::run(...)`. The backend owns the window and main loop;
  on the web it drives the Emscripten `requestAnimationFrame` loop for you (no
  `emscripten_set_main_loop` needed).
- `sokol_impl.cc` — the single translation unit that compiles the sokol +
  fontstash implementations (`SOKOL_IMPL`). The GPU backend is picked by the
  build: `-DSOKOL_GLES3` for web, `-DSOKOL_METAL` (Objective-C++) for macOS.

## Native build (macOS / Metal)

```bash
make          # -> ./demo
./demo
```

## Web build (Emscripten / WebGL2)

### 1. Install the Emscripten SDK

```bash
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk && ./emsdk install latest && ./emsdk activate latest
source ./emsdk_env.sh          # puts emcc on your PATH
```

### 2. Build

From this directory (`examples/web`):

```bash
make web        # -> index.html, index.js, index.wasm
```

### 3. Run it

Browsers won't load wasm over `file://`, so serve it over HTTP:

```bash
make serve      # then open http://localhost:8000/index.html
```

## Key flags (see the `Makefile`)

- `-DAFTER_HOURS_USE_METAL` — selects the sokol backend (defined in `main.cpp`).
- `-DSOKOL_GLES3` — sokol's WebGL2/GLES3 GPU backend (required for emscripten).
- `-sUSE_WEBGL2=1 -sFULL_ES3=1` — enable WebGL2 in the emscripten runtime.
- sokol renders into the page's default `<canvas id="canvas">`, so the stock
  emcc HTML shell works out of the box.
