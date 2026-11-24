RAYLIB_PREFIX := $(shell brew --prefix raylib 2>/dev/null)
BREW_PREFIX   := $(shell brew --prefix 2>/dev/null)

# Absolute, so the binary finds the model whatever directory it runs from.
EXTRA_FLAGS = -DAFTER_HOURS_USE_RAYLIB -isystem $(BREW_PREFIX)/include \
              -isystem $(RAYLIB_PREFIX)/include \
              -DMODEL_PATH='"$(abspath ../tree-large.glb)"'
LDFLAGS     = $(RAYLIB_PREFIX)/lib/libraylib.a -framework OpenGL \
              -framework CoreFoundation -framework Cocoa -framework IOKit \
              -framework CoreGraphics -framework CoreVideo -lobjc

NO_DEFAULT_TEST = 1

include ../../common.mk

.PHONY: test
test: $(EXE)
	$(EXE) --test-frames=3
