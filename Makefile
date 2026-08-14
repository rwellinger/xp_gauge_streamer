SHELL := /bin/bash

XPLANE_ROOT := /Users/robertw/X-Plane 12
# Settings live in Output/ rather than the plugin folder so a plugin update,
# which replaces that folder wholesale, leaves them alone.
SETTINGS_DIR := $(XPLANE_ROOT)/Output/xp_gauge_streamer
# X-Plane's folder name for 64-bit macOS plugins is mac_x64 even for an
# arm64-only binary — the name predates Apple Silicon and is not architecture.
PLUGIN_DIR  := $(XPLANE_ROOT)/Resources/available plugins/xp_gauge_streamer

SDK_SENTINEL      := sdk/XPLM/XPLMPlugin.h
CIVETWEB_SENTINEL := vendor/civetweb/include/civetweb.h
CATCH2_SENTINEL   := vendor/catch2/catch_amalgamated.hpp

CIVETWEB_VERSION := 1.16
CATCH2_VERSION   := 3.15.3

DEPS := $(SDK_SENTINEL) $(CIVETWEB_SENTINEL) $(CATCH2_SENTINEL) jpeg-turbo

.PHONY: help all setup jpeg-turbo build test install format lint sanitize release release-build cleanup-tags cleanup-runs clean distclean

.DEFAULT_GOAL := help

# ── Help ──────────────────────────────────────────────────────────────────────
help:
	@echo "xp_gauge_streamer — X-Plane 12 plugin (macOS, Apple Silicon)"
	@echo ""
	@echo "Usage: make <target>"
	@echo ""
	@echo "Common:"
	@echo "  help            Show this message (default)"
	@echo "  setup           Download X-Plane SDK, civetweb, Catch2; install libjpeg-turbo"
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
	mkdir -p sdk/XPLM sdk/XPWidgets sdk/Libraries/Mac; \
	find "$$TMP/sdk_extracted" -path "*/CHeaders/XPLM/*.h"    -exec cp {} sdk/XPLM/ \;; \
	find "$$TMP/sdk_extracted" -path "*/CHeaders/Widgets/*.h" -exec cp {} sdk/XPWidgets/ \;; \
	cp -R "$$TMP/sdk_extracted"/*/Libraries/Mac/*.framework sdk/Libraries/Mac/ 2>/dev/null || \
	find "$$TMP/sdk_extracted" -name "*.framework" -exec cp -R {} sdk/Libraries/Mac/ \;
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

# libjpeg-turbo comes from Homebrew rather than a vendored copy — it is a
# build-system dependency with a native arm64/NEON build, not a single header.
jpeg-turbo:
	@command -v brew >/dev/null 2>&1 || { \
	    echo "Homebrew not found — install libjpeg-turbo manually."; exit 1; }
	@if brew list jpeg-turbo >/dev/null 2>&1; then \
	    echo "libjpeg-turbo already installed ($$(brew --prefix jpeg-turbo))."; \
	else \
	    echo "Installing libjpeg-turbo..."; brew install jpeg-turbo; \
	fi

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
	@echo "=== Installing xp_gauge_streamer ==="
	@mkdir -p "$(PLUGIN_DIR)/mac_x64"
	@cp build/xp_gauge_streamer.xpl "$(PLUGIN_DIR)/mac_x64/"
	@xattr -dr com.apple.quarantine "$(PLUGIN_DIR)/mac_x64/xp_gauge_streamer.xpl" 2>/dev/null || true
	@codesign --force --deep --sign - "$(PLUGIN_DIR)/mac_x64/xp_gauge_streamer.xpl"
	@echo "Signed:    $(PLUGIN_DIR)/mac_x64/xp_gauge_streamer.xpl"
	@echo "Installed: $(PLUGIN_DIR)/mac_x64/xp_gauge_streamer.xpl"
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
