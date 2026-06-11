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
build/vodka binder-status
build/vodka bridge-status
build/vodka start-services --dry-run
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
- Package state: `android_root/data/system/packages.xml`
- Package list: `android_root/data/system/packages.list`

Vodka reads stored or deflated `AndroidManifest.xml` entries. It can derive the
package name from plain-text manifests and from Android binary XML manifests,
and it records requested permissions, SDK levels, and the first launcher
activity it can identify. `--package NAME` remains available for malformed or
unusual APKs whose package cannot be parsed yet.

`vodka run --dry-run PACKAGE` validates the installed app and prints the launch
plan. Running without `--dry-run` uses the configured runtime backend. The
default backend is `none`; set `runtime.backend=exec` and `runtime.exec` in
`config/vodka.conf`, pass `--backend exec --exec PATH`, or import Android
runtime artifacts with `install-runtime` to configure `runtime.backend=app_process`.

`binder-status --configure` writes `config/binder.conf` and
`android_root/data/system/vodka-binder-services.conf`.

`bridge-status` writes `config/services.conf` and
`android_root/data/system/vodka-service-bridges.conf`.
