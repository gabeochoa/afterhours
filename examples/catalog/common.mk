# Shared settings for catalog examples.
#
# Every example lives at examples/catalog/<category>/<name>/, so this file sits
# two levels above it and the repo root is two above that. Deriving OUT from
# MAKEFILE_LIST rather than a hardcoded ../../../.. means an example that moves
# between categories keeps working.
#
# Include this FIRST — OUT is used in target positions, which make expands
# immediately.
#
#   include ../../common.mk
#   FLAGS = ...
#   all:
#           clang++ $(FLAGS) main.cpp -o $(OUT)/thing.exe && $(OUT)/thing.exe

OUT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))../../output)

# Created at include time so no recipe has to remember to mkdir.
$(shell mkdir -p $(OUT))
