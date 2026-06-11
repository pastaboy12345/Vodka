# Android Root

Vodka starts with a source-controlled Android root skeleton under
`android_root/`. The goal is to give runtime work a stable filesystem contract
before pulling in Android framework or vendor artifacts.

## Layout

The root mirrors modern Android partition boundaries:

- `/system` for core runtime files, framework jars, app stubs, libraries, and
  system init fragments.
- `/vendor`, `/product`, and `/odm` for partition-specific apps, libraries,
  framework extensions, and init fragments.
- `/data`, `/cache`, and `/tmp` for mutable runtime state.
- `/proc`, `/sys`, `/dev`, `/mnt`, and `/storage` as host-backed or emulated
  mount points.

## Current Assumptions

- Initial native target is `x86_64`, with `x86` as a secondary ABI.
- ARM and ARM64 APK support is deferred until a native bridge strategy exists.
- `ro.build.version.sdk=35` and `ro.build.version.release=15` are placeholders
  for app compatibility checks, not a claim that Vodka implements Android 15.
- Android framework, ART, Bionic, PackageManager, Binder services, graphics,
  audio, input, sensors, and permission enforcement are not implemented yet.

## Host Helpers

Bootstrap or refresh the skeleton:

```sh
./scripts/bootstrap_android_root.sh
```

Validate the minimum expected layout:

```sh
./scripts/validate_android_root.sh
```

Both scripts accept an optional root path:

```sh
./scripts/bootstrap_android_root.sh /tmp/vodka-root
./scripts/validate_android_root.sh /tmp/vodka-root
```

## Next Work

The next implementation layer should define how Vodka starts Android userspace:

- process model and environment setup,
- UID/GID mapping between Android and Linux,
- Bionic and dynamic-linker strategy,
- Binder driver or userspace Binder compatibility layer,
- package installation layout under `/data/app` and `/data/data`.
