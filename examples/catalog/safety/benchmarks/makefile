# ECS performance benchmarks. -O3 so the numbers mean something.
NAME        = ecs_benchmarks
EXTRA_FLAGS = -O3 -DNDEBUG -Wmost

include ../../common.mk

build: $(EXE)

verbose: $(EXE)
	@$(EXE) -d yes

list: $(EXE)
	@$(EXE) --list-tests

.PHONY: build verbose list
