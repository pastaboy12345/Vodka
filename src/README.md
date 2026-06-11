# Vodka Runtime Source

Runtime code starts with a small host-side CLI in `vodka.c`.

Current commands:

- `vodka init` creates or refreshes the prefix at `$VODKA_PREFIX` or
  `$HOME/.vodka`.
- `vodka status` reports whether the prefix and Android root are usable.
- `vodka probe [android-root]` validates and inspects an Android root.
- `vodka env` prints the environment paths the runtime will use.
