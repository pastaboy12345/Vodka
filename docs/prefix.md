# Prefix

Vodka uses a prefix directory in the same spirit as Wine. The default prefix is:

```text
$HOME/.vodka
```

Set `VODKA_PREFIX` or pass `--prefix PATH` to use another location.

## Layout

`vodka init` creates:

- `apps/` - installed app metadata.
- `android_root/` - Android filesystem skeleton used by the runtime.
- `cache/` - runtime cache files.
- `config/vodka.conf` - prefix configuration.
- `logs/` - runtime logs.
- `tmp/` - temporary runtime files.
- `VERSION` - prefix schema version.

The init command is idempotent and does not overwrite existing files. This keeps
locally imported framework or app artifacts intact while allowing missing base
directories to be restored.

## Commands

```sh
build/vodka init
build/vodka status
build/vodka install-runtime --from /path/to/android-root
build/vodka install app.apk
build/vodka list
build/vodka run --dry-run com.example.app
build/vodka run com.example.app
build/vodka run --backend exec --exec /path/to/backend com.example.app
build/vodka probe
build/vodka env
```

`probe` defaults to `<prefix>/android_root`. It can also inspect a specific root:

```sh
build/vodka probe android_root
```

## Development

Use a temporary prefix for tests and experiments:

```sh
build/vodka --prefix /tmp/vodka-dev init
build/vodka --prefix /tmp/vodka-dev status
```

## Installed Apps

`vodka install` stages APKs inside the prefix:

- APK payload: `android_root/data/app/<package>-1/base.apk`
- App data: `android_root/data/data/<package>/`
- Metadata: `apps/<package>/metadata.conf`

If the APK has a stored plain-text `AndroidManifest.xml`, Vodka can derive the
package name. Normal APKs often use binary or compressed manifest data, so
`--package NAME` is available until Vodka has a full Android manifest parser.

`vodka run --dry-run PACKAGE` validates the installed app and prints the launch
plan. Running without `--dry-run` uses the configured runtime backend. The
default backend is `none`; set `runtime.backend=exec` and `runtime.exec` in
`config/vodka.conf`, pass `--backend exec --exec PATH`, or import Android
runtime artifacts with `install-runtime` to configure `runtime.backend=app_process`.
