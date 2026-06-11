# Vodka

Type: `experiment`

## Description

Vodka is an experiment in running Android applications on Linux in the spirit of
Wine: provide the userspace expectations Android apps rely on without booting a
full Android device image.

## Structure

- `android_root/` - base Android filesystem skeleton used by the runtime.
- `src/` - Vodka runtime source code.
- `docs/` - design notes and subsystem contracts.
- `assets/` - project assets.
- `scripts/` - host-side setup and validation helpers.
- `notes/` - rough notes and research.

## Starting Point

Build the CLI:

```sh
make
```

Create or refresh the default prefix at `$HOME/.vodka`:

```sh
build/vodka init
```

Use a development prefix somewhere else:

```sh
build/vodka --prefix /tmp/vodka-dev init
build/vodka --prefix /tmp/vodka-dev status
build/vodka --prefix /tmp/vodka-dev probe
```

Install and inspect an APK:

```sh
build/vodka install app.apk
build/vodka list
build/vodka run --dry-run com.example.app
```

For APKs whose manifest package cannot be read without Android tooling, pass the
package explicitly:

```sh
build/vodka install --package com.example.app app.apk
```

Run through an external runtime backend:

```sh
build/vodka run --backend exec --exec /path/to/vodka-app-backend com.example.app
```

Import Android runtime artifacts and use `app_process`:

```sh
build/vodka install-runtime --from /path/to/android-root --binder /dev/binder
build/vodka install app.apk
build/vodka run com.example.app
```

Create or refresh the Android root skeleton:

```sh
./scripts/bootstrap_android_root.sh
```

Validate the expected base layout:

```sh
./scripts/validate_android_root.sh
```

The checked-in root is intentionally not a complete Android system image. It
contains Vodka-owned directories and configuration stubs; framework, ART,
Bionic, APK, and vendor artifacts must be supplied by later import/build steps.
Vodka can now launch a configured external backend with the staged app context;
it can also import Android `app_process`/framework artifacts and launch the
`app_process` backend. Full app compatibility still depends on a working Android
userspace, Binder device, and system services.

Run the current verification target:

```sh
make check
```
