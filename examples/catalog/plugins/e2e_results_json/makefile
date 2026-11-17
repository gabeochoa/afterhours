EXTRA_FLAGS = -DAFTER_HOURS_USE_NONE
include ../../common.mk

.DEFAULT_GOAL := e2e
.PHONY: e2e
e2e: $(EXE)
	@$(EXE) --test-script-dir scripts --results-json $(OUT)/e2e_results.json
	@cat $(OUT)/e2e_results.json
