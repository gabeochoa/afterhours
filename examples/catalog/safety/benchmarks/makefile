# ECS performance benchmarks. -O3 so the numbers mean something.
NAME        = ecs_benchmarks
EXTRA_FLAGS = -O3 -DNDEBUG -Wmost

NO_DEFAULT_TEST = 1

include ../../common.mk

verbose: $(EXE)
	@$(EXE) -d yes

list: $(EXE)
	@$(EXE) --list-tests

.PHONY: verbose list

# Bounded smoke test: one benchmark at minimum sample counts (full run: make run).
.PHONY: test
test: $(EXE)
	$(EXE) "entity_creation" --benchmark-samples 5 --benchmark-resamples 100
