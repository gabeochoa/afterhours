BREW_PREFIX := $(shell brew --prefix 2>/dev/null)
ifeq ($(BREW_PREFIX),)
    BREW_PREFIX := /opt/homebrew
endif

EXTRA_FLAGS = -isystem $(BREW_PREFIX)/include

include ../../common.mk
