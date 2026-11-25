
.PHONY: count test test-unit test-examples check-boundaries

count: 
	git ls-files | grep -e "src" | grep -v "resources" | grep -v "vendor" | xargs wc -l | sort -rn | pr -2 -t -w 100

# Forward RAYLIB_PREFIX / BREW_PREFIX to sub-makes and example scripts only
# when set, so their brew auto-detection still runs when they are unset.
ifneq ($(RAYLIB_PREFIX),)
export RAYLIB_PREFIX
endif
ifneq ($(BREW_PREFIX),)
export BREW_PREFIX
endif
PREFIX_ARGS = $(if $(RAYLIB_PREFIX),RAYLIB_PREFIX="$(RAYLIB_PREFIX)") $(if $(BREW_PREFIX),BREW_PREFIX="$(BREW_PREFIX)")

check-boundaries:
	$(MAKE) -C tests check-boundaries $(PREFIX_ARGS)

test-unit:
	$(MAKE) -C tests test $(PREFIX_ARGS)

test-examples:
	@if [ -f examples/run_all_as_tests.sh ]; then \
		echo "Running examples via examples/run_all_as_tests.sh"; \
		sh examples/run_all_as_tests.sh; \
	elif [ -f examples/catalog/run.sh ]; then \
		echo "Running examples via examples/catalog/run.sh"; \
		bash examples/catalog/run.sh; \
	else \
		echo "No example test runner found (examples/run_all_as_tests.sh or examples/catalog/run.sh)" >&2; \
		exit 1; \
	fi

test: test-unit test-examples
