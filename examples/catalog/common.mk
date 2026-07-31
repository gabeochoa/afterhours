# Shared settings for catalog examples. Include FIRST -- OUT is used in target
# positions, which make expands immediately.
#
#   include ../../common.mk
#   all:
#           clang++ $(FLAGS) main.cpp -o $(OUT)/thing.exe && $(OUT)/thing.exe

# Derived from MAKEFILE_LIST so an example that moves category keeps working.
OUT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))../../output)

$(shell mkdir -p $(OUT))
