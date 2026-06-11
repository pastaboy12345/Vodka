# Vodka Android Root

This directory is the initial Android filesystem view presented to Android
userspace. It is a skeleton, not a bootable Android image.

## Contract

- `ANDROID_ROOT` maps to `/system`.
- `ANDROID_DATA` maps to `/data`.
- Runtime-writable state starts under `/data`, `/cache`, and `/tmp`.
- Host pseudo filesystems such as `/proc`, `/sys`, and `/dev` are mount points,
  not source-controlled content.
- Partition-style directories (`/system`, `/vendor`, `/product`, `/odm`) are
  separate so later import steps can mirror modern Android layouts.

## Current Scope

The root currently provides:

- Android-like directory hierarchy.
- Minimal `init.rc`, `ueventd.rc`, and `fstab.vodka` stubs.
- Generic property files for early runtime discovery.
- Empty application, framework, library, and init-extension directories.

The root does not yet provide ART, Zygote, PackageManager, Binder services,
graphics/audio/input bridges, or native library translation.
