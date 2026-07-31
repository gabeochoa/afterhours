include ../../common.mk
ROOT = ../../../..

RAYLIB_PREFIX := $(shell brew --prefix raylib 2>/dev/null)
BREW_PREFIX := $(shell brew --prefix 2>/dev/null)

FLAGS = -std=c++23 -Wall -Wextra -g -DFMT_HEADER_ONLY -DAFTER_HOURS_USE_RAYLIB \
		 -isystem $(ROOT)/vendor -isystem $(BREW_PREFIX)/include \
		 -isystem $(RAYLIB_PREFIX)/include
LDFLAGS = $(RAYLIB_PREFIX)/lib/libraylib.a -framework OpenGL \
		  -framework CoreFoundation -framework Cocoa -framework IOKit \
		  -framework CoreGraphics -framework CoreVideo -lobjc

all:
	clang++ $(FLAGS) main.cpp $(LDFLAGS) -o $(OUT)/split_layout.exe && $(OUT)/split_layout.exe
