CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
BUILD_DIR ?= build

VODKA := $(BUILD_DIR)/vodka

.PHONY: all check clean

all: $(VODKA)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(VODKA): src/vodka.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< -o $@

check: all
	./scripts/validate_android_root.sh
	$(VODKA) --prefix /tmp/vodka-check-unused probe android_root
	set -e; \
	tmpdir=$$(mktemp -d /tmp/vodka-prefix.XXXXXX); \
	$(VODKA) --prefix "$$tmpdir" init; \
	$(VODKA) --prefix "$$tmpdir" status; \
	$(VODKA) --prefix "$$tmpdir" probe
	set -e; \
	tmpdir=$$(mktemp -d /tmp/vodka-app.XXXXXX); \
	mkdir -p "$$tmpdir/apk"; \
	printf '%s\n' \
		'<manifest xmlns:android="http://schemas.android.com/apk/res/android" package="com.example.vodka">' \
		'  <application android:label="Vodka Test" />' \
		'</manifest>' > "$$tmpdir/apk/AndroidManifest.xml"; \
	( cd "$$tmpdir/apk" && zip -0 -q "$$tmpdir/example.apk" AndroidManifest.xml ); \
	$(VODKA) --prefix "$$tmpdir/prefix" install "$$tmpdir/example.apk"; \
	$(VODKA) --prefix "$$tmpdir/prefix" list; \
	$(VODKA) --prefix "$$tmpdir/prefix" run --dry-run com.example.vodka; \
	printf '%s\n' \
		'#!/usr/bin/env sh' \
		'set -eu' \
		'test "$$VODKA_PACKAGE" = "com.example.vodka"' \
		'test -f "$$VODKA_APK"' \
		'test -d "$$VODKA_APP_DATA"' \
		'printf "backend_package=%s\n" "$$VODKA_PACKAGE"' \
		'printf "backend_activity=%s\n" "$$VODKA_ACTIVITY"' \
		'printf "backend_android_root=%s\n" "$$ANDROID_ROOT"' \
		> "$$tmpdir/backend.sh"; \
	chmod +x "$$tmpdir/backend.sh"; \
	$(VODKA) --prefix "$$tmpdir/prefix" run --backend exec --exec "$$tmpdir/backend.sh" com.example.vodka
	set -e; \
	tmpdir=$$(mktemp -d /tmp/vodka-app-process.XXXXXX); \
	mkdir -p "$$tmpdir/runtime/system/bin" "$$tmpdir/runtime/system/framework" "$$tmpdir/apk"; \
	printf '%s\n' \
		'#!/usr/bin/env sh' \
		'set -eu' \
		'test "$$1" = "/system/bin"' \
		'test "$$2" = "--application"' \
		'test "$$3" = "--nice-name=com.example.vodka"' \
		'test "$$4" = "android.app.ActivityThread"' \
		'test "$$5" = "com.example.vodka"' \
		'test "$$VODKA_PACKAGE" = "com.example.vodka"' \
		'test "$$VODKA_BINDER_DEVICE" = "'"$$tmpdir"'/binder"' \
		'case "$$CLASSPATH" in *"/system/framework/framework.jar"*) ;; *) exit 9 ;; esac' \
		'printf "app_process_package=%s\n" "$$VODKA_PACKAGE"' \
		'printf "app_process_entrypoint=%s\n" "$$VODKA_APP_PROCESS_ENTRYPOINT"' \
		> "$$tmpdir/runtime/system/bin/app_process64"; \
	chmod +x "$$tmpdir/runtime/system/bin/app_process64"; \
	: > "$$tmpdir/runtime/system/framework/core-oj.jar"; \
	: > "$$tmpdir/runtime/system/framework/framework.jar"; \
	: > "$$tmpdir/binder"; \
	printf '%s\n' \
		'<manifest xmlns:android="http://schemas.android.com/apk/res/android" package="com.example.vodka">' \
		'  <application android:label="Vodka Test" />' \
		'</manifest>' > "$$tmpdir/apk/AndroidManifest.xml"; \
	( cd "$$tmpdir/apk" && zip -0 -q "$$tmpdir/example.apk" AndroidManifest.xml ); \
	$(VODKA) --prefix "$$tmpdir/prefix" install-runtime --from "$$tmpdir/runtime" --binder "$$tmpdir/binder"; \
	$(VODKA) --prefix "$$tmpdir/prefix" install "$$tmpdir/example.apk"; \
	$(VODKA) --prefix "$$tmpdir/prefix" run com.example.vodka

clean:
	rm -rf $(BUILD_DIR)
