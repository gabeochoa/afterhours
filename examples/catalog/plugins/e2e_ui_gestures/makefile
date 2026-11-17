BREW_PREFIX := $(shell brew --prefix 2>/dev/null)

EXTRA_FLAGS = -DAFTER_HOURS_USE_NONE -isystem $(BREW_PREFIX)/include
include ../../common.mk

.DEFAULT_GOAL := e2e
.PHONY: e2e
e2e: $(EXE)
	@$(EXE) --test-script scrub.e2e
