# Roadmap Notes

## Phase 0: Root Contract

- Keep `android_root/` source-controlled and reproducible.
- Treat framework/vendor/runtime files as imported artifacts, not hand-written
  source files.
- Validate the root layout before runtime tests.

## Phase 1: Minimal Userspace Probe

- Create the default prefix at `$HOME/.vodka`.
- Stage APKs into the prefix and track installed package metadata.
- Launch installed apps through an external `exec` runtime backend.
- Import Android runtime artifacts and launch `app_process` with ART environment.
- Start a host process with Android-like environment variables.
- Resolve `/system`, `/data`, `/vendor`, and `/product` paths through the Vodka
  root.
- Load and report Android property files.
- Add a smoke test that proves a process can see the expected root contract.

## Phase 2: Runtime Services

- Decide Binder strategy.
- Decide ART/Zygote strategy.
- Define APK install and app data layout.
- Bridge graphics, input, audio, and clipboard to the Linux desktop.
