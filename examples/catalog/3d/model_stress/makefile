RAYLIB_PREFIX := $(shell brew --prefix raylib 2>/dev/null)
BREW_PREFIX   := $(shell brew --prefix 2>/dev/null)

# -O2: a stress test built at -O0 measures the debug build.
# Absolute, so the binary finds the model whatever directory it runs from.
EXTRA_FLAGS = -O2 -DAFTER_HOURS_USE_RAYLIB -isystem $(BREW_PREFIX)/include \
              -isystem $(RAYLIB_PREFIX)/include \
              -DMODEL_PATH='"$(abspath ../tree-large.glb)"'
LDFLAGS     = $(RAYLIB_PREFIX)/lib/libraylib.a -framework OpenGL \
              -framework CoreFoundation -framework Cocoa -framework IOKit \
              -framework CoreGraphics -framework CoreVideo -lobjc

include ../../common.mk
