SHELL := /bin/bash

XPLANE_ROOT := /Users/robertw/X-Plane 12
# Settings live in Output/ rather than the plugin folder so a plugin update,
# which replaces that folder wholesale, leaves them alone.
SETTINGS_DIR := $(XPLANE_ROOT)/Output/xp_gauge_streamer
PLUGIN_DIR  := $(XPLANE_ROOT)/Resources/available plugins/xp_gauge_streamer

# X-Plane's folder name for this platform's binaries. mac_x64 applies to arm64
# too — the name predates Apple Silicon and does not describe the architecture.
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    PLATFORM_FOLDER := mac_x64
else ifeq ($(UNAME_S),Linux)
    PLATFORM_FOLDER := lin_x64
else
    PLATFORM_FOLDER := win_x64
endif

SDK_SENTINEL      := sdk/XPLM/XPLMPlugin.h
CIVETWEB_SENTINEL := vendor/civetweb/include/civetweb.h
CATCH2_SENTINEL   := vendor/catch2/catch_amalgamated.hpp
JSON_SENTINEL     := vendor/json.hpp
IMGUI_SENTINEL    := vendor/imgui/imgui.h
TURBOJPEG_SENTINEL := vendor/libjpeg-turbo/CMakeLists.txt

CIVETWEB_VERSION := 1.16
CATCH2_VERSION   := 3.15.3
JSON_VERSION     := 3.12.0
IMGUI_VERSION    := 1.92.8
TURBOJPEG_VERSION := 3.2.0

DEPS := $(SDK_SENTINEL) $(CIVETWEB_SENTINEL) $(CATCH2_SENTINEL) $(JSON_SENTINEL) $(IMGUI_SENTINEL) $(TURBOJPEG_SENTINEL)

.PHONY: help all setup build test install format lint sanitize release release-build cleanup-tags cleanup-branches cleanup-runs clean distclean

.DEFAULT_GOAL := help

# ── Help ──────────────────────────────────────────────────────────────────────
help:
	@echo "xp_gauge_streamer — X-Plane 12 plugin (macOS arm64, Windows x64, Linux x64)"
	@echo ""
	@echo "Usage: make <target>"
	@echo ""
	@echo "Common:"
	@echo "  help            Show this message (default)"
	@echo "  setup           Download X-Plane SDK, civetweb, Catch2, nlohmann/json, Dear ImGui, libjpeg-turbo"
	@echo "  build           Configure + compile → build/xp_gauge_streamer.xpl"
	@echo "  test            Build and run the Catch2 unit tests"
	@echo "  install         Code-sign and copy plugin, web/ and default settings into X-Plane"
	@echo "  sanitize        Build + run the unit tests under ASan + UBSan"
	@echo "  clean           Remove build/, build-lint/ and build-sanitize/"
	@echo "  distclean       clean + remove sdk/ and vendor/ (everything 'make setup' installed)"
	@echo ""
	@echo "Code quality:"
	@echo "  format          Run clang-format on src/*.{cpp,hpp}"
	@echo "  lint            Run clang-tidy (uses build-lint/ for compile_commands.json)"
	@echo ""
	@echo "Release:"
	@echo "  release VERSION=x.y.z   Tag + push release (commits VERSION.txt)"
	@echo "  release-build           Local release build (-DRELEASE=ON)"
	@echo "  cleanup-tags            Prune local tags removed on origin"
	@echo "  cleanup-branches        Prune local branches whose remote is gone"
	@echo "  cleanup-runs            Delete all GitHub Actions runs except the newest per workflow"

all: format build lint test

# ── Setup ─────────────────────────────────────────────────────────────────────
setup: $(DEPS)
	@echo "Setup complete. Run 'make build' to compile."

