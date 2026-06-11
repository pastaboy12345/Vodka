# Runtime Backend

Vodka's runtime backend boundary is intentionally small: the CLI validates the
prefix, Android root, APK, app data directory, and metadata, then hands the app
context to a backend process.

## Backends

- `none` - default. Validates the launch plan and refuses to execute.
- `exec` - launches an external executable as a child process.
- `app_process` - launches Android `app_process64` or `app_process` from the
  prefix.

Select a backend for one run:

```sh
build/vodka run --backend exec --exec /path/to/backend com.example.app
```

Or configure the prefix:

```text
runtime.backend=exec
runtime.exec=/path/to/backend
```

## App Process Backend

Import runtime artifacts first:

```sh
build/vodka install-runtime --from /path/to/android-root --binder /dev/binder
build/vodka binder-status
```

Then run an installed package:

```sh
build/vodka run com.example.app
```

The backend executes:

```text
app_process /system/bin --application --nice-name=<package> android.app.ActivityThread <package> <activity> <apk> <data_dir>
```

The entrypoint, classpath, Binder device, and app_process path can be overridden:

```sh
build/vodka run \
  --backend app_process \
  --exec /path/to/app_process64 \
  --entrypoint android.app.ActivityThread \
  --classpath /system/framework/framework.jar \
  --binder /dev/binder \
  com.example.app
```

`binder-status --binder DEVICE --configure` updates `runtime.binder.device` and
writes the Binder service expectation file consumed by status and run planning.

Service bridges can be configured independently:

```sh
build/vodka bridge-status --service package --exec build/vodka-package-bridge
build/vodka start-services --service package --wait
```

The app_process backend does not start service bridges automatically; start them
before launching apps when the backend depends on bridged services.
The built `build/vodka-<service>-bridge` executables validate the bridge
environment and write per-service runtime state. They are the host bridge
process boundary for the next Binder service work, not a complete Android
system-service implementation.

## Exec Contract

Arguments:

```text
backend PACKAGE ACTIVITY APK DATA_DIR
```

Environment:

```text
VODKA_PREFIX
VODKA_ANDROID_ROOT
VODKA_PACKAGE
VODKA_ACTIVITY
VODKA_APK
VODKA_APP_DATA
VODKA_METADATA
VODKA_RUNTIME_BACKEND
VODKA_RUNTIME_EXEC
VODKA_APP_PROCESS
VODKA_APP_PROCESS_ENTRYPOINT
VODKA_CLASSPATH
VODKA_BINDER_DEVICE
CLASSPATH
ANDROID_ROOT=/system
ANDROID_DATA=/data
ANDROID_STORAGE=/storage
EXTERNAL_STORAGE=/sdcard
```

The backend exit code becomes the `vodka run` exit code.

## Next Native Backend Work

The `exec` backend is the bridge for experiments, and `app_process` is the first
native Android ART entrypoint. Vodka still needs:

- Binder service compatibility,
- PackageManager-compatible installed-package state,
- graphics, input, audio, clipboard, and lifecycle bridges.
