RAYLIB_PREFIX := $(shell brew --prefix raylib 2>/dev/null)
BREW_PREFIX   := $(shell brew --prefix 2>/dev/null)

EXTRA_FLAGS = -DAFTER_HOURS_USE_RAYLIB -isystem $(BREW_PREFIX)/include \
              -isystem $(RAYLIB_PREFIX)/include
LDFLAGS     = $(RAYLIB_PREFIX)/lib/libraylib.a -framework OpenGL \
              -framework CoreFoundation -framework Cocoa -framework IOKit \
              -framework CoreGraphics -framework CoreVideo -lobjc

include ../../common.mk
