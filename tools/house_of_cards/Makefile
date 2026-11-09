# house_of_cards: bake impostor cards from a model, and score them against it.
#
#   make           build bake and compare into output/
#   make check     bake every example model and compare; strips, heatmaps and
#                  bakes land in output/house_of_cards/

CXX := clang++
RAYLIB_PREFIX := $(shell brew --prefix raylib 2>/dev/null)
BREW_PREFIX   := $(shell brew --prefix 2>/dev/null)
ROOT := $(realpath ../..)
OUT  := $(ROOT)/output
# <afterhours/...> through a symlink to this checkout, included as a system
# header (as examples/Makefile and tests/Makefile do), so library headers'
# own warnings stay out of the tools' build.
INCLUDE_ROOT := $(CURDIR)/.afh-include
$(shell mkdir -p $(INCLUDE_ROOT) && ln -sfn $(ROOT) $(INCLUDE_ROOT)/afterhours)
CARDS := $(OUT)/house_of_cards

FLAGS := -std=c++20 -Wall -Wextra -Wconversion -O2 -g -DAFTER_HOURS_USE_RAYLIB \
         -DFMT_HEADER_ONLY -isystem $(RAYLIB_PREFIX)/include \
         -isystem $(BREW_PREFIX)/include -isystem $(ROOT)/vendor \
         -isystem $(INCLUDE_ROOT) -MD -MP
LDFLAGS := $(RAYLIB_PREFIX)/lib/libraylib.a -framework OpenGL \
           -framework CoreFoundation -framework Cocoa -framework IOKit \
           -framework CoreGraphics -framework CoreVideo -lobjc

MODELS_DIR := $(ROOT)/examples/catalog/3d
MODELS := $(MODELS_DIR)/tree-large.glb $(wildcard $(MODELS_DIR)/kaykit/*.gltf)

.PHONY: all check clean

all: $(OUT)/hoc_bake $(OUT)/hoc_compare

$(OUT)/hoc_bake: bake.cpp
	$(CXX) $(FLAGS) -MF $@.d $< $(LDFLAGS) -o $@

$(OUT)/hoc_compare: compare.cpp
	$(CXX) $(FLAGS) -MF $@.d $< $(LDFLAGS) -o $@

check: all
	@mkdir -p $(CARDS)
	@for m in $(MODELS); do \
	  n=$$(basename $${m%.*}); \
	  $(OUT)/hoc_bake $$m $(CARDS)/$$n >/dev/null && \
	  $(OUT)/hoc_compare $$m $(CARDS)/$$n --strip $(CARDS)/$${n}_strip.png \
	    --diff $(CARDS)/$${n}_diff.png | tail -n +2 || exit 1; \
	done

clean:
	rm -rf $(OUT)/hoc_bake $(OUT)/hoc_compare $(OUT)/hoc_*.d $(OUT)/hoc_*.dSYM $(CARDS) $(INCLUDE_ROOT)

-include $(OUT)/hoc_bake.d $(OUT)/hoc_compare.d
