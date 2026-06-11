# App Runtime

Vodka can prepare Android apps inside a prefix and launch a configured runtime
backend for an installed package.

## Current Flow

```sh
build/vodka init
build/vodka install-runtime --from /path/to/android-root --binder /dev/binder
build/vodka install app.apk
build/vodka list
build/vodka run --dry-run com.example.app
build/vodka run com.example.app
build/vodka run --backend exec --exec /path/to/backend com.example.app
```

`install` performs the first Android-style staging work:

- validates that the APK is a ZIP file with `AndroidManifest.xml`,
- derives the package name from a stored plain-text manifest when possible,
- accepts `--package NAME` when the manifest cannot be parsed yet,
- copies the APK to `android_root/data/app/<package>-1/base.apk`,
- creates `android_root/data/data/<package>/`,
- writes prefix metadata to `apps/<package>/metadata.conf`.

`run --dry-run` validates the root, installed APK, app data directory, and
metadata, then prints the environment and paths the runtime backend will
consume.

## Runtime Backend

Vodka has two launch backends:

- `exec` launches an external executable and passes the app context as both
  arguments and environment.
- `app_process` launches Android's `app_process64` or `app_process` from the
  prefix, using `android.app.ActivityThread` by default.

Configure it per invocation:

```sh
build/vodka run --backend exec --exec /path/to/backend com.example.app
```

Or persist it in `config/vodka.conf`:

```text
runtime.backend=exec
runtime.exec=/path/to/backend
```

The executable receives these arguments:

```text
backend PACKAGE ACTIVITY APK DATA_DIR
```

It also receives these environment variables:

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
ANDROID_ROOT
ANDROID_DATA
ANDROID_STORAGE
EXTERNAL_STORAGE
```

The default backend is `none`, which validates the launch plan but refuses to
execute.

## Android Runtime Install

Import an Android userspace tree:

```sh
build/vodka install-runtime --from /path/to/android-root --binder /dev/binder
```

The source may be an Android root containing `system/`, or a `system/` directory
itself. Vodka copies runtime artifacts into the prefix, resolves
`system/bin/app_process64` or `system/bin/app_process`, builds a framework
classpath from known framework jars, and sets:

```text
runtime.backend=app_process
runtime.app_process=<prefix>/android_root/system/bin/app_process64
runtime.entrypoint=android.app.ActivityThread
runtime.classpath=...
runtime.binder.device=/dev/binder
```

`vodka status` reports `binder_ready` and whether the configured app process is
executable.

## Built-In Runtime Work

The app_process backend reaches the real ART/app process entrypoint, but useful
Android app execution still requires:

- Zygote/app process startup model,
- Binder driver or userspace Binder compatibility layer,
- PackageManager-compatible package and permission state,
- graphics, input, audio, clipboard, and lifecycle bridges.

Until those exist in Vodka itself, use `runtime.backend=exec` to route launches
to an external experimental app-process backend.
