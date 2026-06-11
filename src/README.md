# Vodka Runtime Source

Runtime code starts with a small host-side CLI in `vodka.c`.

Current commands:

- `vodka init` creates or refreshes the prefix at `$VODKA_PREFIX` or
  `$HOME/.vodka`.
- `vodka status` reports whether the prefix and Android root are usable.
- `vodka install-runtime --from ROOT` imports Android ART/app_process runtime
  artifacts and configures the `app_process` backend.
- `vodka install [--package NAME] APK` stages an APK in the prefix.
- `vodka list` reports installed packages.
- `vodka run [--dry-run] [--backend NAME] [--exec PATH] PACKAGE` validates and
  launches through the configured runtime backend.
- `vodka probe [android-root]` validates and inspects an Android root.
- `vodka env` prints the environment paths the runtime will use.
