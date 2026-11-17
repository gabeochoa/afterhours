SRCS        = main.cpp ../../../web/sokol_impl.cc
EXTRA_FLAGS = -DAFTER_HOURS_USE_SOKOL -x objective-c++ \
              -DFONT_PATH='"$(abspath ../../../../tests/fixtures/AtkinsonHyperlegible-Regular.ttf)"'
LDFLAGS     = -framework Metal -framework MetalKit -framework Cocoa \
              -framework QuartzCore -framework AudioToolbox -framework Foundation

include ../../common.mk