$(SDK_SENTINEL):
	@echo "Downloading X-Plane SDK..."
	@set -euo pipefail; \
	TMP=$$(mktemp -d); \
	trap "rm -rf $$TMP" EXIT; \
	curl -fsSL "https://developer.x-plane.com/wp-content/plugins/code-sample-generation/sdk_zip_files/XPSDK430.zip" \
	     -o "$$TMP/sdk.zip"; \
	unzip -q "$$TMP/sdk.zip" -d "$$TMP/sdk_extracted"; \
	mkdir -p sdk/XPLM sdk/XPWidgets sdk/Libraries/Mac sdk/Libraries/Win; \
	find "$$TMP/sdk_extracted" -path "*/CHeaders/XPLM/*.h"    -exec cp {} sdk/XPLM/ \;; \
	find "$$TMP/sdk_extracted" -path "*/CHeaders/Widgets/*.h" -exec cp {} sdk/XPWidgets/ \;; \
	cp -R "$$TMP/sdk_extracted"/*/Libraries/Mac/*.framework sdk/Libraries/Mac/ 2>/dev/null || \
	find "$$TMP/sdk_extracted" -name "*.framework" -exec cp -R {} sdk/Libraries/Mac/ \;; \
	find "$$TMP/sdk_extracted" -path "*/Libraries/Win/*.lib" -exec cp {} sdk/Libraries/Win/ \;
	@echo "SDK headers installed."

$(CIVETWEB_SENTINEL):
	@echo "Downloading civetweb v$(CIVETWEB_VERSION)..."
	@set -euo pipefail; \
	TMP=$$(mktemp -d); \
	trap "rm -rf $$TMP" EXIT; \
	mkdir -p vendor/civetweb/src vendor/civetweb/include; \
	curl -fsSL "https://github.com/civetweb/civetweb/archive/refs/tags/v$(CIVETWEB_VERSION).tar.gz" \
	     -o "$$TMP/civetweb.tar.gz"; \
	tar -xzf "$$TMP/civetweb.tar.gz" -C "$$TMP/"; \
	SRC="$$TMP/civetweb-$(CIVETWEB_VERSION)"; \
	cp "$$SRC"/src/civetweb.c "$$SRC"/src/*.inl "$$SRC"/src/civetweb_private_lua.h vendor/civetweb/src/; \
	cp "$$SRC"/include/civetweb.h vendor/civetweb/include/
	@echo "civetweb installed."

$(CATCH2_SENTINEL):
	@echo "Downloading Catch2 v$(CATCH2_VERSION) (amalgamated)..."
	@set -euo pipefail; \
	TMP=$$(mktemp -d); \
	trap "rm -rf $$TMP" EXIT; \
	mkdir -p vendor/catch2; \
	curl -fsSL "https://github.com/catchorg/Catch2/archive/refs/tags/v$(CATCH2_VERSION).tar.gz" \
	     -o "$$TMP/catch2.tar.gz"; \
	tar -xzf "$$TMP/catch2.tar.gz" -C "$$TMP/"; \
	cp "$$TMP/Catch2-$(CATCH2_VERSION)/extras/catch_amalgamated.hpp" vendor/catch2/; \
	cp "$$TMP/Catch2-$(CATCH2_VERSION)/extras/catch_amalgamated.cpp" vendor/catch2/
	@echo "Catch2 installed."

$(JSON_SENTINEL):
	@echo "Downloading nlohmann/json v$(JSON_VERSION)..."
	@mkdir -p vendor
	@curl -fsSL "https://github.com/nlohmann/json/releases/download/v$(JSON_VERSION)/json.hpp" \
	     -o vendor/json.hpp
	@echo "nlohmann/json installed."

$(IMGUI_SENTINEL):
	@echo "Downloading Dear ImGui v$(IMGUI_VERSION)..."
	@set -euo pipefail; \
	TMP=$$(mktemp -d); \
	trap "rm -rf $$TMP" EXIT; \
	mkdir -p vendor/imgui/backends; \
	curl -fsSL "https://github.com/ocornut/imgui/archive/refs/tags/v$(IMGUI_VERSION).zip" -o "$$TMP/imgui.zip"; \
	unzip -q "$$TMP/imgui.zip" -d "$$TMP/"; \
	SRC="$$TMP/imgui-$(IMGUI_VERSION)"; \
	cp "$$SRC"/imgui.{h,cpp} vendor/imgui/; \
	cp "$$SRC"/imgui_{draw,tables,widgets}.cpp vendor/imgui/; \
	cp "$$SRC"/imgui_internal.h "$$SRC"/imconfig.h vendor/imgui/; \
	cp "$$SRC"/imstb_{textedit,rectpack,truetype}.h vendor/imgui/; \
	cp "$$SRC"/backends/imgui_impl_opengl2.{h,cpp} vendor/imgui/backends/
	@echo "Dear ImGui installed."

# Built from source as a CMake subproject rather than taken from a package
# manager: Homebrew, apt and vcpkg disagree on layout, static/shared defaults and
# version, and the plugin needs one predictable static library on all three
# platforms. SIMD needs NASM on x86 (Linux: apt install nasm, Windows: choco
# install nasm); Apple Silicon uses NEON and needs nothing.
$(TURBOJPEG_SENTINEL):
	@echo "Downloading libjpeg-turbo v$(TURBOJPEG_VERSION)..."
	@set -euo pipefail; \
	TMP=$$(mktemp -d); \
	trap "rm -rf $$TMP" EXIT; \
	curl -fsSL "https://github.com/libjpeg-turbo/libjpeg-turbo/archive/refs/tags/$(TURBOJPEG_VERSION).tar.gz" \
	     -o "$$TMP/libjpeg-turbo.tar.gz"; \
	tar -xzf "$$TMP/libjpeg-turbo.tar.gz" -C "$$TMP/"; \
	rm -rf vendor/libjpeg-turbo; \
	mkdir -p vendor; \
	mv "$$TMP/libjpeg-turbo-$(TURBOJPEG_VERSION)" vendor/libjpeg-turbo
	@echo "libjpeg-turbo installed." 

# ── Build ─────────────────────────────────────────────────────────────────────
build: $(DEPS)
	@echo "=== Building xp_gauge_streamer ==="
	cmake -B build -DCMAKE_BUILD_TYPE=Release -Wno-dev
	cmake --build build --parallel
	@echo ""
	@file build/xp_gauge_streamer.xpl
	@echo "Done. Run 'make install' to deploy."

# ── Test ──────────────────────────────────────────────────────────────────────
test: build
	@echo "=== Running xp_gauge_streamer tests ==="
	@./build/xp_gauge_streamer_tests

# ── Install ───────────────────────────────────────────────────────────────────
install:
	@if [ ! -f "build/xp_gauge_streamer.xpl" ]; then \
	    echo "Plugin not built yet. Run 'make build' first."; exit 1; \
	fi
	@echo "=== Installing xp_gauge_streamer ($(PLATFORM_FOLDER)) ==="
	@mkdir -p "$(PLUGIN_DIR)/$(PLATFORM_FOLDER)"
	@cp build/xp_gauge_streamer.xpl "$(PLUGIN_DIR)/$(PLATFORM_FOLDER)/"
	@if [ "$(UNAME_S)" = "Darwin" ]; then \
	    xattr -dr com.apple.quarantine "$(PLUGIN_DIR)/$(PLATFORM_FOLDER)/xp_gauge_streamer.xpl" 2>/dev/null || true; \
	    codesign --force --deep --sign - "$(PLUGIN_DIR)/$(PLATFORM_FOLDER)/xp_gauge_streamer.xpl"; \
	    echo "Signed:    $(PLUGIN_DIR)/$(PLATFORM_FOLDER)/xp_gauge_streamer.xpl"; \
	fi
	@echo "Installed: $(PLUGIN_DIR)/$(PLATFORM_FOLDER)/xp_gauge_streamer.xpl"
	@rm -rf "$(PLUGIN_DIR)/web"
	@cp -R web "$(PLUGIN_DIR)/web"
	@echo "Installed: $(PLUGIN_DIR)/web (HTTP document root)"
	@if [ -f "$(SETTINGS_DIR)/settings.cfg" ]; then \
	    echo "Kept:      $(SETTINGS_DIR)/settings.cfg (already present)"; \
	else \
	    mkdir -p "$(SETTINGS_DIR)"; \
	    cp config/settings.cfg "$(SETTINGS_DIR)/"; \
	    echo "Installed: $(SETTINGS_DIR)/settings.cfg"; \
	fi
	@echo ""
	@echo "Plugin installed. Restart X-Plane to load it."

# ── Lint ──────────────────────────────────────────────────────────────────────
format:
	@command -v clang-format >/dev/null 2>&1 || { \
	    echo "clang-format not found. Install with: brew install llvm"; \
	    echo "Then add to PATH: export PATH=\"\$$(brew --prefix llvm)/bin:\$$PATH\""; \
	    exit 1; }
	clang-format -i src/*.cpp src/*.hpp

lint: $(DEPS)
	@command -v clang-tidy >/dev/null 2>&1 || { \
	    echo "clang-tidy not found. Install with: brew install llvm"; \
	    echo "Then add to PATH: export PATH=\"\$$(brew --prefix llvm)/bin:\$$PATH\""; \
	    exit 1; }
	cmake -B build-lint -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -Wno-dev
	clang-tidy -p build-lint --extra-arg="-isysroot" --extra-arg="$(shell xcrun --show-sdk-path)" src/*.cpp

# ── Sanitize ──────────────────────────────────────────────────────────────────
# Builds + runs only the SDK-free Catch2 tests under AddressSanitizer +
# UndefinedBehaviorSanitizer. The .xpl plugin is intentionally NOT instrumented
# — ASan inside the X-Plane process is fragile on macOS ARM64 (dyld + code
# signing). For leak / heap analysis of the running plugin, attach
# Instruments.app to the X-Plane process.
sanitize: $(DEPS)
	@echo "=== Configuring sanitizer build (ASan + UBSan) ==="
	cmake -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DXP_GAUGE_STREAMER_SANITIZE=ON -Wno-dev
	@echo "=== Building xp_gauge_streamer_tests with ASan + UBSan ==="
	cmake --build build-sanitize --target xp_gauge_streamer_tests --parallel
	@echo ""
	@echo "=== Running unit tests under ASan + UBSan ==="
	@ASAN_OPTIONS=detect_leaks=0:abort_on_error=1:print_stacktrace=1 \
	 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
		./build-sanitize/xp_gauge_streamer_tests
	@echo ""
	@echo "Sanitizer run clean."

# ── Release ───────────────────────────────────────────────────────────────────
release:
	@if [ -z "$(VERSION)" ]; then \
	    echo "Usage: make release VERSION=1.2.1"; exit 1; \
	fi
	@if ! git diff --quiet || ! git diff --cached --quiet; then \
	    echo "Uncommitted changes present. Commit or stash first."; exit 1; \
	fi
	@if [ -n "$$(git ls-files --others --exclude-standard)" ]; then \
	    echo "Untracked files present. Commit or clean up first."; exit 1; \
	fi
	@echo "$(VERSION)" > VERSION.txt
	@git add VERSION.txt
	@git commit -m "release $(VERSION)"
	@git push origin main
	@git tag -a "v$(VERSION)" -m "Release $(VERSION)"
	@git push origin "v$(VERSION)"
	@echo "Released v$(VERSION) and pushed tag to origin."

release-build: $(DEPS)
	@echo "=== Building xp_gauge_streamer (release) ==="
	cmake -B build -DCMAKE_BUILD_TYPE=Release -DRELEASE=ON -Wno-dev
	cmake --build build --parallel
	@echo ""
	@file build/xp_gauge_streamer.xpl
	@echo "Done. Release build with version from VERSION.txt."

# ── Cleanup Tags ──────────────────────────────────────────────────────────────
cleanup-tags:
	git fetch --prune --prune-tags origin
	@echo "Local tags synced with remote."

# ── Cleanup Branches ──────────────────────────────────────────────────────────
cleanup-branches:
	@echo "Pruning remote-tracking references..."
	@git fetch --prune origin
	@echo ""
	@echo "Local branches whose upstream is gone:"
	@STALE=$$(git for-each-ref --format '%(refname:short) %(upstream:track)' refs/heads | awk '$$2 == "[gone]" {print $$1}'); \
	if [ -z "$$STALE" ]; then \
	    echo "  (none)"; \
	else \
	    echo "$$STALE" | sed 's/^/  /'; \
	    echo ""; \
	    echo "$$STALE" | xargs -n1 git branch -d; \
	fi
	@echo "Local branches synced with remote."

# ── Cleanup GitHub Actions runs ───────────────────────────────────────────────
cleanup-runs:
	@command -v gh >/dev/null 2>&1 || { \
	    echo "gh not found. Install with: brew install gh"; exit 1; }
	@echo "Deleting GitHub Actions runs (keeping newest per workflow)..."
	@for wf in $$(gh workflow list --json id -q '.[].id'); do \
	    gh run list --workflow=$$wf --limit 1000 --json databaseId -q '.[1:] | .[].databaseId' \
	        | xargs -I {} gh run delete {}; \
	done
	@echo "Cleanup complete."

# ── Clean ─────────────────────────────────────────────────────────────────────
clean:
	rm -rf build build-lint build-sanitize

# ── Distclean ─────────────────────────────────────────────────────────────────
# Remove everything 'make setup' downloaded so a full re-bootstrap is forced.
distclean: clean
	rm -rf sdk/ vendor/
	@echo "Removed sdk/ and vendor/. Run 'make setup' to re-download dependencies."
