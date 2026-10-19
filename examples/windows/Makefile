# Makefile for the afterhours SOKOL Windows example.
#
#   make        - Windows/Vulkan cross-build (zig, from macOS) -> ./demo.exe
#   make clean  - remove build artifacts
#
# Builds the demo app in ../web (shared with the web + native Metal builds);
# only the build lives here. demo.exe runs on the Windows box.
#
# The import lib is a build output, not checked in.

# <afterhours/...> lives under vendor/ ; <sokol/...>,<fontstash/...> under
# vendor/afterhours/vendor/.
INCLUDES := -isystem ../../.. -isystem ../../vendor
CXXFLAGS := -std=c++20

VULKAN_INCLUDE := ../../vendor/vulkan/include
VULKAN_DEF := ../../vendor/vulkan/vulkan-1.def

libvulkan-1.a: $(VULKAN_DEF)
	zig dlltool -d $(VULKAN_DEF) -l $@ -m i386:x86-64

demo.exe: ../web/main.cpp ../web/sokol_impl.cc libvulkan-1.a
	zig c++ -target x86_64-windows-gnu $(CXXFLAGS) -DAFTER_HOURS_USE_VULKAN $(INCLUDES) -isystem $(VULKAN_INCLUDE) ../web/main.cpp ../web/sokol_impl.cc -L. -lvulkan-1 -lkernel32 -luser32 -lshell32 -lgdi32 -o demo.exe

all: demo.exe

win: demo.exe

clean:
	rm -f demo.exe libvulkan-1.a

.PHONY: all win clean
