# Makefile for the afterhours SOKOL web example.
#
#   make        - native build (macOS/Metal, for quick local testing) -> ./demo
#   make web    - Emscripten/WebGL2 build                    -> index.html/.js/.wasm
#   make serve  - build web, then serve it on :8000
#   make clean  - remove build artifacts
#
# Sokol targets WebGL natively, so `make web` is a single emcc invocation and
# needs nothing but the Emscripten SDK (see README).

# <afterhours/...> lives under vendor/ ; <sokol/...>,<fontstash/...> under
# vendor/afterhours/vendor/.
INCLUDES := -isystem ../../.. -isystem ../../vendor
CXXFLAGS := -std=c++20

# ---- Native build (macOS, Metal) --------------------------------------------
# The sokol implementation must be compiled as Objective-C++ for Metal.
FRAMEWORKS := -framework Metal -framework MetalKit -framework Cocoa \
    -framework QuartzCore -framework AudioToolbox -framework Foundation

demo: main.cpp sokol_impl.cc
	clang++ $(CXXFLAGS) -DSOKOL_METAL $(INCLUDES) -x objective-c++ -c sokol_impl.cc -o sokol_impl.o
	clang++ $(CXXFLAGS) -DSOKOL_METAL $(INCLUDES) -c main.cpp -o main.o
	clang++ main.o sokol_impl.o $(FRAMEWORKS) -o demo

# ---- Web build (Emscripten, WebGL2) -----------------------------------------
web: main.cpp sokol_impl.cc
	emcc $(CXXFLAGS) -DSOKOL_GLES3 $(INCLUDES) main.cpp sokol_impl.cc \
	    -sUSE_WEBGL2=1 -sFULL_ES3=1 -sALLOW_MEMORY_GROWTH=1 -o index.html

serve: web
	@echo "Serving on http://localhost:8000/index.html"
	python3 -m http.server 8000

clean:
	rm -f demo main.o sokol_impl.o index.html index.js index.wasm

.PHONY: web serve clean
