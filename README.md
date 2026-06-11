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
