RAYLIB_PREFIX := $(shell brew --prefix raylib 2>/dev/null)
BREW_PREFIX   := $(shell brew --prefix 2>/dev/null)

EXTRA_FLAGS = -DAFTER_HOURS_USE_RAYLIB -DAFTER_HOURS_ENABLE_E2E_TESTING \
              -isystem $(BREW_PREFIX)/include -isystem $(RAYLIB_PREFIX)/include \
              -DSCRIPT_PATH='"$(abspath shortcut.e2e)"'
LDFLAGS     = $(RAYLIB_PREFIX)/lib/libraylib.a -framework OpenGL \
              -framework CoreFoundation -framework Cocoa -framework IOKit \
              -framework CoreGraphics -framework CoreVideo -framework QuartzCore -framework CoreAudio -lobjc

include ../../common.mk
