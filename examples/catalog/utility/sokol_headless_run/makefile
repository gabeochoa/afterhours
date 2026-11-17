BREW_PREFIX := $(shell brew --prefix 2>/dev/null)
AFH_ROOT    := $(abspath ../../../..)
AFH_INCLUDE := $(AFH_ROOT)/output/afh-include
$(shell mkdir -p $(AFH_INCLUDE) && ln -sfn $(AFH_ROOT) $(AFH_INCLUDE)/afterhours)

SRCS        = main.cpp ../../../web/sokol_impl.cc
EXTRA_FLAGS = -DAFTER_HOURS_USE_SOKOL -x objective-c++ \
              -isystem $(AFH_INCLUDE) -isystem $(BREW_PREFIX)/include
LDFLAGS     = -framework Metal -framework MetalKit -framework Cocoa \
              -framework QuartzCore -framework AudioToolbox -framework Foundation

include ../../common.mk
