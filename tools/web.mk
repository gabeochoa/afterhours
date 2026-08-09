# afterhours/tools/web.mk — opt-in Emscripten/WebAssembly packaging for raylib games.
#
# Nothing in afterhours calls this; include it from your game Makefile when you
# want `make web`. Example:
#
#   WEB_NAME := MyGame
#   WEB_VERSION := 0.1.0
#   WEB_SRCS := $(SRC_FILES) vendor/afterhours/src/plugins/files.cpp
#   WEB_CXXFLAGS := -std=c++23 -O2 -DNDEBUG -DPLATFORM_WEB \
#       -DAFTER_HOURS_USE_RAYLIB $(INCLUDES) ...
#   OBJ_DIR := ./output
#   RAYLIB_WEB_SRC := /path/to/raylib   # source tree with PLATFORM_WEB .a
#   include vendor/afterhours/tools/web.mk
#
# Optional before include:
#   define WEB_STAGE_TRIM
#   	rm -rf $(1)/dev_only_assets
#   endef
#
# Known-good link flags (do not "improve" without re-testing audio):
#   - no ALLOW_MEMORY_GROWTH (miniaudio ScriptProcessorNode + HEAPF32 views)
#   - EXPORTED_RUNTIME_METHODS includes HEAPF32
#   - ASYNCIFY for a blocking main loop
#   - tools/web/shell.html (fullscreen gesture + full-bleed canvas)

# Directory of this file (…/afterhours/tools/), even when included from a game.
AH_WEB_MK_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
AH_WEB_SHELL_SRC := $(AH_WEB_MK_DIR)web/shell.html

.PHONY: web web-build web-serve web-clean web-raylib web-stage-resources

WEB_NAME ?= Game
WEB_VERSION ?= 0.1.0
WEB_TITLE ?= $(WEB_NAME)
WEB_RESOURCES_DIR ?= resources
OBJ_DIR ?= ./output

