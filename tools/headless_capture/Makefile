# Makefile for the afterhours sokol/Metal HEADLESS render-to-PNG harness.
#
#   make        -> builds ./headless_capture (Metal, obj-c++), then adhoc-signs it
#   make run    -> build + run, writing out.png (no window; works lid-closed)
#   make clean  -> remove build artifacts
#
# Include layout: this harness includes <afterhours/...> and <sokol/...>. Point
# AH_INC at a dir that contains an "afterhours" entry resolving to the repo root
# (a symlink works well for an out-of-tree worktree whose dir isn't named
# "afterhours"):
#
#   mkdir -p /tmp/ah-inc && ln -sf <repo-root> /tmp/ah-inc/afterhours
#   make AH_INC=/tmp/ah-inc
#
# Defaults assume the repo root is two levels up and its dir is named
# "afterhours"; override AH_INC for a differently-named worktree.

AH_INC    ?= ../..
VENDOR    ?= ../../vendor
INCLUDES  := -isystem $(AH_INC) -isystem $(VENDOR)
CXXFLAGS  := -std=c++20 -DSOKOL_METAL
FRAMEWORKS := -framework Metal -framework MetalKit -framework Cocoa \
    -framework Foundation -framework QuartzCore -framework CoreGraphics \
    -framework ImageIO -framework CoreServices -framework AudioToolbox

headless_capture: headless_capture.mm sokol_impl.mm
	clang++ $(CXXFLAGS) $(INCLUDES) -x objective-c++ -c sokol_impl.mm -o sokol_impl.o
	clang++ $(CXXFLAGS) $(INCLUDES) -x objective-c++ -c headless_capture.mm -o headless_capture.o
	clang++ headless_capture.o sokol_impl.o $(FRAMEWORKS) -o headless_capture
	codesign -s - -f headless_capture

run: headless_capture
	./headless_capture out.png

clean:
	rm -f headless_capture headless_capture.o sokol_impl.o out.png

.PHONY: run clean
