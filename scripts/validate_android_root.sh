#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd "$(dirname "$0")" && pwd)
repo_root=$(CDPATH= cd "$script_dir/.." && pwd)
root=${1:-"$repo_root/android_root"}
failures=0

note_failure() {
  printf 'missing: %s\n' "$1" >&2
  failures=$((failures + 1))
}

require_dir() {
  [ -d "$root/$1" ] || note_failure "$1/"
}

require_file() {
  [ -f "$root/$1" ] || note_failure "$1"
}

require_prop() {
  file=$1
  prop=$2

  if [ ! -f "$root/$file" ]; then
    note_failure "$file"
    return 0
  fi

  if ! grep -q "^$prop=" "$root/$file"; then
    note_failure "$file:$prop"
  fi
}

for dir in \
  acct apex cache config data data/app data/data data/local/tmp data/misc data/system \
  debug_ramdisk dev linkerconfig metadata mnt \
  odm odm/app odm/etc/init odm/framework odm/lib odm/lib64 odm/priv-app \
  oem proc \
  product product/app product/etc/init product/framework product/lib product/lib64 product/priv-app \
  sdcard storage/emulated/0 \
  sys system system/app system/bin system/etc/init system/framework system/lib system/lib64 system/priv-app \
  tmp vendor vendor/app vendor/bin vendor/etc/init vendor/framework vendor/lib vendor/lib64 vendor/priv-app
do
  require_dir "$dir"
done

for file in \
  README.md init.rc fstab.vodka ueventd.rc default.prop \
  system/build.prop vendor/build.prop product/build.prop system/etc/hosts
do
  require_file "$file"
done

require_prop default.prop ro.vodka.root
require_prop system/build.prop ro.build.version.sdk
require_prop system/build.prop ro.product.cpu.abilist
require_prop vendor/build.prop ro.vendor.vodka.root
require_prop product/build.prop ro.product.product.name

if [ "$failures" -ne 0 ]; then
  printf 'Android root validation failed with %s issue(s): %s\n' "$failures" "$root" >&2
  exit 1
fi

printf 'Android root validation passed: %s\n' "$root"