# ---- Emscripten toolchain ---------------------------------------------------
EMSDK ?= F:/emsdk
EMSCRIPTEN ?= $(EMSDK)/upstream/emscripten
EMSDK_NODE_DIR := $(lastword $(sort $(wildcard $(EMSDK)/node/*)))
# MSYS make recipes use /bin/sh, which does NOT search F:/... on PATH — only
# /f/... style. cygpath converts when present; otherwise leave as-is (Unix).
EMSDK_PATH := $(shell cygpath -u '$(EMSDK)' 2>/dev/null)
ifeq ($(strip $(EMSDK_PATH)),)
EMSDK_PATH := $(EMSDK)
endif
EMSCRIPTEN_PATH := $(EMSDK_PATH)/upstream/emscripten
EMSDK_NODE_PATH := $(shell cygpath -u '$(EMSDK_NODE_DIR)' 2>/dev/null)
ifeq ($(strip $(EMSDK_NODE_PATH)),)
EMSDK_NODE_PATH := $(EMSDK_NODE_DIR)
endif

ifeq ($(shell command -v em++ >/dev/null 2>&1 && echo ok),)
  ifneq ($(wildcard $(EMSCRIPTEN)/em++)$(wildcard $(EMSCRIPTEN)/em++.bat),)
    export PATH := $(EMSCRIPTEN_PATH):$(EMSDK_PATH):$(EMSDK_NODE_PATH):$(PATH)
    export EMSDK
    export EMSDK_NODE := $(EMSDK_NODE_DIR)/node.exe
  endif
endif

RAYLIB_WEB_SRC ?= F:/raylib-src
RAYLIB_WEB_A ?= $(RAYLIB_WEB_SRC)/src/libraylib.a
WEB_INITIAL_MEMORY ?= 536870912

WEB_OUT := $(OBJ_DIR)/web
WEB_OBJ := $(WEB_OUT)/obj
WEB_RES := $(WEB_OUT)/resources
WEB_HTML := $(WEB_OUT)/index.html
WEB_SHELL := $(WEB_OUT)/shell.html
WEB_ZIP_NAME := $(WEB_NAME)-$(WEB_VERSION)-web.zip
WEB_ZIP := $(OBJ_DIR)/$(WEB_ZIP_NAME)

ifeq ($(strip $(WEB_SRCS)),)
$(error WEB_SRCS is empty — set it to your game .cpp list before including afterhours/tools/web.mk)
endif
WEB_OBJS := $(WEB_SRCS:%.cpp=$(WEB_OBJ)/%.o)

WEB_INCLUDES ?= -isystem vendor/ -Isrc/ -I$(RAYLIB_WEB_SRC)/src
WEB_DEFS ?= -DPLATFORM_WEB -DAFTER_HOURS_USE_RAYLIB -DFMT_HEADER_ONLY \
	-DVector2Type=raylib::Vector2 -DRectangleType=raylib::Rectangle \
	-DTextureType=raylib::Texture2D
WEB_CXXFLAGS ?= -std=c++23 -O2 -DNDEBUG -Wall -Wno-unused-command-line-argument \
	$(WEB_DEFS) $(WEB_INCLUDES)

WEB_LDFLAGS := $(RAYLIB_WEB_A) \
	-sUSE_GLFW=3 -sASYNCIFY -sASYNCIFY_STACK_SIZE=1048576 \
	-sFORCE_FILESYSTEM=1 \
	-sINITIAL_MEMORY=$(WEB_INITIAL_MEMORY) \
	-sEXPORTED_RUNTIME_METHODS=ccall,cwrap,HEAPF32,HEAPU8 \
	-sMINIFY_HTML=0 \
	--shell-file $(WEB_SHELL) \
	--preload-file $(WEB_RES)@$(WEB_RESOURCES_DIR) \
	-o $(WEB_HTML)

define WEB_CHECK_EMCC
	@command -v em++ >/dev/null 2>&1 || { \
	  echo "em++ not found. Install/activate the Emscripten SDK:"; \
	  echo "  EMSDK=$(EMSDK)"; \
	  echo "  PowerShell (note the leading dot):  . $(EMSDK)/emsdk_env.ps1"; \
	  echo "  cmd.exe:   $(EMSDK)/emsdk_env.bat"; \
	  echo "  Unix:      source \"\$$HOME/emsdk/emsdk_env.sh\""; \
	  exit 1; \
	}
endef

# Optional trim hook: games may `define WEB_STAGE_TRIM` … `endef` with $(1)=staged dir.
ifndef WEB_STAGE_TRIM
define WEB_STAGE_TRIM
endef
endif

$(RAYLIB_WEB_A):
	@$(MAKE) web-raylib

web-raylib:
	$(WEB_CHECK_EMCC)
	@test -f $(RAYLIB_WEB_SRC)/src/Makefile || { \
	  echo "raylib web sources not found at $(RAYLIB_WEB_SRC)"; \
	  echo "Clone them, e.g.: git clone --depth 1 --branch 5.5 https://github.com/raysan5/raylib.git $(RAYLIB_WEB_SRC)"; \
	  exit 1; \
	}
	$(MAKE) -C $(RAYLIB_WEB_SRC)/src PLATFORM=PLATFORM_WEB RAYLIB_LIBTYPE=STATIC
	@test -f $(RAYLIB_WEB_A) || { echo "expected $(RAYLIB_WEB_A)"; exit 1; }

web-stage-resources:
	@rm -rf $(WEB_RES)
	@mkdir -p $(WEB_RES)
	@cp -R $(WEB_RESOURCES_DIR)/. $(WEB_RES)/
	@$(call WEB_STAGE_TRIM,$(WEB_RES))
	@mkdir -p $(WEB_OUT)
	@sed 's/WEB_TITLE_PLACEHOLDER/$(WEB_TITLE)/g' $(AH_WEB_SHELL_SRC) > $(WEB_SHELL)

$(WEB_OBJ)/%.o: %.cpp | $(RAYLIB_WEB_A)
	$(WEB_CHECK_EMCC)
	@mkdir -p $(dir $@)
	em++ $(WEB_CXXFLAGS) -c $< -o $@

web-build: $(RAYLIB_WEB_A) web-stage-resources $(WEB_OBJS)
	$(WEB_CHECK_EMCC)
	@mkdir -p $(WEB_OUT)
	em++ $(WEB_CXXFLAGS) $(WEB_OBJS) $(WEB_LDFLAGS)
	@echo "Web build: $(WEB_HTML)"

web: web-build
	@rm -f $(WEB_ZIP)
	@cd $(OBJ_DIR) && /c/Windows/System32/tar.exe -a -cf $(WEB_ZIP_NAME) web 2>/dev/null \
	  || (cd $(OBJ_DIR) && zip -r -q $(WEB_ZIP_NAME) web)
	@echo "Packaged: $(WEB_ZIP) ($$(du -h $(WEB_ZIP) | cut -f1))"

web-serve: web-build
	@echo "Serving http://localhost:8000/index.html  (ctrl-c to stop)"
	@cd $(WEB_OUT) && python3 -m http.server 8000

web-clean:
	rm -rf $(WEB_OUT) $(WEB_ZIP)
