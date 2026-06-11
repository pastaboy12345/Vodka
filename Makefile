CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
LDLIBS ?= -lz
BUILD_DIR ?= build

VODKA := $(BUILD_DIR)/vodka
BRIDGE_SERVICES := activity package window display input power surfaceflinger sensorservice audio clipboard
BRIDGES := $(addprefix $(BUILD_DIR)/vodka-,$(addsuffix -bridge,$(BRIDGE_SERVICES)))
PACKAGE_BRIDGE := $(abspath $(BUILD_DIR)/vodka-package-bridge)

.PHONY: all check clean

all: $(VODKA) $(BRIDGES)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(VODKA): src/vodka.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< -o $@ $(LDLIBS)

$(BUILD_DIR)/vodka-%-bridge: src/vodka_bridge.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DVODKA_EXPECTED_SERVICE='"$*"' $< -o $@

check: all
	./scripts/validate_android_root.sh
	$(VODKA) --prefix /tmp/vodka-check-unused probe android_root
	for service in $(BRIDGE_SERVICES); do test -x "$(BUILD_DIR)/vodka-$$service-bridge"; done
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
		'  <uses-permission android:name="android.permission.INTERNET" />' \
		'  <application android:label="Vodka Test" />' \
		'</manifest>' > "$$tmpdir/apk/AndroidManifest.xml"; \
	( cd "$$tmpdir/apk" && zip -0 -q "$$tmpdir/example.apk" AndroidManifest.xml ); \
	$(VODKA) --prefix "$$tmpdir/prefix" install "$$tmpdir/example.apk"; \
	test -f "$$tmpdir/prefix/android_root/data/system/packages.xml"; \
	test -f "$$tmpdir/prefix/android_root/data/system/packages.list"; \
	grep -q 'com.example.vodka' "$$tmpdir/prefix/android_root/data/system/packages.xml"; \
	grep -q 'android.permission.INTERNET' "$$tmpdir/prefix/android_root/data/system/packages.xml"; \
	grep -q 'requested_permissions=android.permission.INTERNET' "$$tmpdir/prefix/apps/com.example.vodka/metadata.conf"; \
	grep -q 'com.example.vodka' "$$tmpdir/prefix/android_root/data/system/packages.list"; \
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
	tmpdir=$$(mktemp -d /tmp/vodka-binary-manifest.XXXXXX); \
	python3 scripts/make_binary_manifest_apk.py "$$tmpdir/binary.apk"; \
	$(VODKA) --prefix "$$tmpdir/prefix" install "$$tmpdir/binary.apk"; \
	grep -q 'package=com.example.binary' "$$tmpdir/prefix/apps/com.example.binary/metadata.conf"; \
	grep -q 'manifest_format=binary' "$$tmpdir/prefix/apps/com.example.binary/metadata.conf"; \
	grep -q 'launch_activity=com.example.binary.MainActivity' "$$tmpdir/prefix/apps/com.example.binary/metadata.conf"; \
	grep -q 'requested_permissions=android.permission.INTERNET' "$$tmpdir/prefix/apps/com.example.binary/metadata.conf"; \
	grep -q 'min_sdk=23' "$$tmpdir/prefix/apps/com.example.binary/metadata.conf"; \
	grep -q 'target_sdk=31' "$$tmpdir/prefix/apps/com.example.binary/metadata.conf"; \
	grep -q 'targetSdkVersion="31"' "$$tmpdir/prefix/android_root/data/system/packages.xml"; \
	$(VODKA) --prefix "$$tmpdir/prefix" run --dry-run com.example.binary
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
	$(VODKA) --prefix "$$tmpdir/prefix" binder-status --binder "$$tmpdir/binder" --configure; \
	$(VODKA) --prefix "$$tmpdir/prefix" bridge-status --service package --exec "$(PACKAGE_BRIDGE)"; \
	test -f "$$tmpdir/prefix/android_root/data/system/vodka-service-bridges.conf"; \
	printf '%s\n' \
		'<manifest xmlns:android="http://schemas.android.com/apk/res/android" package="com.example.vodka">' \
		'  <application android:label="Vodka Test" />' \
		'</manifest>' > "$$tmpdir/apk/AndroidManifest.xml"; \
	( cd "$$tmpdir/apk" && zip -0 -q "$$tmpdir/example.apk" AndroidManifest.xml ); \
	$(VODKA) --prefix "$$tmpdir/prefix" install-runtime --from "$$tmpdir/runtime" --binder "$$tmpdir/binder"; \
	test -f "$$tmpdir/prefix/android_root/data/system/vodka-binder-services.conf"; \
	grep -q 'service.activity=required' "$$tmpdir/prefix/android_root/data/system/vodka-binder-services.conf"; \
	$(VODKA) --prefix "$$tmpdir/prefix" install "$$tmpdir/example.apk"; \
	$(VODKA) --prefix "$$tmpdir/prefix" start-services --dry-run --service package; \
	$(VODKA) --prefix "$$tmpdir/prefix" start-services --wait --service package; \
	test -f "$$tmpdir/prefix/android_root/data/system/vodka-service-package.state"; \
	grep -q 'status=ready' "$$tmpdir/prefix/android_root/data/system/vodka-service-package.state"; \
	grep -q 'package_count=1' "$$tmpdir/prefix/android_root/data/system/vodka-service-package.state"; \
	$(VODKA) --prefix "$$tmpdir/prefix" run com.example.vodka

clean:
	rm -rf $(BUILD_DIR)
