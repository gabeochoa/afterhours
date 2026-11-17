BREW_PREFIX := $(shell brew --prefix 2>/dev/null)

SRCS        = main.cpp ../../../../src/plugins/files.cpp
EXTRA_FLAGS = -DAFTER_HOURS_USE_NONE -I../../../.. -isystem $(BREW_PREFIX)/include

include ../../common.mk
