# Convenience wrapper around CMake/CTest. All real build logic lives in CMakeLists.txt.
#
# Variables (override on the command line, e.g. `make build QT_MAJOR_VERSION=6`):
#   BUILD_DIR         build directory                      (default: build)
#   BUILD_TYPE        CMake build type                     (default: Debug)
#   QT_MAJOR_VERSION  5 or 6; empty lets CMake auto-detect (default: empty)
#   JOBS              parallel build jobs                  (default: nproc)
#   PREFIX            per-user install prefix              (default: ~/.local)
#   SCALE             social preview size multiplier       (default: 1, i.e. 640x320)
#   REFRESH_MENU      1 = make running Plasma pick up menu/icon changes after install/uninstall (default: 1)
#
# Switching QT_MAJOR_VERSION needs a fresh build directory: use `make rebuild`
# or a different BUILD_DIR (e.g. BUILD_DIR=build-qt6).

BUILD_DIR        ?= build
BUILD_TYPE       ?= Debug
QT_MAJOR_VERSION ?=
JOBS             ?= $(shell nproc 2>/dev/null || echo 2)
PREFIX           ?= $(HOME)/.local
REFRESH_MENU     ?= 1
SCALE            ?= 1

APP := $(BUILD_DIR)/bin/konsole-ssh-manager

CMAKE_ARGS := -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCMAKE_INSTALL_PREFIX=$(PREFIX)
ifneq ($(strip $(QT_MAJOR_VERSION)),)
CMAKE_ARGS += -DQT_MAJOR_VERSION=$(QT_MAJOR_VERSION)
endif

DESKTOP_ID := ro.binarylogic.konsole-ssh-manager
INSTALLED   := $(PREFIX)/bin/konsole-ssh-manager $(PREFIX)/bin/konsole-ssh-session \
               $(PREFIX)/share/applications/$(DESKTOP_ID).desktop \
               $(PREFIX)/share/icons/hicolor/scalable/apps/$(DESKTOP_ID).svg
# kbuildsycoca6 on Plasma 6 (Debian 13), kbuildsycoca5 on Plasma 5 (Debian 12).
MENU_CACHE_TOOL := $(firstword $(foreach tool,kbuildsycoca6 kbuildsycoca5,$(shell command -v $(tool) 2>/dev/null)))

DEBIAN_MAJOR := $(shell cut -d. -f1 /etc/debian_version 2>/dev/null)
DEPS_COMMON  := build-essential cmake extra-cmake-modules konsole openssh-client clang-format
DEPS_12      := qtbase5-dev libkf5parts-dev libkf5coreaddons-dev libkf5i18n-dev
DEPS_13      := qt6-base-dev libkf6parts-dev libkf6coreaddons-dev libkf6i18n-dev

.DEFAULT_GOAL := build
.PHONY: help deps configure build test run install uninstall refresh-menu screenshot social-preview format clean distclean rebuild

help: ## Show this help
	@grep -E '^[a-z-]+:.*## ' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*## "} {printf "  %-15s %s\n", $$1, $$2}'

deps: ## Install build dependencies for this Debian release (uses sudo apt)
ifeq ($(DEBIAN_MAJOR),12)
	sudo apt install $(DEPS_COMMON) $(DEPS_12)
else ifeq ($(DEBIAN_MAJOR),13)
	sudo apt install $(DEPS_COMMON) $(DEPS_13)
else
	@echo "Unsupported system: only Debian 12 and Debian 13 are supported." >&2; exit 1
endif

$(BUILD_DIR)/CMakeCache.txt:
	cmake -S . -B $(BUILD_DIR) $(CMAKE_ARGS)

configure: ## (Re)run CMake configuration
	cmake -S . -B $(BUILD_DIR) $(CMAKE_ARGS)

build: $(BUILD_DIR)/CMakeCache.txt ## Build the app and tests
	cmake --build $(BUILD_DIR) -j$(JOBS)

test: build ## Build and run the test suite
	ctest --test-dir $(BUILD_DIR) --output-on-failure

run: build ## Build and start the app from the build directory
	./$(APP)

install: ## Build, then install for the current user (executables to PREFIX/bin, menu entry, icon)
	cmake -S . -B $(BUILD_DIR) $(CMAKE_ARGS)
	cmake --build $(BUILD_DIR) -j$(JOBS)
	cmake --install $(BUILD_DIR)
	@$(MAKE) --no-print-directory refresh-menu

uninstall: ## Remove what `make install` installed (keeps settings, backups and SSH config)
	rm -f $(INSTALLED)
	@$(MAKE) --no-print-directory refresh-menu

# Rebuilds the menu cache, then tells running KDE programs (Plasma, KWin) to reload
# their icon themes: they only scan icon folders at startup, so a newly created
# ~/.local/share/icons/hicolor/scalable/apps would otherwise stay unseen until re-login.
refresh-menu:
	@if [ "$(REFRESH_MENU)" = 1 ] && [ -n "$(MENU_CACHE_TOOL)" ]; then $(MENU_CACHE_TOOL) >/dev/null 2>&1 || true; fi
	@if [ "$(REFRESH_MENU)" = 1 ] && command -v dbus-send >/dev/null; then \
		dbus-send --session --type=signal /KIconLoader org.kde.KIconLoader.iconChanged int32:0 >/dev/null 2>&1 || true; fi

screenshot: ## Regenerate docs/images/screenshot.png (app rendered off-screen with demo hosts only)
	cmake -S . -B $(BUILD_DIR) $(CMAKE_ARGS) -DKSSHM_BUILD_SCREENSHOT_TOOL=ON
	cmake --build $(BUILD_DIR) -j$(JOBS) --target konsole-ssh-manager-screenshot
	./$(BUILD_DIR)/bin/konsole-ssh-manager-screenshot tools/screenshot/demo-ssh $(BUILD_DIR)/screenshot-home docs/images/screenshot.png

social-preview: ## Regenerate data/social-preview.png (GitHub social preview; SCALE=2 for 1280x640)
	cmake -S . -B $(BUILD_DIR) $(CMAKE_ARGS) -DKSSHM_BUILD_SOCIAL_PREVIEW_TOOL=ON
	cmake --build $(BUILD_DIR) -j$(JOBS) --target konsole-ssh-manager-social-preview
	./$(BUILD_DIR)/bin/konsole-ssh-manager-social-preview data/icons/$(DESKTOP_ID).svg data/social-preview.png $(SCALE)

format: $(BUILD_DIR)/CMakeCache.txt ## Format sources with clang-format (KDE style)
	cmake --build $(BUILD_DIR) --target clang-format

clean: ## Remove build outputs, keep the CMake configuration
	@if [ -f $(BUILD_DIR)/CMakeCache.txt ]; then cmake --build $(BUILD_DIR) --target clean; fi

distclean: ## Delete the whole build directory
	rm -rf $(BUILD_DIR)

rebuild: distclean build ## Delete the build directory and build from scratch
