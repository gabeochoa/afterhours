EXTRA_FLAGS = -DAFTER_HOURS_USE_NONE

NO_DEFAULT_TEST = 1

include ../../common.mk

# Bounded test: only the [verify] layout checks, not the [benchmark] suite.
.PHONY: test
test: $(EXE)
	$(EXE) "[verify]"
