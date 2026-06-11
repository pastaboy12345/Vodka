# Android Compatibility

Vodka now writes the first Android compatibility state into the prefix when an
APK is installed.

## Package State

`vodka install` creates or refreshes:

```text
android_root/data/system/packages.xml
android_root/data/system/packages.list
android_root/data/system/users/0/package-restrictions.xml
```

Each installed package gets a stable app UID starting at `10000`, matching the
normal Android app UID range. The state is intentionally minimal, but it gives
future PackageManager and system-service work a deterministic installed-package
database instead of only a copied APK.

Vodka reads stored or deflated `AndroidManifest.xml` entries and extracts the
package name from plain-text or Android binary XML manifests. It records
`uses-permission` declarations in package metadata and emits matching `<perms>`
entries in `packages.xml` when those permissions can be parsed.

The per-app metadata remains under:

```text
apps/<package>/metadata.conf
```

That metadata includes `manifest_format=`, `launch_activity=`,
`requested_permissions=`, `min_sdk=`, and `target_sdk=` when those values can be
read from the manifest.

## Binder State

`binder-status` reports configured and candidate Binder devices:

```sh
build/vodka binder-status
build/vodka binder-status --binder /dev/binder --configure
```

When configured, Vodka writes:

```text
config/binder.conf
android_root/data/system/vodka-binder-services.conf
```

The service-state file records the Android services the runtime expects to
eventually provide or bridge:

```text
activity
package
window
display
input
power
surfaceflinger
sensorservice
audio
```

This is not a Binder implementation yet. It is the compatibility contract the
app_process backend checks and passes forward while native Binder service work is
still being built.

## Service Bridges

Vodka can now track and launch service bridge processes. A bridge is an external
process responsible for one Android Binder-facing service.

Configure a bridge:

```sh
build/vodka bridge-status --service package --exec build/vodka-package-bridge
```

Inspect configured bridges:

```sh
build/vodka bridge-status
```

Start bridges:

```sh
build/vodka start-services --dry-run
build/vodka start-services --service package --wait
```

Required services:

```text
activity
package
window
display
input
power
surfaceflinger
```

Optional services:

```text
sensorservice
audio
clipboard
```

Bridge configuration is stored in:

```text
config/services.conf
android_root/data/system/vodka-service-bridges.conf
```

A bridge executable receives:

```text
bridge SERVICE BINDER_SERVICE BINDER_DEVICE
```

Environment:

```text
VODKA_PREFIX
VODKA_ANDROID_ROOT
VODKA_BINDER_DEVICE
VODKA_SERVICE
VODKA_BINDER_SERVICE
VODKA_SERVICE_REQUIRED
VODKA_PACKAGES_XML
VODKA_PACKAGES_LIST
VODKA_SERVICE_STATE
ANDROID_ROOT=/system
ANDROID_DATA=/data
```

`make` builds concrete host bridge executables for each tracked service:

```text
build/vodka-activity-bridge
build/vodka-package-bridge
build/vodka-window-bridge
build/vodka-display-bridge
build/vodka-input-bridge
build/vodka-power-bridge
build/vodka-surfaceflinger-bridge
build/vodka-sensorservice-bridge
build/vodka-audio-bridge
build/vodka-clipboard-bridge
```

These are host-side bridge processes, not a Binder protocol implementation yet.
They validate the service contract, open the configured Binder device, inspect
PackageManager state, and write per-service runtime state such as:

```text
android_root/data/system/vodka-service-package.state
```
