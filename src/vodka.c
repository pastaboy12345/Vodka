#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <zlib.h>

#define VODKA_PATH_MAX 4096
#define VODKA_KEY_MAX 128
#define VODKA_VALUE_MAX VODKA_PATH_MAX
#define VODKA_SOURCE_MAX 128
#define VODKA_MAX_PROPERTIES 256
#define VODKA_PACKAGE_MAX 256
#define VODKA_ACTIVITY_MAX 512
#define VODKA_PERMISSION_MAX 256
#define VODKA_MAX_PERMISSIONS 128
#define VODKA_MAX_APPS 512
#define VODKA_ZIP_TAIL_MAX 65557
#define VODKA_MANIFEST_MAX 1048576
#define VODKA_APP_UID_BASE 10000
#define VODKA_DEFAULT_TARGET_SDK 35
#define VODKA_PREFIX_VERSION "0"
#define VODKA_ZIP_METHOD_STORE 0
#define VODKA_ZIP_METHOD_DEFLATE 8
#define VODKA_AXML_NO_INDEX 0xffffffffu
#define VODKA_AXML_RES_STRING_POOL_TYPE 0x0001
#define VODKA_AXML_RES_XML_TYPE 0x0003
#define VODKA_AXML_START_ELEMENT_TYPE 0x0102
#define VODKA_AXML_END_ELEMENT_TYPE 0x0103
#define VODKA_AXML_STRING_POOL_UTF8_FLAG 0x00000100u
#define VODKA_AXML_TYPE_STRING 0x03
#define VODKA_AXML_TYPE_INT_DEC 0x10
#define VODKA_AXML_TYPE_INT_HEX 0x11

struct vodka_property {
    char key[VODKA_KEY_MAX];
    char value[VODKA_VALUE_MAX];
    char source[VODKA_SOURCE_MAX];
};

struct vodka_properties {
    struct vodka_property items[VODKA_MAX_PROPERTIES];
    size_t count;
};

struct vodka_cli {
    const char *program;
    const char *prefix;
};

struct static_file {
    const char *relative;
    const char *content;
};

struct service_bridge {
    const char *name;
    const char *binder_name;
    bool required;
};

struct apk_info {
    bool valid_zip;
    bool has_manifest;
    uint16_t manifest_method;
    uint32_t manifest_compressed_size;
    uint32_t manifest_uncompressed_size;
    uint32_t manifest_local_offset;
};

struct manifest_permissions {
    char items[VODKA_MAX_PERMISSIONS][VODKA_PERMISSION_MAX];
    size_t count;
};

struct manifest_details {
    char package[VODKA_PACKAGE_MAX];
    char launch_activity[VODKA_ACTIVITY_MAX];
    struct manifest_permissions permissions;
    int min_sdk;
    int target_sdk;
    bool has_package;
    bool has_launch_activity;
    bool has_min_sdk;
    bool has_target_sdk;
    bool parsed;
    char format[16];
};

struct axml_string_pool {
    const unsigned char *chunk;
    size_t chunk_size;
    uint32_t string_count;
    uint32_t style_count;
    uint32_t strings_start;
    bool utf8;
};

struct run_options {
    bool dry_run;
    const char *activity_override;
    const char *backend_override;
    const char *exec_override;
    const char *entrypoint_override;
    const char *classpath_override;
    const char *binder_override;
};

struct run_plan {
    const char *prefix;
    char android_root[VODKA_PATH_MAX];
    char metadata_path[VODKA_PATH_MAX];
    const char *package;
    const char *activity;
    const char *apk;
    const char *data_dir;
    const char *backend;
    const char *exec_path;
    const char *entrypoint;
    const char *classpath;
    const char *binder_device;
    bool binder_ready;
};

static const char *const prefix_dirs[] = {
    "android_root",
    "apps",
    "cache",
    "config",
    "logs",
    "tmp",
};

static const struct service_bridge service_bridges[] = {
    {"activity", "activity", true},
    {"package", "package", true},
    {"window", "window", true},
    {"display", "display", true},
    {"input", "input", true},
    {"power", "power", true},
    {"surfaceflinger", "SurfaceFlinger", true},
    {"sensorservice", "sensorservice", false},
    {"audio", "audio", false},
    {"clipboard", "clipboard", false},
};

static bool set_runtime_env(const char *key, const char *value);

static const char *const android_root_dirs[] = {
    "acct",
    "apex",
    "cache",
    "config",
    "data/app",
    "data/data",
    "data/local/tmp",
    "data/misc",
    "data/system",
    "debug_ramdisk",
    "dev",
    "linkerconfig",
    "metadata",
    "mnt",
    "odm/app",
    "odm/etc/init",
    "odm/framework",
    "odm/lib",
    "odm/lib64",
    "odm/priv-app",
    "oem",
    "proc",
    "product/app",
    "product/etc/init",
    "product/framework",
    "product/lib",
    "product/lib64",
    "product/priv-app",
    "sdcard",
    "storage/emulated/0",
    "sys",
    "system/app",
    "system/bin",
    "system/etc/init",
    "system/etc/permissions",
    "system/etc/sysconfig",
    "system/framework",
    "system/lib",
    "system/lib64",
    "system/priv-app",
    "tmp",
    "vendor/app",
    "vendor/bin",
    "vendor/etc/init",
    "vendor/framework",
    "vendor/lib",
    "vendor/lib64",
    "vendor/priv-app",
};

static const struct static_file android_root_files[] = {
    {
        "README.md",
        "# Vodka Android Root\n"
        "\n"
        "This directory is the initial Android filesystem view presented to Android\n"
        "userspace. It is a skeleton, not a bootable Android image.\n"
    },
    {
        "init.rc",
        "# Minimal Android init stub for the Vodka runtime.\n"
        "\n"
        "import /system/etc/init/*.rc\n"
        "import /vendor/etc/init/*.rc\n"
        "import /product/etc/init/*.rc\n"
        "import /odm/etc/init/*.rc\n"
        "\n"
        "on early-init\n"
        "    mkdir /dev 0755 root root\n"
        "    mkdir /proc 0555 root root\n"
        "    mkdir /sys 0555 root root\n"
        "    mkdir /mnt 0755 root root\n"
        "\n"
        "on init\n"
        "    export ANDROID_ROOT /system\n"
        "    export ANDROID_DATA /data\n"
        "    export ANDROID_STORAGE /storage\n"
        "    export EXTERNAL_STORAGE /sdcard\n"
        "\n"
        "    mkdir /cache 0770 system cache\n"
        "    mkdir /data 0771 system system\n"
        "    mkdir /data/app 0771 system system\n"
        "    mkdir /data/data 0771 system system\n"
        "    mkdir /data/local 0751 shell shell\n"
        "    mkdir /data/local/tmp 0771 shell shell\n"
        "    mkdir /data/misc 01771 system misc\n"
        "    mkdir /data/system 0775 system system\n"
        "    mkdir /storage 0755 root root\n"
        "    mkdir /storage/emulated 0555 root root\n"
        "    mkdir /tmp 0771 shell shell\n"
    },
    {
        "fstab.vodka",
        "# Android fs_mgr-style mount table for Vodka's initial root.\n"
        "\n"
        "# <src>     <mount_point>  <type>  <mount_flags>           <fs_mgr_flags>\n"
        "proc        /proc          proc    nosuid,nodev,noexec     wait\n"
        "sysfs       /sys           sysfs   nosuid,nodev,noexec     wait\n"
        "tmpfs       /dev           tmpfs   mode=0755,nosuid        wait\n"
        "tmpfs       /mnt           tmpfs   mode=0755,nosuid,nodev  wait\n"
        "tmpfs       /tmp           tmpfs   mode=1777,nosuid,nodev  wait\n"
    },
    {
        "ueventd.rc",
        "# Device node permissions expected by early Android userspace.\n"
        "\n"
        "/dev/null       0666 root root\n"
        "/dev/zero       0666 root root\n"
        "/dev/full       0666 root root\n"
        "/dev/random     0666 root root\n"
        "/dev/urandom    0666 root root\n"
        "/dev/ashmem     0666 root root\n"
        "/dev/binder     0666 root root\n"
        "/dev/hwbinder   0666 root root\n"
        "/dev/vndbinder  0666 root root\n"
    },
    {
        "default.prop",
        "# Root-level properties for the initial Vodka Android environment.\n"
        "\n"
        "ro.secure=0\n"
        "ro.debuggable=1\n"
        "ro.adb.secure=0\n"
        "ro.vodka.root=1\n"
        "ro.vodka.root.version=0\n"
    },
    {
        "system/build.prop",
        "# Generic system properties for Vodka's initial Android root.\n"
        "\n"
        "ro.product.brand=Vodka\n"
        "ro.product.manufacturer=Vodka\n"
        "ro.product.model=Vodka Linux Runtime\n"
        "ro.product.name=vodka_x86_64\n"
        "ro.product.device=vodka\n"
        "ro.build.product=vodka\n"
        "\n"
        "ro.build.type=userdebug\n"
        "ro.build.tags=test-keys\n"
        "ro.build.version.sdk=35\n"
        "ro.build.version.release=15\n"
        "ro.build.version.codename=REL\n"
        "ro.build.version.incremental=0\n"
        "\n"
        "ro.product.cpu.abilist=x86_64,x86\n"
        "ro.product.cpu.abilist64=x86_64\n"
        "ro.product.cpu.abilist32=x86\n"
        "ro.dalvik.vm.isa.x86_64=x86_64\n"
        "ro.dalvik.vm.isa.x86=x86\n"
        "\n"
        "persist.sys.locale=en-US\n"
        "ro.config.low_ram=false\n"
    },
    {
        "vendor/build.prop",
        "# Vendor partition properties for the initial Vodka root.\n"
        "\n"
        "ro.vendor.vodka.root=1\n"
    },
    {
        "product/build.prop",
        "# Product partition properties for the initial Vodka root.\n"
        "\n"
        "ro.product.product.brand=Vodka\n"
        "ro.product.product.name=vodka\n"
        "ro.product.product.device=vodka\n"
        "ro.product.product.model=Vodka Linux Runtime\n"
    },
    {
        "system/etc/hosts",
        "127.0.0.1 localhost\n"
        "::1 localhost ip6-localhost\n"
    },
};

static void copy_string(char *dest, size_t dest_size, const char *source)
{
    if (dest_size == 0) {
        return;
    }

    snprintf(dest, dest_size, "%s", source);
}

static char *trim(char *value)
{
    while (isspace((unsigned char)*value)) {
        value++;
    }

    if (*value == '\0') {
        return value;
    }

    char *end = value + strlen(value) - 1;
    while (end > value && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }

    return value;
}

static bool join_path(char *dest, size_t dest_size, const char *root, const char *relative)
{
    const size_t root_len = strlen(root);
    const char *separator = (root_len > 0 && root[root_len - 1] == '/') ? "" : "/";
    const int written = snprintf(dest, dest_size, "%s%s%s", root, separator, relative);
    return written >= 0 && (size_t)written < dest_size;
}

static bool default_prefix(char *dest, size_t dest_size)
{
    const char *env_prefix = getenv("VODKA_PREFIX");
    if (env_prefix != NULL && *env_prefix != '\0') {
        copy_string(dest, dest_size, env_prefix);
        return strlen(env_prefix) < dest_size;
    }

    const char *home = getenv("HOME");
    if (home == NULL || *home == '\0') {
        fprintf(stderr, "HOME is not set; use --prefix or VODKA_PREFIX\n");
        return false;
    }

    return join_path(dest, dest_size, home, ".vodka");
}

static bool path_exists(const char *path)
{
    struct stat stat_buffer;
    return stat(path, &stat_buffer) == 0;
}

static bool is_directory(const char *path)
{
    struct stat stat_buffer;
    return stat(path, &stat_buffer) == 0 && S_ISDIR(stat_buffer.st_mode);
}

static bool mkdir_one(const char *path, mode_t mode)
{
    if (mkdir(path, mode) == 0) {
        return true;
    }

    if (errno == EEXIST && is_directory(path)) {
        return true;
    }

    fprintf(stderr, "failed to create directory %s: %s\n", path, strerror(errno));
    return false;
}

static bool mkdir_p(const char *path, mode_t mode)
{
    char current[VODKA_PATH_MAX];

    if (strlen(path) >= sizeof(current)) {
        fprintf(stderr, "path too long: %s\n", path);
        return false;
    }

    copy_string(current, sizeof(current), path);

    size_t start = 0;
    if (current[0] == '/') {
        start = 1;
    }

    for (char *cursor = current + start; *cursor != '\0'; cursor++) {
        if (*cursor != '/') {
            continue;
        }

        *cursor = '\0';
        if (current[0] != '\0' && !mkdir_one(current, mode)) {
            return false;
        }
        *cursor = '/';
    }

    return mkdir_one(current, mode);
}

static bool ensure_parent_dir(const char *path)
{
    char parent[VODKA_PATH_MAX];

    if (strlen(path) >= sizeof(parent)) {
        fprintf(stderr, "path too long: %s\n", path);
        return false;
    }

    copy_string(parent, sizeof(parent), path);

    char *slash = strrchr(parent, '/');
    if (slash == NULL) {
        return true;
    }

    if (slash == parent) {
        slash[1] = '\0';
    } else {
        *slash = '\0';
    }

    return mkdir_p(parent, 0755);
}

static bool write_file_if_missing(const char *path, const char *content)
{
    if (path_exists(path)) {
        return true;
    }

    if (!ensure_parent_dir(path)) {
        return false;
    }

    FILE *file = fopen(path, "w");
    if (file == NULL) {
        fprintf(stderr, "failed to create %s: %s\n", path, strerror(errno));
        return false;
    }

    if (fputs(content, file) == EOF) {
        fprintf(stderr, "failed to write %s\n", path);
        fclose(file);
        return false;
    }

    if (fclose(file) != 0) {
        fprintf(stderr, "failed to close %s: %s\n", path, strerror(errno));
        return false;
    }

    return true;
}

static bool write_file_replace(const char *path, const char *content)
{
    if (!ensure_parent_dir(path)) {
        return false;
    }

    FILE *file = fopen(path, "w");
    if (file == NULL) {
        fprintf(stderr, "failed to create %s: %s\n", path, strerror(errno));
        return false;
    }

    if (fputs(content, file) == EOF) {
        fprintf(stderr, "failed to write %s\n", path);
        fclose(file);
        return false;
    }

    if (fclose(file) != 0) {
        fprintf(stderr, "failed to close %s: %s\n", path, strerror(errno));
        return false;
    }

    return true;
}

static bool copy_file_replace(const char *source, const char *dest)
{
    if (!ensure_parent_dir(dest)) {
        return false;
    }

    FILE *in = fopen(source, "rb");
    if (in == NULL) {
        fprintf(stderr, "failed to open %s: %s\n", source, strerror(errno));
        return false;
    }

    FILE *out = fopen(dest, "wb");
    if (out == NULL) {
        fprintf(stderr, "failed to create %s: %s\n", dest, strerror(errno));
        fclose(in);
        return false;
    }

    bool ok = true;
    unsigned char buffer[65536];
    size_t read_count = 0;

    while ((read_count = fread(buffer, 1, sizeof(buffer), in)) > 0) {
        if (fwrite(buffer, 1, read_count, out) != read_count) {
            fprintf(stderr, "failed to write %s\n", dest);
            ok = false;
            break;
        }
    }

    if (ferror(in)) {
        fprintf(stderr, "failed to read %s\n", source);
        ok = false;
    }

    if (fclose(out) != 0) {
        fprintf(stderr, "failed to close %s: %s\n", dest, strerror(errno));
        ok = false;
    }
    fclose(in);

    return ok;
}

static bool copy_file_replace_mode(const char *source, const char *dest, mode_t mode)
{
    if (!copy_file_replace(source, dest)) {
        return false;
    }

    if (chmod(dest, mode & 0777) != 0) {
        fprintf(stderr, "failed to chmod %s: %s\n", dest, strerror(errno));
        return false;
    }

    return true;
}

static bool copy_symlink_replace(const char *source, const char *dest)
{
    char target[VODKA_PATH_MAX];
    const ssize_t length = readlink(source, target, sizeof(target) - 1);
    if (length < 0) {
        fprintf(stderr, "failed to read symlink %s: %s\n", source, strerror(errno));
        return false;
    }
    target[length] = '\0';

    if (!ensure_parent_dir(dest)) {
        return false;
    }

    if (unlink(dest) != 0 && errno != ENOENT) {
        fprintf(stderr, "failed to replace %s: %s\n", dest, strerror(errno));
        return false;
    }

    if (symlink(target, dest) != 0) {
        fprintf(stderr, "failed to create symlink %s -> %s: %s\n", dest, target, strerror(errno));
        return false;
    }

    return true;
}

static bool copy_tree_replace(const char *source, const char *dest)
{
    struct stat stat_buffer;
    if (lstat(source, &stat_buffer) != 0) {
        fprintf(stderr, "failed to stat %s: %s\n", source, strerror(errno));
        return false;
    }

    if (S_ISLNK(stat_buffer.st_mode)) {
        return copy_symlink_replace(source, dest);
    }

    if (S_ISREG(stat_buffer.st_mode)) {
        return copy_file_replace_mode(source, dest, stat_buffer.st_mode);
    }

    if (!S_ISDIR(stat_buffer.st_mode)) {
        return true;
    }

    if (!mkdir_p(dest, stat_buffer.st_mode & 0777)) {
        return false;
    }

    DIR *dir = opendir(source);
    if (dir == NULL) {
        fprintf(stderr, "failed to open directory %s: %s\n", source, strerror(errno));
        return false;
    }

    bool ok = true;
    struct dirent *entry = NULL;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char source_child[VODKA_PATH_MAX];
        char dest_child[VODKA_PATH_MAX];
        if (!join_path(source_child, sizeof(source_child), source, entry->d_name) ||
            !join_path(dest_child, sizeof(dest_child), dest, entry->d_name)) {
            fprintf(stderr, "path too long while copying %s\n", source);
            ok = false;
            continue;
        }

        if (!copy_tree_replace(source_child, dest_child)) {
            ok = false;
        }
    }

    closedir(dir);
    return ok;
}

static bool copy_tree_if_exists(const char *source, const char *dest)
{
    if (!path_exists(source)) {
        return true;
    }

    return copy_tree_replace(source, dest);
}

static uint16_t read_le16(const unsigned char *value)
{
    return (uint16_t)value[0] | ((uint16_t)value[1] << 8);
}

static uint32_t read_le32(const unsigned char *value)
{
    return (uint32_t)value[0] |
        ((uint32_t)value[1] << 8) |
        ((uint32_t)value[2] << 16) |
        ((uint32_t)value[3] << 24);
}

static bool is_package_start(unsigned char value)
{
    return isalpha(value) || value == '_';
}

static bool is_package_char(unsigned char value)
{
    return isalnum(value) || value == '_';
}

static bool validate_package_name(const char *package)
{
    if (package == NULL || *package == '\0' || strlen(package) >= VODKA_PACKAGE_MAX) {
        return false;
    }

    bool saw_dot = false;
    bool need_segment_start = true;

    for (const unsigned char *cursor = (const unsigned char *)package; *cursor != '\0'; cursor++) {
        if (*cursor == '.') {
            if (need_segment_start) {
                return false;
            }
            saw_dot = true;
            need_segment_start = true;
            continue;
        }

        if (need_segment_start) {
            if (!is_package_start(*cursor)) {
                return false;
            }
            need_segment_start = false;
            continue;
        }

        if (!is_package_char(*cursor)) {
            return false;
        }
    }

    return saw_dot && !need_segment_start;
}

static bool validate_permission_name(const char *permission)
{
    if (permission == NULL || *permission == '\0' || strlen(permission) >= VODKA_PERMISSION_MAX) {
        return false;
    }

    bool saw_dot = false;
    bool last_was_dot = true;
    for (const unsigned char *cursor = (const unsigned char *)permission; *cursor != '\0'; cursor++) {
        if (*cursor == '.') {
            if (last_was_dot) {
                return false;
            }
            saw_dot = true;
            last_was_dot = true;
            continue;
        }
        if (!isalnum(*cursor) && *cursor != '_') {
            return false;
        }
        last_was_dot = false;
    }

    return saw_dot && !last_was_dot;
}

static bool add_manifest_permission(struct manifest_permissions *permissions, const char *name)
{
    if (!validate_permission_name(name)) {
        return true;
    }

    for (size_t i = 0; i < permissions->count; i++) {
        if (strcmp(permissions->items[i], name) == 0) {
            return true;
        }
    }

    if (permissions->count >= VODKA_MAX_PERMISSIONS) {
        fprintf(stderr, "too many requested permissions in manifest; increase VODKA_MAX_PERMISSIONS\n");
        return false;
    }

    copy_string(permissions->items[permissions->count], VODKA_PERMISSION_MAX, name);
    permissions->count++;
    return true;
}

static bool prefix_apps_dir(char *dest, size_t dest_size, const char *prefix)
{
    return join_path(dest, dest_size, prefix, "apps");
}

static bool package_metadata_path(
    char *dest,
    size_t dest_size,
    const char *prefix,
    const char *package)
{
    char apps[VODKA_PATH_MAX];
    char package_dir[VODKA_PATH_MAX];

    if (!prefix_apps_dir(apps, sizeof(apps), prefix)) {
        return false;
    }
    if (!join_path(package_dir, sizeof(package_dir), apps, package)) {
        return false;
    }

    return join_path(dest, dest_size, package_dir, "metadata.conf");
}

static bool package_android_app_dir(
    char *dest,
    size_t dest_size,
    const char *android_root,
    const char *package)
{
    char data_app[VODKA_PATH_MAX];
    char app_slot[VODKA_PACKAGE_MAX + 3];

    if (snprintf(app_slot, sizeof(app_slot), "%s-1", package) >= (int)sizeof(app_slot)) {
        return false;
    }
    if (!join_path(data_app, sizeof(data_app), android_root, "data/app")) {
        return false;
    }

    return join_path(dest, dest_size, data_app, app_slot);
}

static bool package_android_data_dir(
    char *dest,
    size_t dest_size,
    const char *android_root,
    const char *package)
{
    char data_data[VODKA_PATH_MAX];

    if (!join_path(data_data, sizeof(data_data), android_root, "data/data")) {
        return false;
    }

    return join_path(dest, dest_size, data_data, package);
}

static bool inspect_apk(const char *path, struct apk_info *info)
{
    memset(info, 0, sizeof(*info));

    struct stat stat_buffer;
    if (stat(path, &stat_buffer) != 0) {
        fprintf(stderr, "failed to stat %s: %s\n", path, strerror(errno));
        return false;
    }
    if (!S_ISREG(stat_buffer.st_mode)) {
        fprintf(stderr, "not a regular file: %s\n", path);
        return false;
    }
    if (stat_buffer.st_size < 22) {
        fprintf(stderr, "not an APK/ZIP file: %s\n", path);
        return false;
    }

    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "failed to open %s: %s\n", path, strerror(errno));
        return false;
    }

    const size_t tail_size = stat_buffer.st_size > VODKA_ZIP_TAIL_MAX ?
        VODKA_ZIP_TAIL_MAX :
        (size_t)stat_buffer.st_size;
    unsigned char *tail = malloc(tail_size);
    if (tail == NULL) {
        fprintf(stderr, "out of memory while inspecting %s\n", path);
        fclose(file);
        return false;
    }

    bool ok = true;
    const long tail_offset = (long)stat_buffer.st_size - (long)tail_size;
    if (fseek(file, tail_offset, SEEK_SET) != 0 ||
        fread(tail, 1, tail_size, file) != tail_size) {
        fprintf(stderr, "failed to read ZIP footer from %s\n", path);
        free(tail);
        fclose(file);
        return false;
    }

    size_t eocd_offset = 0;
    bool found_eocd = false;
    if (tail_size >= 22) {
        for (size_t i = tail_size - 22 + 1; i > 0; i--) {
            const size_t pos = i - 1;
            if (read_le32(tail + pos) == 0x06054b50u) {
                eocd_offset = pos;
                found_eocd = true;
                break;
            }
        }
    }

    if (!found_eocd) {
        fprintf(stderr, "missing ZIP end-of-central-directory: %s\n", path);
        free(tail);
        fclose(file);
        return false;
    }

    const unsigned char *eocd = tail + eocd_offset;
    const uint16_t entry_count = read_le16(eocd + 10);
    const uint32_t central_size = read_le32(eocd + 12);
    const uint32_t central_offset = read_le32(eocd + 16);

    free(tail);

    if ((uint64_t)central_offset + (uint64_t)central_size > (uint64_t)stat_buffer.st_size) {
        fprintf(stderr, "invalid ZIP central directory: %s\n", path);
        fclose(file);
        return false;
    }

    if (fseek(file, (long)central_offset, SEEK_SET) != 0) {
        fprintf(stderr, "failed to seek ZIP central directory in %s\n", path);
        fclose(file);
        return false;
    }

    info->valid_zip = true;

    for (uint16_t i = 0; i < entry_count; i++) {
        unsigned char header[46];
        if (fread(header, 1, sizeof(header), file) != sizeof(header)) {
            fprintf(stderr, "failed to read ZIP central entry from %s\n", path);
            ok = false;
            break;
        }

        if (read_le32(header) != 0x02014b50u) {
            fprintf(stderr, "invalid ZIP central entry in %s\n", path);
            ok = false;
            break;
        }

        const uint16_t method = read_le16(header + 10);
        const uint32_t compressed_size = read_le32(header + 20);
        const uint32_t uncompressed_size = read_le32(header + 24);
        const uint16_t name_len = read_le16(header + 28);
        const uint16_t extra_len = read_le16(header + 30);
        const uint16_t comment_len = read_le16(header + 32);
        const uint32_t local_offset = read_le32(header + 42);

        char *name = malloc((size_t)name_len + 1);
        if (name == NULL) {
            fprintf(stderr, "out of memory while reading ZIP entry name\n");
            ok = false;
            break;
        }

        if (fread(name, 1, name_len, file) != name_len) {
            fprintf(stderr, "failed to read ZIP entry name from %s\n", path);
            free(name);
            ok = false;
            break;
        }
        name[name_len] = '\0';

        if (strcmp(name, "AndroidManifest.xml") == 0) {
            info->has_manifest = true;
            info->manifest_method = method;
            info->manifest_compressed_size = compressed_size;
            info->manifest_uncompressed_size = uncompressed_size;
            info->manifest_local_offset = local_offset;
        }

        free(name);

        if (fseek(file, (long)extra_len + (long)comment_len, SEEK_CUR) != 0) {
            fprintf(stderr, "failed to skip ZIP central entry data in %s\n", path);
            ok = false;
            break;
        }
    }

    fclose(file);

    if (ok && !info->has_manifest) {
        fprintf(stderr, "APK is missing AndroidManifest.xml: %s\n", path);
        return false;
    }

    return ok;
}

static bool read_manifest_entry(
    const char *apk_path,
    const struct apk_info *info,
    unsigned char **data_out,
    size_t *size_out)
{
    *data_out = NULL;
    *size_out = 0;

    if (!info->has_manifest) {
        return false;
    }
    if (info->manifest_uncompressed_size == 0 ||
        info->manifest_uncompressed_size > VODKA_MANIFEST_MAX) {
        return false;
    }
    if (info->manifest_method != VODKA_ZIP_METHOD_STORE &&
        info->manifest_method != VODKA_ZIP_METHOD_DEFLATE) {
        return false;
    }
    if (info->manifest_compressed_size == 0 ||
        info->manifest_compressed_size > VODKA_MANIFEST_MAX * 2u) {
        return false;
    }

    FILE *file = fopen(apk_path, "rb");
    if (file == NULL) {
        fprintf(stderr, "failed to open %s: %s\n", apk_path, strerror(errno));
        return false;
    }

    bool ok = true;
    if (fseek(file, (long)info->manifest_local_offset, SEEK_SET) != 0) {
        fclose(file);
        return false;
    }

    unsigned char local_header[30];
    if (fread(local_header, 1, sizeof(local_header), file) != sizeof(local_header) ||
        read_le32(local_header) != 0x04034b50u) {
        fclose(file);
        return false;
    }

    const uint16_t name_len = read_le16(local_header + 26);
    const uint16_t extra_len = read_le16(local_header + 28);
    if (fseek(file, (long)name_len + (long)extra_len, SEEK_CUR) != 0) {
        fclose(file);
        return false;
    }

    unsigned char *compressed = malloc((size_t)info->manifest_compressed_size);
    unsigned char *manifest = malloc((size_t)info->manifest_uncompressed_size + 1);
    if (compressed == NULL || manifest == NULL) {
        fprintf(stderr, "out of memory while reading manifest\n");
        free(compressed);
        free(manifest);
        fclose(file);
        return false;
    }

    if (fread(compressed, 1, info->manifest_compressed_size, file) != info->manifest_compressed_size) {
        ok = false;
    }
    fclose(file);

    if (ok && info->manifest_method == VODKA_ZIP_METHOD_STORE) {
        if (info->manifest_compressed_size != info->manifest_uncompressed_size) {
            ok = false;
        } else {
            memcpy(manifest, compressed, info->manifest_uncompressed_size);
        }
    } else if (ok && info->manifest_method == VODKA_ZIP_METHOD_DEFLATE) {
        z_stream stream;
        memset(&stream, 0, sizeof(stream));

        stream.next_in = compressed;
        stream.avail_in = info->manifest_compressed_size;
        stream.next_out = manifest;
        stream.avail_out = info->manifest_uncompressed_size;

        if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
            ok = false;
        } else {
            const int zstatus = inflate(&stream, Z_FINISH);
            if (zstatus != Z_STREAM_END ||
                stream.total_out != info->manifest_uncompressed_size) {
                ok = false;
            }
            if (inflateEnd(&stream) != Z_OK) {
                ok = false;
            }
        }
    }

    free(compressed);

    if (!ok) {
        free(manifest);
        return false;
    }

    manifest[info->manifest_uncompressed_size] = '\0';
    *data_out = manifest;
    *size_out = info->manifest_uncompressed_size;
    return true;
}

static char *find_substring(char *haystack, const char *needle)
{
    const size_t needle_len = strlen(needle);

    if (needle_len == 0) {
        return haystack;
    }

    for (char *cursor = haystack; *cursor != '\0'; cursor++) {
        if (strncmp(cursor, needle, needle_len) == 0) {
            return cursor;
        }
    }

    return NULL;
}

static bool contains_substring_before(const char *start, const char *limit, const char *needle)
{
    const size_t needle_len = strlen(needle);

    if (needle_len == 0) {
        return true;
    }

    for (const char *cursor = start; cursor < limit && *cursor != '\0'; cursor++) {
        if ((size_t)(limit - cursor) < needle_len) {
            return false;
        }
        if (strncmp(cursor, needle, needle_len) == 0) {
            return true;
        }
    }

    return false;
}

static bool read_plain_manifest(
    const char *apk_path,
    const struct apk_info *info,
    char **manifest_out)
{
    *manifest_out = NULL;

    unsigned char *manifest = NULL;
    size_t manifest_size = 0;
    if (!read_manifest_entry(apk_path, info, &manifest, &manifest_size)) {
        return false;
    }

    size_t start = 0;
    while (start < manifest_size && isspace(manifest[start])) {
        start++;
    }
    if (start >= manifest_size || manifest[start] != '<') {
        free(manifest);
        return false;
    }

    *manifest_out = (char *)manifest;
    return true;
}

static bool extract_quoted_attribute(
    const char *start,
    const char *limit,
    const char *attribute,
    char *dest,
    size_t dest_size)
{
    const size_t attribute_len = strlen(attribute);

    for (const char *cursor = start; cursor < limit && *cursor != '\0'; cursor++) {
        if ((size_t)(limit - cursor) < attribute_len ||
            strncmp(cursor, attribute, attribute_len) != 0) {
            continue;
        }
        if (cursor > start) {
            const unsigned char before = (unsigned char)cursor[-1];
            if (isalnum(before) || before == '_' || before == ':' || before == '-') {
                continue;
            }
        }

        cursor += attribute_len;
        while (cursor < limit && isspace((unsigned char)*cursor)) {
            cursor++;
        }
        if (cursor >= limit || *cursor != '=') {
            continue;
        }
        cursor++;
        while (cursor < limit && isspace((unsigned char)*cursor)) {
            cursor++;
        }
        if (cursor >= limit || (*cursor != '"' && *cursor != '\'')) {
            continue;
        }

        const char quote = *cursor++;
        const char *end = cursor;
        while (end < limit && *end != quote) {
            end++;
        }
        if (end >= limit) {
            return false;
        }

        const size_t len = (size_t)(end - cursor);
        if (len >= dest_size) {
            return false;
        }
        memcpy(dest, cursor, len);
        dest[len] = '\0';
        return true;
    }

    return false;
}

static bool parse_nonnegative_int(const char *value, int *dest)
{
    if (value == NULL || value[0] == '\0') {
        return false;
    }

    errno = 0;
    char *end = NULL;
    const long parsed = strtol(value, &end, 0);
    if (errno != 0 || end == value || *end != '\0' || parsed < 0 || parsed > 2147483647L) {
        return false;
    }

    *dest = (int)parsed;
    return true;
}

static void set_manifest_package(struct manifest_details *details, const char *package)
{
    if (!validate_package_name(package)) {
        return;
    }

    copy_string(details->package, sizeof(details->package), package);
    details->has_package = true;
}

static bool normalize_activity_name(
    char *dest,
    size_t dest_size,
    const char *package,
    const char *activity)
{
    if (activity == NULL || activity[0] == '\0') {
        return false;
    }

    if (activity[0] == '.') {
        if (package == NULL || package[0] == '\0') {
            return false;
        }

        const int written = snprintf(dest, dest_size, "%s%s", package, activity);
        return written >= 0 && (size_t)written < dest_size;
    }

    if (strchr(activity, '.') == NULL) {
        if (package == NULL || package[0] == '\0') {
            return false;
        }

        const int written = snprintf(dest, dest_size, "%s.%s", package, activity);
        return written >= 0 && (size_t)written < dest_size;
    }

    if (strlen(activity) >= dest_size) {
        return false;
    }

    copy_string(dest, dest_size, activity);
    return true;
}

static void set_manifest_launch_activity(struct manifest_details *details, const char *activity)
{
    if (details->has_launch_activity || !details->has_package) {
        return;
    }

    char normalized[VODKA_ACTIVITY_MAX];
    if (!normalize_activity_name(normalized, sizeof(normalized), details->package, activity)) {
        return;
    }

    copy_string(details->launch_activity, sizeof(details->launch_activity), normalized);
    details->has_launch_activity = true;
}

static bool read_axml_utf8_length(
    const unsigned char *data,
    size_t size,
    size_t *cursor,
    size_t *length)
{
    if (*cursor >= size) {
        return false;
    }

    unsigned char first = data[(*cursor)++];
    if ((first & 0x80u) == 0) {
        *length = first;
        return true;
    }

    if (*cursor >= size) {
        return false;
    }

    *length = ((size_t)(first & 0x7fu) << 8) | data[(*cursor)++];
    return true;
}

static bool read_axml_utf16_length(
    const unsigned char *data,
    size_t size,
    size_t *cursor,
    size_t *length)
{
    if (*cursor + 2 > size) {
        return false;
    }

    uint16_t first = read_le16(data + *cursor);
    *cursor += 2;
    if ((first & 0x8000u) == 0) {
        *length = first;
        return true;
    }

    if (*cursor + 2 > size) {
        return false;
    }

    const uint16_t second = read_le16(data + *cursor);
    *cursor += 2;
    *length = ((size_t)(first & 0x7fffu) << 16) | second;
    return true;
}

static bool append_utf8_codepoint(char *dest, size_t dest_size, size_t *used, uint32_t codepoint)
{
    unsigned char encoded[4];
    size_t encoded_len = 0;

    if (codepoint <= 0x7fu) {
        encoded[encoded_len++] = (unsigned char)codepoint;
    } else if (codepoint <= 0x7ffu) {
        encoded[encoded_len++] = (unsigned char)(0xc0u | (codepoint >> 6));
        encoded[encoded_len++] = (unsigned char)(0x80u | (codepoint & 0x3fu));
    } else if (codepoint <= 0xffffu) {
        encoded[encoded_len++] = (unsigned char)(0xe0u | (codepoint >> 12));
        encoded[encoded_len++] = (unsigned char)(0x80u | ((codepoint >> 6) & 0x3fu));
        encoded[encoded_len++] = (unsigned char)(0x80u | (codepoint & 0x3fu));
    } else if (codepoint <= 0x10ffffu) {
        encoded[encoded_len++] = (unsigned char)(0xf0u | (codepoint >> 18));
        encoded[encoded_len++] = (unsigned char)(0x80u | ((codepoint >> 12) & 0x3fu));
        encoded[encoded_len++] = (unsigned char)(0x80u | ((codepoint >> 6) & 0x3fu));
        encoded[encoded_len++] = (unsigned char)(0x80u | (codepoint & 0x3fu));
    } else {
        return false;
    }

    if (*used + encoded_len >= dest_size) {
        return false;
    }

    memcpy(dest + *used, encoded, encoded_len);
    *used += encoded_len;
    dest[*used] = '\0';
    return true;
}

static bool axml_string_pool_init(
    struct axml_string_pool *pool,
    const unsigned char *chunk,
    size_t chunk_size)
{
    if (chunk_size < 28 || read_le16(chunk) != VODKA_AXML_RES_STRING_POOL_TYPE) {
        return false;
    }

    const uint16_t header_size = read_le16(chunk + 2);
    if (header_size < 28 || header_size > chunk_size) {
        return false;
    }

    const uint32_t string_count = read_le32(chunk + 8);
    const uint32_t style_count = read_le32(chunk + 12);
    const uint32_t flags = read_le32(chunk + 16);
    const uint32_t strings_start = read_le32(chunk + 20);
    const size_t offsets_end = (size_t)header_size + ((size_t)string_count * 4u) + ((size_t)style_count * 4u);

    if (strings_start < offsets_end || strings_start >= chunk_size) {
        return false;
    }

    pool->chunk = chunk;
    pool->chunk_size = chunk_size;
    pool->string_count = string_count;
    pool->style_count = style_count;
    pool->strings_start = strings_start;
    pool->utf8 = (flags & VODKA_AXML_STRING_POOL_UTF8_FLAG) != 0;
    return true;
}

static bool axml_string_pool_get(
    const struct axml_string_pool *pool,
    uint32_t index,
    char *dest,
    size_t dest_size)
{
    if (dest_size == 0) {
        return false;
    }
    dest[0] = '\0';

    if (pool->chunk == NULL || index == VODKA_AXML_NO_INDEX || index >= pool->string_count) {
        return false;
    }

    const uint16_t header_size = read_le16(pool->chunk + 2);
    const size_t offset_position = (size_t)header_size + ((size_t)index * 4u);
    if (offset_position + 4 > pool->chunk_size) {
        return false;
    }

    const uint32_t string_offset = read_le32(pool->chunk + offset_position);
    const size_t string_start = (size_t)pool->strings_start + string_offset;
    if (string_start >= pool->chunk_size) {
        return false;
    }

    size_t cursor = string_start;
    if (pool->utf8) {
        size_t utf16_len = 0;
        size_t byte_len = 0;
        (void)utf16_len;
        if (!read_axml_utf8_length(pool->chunk, pool->chunk_size, &cursor, &utf16_len) ||
            !read_axml_utf8_length(pool->chunk, pool->chunk_size, &cursor, &byte_len) ||
            cursor + byte_len > pool->chunk_size ||
            byte_len >= dest_size) {
            return false;
        }

        memcpy(dest, pool->chunk + cursor, byte_len);
        dest[byte_len] = '\0';
        return true;
    }

    size_t char_count = 0;
    if (!read_axml_utf16_length(pool->chunk, pool->chunk_size, &cursor, &char_count) ||
        char_count > (pool->chunk_size - cursor) / 2u) {
        return false;
    }

    size_t used = 0;
    for (size_t i = 0; i < char_count; i++) {
        uint32_t codepoint = read_le16(pool->chunk + cursor + (i * 2u));

        if (codepoint >= 0xd800u && codepoint <= 0xdbffu && i + 1 < char_count) {
            const uint32_t low = read_le16(pool->chunk + cursor + ((i + 1u) * 2u));
            if (low >= 0xdc00u && low <= 0xdfffu) {
                codepoint = 0x10000u + (((codepoint - 0xd800u) << 10) | (low - 0xdc00u));
                i++;
            }
        }

        if (!append_utf8_codepoint(dest, dest_size, &used, codepoint)) {
            return false;
        }
    }

    return true;
}

static bool axml_attribute_string_value(
    const struct axml_string_pool *pool,
    const unsigned char *attribute,
    char *dest,
    size_t dest_size)
{
    const uint32_t raw_value = read_le32(attribute + 8);
    const uint8_t value_type = attribute[15];
    const uint32_t value_data = read_le32(attribute + 16);

    if (raw_value != VODKA_AXML_NO_INDEX) {
        return axml_string_pool_get(pool, raw_value, dest, dest_size);
    }

    if (value_type == VODKA_AXML_TYPE_STRING) {
        return axml_string_pool_get(pool, value_data, dest, dest_size);
    }

    if (value_type == VODKA_AXML_TYPE_INT_DEC || value_type == VODKA_AXML_TYPE_INT_HEX) {
        const int written = snprintf(dest, dest_size, "%u", value_data);
        return written >= 0 && (size_t)written < dest_size;
    }

    return false;
}

static bool axml_attribute_int_value(
    const struct axml_string_pool *pool,
    const unsigned char *attribute,
    int *dest)
{
    const uint8_t value_type = attribute[15];
    const uint32_t value_data = read_le32(attribute + 16);

    if (value_type == VODKA_AXML_TYPE_INT_DEC || value_type == VODKA_AXML_TYPE_INT_HEX) {
        if (value_data > 2147483647u) {
            return false;
        }
        *dest = (int)value_data;
        return true;
    }

    char value[64];
    if (!axml_attribute_string_value(pool, attribute, value, sizeof(value))) {
        return false;
    }

    return parse_nonnegative_int(value, dest);
}

static bool parse_binary_manifest_start_element(
    struct manifest_details *details,
    const struct axml_string_pool *pool,
    const unsigned char *chunk,
    size_t chunk_size,
    size_t depth,
    size_t *component_depth,
    size_t *filter_depth,
    bool *filter_has_main_action,
    bool *filter_has_launcher_category,
    char *component_activity,
    size_t component_activity_size)
{
    if (chunk_size < 36) {
        return false;
    }

    const uint32_t tag_index = read_le32(chunk + 20);
    const uint16_t attribute_start = read_le16(chunk + 24);
    const uint16_t attribute_size = read_le16(chunk + 26);
    const uint16_t attribute_count = read_le16(chunk + 28);
    const size_t attribute_base = 16u + attribute_start;
    char tag[128];

    if (attribute_size < 20 ||
        attribute_base > chunk_size ||
        attribute_base + ((size_t)attribute_count * attribute_size) > chunk_size ||
        !axml_string_pool_get(pool, tag_index, tag, sizeof(tag))) {
        return false;
    }

    if (strcmp(tag, "intent-filter") == 0 && *component_depth != 0) {
        *filter_depth = depth;
        *filter_has_main_action = false;
        *filter_has_launcher_category = false;
    }

    bool is_component = strcmp(tag, "activity") == 0 || strcmp(tag, "activity-alias") == 0;
    if (is_component) {
        *component_depth = depth;
        *filter_depth = 0;
        component_activity[0] = '\0';
    }

    for (uint16_t i = 0; i < attribute_count; i++) {
        const unsigned char *attribute = chunk + attribute_base + ((size_t)i * attribute_size);
        const uint32_t name_index = read_le32(attribute + 4);
        char attr_name[128];

        if (!axml_string_pool_get(pool, name_index, attr_name, sizeof(attr_name))) {
            continue;
        }

        if (strcmp(tag, "manifest") == 0 && strcmp(attr_name, "package") == 0) {
            char value[VODKA_PACKAGE_MAX];
            if (axml_attribute_string_value(pool, attribute, value, sizeof(value))) {
                set_manifest_package(details, value);
            }
            continue;
        }

        if (strcmp(tag, "uses-permission") == 0 && strcmp(attr_name, "name") == 0) {
            char value[VODKA_PERMISSION_MAX];
            if (axml_attribute_string_value(pool, attribute, value, sizeof(value)) &&
                !add_manifest_permission(&details->permissions, value)) {
                return false;
            }
            continue;
        }

        if (strcmp(tag, "uses-sdk") == 0 && strcmp(attr_name, "minSdkVersion") == 0) {
            int value = 0;
            if (axml_attribute_int_value(pool, attribute, &value)) {
                details->min_sdk = value;
                details->has_min_sdk = true;
            }
            continue;
        }

        if (strcmp(tag, "uses-sdk") == 0 && strcmp(attr_name, "targetSdkVersion") == 0) {
            int value = 0;
            if (axml_attribute_int_value(pool, attribute, &value)) {
                details->target_sdk = value;
                details->has_target_sdk = true;
            }
            continue;
        }

        if (is_component && strcmp(attr_name, "name") == 0) {
            char value[VODKA_ACTIVITY_MAX];
            if (axml_attribute_string_value(pool, attribute, value, sizeof(value)) &&
                normalize_activity_name(component_activity, component_activity_size, details->package, value)) {
                continue;
            }
        }

        if (*filter_depth != 0 &&
            strcmp(tag, "action") == 0 &&
            strcmp(attr_name, "name") == 0) {
            char value[VODKA_VALUE_MAX];
            if (axml_attribute_string_value(pool, attribute, value, sizeof(value)) &&
                strcmp(value, "android.intent.action.MAIN") == 0) {
                *filter_has_main_action = true;
            }
            continue;
        }

        if (*filter_depth != 0 &&
            strcmp(tag, "category") == 0 &&
            strcmp(attr_name, "name") == 0) {
            char value[VODKA_VALUE_MAX];
            if (axml_attribute_string_value(pool, attribute, value, sizeof(value)) &&
                strcmp(value, "android.intent.category.LAUNCHER") == 0) {
                *filter_has_launcher_category = true;
            }
            continue;
        }
    }

    return true;
}

static bool parse_binary_manifest_details(
    const char *apk_path,
    const struct apk_info *info,
    struct manifest_details *details)
{
    unsigned char *manifest = NULL;
    size_t manifest_size = 0;
    if (!read_manifest_entry(apk_path, info, &manifest, &manifest_size)) {
        return false;
    }

    if (manifest_size < 8 ||
        read_le16(manifest) != VODKA_AXML_RES_XML_TYPE ||
        read_le16(manifest + 2) < 8 ||
        read_le32(manifest + 4) > manifest_size) {
        free(manifest);
        return false;
    }

    const size_t xml_size = read_le32(manifest + 4);
    struct axml_string_pool pool = {0};
    bool found_string_pool = false;

    size_t offset = read_le16(manifest + 2);
    while (offset + 8 <= xml_size) {
        const unsigned char *chunk = manifest + offset;
        const uint16_t type = read_le16(chunk);
        const uint16_t header_size = read_le16(chunk + 2);
        const uint32_t chunk_size = read_le32(chunk + 4);

        if (header_size < 8 || chunk_size < header_size || offset + chunk_size > xml_size) {
            free(manifest);
            return false;
        }

        if (type == VODKA_AXML_RES_STRING_POOL_TYPE) {
            found_string_pool = axml_string_pool_init(&pool, chunk, chunk_size);
            break;
        }

        offset += chunk_size;
    }

    if (!found_string_pool) {
        free(manifest);
        return false;
    }

    details->parsed = true;
    copy_string(details->format, sizeof(details->format), "binary");
    details->target_sdk = VODKA_DEFAULT_TARGET_SDK;

    size_t depth = 0;
    size_t component_depth = 0;
    size_t filter_depth = 0;
    bool filter_has_main_action = false;
    bool filter_has_launcher_category = false;
    char component_activity[VODKA_ACTIVITY_MAX];
    component_activity[0] = '\0';

    offset = read_le16(manifest + 2);
    while (offset + 8 <= xml_size) {
        const unsigned char *chunk = manifest + offset;
        const uint16_t type = read_le16(chunk);
        const uint16_t header_size = read_le16(chunk + 2);
        const uint32_t chunk_size = read_le32(chunk + 4);

        if (header_size < 8 || chunk_size < header_size || offset + chunk_size > xml_size) {
            free(manifest);
            return false;
        }

        if (type == VODKA_AXML_START_ELEMENT_TYPE) {
            depth++;
            if (!parse_binary_manifest_start_element(
                    details,
                    &pool,
                    chunk,
                    chunk_size,
                    depth,
                    &component_depth,
                    &filter_depth,
                    &filter_has_main_action,
                    &filter_has_launcher_category,
                    component_activity,
                    sizeof(component_activity))) {
                free(manifest);
                return false;
            }
        } else if (type == VODKA_AXML_END_ELEMENT_TYPE) {
            char tag[128];
            if (chunk_size >= 24 &&
                axml_string_pool_get(&pool, read_le32(chunk + 20), tag, sizeof(tag))) {
                if (filter_depth == depth && strcmp(tag, "intent-filter") == 0) {
                    if (!details->has_launch_activity &&
                        filter_has_main_action &&
                        filter_has_launcher_category &&
                        component_activity[0] != '\0') {
                        copy_string(details->launch_activity, sizeof(details->launch_activity), component_activity);
                        details->has_launch_activity = true;
                    }
                    filter_depth = 0;
                    filter_has_main_action = false;
                    filter_has_launcher_category = false;
                }

                if (component_depth == depth &&
                    (strcmp(tag, "activity") == 0 || strcmp(tag, "activity-alias") == 0)) {
                    component_depth = 0;
                    component_activity[0] = '\0';
                }
            }

            if (depth > 0) {
                depth--;
            }
        }

        offset += chunk_size;
    }

    free(manifest);
    return true;
}

static bool extract_plain_manifest_details(
    const char *apk_path,
    const struct apk_info *info,
    struct manifest_details *details)
{
    char *manifest = NULL;
    if (!read_plain_manifest(apk_path, info, &manifest)) {
        return false;
    }

    details->parsed = true;
    copy_string(details->format, sizeof(details->format), "text");
    details->target_sdk = VODKA_DEFAULT_TARGET_SDK;

    char *package_key = find_substring(manifest, "package");
    while (package_key != NULL) {
        char *cursor = package_key + strlen("package");
        while (isspace((unsigned char)*cursor)) {
            cursor++;
        }
        if (*cursor == '=') {
            cursor++;
            while (isspace((unsigned char)*cursor)) {
                cursor++;
            }
            if (*cursor == '"' || *cursor == '\'') {
                const char quote = *cursor++;
                char *end = strchr(cursor, quote);
                if (end != NULL) {
                    char value[VODKA_PACKAGE_MAX];
                    const size_t len = (size_t)(end - cursor);
                    if (len < sizeof(value)) {
                        memcpy(value, cursor, len);
                        value[len] = '\0';
                        set_manifest_package(details, value);
                    }
                }
                break;
            }
        }

        package_key = find_substring(package_key + 1, "package");
    }

    bool ok = true;
    char *tag = find_substring(manifest, "<uses-permission");
    while (tag != NULL) {
        char *tag_end = strchr(tag, '>');
        if (tag_end == NULL) {
            break;
        }

        char permission[VODKA_PERMISSION_MAX];
        if (extract_quoted_attribute(tag, tag_end, "android:name", permission, sizeof(permission)) ||
            extract_quoted_attribute(tag, tag_end, "name", permission, sizeof(permission))) {
            if (!add_manifest_permission(&details->permissions, permission)) {
                ok = false;
                break;
            }
        }

        tag = find_substring(tag_end + 1, "<uses-permission");
    }

    tag = find_substring(manifest, "<uses-sdk");
    if (tag != NULL) {
        char *tag_end = strchr(tag, '>');
        if (tag_end != NULL) {
            char value[64];
            int parsed = 0;

            if ((extract_quoted_attribute(tag, tag_end, "android:minSdkVersion", value, sizeof(value)) ||
                    extract_quoted_attribute(tag, tag_end, "minSdkVersion", value, sizeof(value))) &&
                parse_nonnegative_int(value, &parsed)) {
                details->min_sdk = parsed;
                details->has_min_sdk = true;
            }

            if ((extract_quoted_attribute(tag, tag_end, "android:targetSdkVersion", value, sizeof(value)) ||
                    extract_quoted_attribute(tag, tag_end, "targetSdkVersion", value, sizeof(value))) &&
                parse_nonnegative_int(value, &parsed)) {
                details->target_sdk = parsed;
                details->has_target_sdk = true;
            }
        }
    }

    tag = find_substring(manifest, "<activity");
    while (tag != NULL && details->has_package && !details->has_launch_activity) {
        char *tag_end = strchr(tag, '>');
        if (tag_end == NULL) {
            break;
        }

        char activity[VODKA_ACTIVITY_MAX];
        if (!(extract_quoted_attribute(tag, tag_end, "android:name", activity, sizeof(activity)) ||
                extract_quoted_attribute(tag, tag_end, "name", activity, sizeof(activity)))) {
            tag = find_substring(tag_end + 1, "<activity");
            continue;
        }

        char *activity_end = find_substring(tag_end + 1, "</activity");
        if (activity_end == NULL) {
            break;
        }

        char *filter = find_substring(tag_end + 1, "<intent-filter");
        while (filter != NULL && filter < activity_end) {
            char *filter_end = find_substring(filter, "</intent-filter");
            if (filter_end == NULL || filter_end > activity_end) {
                break;
            }

            if (contains_substring_before(filter, filter_end, "android.intent.action.MAIN") &&
                contains_substring_before(filter, filter_end, "android.intent.category.LAUNCHER")) {
                set_manifest_launch_activity(details, activity);
                break;
            }

            filter = find_substring(filter_end + 1, "<intent-filter");
        }

        tag = find_substring(activity_end + 1, "<activity");
    }

    free(manifest);
    return ok;
}

static void init_manifest_details(struct manifest_details *details)
{
    memset(details, 0, sizeof(*details));
    details->target_sdk = VODKA_DEFAULT_TARGET_SDK;
    copy_string(details->format, sizeof(details->format), "unknown");
}

static bool extract_manifest_details(
    const char *apk_path,
    const struct apk_info *info,
    struct manifest_details *details)
{
    struct manifest_details parsed;

    init_manifest_details(details);

    init_manifest_details(&parsed);
    if (parse_binary_manifest_details(apk_path, info, &parsed)) {
        *details = parsed;
        return true;
    }

    init_manifest_details(&parsed);
    if (extract_plain_manifest_details(apk_path, info, &parsed)) {
        *details = parsed;
        return true;
    }

    return true;
}

static bool setup_android_root(const char *root)
{
    bool ok = mkdir_p(root, 0755);

    for (size_t i = 0; i < sizeof(android_root_dirs) / sizeof(android_root_dirs[0]); i++) {
        char path[VODKA_PATH_MAX];
        if (!join_path(path, sizeof(path), root, android_root_dirs[i])) {
            fprintf(stderr, "path too long: %s/%s\n", root, android_root_dirs[i]);
            ok = false;
            continue;
        }

        if (!mkdir_p(path, 0755)) {
            ok = false;
        }
    }

    for (size_t i = 0; i < sizeof(android_root_files) / sizeof(android_root_files[0]); i++) {
        char path[VODKA_PATH_MAX];
        if (!join_path(path, sizeof(path), root, android_root_files[i].relative)) {
            fprintf(stderr, "path too long: %s/%s\n", root, android_root_files[i].relative);
            ok = false;
            continue;
        }

        if (!write_file_if_missing(path, android_root_files[i].content)) {
            ok = false;
        }
    }

    return ok;
}

static bool setup_prefix(const char *prefix)
{
    bool ok = mkdir_p(prefix, 0755);

    for (size_t i = 0; i < sizeof(prefix_dirs) / sizeof(prefix_dirs[0]); i++) {
        char path[VODKA_PATH_MAX];
        if (!join_path(path, sizeof(path), prefix, prefix_dirs[i])) {
            fprintf(stderr, "path too long: %s/%s\n", prefix, prefix_dirs[i]);
            ok = false;
            continue;
        }

        if (!mkdir_p(path, 0755)) {
            ok = false;
        }
    }

    char config_path[VODKA_PATH_MAX];
    if (!join_path(config_path, sizeof(config_path), prefix, "config/vodka.conf")) {
        fprintf(stderr, "path too long: %s/config/vodka.conf\n", prefix);
        return false;
    }

    char config_content[VODKA_PATH_MAX + 256];
    const int written = snprintf(
        config_content,
        sizeof(config_content),
        "# Vodka prefix configuration\n"
        "prefix.version=%s\n"
        "android.root=android_root\n"
        "android.data=android_root/data\n"
        "runtime.abi=x86_64\n"
        "runtime.backend=none\n"
        "runtime.exec=\n"
        "runtime.exec.args=package activity apk data_dir\n"
        "runtime.app_process=\n"
        "runtime.entrypoint=android.app.ActivityThread\n"
        "runtime.classpath=\n"
        "runtime.binder.device=/dev/binder\n",
        VODKA_PREFIX_VERSION);

    if (written < 0 || (size_t)written >= sizeof(config_content)) {
        fprintf(stderr, "prefix config is too large\n");
        ok = false;
    } else if (!write_file_if_missing(config_path, config_content)) {
        ok = false;
    }

    char version_path[VODKA_PATH_MAX];
    if (!join_path(version_path, sizeof(version_path), prefix, "VERSION")) {
        fprintf(stderr, "path too long: %s/VERSION\n", prefix);
        ok = false;
    } else if (!write_file_if_missing(version_path, VODKA_PREFIX_VERSION "\n")) {
        ok = false;
    }

    char root_path[VODKA_PATH_MAX];
    if (!join_path(root_path, sizeof(root_path), prefix, "android_root")) {
        fprintf(stderr, "path too long: %s/android_root\n", prefix);
        ok = false;
    } else if (!setup_android_root(root_path)) {
        ok = false;
    }

    return ok;
}

static struct vodka_property *find_property(struct vodka_properties *properties, const char *key)
{
    for (size_t i = 0; i < properties->count; i++) {
        if (strcmp(properties->items[i].key, key) == 0) {
            return &properties->items[i];
        }
    }

    return NULL;
}

static const char *property_value(
    struct vodka_properties *properties,
    const char *key,
    const char *fallback)
{
    struct vodka_property *property = find_property(properties, key);

    if (property == NULL || property->value[0] == '\0') {
        return fallback;
    }

    return property->value;
}

static bool put_property(
    struct vodka_properties *properties,
    const char *key,
    const char *value,
    const char *source)
{
    if (strlen(key) >= VODKA_KEY_MAX) {
        fprintf(stderr, "property key too long in %s: %s\n", source, key);
        return false;
    }

    if (strlen(value) >= VODKA_VALUE_MAX) {
        fprintf(stderr, "property value too long in %s: %s\n", source, key);
        return false;
    }

    struct vodka_property *property = find_property(properties, key);
    if (property == NULL) {
        if (properties->count >= VODKA_MAX_PROPERTIES) {
            fprintf(stderr, "too many properties; increase VODKA_MAX_PROPERTIES\n");
            return false;
        }

        property = &properties->items[properties->count++];
        copy_string(property->key, sizeof(property->key), key);
    }

    copy_string(property->value, sizeof(property->value), value);
    copy_string(property->source, sizeof(property->source), source);
    return true;
}

static bool load_property_file(
    struct vodka_properties *properties,
    const char *path,
    const char *source_name)
{
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        fprintf(stderr, "failed to open %s: %s\n", path, strerror(errno));
        return false;
    }

    bool ok = true;
    char line[1024];
    size_t line_number = 0;

    while (fgets(line, sizeof(line), file) != NULL) {
        line_number++;

        char *entry = trim(line);
        if (*entry == '\0' || *entry == '#') {
            continue;
        }

        char *equals = strchr(entry, '=');
        if (equals == NULL) {
            fprintf(stderr, "invalid property line in %s:%zu\n", path, line_number);
            ok = false;
            continue;
        }

        *equals = '\0';
        char *key = trim(entry);
        char *value = trim(equals + 1);

        if (*key == '\0') {
            fprintf(stderr, "empty property key in %s:%zu\n", path, line_number);
            ok = false;
            continue;
        }

        if (!put_property(properties, key, value, source_name)) {
            ok = false;
        }
    }

    if (ferror(file)) {
        fprintf(stderr, "failed to read %s\n", path);
        ok = false;
    }

    fclose(file);
    return ok;
}

static bool require_directory(const char *root, const char *relative)
{
    char path[VODKA_PATH_MAX];
    if (!join_path(path, sizeof(path), root, relative)) {
        fprintf(stderr, "path too long: %s/%s\n", root, relative);
        return false;
    }

    if (!is_directory(path)) {
        fprintf(stderr, "missing directory %s\n", path);
        return false;
    }

    return true;
}

static bool load_root_properties(const char *root, struct vodka_properties *properties)
{
    static const struct {
        const char *relative;
        const char *source;
    } files[] = {
        {"default.prop", "default.prop"},
        {"system/build.prop", "system/build.prop"},
        {"vendor/build.prop", "vendor/build.prop"},
        {"product/build.prop", "product/build.prop"},
    };

    bool ok = true;

    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
        char path[VODKA_PATH_MAX];
        if (!join_path(path, sizeof(path), root, files[i].relative)) {
            fprintf(stderr, "path too long: %s/%s\n", root, files[i].relative);
            ok = false;
            continue;
        }

        if (!load_property_file(properties, path, files[i].source)) {
            ok = false;
        }
    }

    return ok;
}

static bool print_required_property(struct vodka_properties *properties, const char *key)
{
    struct vodka_property *property = find_property(properties, key);
    if (property == NULL) {
        fprintf(stderr, "missing property: %s\n", key);
        return false;
    }

    printf("property %s=%s (%s)\n", property->key, property->value, property->source);
    return true;
}

static bool validate_android_root(const char *root, struct vodka_properties *properties)
{
    bool ok = true;

    static const char *const required_dirs[] = {
        "system",
        "system/bin",
        "system/lib64",
        "vendor",
        "product",
        "data",
        "data/app",
        "data/data",
        "dev",
        "proc",
        "sys",
    };

    for (size_t i = 0; i < sizeof(required_dirs) / sizeof(required_dirs[0]); i++) {
        if (!require_directory(root, required_dirs[i])) {
            ok = false;
        }
    }

    if (!load_root_properties(root, properties)) {
        ok = false;
    }

    if (find_property(properties, "ro.vodka.root") == NULL) {
        fprintf(stderr, "missing property: ro.vodka.root\n");
        ok = false;
    }
    if (find_property(properties, "ro.build.version.sdk") == NULL) {
        fprintf(stderr, "missing property: ro.build.version.sdk\n");
        ok = false;
    }
    if (find_property(properties, "ro.product.cpu.abilist") == NULL) {
        fprintf(stderr, "missing property: ro.product.cpu.abilist\n");
        ok = false;
    }

    return ok;
}

static bool prefix_android_root(char *dest, size_t dest_size, const char *prefix)
{
    return join_path(dest, dest_size, prefix, "android_root");
}

static bool prefix_config_path(char *dest, size_t dest_size, const char *prefix)
{
    return join_path(dest, dest_size, prefix, "config/vodka.conf");
}

static bool load_prefix_config(const char *prefix, struct vodka_properties *config)
{
    char config_path[VODKA_PATH_MAX];

    if (!prefix_config_path(config_path, sizeof(config_path), prefix)) {
        fprintf(stderr, "path too long: %s/config/vodka.conf\n", prefix);
        return false;
    }

    if (!path_exists(config_path)) {
        return true;
    }

    return load_property_file(config, config_path, "config/vodka.conf");
}

static bool save_property_file(
    const char *path,
    const char *heading,
    const struct vodka_properties *properties)
{
    if (!ensure_parent_dir(path)) {
        return false;
    }

    FILE *file = fopen(path, "w");
    if (file == NULL) {
        fprintf(stderr, "failed to create %s: %s\n", path, strerror(errno));
        return false;
    }

    if (heading != NULL && heading[0] != '\0') {
        fprintf(file, "%s\n", heading);
    }

    for (size_t i = 0; i < properties->count; i++) {
        fprintf(file, "%s=%s\n", properties->items[i].key, properties->items[i].value);
    }

    if (fclose(file) != 0) {
        fprintf(stderr, "failed to close %s: %s\n", path, strerror(errno));
        return false;
    }

    return true;
}

static const char *first_non_empty3(const char *first, const char *second, const char *third)
{
    if (first != NULL && first[0] != '\0') {
        return first;
    }
    if (second != NULL && second[0] != '\0') {
        return second;
    }
    if (third != NULL && third[0] != '\0') {
        return third;
    }

    return NULL;
}

static const char *runtime_backend_name(
    struct vodka_properties *config,
    const char *backend_override)
{
    const char *env_backend = getenv("VODKA_RUNTIME_BACKEND");
    const char *configured = property_value(config, "runtime.backend", "none");
    const char *selected = first_non_empty3(backend_override, env_backend, configured);

    return selected == NULL ? "none" : selected;
}

static const char *runtime_exec_path(
    struct vodka_properties *config,
    const char *exec_override)
{
    const char *env_exec = getenv("VODKA_RUNTIME_EXEC");
    const char *configured = property_value(config, "runtime.exec", NULL);

    return first_non_empty3(exec_override, env_exec, configured);
}

static const char *runtime_app_process_configured_path(
    struct vodka_properties *config,
    const char *exec_override)
{
    const char *env_path = getenv("VODKA_APP_PROCESS");
    const char *configured = property_value(config, "runtime.app_process", NULL);

    return first_non_empty3(exec_override, env_path, configured);
}

static bool resolve_default_app_process_path(
    char *dest,
    size_t dest_size,
    const char *android_root)
{
    char candidate[VODKA_PATH_MAX];

    if (join_path(candidate, sizeof(candidate), android_root, "system/bin/app_process64") &&
        path_exists(candidate)) {
        copy_string(dest, dest_size, candidate);
        return strlen(candidate) < dest_size;
    }

    if (join_path(candidate, sizeof(candidate), android_root, "system/bin/app_process") &&
        path_exists(candidate)) {
        copy_string(dest, dest_size, candidate);
        return strlen(candidate) < dest_size;
    }

    dest[0] = '\0';
    return true;
}

static const char *runtime_entrypoint(
    struct vodka_properties *config,
    const char *entrypoint_override)
{
    const char *env_entrypoint = getenv("VODKA_APP_PROCESS_ENTRYPOINT");
    const char *configured = property_value(config, "runtime.entrypoint", "android.app.ActivityThread");
    const char *selected = first_non_empty3(entrypoint_override, env_entrypoint, configured);

    return selected == NULL ? "android.app.ActivityThread" : selected;
}

static const char *runtime_classpath(
    struct vodka_properties *config,
    const char *classpath_override)
{
    const char *env_classpath = getenv("VODKA_CLASSPATH");
    const char *configured = property_value(config, "runtime.classpath", NULL);

    return first_non_empty3(classpath_override, env_classpath, configured);
}

static const char *runtime_binder_device(
    struct vodka_properties *config,
    const char *binder_override)
{
    const char *env_binder = getenv("VODKA_BINDER_DEVICE");
    const char *configured = property_value(config, "runtime.binder.device", "/dev/binder");
    const char *selected = first_non_empty3(binder_override, env_binder, configured);

    return selected == NULL ? "/dev/binder" : selected;
}

static bool binder_device_ready(const char *path)
{
    struct stat stat_buffer;

    if (path == NULL || path[0] == '\0') {
        return false;
    }
    if (stat(path, &stat_buffer) != 0) {
        return false;
    }

    return S_ISCHR(stat_buffer.st_mode) || S_ISREG(stat_buffer.st_mode);
}

static const struct service_bridge *find_service_bridge(const char *name)
{
    for (size_t i = 0; i < sizeof(service_bridges) / sizeof(service_bridges[0]); i++) {
        if (strcmp(service_bridges[i].name, name) == 0) {
            return &service_bridges[i];
        }
    }

    return NULL;
}

static bool prefix_services_config_path(char *dest, size_t dest_size, const char *prefix)
{
    return join_path(dest, dest_size, prefix, "config/services.conf");
}

static bool android_services_state_path(char *dest, size_t dest_size, const char *prefix)
{
    char android_root[VODKA_PATH_MAX];

    if (!prefix_android_root(android_root, sizeof(android_root), prefix)) {
        return false;
    }

    return join_path(dest, dest_size, android_root, "data/system/vodka-service-bridges.conf");
}

static bool load_services_config(const char *prefix, struct vodka_properties *config)
{
    char path[VODKA_PATH_MAX];

    if (!prefix_services_config_path(path, sizeof(path), prefix)) {
        fprintf(stderr, "path too long: %s/config/services.conf\n", prefix);
        return false;
    }

    if (!path_exists(path)) {
        return true;
    }

    return load_property_file(config, path, "config/services.conf");
}

static bool service_exec_key(char *dest, size_t dest_size, const char *service)
{
    const int written = snprintf(dest, dest_size, "service.%s.exec", service);
    return written >= 0 && (size_t)written < dest_size;
}

static const char *service_exec_path(struct vodka_properties *config, const char *service)
{
    char key[VODKA_KEY_MAX];

    if (!service_exec_key(key, sizeof(key), service)) {
        return NULL;
    }

    return property_value(config, key, NULL);
}

static bool save_services_config(const char *prefix, struct vodka_properties *config)
{
    char path[VODKA_PATH_MAX];

    if (!prefix_services_config_path(path, sizeof(path), prefix)) {
        fprintf(stderr, "path too long: %s/config/services.conf\n", prefix);
        return false;
    }

    return save_property_file(path, "# Vodka service bridge configuration", config);
}

static bool write_service_bridge_state(
    const char *prefix,
    const char *binder,
    struct vodka_properties *services_config)
{
    char path[VODKA_PATH_MAX];

    if (!android_services_state_path(path, sizeof(path), prefix)) {
        fprintf(stderr, "service bridge state path is too long\n");
        return false;
    }
    if (!ensure_parent_dir(path)) {
        return false;
    }

    FILE *file = fopen(path, "w");
    if (file == NULL) {
        fprintf(stderr, "failed to create %s: %s\n", path, strerror(errno));
        return false;
    }

    fprintf(
        file,
        "# Vodka service bridge state\n"
        "binder.device=%s\n"
        "binder.ready=%s\n",
        binder == NULL ? "" : binder,
        binder_device_ready(binder) ? "yes" : "no");

    for (size_t i = 0; i < sizeof(service_bridges) / sizeof(service_bridges[0]); i++) {
        const char *exec_path = service_exec_path(services_config, service_bridges[i].name);
        fprintf(file, "service.%s.required=%s\n", service_bridges[i].name, service_bridges[i].required ? "yes" : "no");
        fprintf(file, "service.%s.binder=%s\n", service_bridges[i].name, service_bridges[i].binder_name);
        fprintf(file, "service.%s.exec=%s\n", service_bridges[i].name, exec_path == NULL ? "" : exec_path);
        fprintf(
            file,
            "service.%s.ready=%s\n",
            service_bridges[i].name,
            exec_path != NULL && access(exec_path, X_OK) == 0 ? "yes" : "no");
    }

    if (fclose(file) != 0) {
        fprintf(stderr, "failed to close %s: %s\n", path, strerror(errno));
        return false;
    }

    return true;
}

static const char *binder_device_kind(const char *path)
{
    struct stat stat_buffer;

    if (path == NULL || path[0] == '\0') {
        return "missing";
    }
    if (stat(path, &stat_buffer) != 0) {
        return "missing";
    }
    if (S_ISCHR(stat_buffer.st_mode)) {
        return "char";
    }
    if (S_ISREG(stat_buffer.st_mode)) {
        return "regular";
    }

    return "unsupported";
}

static bool write_binder_compat_state(const char *prefix, const char *binder)
{
    char android_root[VODKA_PATH_MAX];
    char binder_config[VODKA_PATH_MAX];
    char service_state[VODKA_PATH_MAX];

    if (!prefix_android_root(android_root, sizeof(android_root), prefix) ||
        !join_path(binder_config, sizeof(binder_config), prefix, "config/binder.conf") ||
        !join_path(service_state, sizeof(service_state), android_root, "data/system/vodka-binder-services.conf")) {
        fprintf(stderr, "binder state path is too long\n");
        return false;
    }

    if (!ensure_parent_dir(binder_config) || !ensure_parent_dir(service_state)) {
        return false;
    }

    FILE *binder_file = fopen(binder_config, "w");
    FILE *service_file = fopen(service_state, "w");
    if (binder_file == NULL || service_file == NULL) {
        fprintf(stderr, "failed to create Binder compatibility state: %s\n", strerror(errno));
        if (binder_file != NULL) {
            fclose(binder_file);
        }
        if (service_file != NULL) {
            fclose(service_file);
        }
        return false;
    }

    FILE *files[] = {binder_file, service_file};
    for (size_t file_index = 0; file_index < sizeof(files) / sizeof(files[0]); file_index++) {
        FILE *file = files[file_index];
        fprintf(
            file,
            "# Vodka Binder compatibility state\n"
            "binder.device=%s\n"
            "binder.ready=%s\n"
            "binder.kind=%s\n",
            binder == NULL ? "" : binder,
            binder_device_ready(binder) ? "yes" : "no",
            binder_device_kind(binder));

        for (size_t i = 0; i < sizeof(service_bridges) / sizeof(service_bridges[0]); i++) {
            fprintf(
                file,
                "service.%s=%s\n",
                service_bridges[i].name,
                service_bridges[i].required ? "required" : "optional");
            fprintf(file, "service.%s.binder=%s\n", service_bridges[i].name, service_bridges[i].binder_name);
        }
    }

    bool ok = true;
    if (fclose(binder_file) != 0) {
        ok = false;
    }
    if (fclose(service_file) != 0) {
        ok = false;
    }
    if (!ok) {
        fprintf(stderr, "failed to close Binder compatibility state files\n");
    }

    return ok;
}

static bool validate_runtime_backend_name(const char *backend)
{
    return strcmp(backend, "none") == 0 ||
        strcmp(backend, "exec") == 0 ||
        strcmp(backend, "app_process") == 0;
}

static int compare_package_names(const void *left, const void *right)
{
    return strcmp((const char *)left, (const char *)right);
}

static size_t collect_installed_packages(
    const char *prefix,
    char packages[VODKA_MAX_APPS][VODKA_PACKAGE_MAX])
{
    char apps_dir[VODKA_PATH_MAX];
    if (!prefix_apps_dir(apps_dir, sizeof(apps_dir), prefix)) {
        return 0;
    }

    DIR *dir = opendir(apps_dir);
    if (dir == NULL) {
        return 0;
    }

    size_t count = 0;
    struct dirent *entry = NULL;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }
        if (!validate_package_name(entry->d_name)) {
            continue;
        }

        char metadata[VODKA_PATH_MAX];
        if (!package_metadata_path(metadata, sizeof(metadata), prefix, entry->d_name)) {
            continue;
        }
        if (!path_exists(metadata)) {
            continue;
        }

        if (count < VODKA_MAX_APPS) {
            copy_string(packages[count], VODKA_PACKAGE_MAX, entry->d_name);
            count++;
        }
    }

    closedir(dir);
    qsort(packages, count, VODKA_PACKAGE_MAX, compare_package_names);
    return count;
}

static int property_int_value(
    struct vodka_properties *properties,
    const char *key,
    int fallback)
{
    const char *value = property_value(properties, key, NULL);
    char *end = NULL;
    long parsed = 0;

    if (value == NULL || value[0] == '\0') {
        return fallback;
    }

    errno = 0;
    parsed = strtol(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' || parsed < 0 || parsed > 2147483647L) {
        return fallback;
    }

    return (int)parsed;
}

static int installed_package_uid(const char *prefix, const char *package)
{
    char metadata_path[VODKA_PATH_MAX];
    struct vodka_properties metadata = {0};

    if (!package_metadata_path(metadata_path, sizeof(metadata_path), prefix, package)) {
        return -1;
    }
    if (!path_exists(metadata_path)) {
        return -1;
    }
    if (!load_property_file(&metadata, metadata_path, "metadata")) {
        return -1;
    }

    return property_int_value(&metadata, "uid", -1);
}

static int next_available_app_uid(const char *prefix)
{
    char packages[VODKA_MAX_APPS][VODKA_PACKAGE_MAX];
    const size_t count = collect_installed_packages(prefix, packages);
    int max_uid = VODKA_APP_UID_BASE - 1;

    for (size_t i = 0; i < count; i++) {
        const int uid = installed_package_uid(prefix, packages[i]);
        if (uid >= VODKA_APP_UID_BASE && uid > max_uid) {
            max_uid = uid;
        }
    }

    return max_uid + 1;
}

static bool host_to_android_path(
    char *dest,
    size_t dest_size,
    const char *android_root,
    const char *host_path)
{
    const size_t root_len = strlen(android_root);
    const char *relative = host_path;

    if (strncmp(host_path, android_root, root_len) == 0) {
        relative = host_path + root_len;
        if (*relative == '\0') {
            relative = "/";
        }
    }

    if (*relative != '/') {
        const int written = snprintf(dest, dest_size, "/%s", relative);
        return written >= 0 && (size_t)written < dest_size;
    }

    copy_string(dest, dest_size, relative);
    return strlen(relative) < dest_size;
}

static void write_xml_escaped(FILE *file, const char *value)
{
    for (const char *cursor = value; cursor != NULL && *cursor != '\0'; cursor++) {
        switch (*cursor) {
        case '&':
            fputs("&amp;", file);
            break;
        case '<':
            fputs("&lt;", file);
            break;
        case '>':
            fputs("&gt;", file);
            break;
        case '"':
            fputs("&quot;", file);
            break;
        case '\'':
            fputs("&apos;", file);
            break;
        default:
            fputc(*cursor, file);
            break;
        }
    }
}

static bool permissions_to_metadata_value(
    char *dest,
    size_t dest_size,
    const struct manifest_permissions *permissions)
{
    size_t used = 0;

    if (dest_size == 0) {
        return false;
    }
    dest[0] = '\0';

    for (size_t i = 0; i < permissions->count; i++) {
        const char *separator = i == 0 ? "" : ",";
        const size_t separator_len = strlen(separator);
        const size_t permission_len = strlen(permissions->items[i]);

        if (used + separator_len + permission_len >= dest_size) {
            return false;
        }

        memcpy(dest + used, separator, separator_len);
        used += separator_len;
        memcpy(dest + used, permissions->items[i], permission_len);
        used += permission_len;
        dest[used] = '\0';
    }

    return true;
}

static void write_permission_items_xml(FILE *file, const char *requested_permissions)
{
    if (requested_permissions == NULL || requested_permissions[0] == '\0') {
        fputs("    <perms />\n", file);
        return;
    }

    fputs("    <perms>\n", file);

    const char *cursor = requested_permissions;
    while (*cursor != '\0') {
        const char *end = strchr(cursor, ',');
        const size_t len = end == NULL ? strlen(cursor) : (size_t)(end - cursor);

        if (len > 0 && len < VODKA_PERMISSION_MAX) {
            char permission[VODKA_PERMISSION_MAX];
            memcpy(permission, cursor, len);
            permission[len] = '\0';
            if (validate_permission_name(permission)) {
                fputs("      <item name=\"", file);
                write_xml_escaped(file, permission);
                fputs("\" granted=\"false\" flags=\"0\" />\n", file);
            }
        }

        if (end == NULL) {
            break;
        }
        cursor = end + 1;
    }

    fputs("    </perms>\n", file);
}

static bool regenerate_package_state(const char *prefix)
{
    char android_root[VODKA_PATH_MAX];
    char packages_xml[VODKA_PATH_MAX];
    char packages_list[VODKA_PATH_MAX];
    char restrictions_xml[VODKA_PATH_MAX];
    char packages[VODKA_MAX_APPS][VODKA_PACKAGE_MAX];

    if (!prefix_android_root(android_root, sizeof(android_root), prefix) ||
        !join_path(packages_xml, sizeof(packages_xml), android_root, "data/system/packages.xml") ||
        !join_path(packages_list, sizeof(packages_list), android_root, "data/system/packages.list") ||
        !join_path(restrictions_xml, sizeof(restrictions_xml), android_root, "data/system/users/0/package-restrictions.xml")) {
        fprintf(stderr, "package state path is too long\n");
        return false;
    }

    if (!ensure_parent_dir(packages_xml) ||
        !ensure_parent_dir(packages_list) ||
        !ensure_parent_dir(restrictions_xml)) {
        return false;
    }

    const size_t count = collect_installed_packages(prefix, packages);

    FILE *list = fopen(packages_list, "w");
    if (list == NULL) {
        fprintf(stderr, "failed to create %s: %s\n", packages_list, strerror(errno));
        return false;
    }

    FILE *xml = fopen(packages_xml, "w");
    if (xml == NULL) {
        fprintf(stderr, "failed to create %s: %s\n", packages_xml, strerror(errno));
        fclose(list);
        return false;
    }

    FILE *restrictions = fopen(restrictions_xml, "w");
    if (restrictions == NULL) {
        fprintf(stderr, "failed to create %s: %s\n", restrictions_xml, strerror(errno));
        fclose(list);
        fclose(xml);
        return false;
    }

    fputs("<?xml version='1.0' encoding='utf-8' standalone='yes' ?>\n<packages>\n", xml);
    fprintf(
        xml,
        "  <last-platform-version internal=\"%d\" external=\"%d\" fingerprint=\"vodka\" />\n",
        VODKA_DEFAULT_TARGET_SDK,
        VODKA_DEFAULT_TARGET_SDK);
    fputs("<?xml version='1.0' encoding='utf-8' standalone='yes' ?>\n<package-restrictions>\n", restrictions);

    bool ok = true;
    for (size_t i = 0; i < count; i++) {
        char metadata_path[VODKA_PATH_MAX];
        struct vodka_properties metadata = {0};

        if (!package_metadata_path(metadata_path, sizeof(metadata_path), prefix, packages[i]) ||
            !load_property_file(&metadata, metadata_path, "metadata")) {
            ok = false;
            continue;
        }

        const char *apk = property_value(&metadata, "apk", "");
        const char *data_dir = property_value(&metadata, "data_dir", "");
        const char *requested_permissions = property_value(&metadata, "requested_permissions", "");
        const int uid = property_int_value(&metadata, "uid", VODKA_APP_UID_BASE + (int)i);
        const int target_sdk = property_int_value(&metadata, "target_sdk", VODKA_DEFAULT_TARGET_SDK);
        char android_apk[VODKA_PATH_MAX];
        char android_data[VODKA_PATH_MAX];
        char code_path[VODKA_PATH_MAX];

        if (!host_to_android_path(android_apk, sizeof(android_apk), android_root, apk) ||
            !host_to_android_path(android_data, sizeof(android_data), android_root, data_dir)) {
            ok = false;
            continue;
        }

        copy_string(code_path, sizeof(code_path), android_apk);
        char *slash = strrchr(code_path, '/');
        if (slash != NULL && strcmp(slash, "/base.apk") == 0) {
            *slash = '\0';
        }

        fprintf(
            list,
            "%s %d 0 %s default %d 0 0\n",
            packages[i],
            uid,
            android_data,
            target_sdk);

        fputs("  <package name=\"", xml);
        write_xml_escaped(xml, packages[i]);
        fputs("\" codePath=\"", xml);
        write_xml_escaped(xml, code_path);
        fprintf(
            xml,
            "\" nativeLibraryPath=\"%s/lib\" publicFlags=\"0\" privateFlags=\"0\" ft=\"0\" it=\"0\" ut=\"0\" version=\"1\" userId=\"%d\" targetSdkVersion=\"%d\">\n",
            code_path,
            uid,
            target_sdk);
        write_permission_items_xml(xml, requested_permissions);
        fputs("  </package>\n", xml);

        fputs("  <pkg name=\"", restrictions);
        write_xml_escaped(restrictions, packages[i]);
        fputs("\" installed=\"true\" stopped=\"false\" hidden=\"false\" />\n", restrictions);
    }

    fputs("</packages>\n", xml);
    fputs("</package-restrictions>\n", restrictions);

    if (fclose(list) != 0 || fclose(xml) != 0 || fclose(restrictions) != 0) {
        fprintf(stderr, "failed to close package state files\n");
        ok = false;
    }

    return ok;
}

static int command_init(const struct vodka_cli *cli)
{
    if (!setup_prefix(cli->prefix)) {
        return 1;
    }

    char root[VODKA_PATH_MAX];
    if (!prefix_android_root(root, sizeof(root), cli->prefix)) {
        fprintf(stderr, "path too long: %s/android_root\n", cli->prefix);
        return 1;
    }

    printf("prefix=%s\n", cli->prefix);
    printf("android_root=%s\n", root);
    printf("status=ready\n");
    return 0;
}

static int command_status(const struct vodka_cli *cli)
{
    char root[VODKA_PATH_MAX];
    if (!prefix_android_root(root, sizeof(root), cli->prefix)) {
        fprintf(stderr, "path too long: %s/android_root\n", cli->prefix);
        return 1;
    }

    printf("prefix=%s\n", cli->prefix);
    printf("prefix_exists=%s\n", is_directory(cli->prefix) ? "yes" : "no");
    printf("android_root=%s\n", root);
    printf("android_root_exists=%s\n", is_directory(root) ? "yes" : "no");

    char config_path[VODKA_PATH_MAX];
    if (!prefix_config_path(config_path, sizeof(config_path), cli->prefix)) {
        fprintf(stderr, "path too long: %s/config/vodka.conf\n", cli->prefix);
        return 1;
    }
    printf("config_exists=%s\n", path_exists(config_path) ? "yes" : "no");

    struct vodka_properties config = {0};
    if (!load_prefix_config(cli->prefix, &config)) {
        return 1;
    }

    const char *backend = runtime_backend_name(&config, NULL);
    const char *exec_path = runtime_exec_path(&config, NULL);
    printf("runtime_backend=%s\n", backend);
    if (exec_path != NULL) {
        printf("runtime_exec=%s\n", exec_path);
        printf("runtime_exec_executable=%s\n", access(exec_path, X_OK) == 0 ? "yes" : "no");
    } else {
        printf("runtime_exec=\n");
        printf("runtime_exec_executable=no\n");
    }
    const char *configured_app_process = runtime_app_process_configured_path(&config, NULL);
    printf("runtime_app_process=%s\n", configured_app_process == NULL ? "" : configured_app_process);
    printf(
        "runtime_app_process_executable=%s\n",
        configured_app_process != NULL && access(configured_app_process, X_OK) == 0 ? "yes" : "no");
    const char *binder = runtime_binder_device(&config, NULL);
    printf("binder_device=%s\n", binder);
    printf("binder_kind=%s\n", binder_device_kind(binder));
    printf("binder_ready=%s\n", binder_device_ready(binder) ? "yes" : "no");
    char binder_state[VODKA_PATH_MAX];
    if (join_path(binder_state, sizeof(binder_state), root, "data/system/vodka-binder-services.conf")) {
        printf("binder_services_state=%s\n", path_exists(binder_state) ? "yes" : "no");
    }

    if (is_directory(root)) {
        struct vodka_properties properties = {0};
        if (!validate_android_root(root, &properties)) {
            printf("root_valid=no\n");
            return 1;
        }
        printf("root_valid=yes\n");
        printf("loaded_properties=%zu\n", properties.count);
    } else {
        printf("root_valid=no\n");
    }

    char packages[VODKA_MAX_APPS][VODKA_PACKAGE_MAX];
    printf("installed_apps=%zu\n", collect_installed_packages(cli->prefix, packages));

    return 0;
}

static int command_binder_status(const struct vodka_cli *cli, int argc, char **argv)
{
    const char *binder_override = NULL;
    bool configure = false;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--binder") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--binder requires a device path\n");
                return 2;
            }
            binder_override = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--configure") == 0) {
            configure = true;
            continue;
        }

        fprintf(stderr, "usage: %s [--prefix PATH] binder-status [--binder DEVICE] [--configure]\n", cli->program);
        return 2;
    }

    if (configure && !setup_prefix(cli->prefix)) {
        return 1;
    }

    struct vodka_properties config = {0};
    if (!load_prefix_config(cli->prefix, &config)) {
        return 1;
    }

    const char *binder = binder_override == NULL ?
        runtime_binder_device(&config, NULL) :
        binder_override;

    if (configure) {
        char config_path[VODKA_PATH_MAX];
        struct vodka_properties services_config = {0};
        if (!prefix_config_path(config_path, sizeof(config_path), cli->prefix)) {
            fprintf(stderr, "path too long: %s/config/vodka.conf\n", cli->prefix);
            return 1;
        }
        if (!load_services_config(cli->prefix, &services_config) ||
            !put_property(&config, "runtime.binder.device", binder, "config/vodka.conf") ||
            !save_property_file(config_path, "# Vodka prefix configuration", &config) ||
            !write_binder_compat_state(cli->prefix, binder) ||
            !write_service_bridge_state(cli->prefix, binder, &services_config)) {
            return 1;
        }
    }

    static const char *const candidates[] = {
        "/dev/binder",
        "/dev/binderfs/binder",
        "/dev/vndbinder",
        "/dev/hwbinder",
    };

    printf("prefix=%s\n", cli->prefix);
    printf("binder_device=%s\n", binder);
    printf("binder_kind=%s\n", binder_device_kind(binder));
    printf("binder_ready=%s\n", binder_device_ready(binder) ? "yes" : "no");
    printf("configured=%s\n", configure ? "yes" : "no");

    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        printf(
            "candidate=%s kind=%s ready=%s\n",
            candidates[i],
            binder_device_kind(candidates[i]),
            binder_device_ready(candidates[i]) ? "yes" : "no");
    }

    return 0;
}

static int command_bridge_status(const struct vodka_cli *cli, int argc, char **argv)
{
    const char *configure_service = NULL;
    const char *configure_exec = NULL;
    bool write_state = false;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--service") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--service requires a service name\n");
                return 2;
            }
            configure_service = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--exec") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--exec requires a path\n");
                return 2;
            }
            configure_exec = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--write-state") == 0) {
            write_state = true;
            continue;
        }

        fprintf(stderr, "usage: %s [--prefix PATH] bridge-status [--service NAME --exec PATH] [--write-state]\n", cli->program);
        return 2;
    }

    if ((configure_service == NULL) != (configure_exec == NULL)) {
        fprintf(stderr, "--service and --exec must be used together\n");
        return 2;
    }

    if (configure_service != NULL && find_service_bridge(configure_service) == NULL) {
        fprintf(stderr, "unknown service bridge: %s\n", configure_service);
        return 2;
    }

    if ((configure_service != NULL || write_state) && !setup_prefix(cli->prefix)) {
        return 1;
    }

    struct vodka_properties services_config = {0};
    if (!load_services_config(cli->prefix, &services_config)) {
        return 1;
    }

    if (configure_service != NULL) {
        char key[VODKA_KEY_MAX];
        if (!service_exec_key(key, sizeof(key), configure_service) ||
            !put_property(&services_config, key, configure_exec, "config/services.conf") ||
            !save_services_config(cli->prefix, &services_config)) {
            return 1;
        }
        write_state = true;
    }

    struct vodka_properties prefix_config = {0};
    if (!load_prefix_config(cli->prefix, &prefix_config)) {
        return 1;
    }
    const char *binder = runtime_binder_device(&prefix_config, NULL);

    if (write_state && !write_service_bridge_state(cli->prefix, binder, &services_config)) {
        return 1;
    }

    printf("prefix=%s\n", cli->prefix);
    printf("binder_device=%s\n", binder);
    printf("binder_ready=%s\n", binder_device_ready(binder) ? "yes" : "no");
    printf("configured=%s\n", configure_service == NULL ? "no" : "yes");

    for (size_t i = 0; i < sizeof(service_bridges) / sizeof(service_bridges[0]); i++) {
        const char *exec_path = service_exec_path(&services_config, service_bridges[i].name);
        printf(
            "service=%s binder=%s required=%s exec=%s ready=%s\n",
            service_bridges[i].name,
            service_bridges[i].binder_name,
            service_bridges[i].required ? "yes" : "no",
            exec_path == NULL ? "" : exec_path,
            exec_path != NULL && access(exec_path, X_OK) == 0 ? "yes" : "no");
    }

    return 0;
}

static bool set_service_bridge_env(
    const char *prefix,
    const char *binder,
    const struct service_bridge *service)
{
    char android_root[VODKA_PATH_MAX];
    char packages_xml[VODKA_PATH_MAX];
    char packages_list[VODKA_PATH_MAX];
    char service_state[VODKA_PATH_MAX];

    if (!prefix_android_root(android_root, sizeof(android_root), prefix) ||
        !join_path(packages_xml, sizeof(packages_xml), android_root, "data/system/packages.xml") ||
        !join_path(packages_list, sizeof(packages_list), android_root, "data/system/packages.list") ||
        !android_services_state_path(service_state, sizeof(service_state), prefix)) {
        fprintf(stderr, "service bridge environment path is too long\n");
        return false;
    }

    return
        set_runtime_env("VODKA_PREFIX", prefix) &&
        set_runtime_env("VODKA_ANDROID_ROOT", android_root) &&
        set_runtime_env("VODKA_BINDER_DEVICE", binder) &&
        set_runtime_env("VODKA_SERVICE", service->name) &&
        set_runtime_env("VODKA_BINDER_SERVICE", service->binder_name) &&
        set_runtime_env("VODKA_SERVICE_REQUIRED", service->required ? "yes" : "no") &&
        set_runtime_env("VODKA_PACKAGES_XML", packages_xml) &&
        set_runtime_env("VODKA_PACKAGES_LIST", packages_list) &&
        set_runtime_env("VODKA_SERVICE_STATE", service_state) &&
        set_runtime_env("ANDROID_ROOT", "/system") &&
        set_runtime_env("ANDROID_DATA", "/data");
}

static int launch_service_bridge(
    const char *prefix,
    const char *binder,
    const struct service_bridge *service,
    const char *exec_path,
    bool wait_for_exit)
{
    if (exec_path == NULL || exec_path[0] == '\0') {
        fprintf(stderr, "service bridge is not configured: %s\n", service->name);
        return service->required ? 1 : 0;
    }
    if (access(exec_path, X_OK) != 0) {
        fprintf(stderr, "service bridge executable is not executable: %s: %s\n", exec_path, strerror(errno));
        return 1;
    }

    fflush(NULL);
    const pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "failed to fork service bridge %s: %s\n", service->name, strerror(errno));
        return 1;
    }

    if (pid == 0) {
        if (!set_service_bridge_env(prefix, binder, service)) {
            _exit(126);
        }

        execl(exec_path, exec_path, service->name, service->binder_name, binder, (char *)NULL);
        fprintf(stderr, "failed to execute service bridge %s: %s\n", exec_path, strerror(errno));
        _exit(errno == ENOENT ? 127 : 126);
    }

    printf("service=%s pid=%ld status=started\n", service->name, (long)pid);

    if (!wait_for_exit) {
        return 0;
    }

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR) {
            continue;
        }
        fprintf(stderr, "failed to wait for service bridge %s: %s\n", service->name, strerror(errno));
        return 1;
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        fprintf(stderr, "service bridge %s terminated by signal %d\n", service->name, WTERMSIG(status));
        return 128 + WTERMSIG(status);
    }

    return 1;
}

static int command_start_services(const struct vodka_cli *cli, int argc, char **argv)
{
    bool dry_run = false;
    bool optional = false;
    bool wait_for_exit = false;
    const char *only_service = NULL;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--dry-run") == 0) {
            dry_run = true;
            continue;
        }
        if (strcmp(argv[i], "--optional") == 0) {
            optional = true;
            continue;
        }
        if (strcmp(argv[i], "--wait") == 0) {
            wait_for_exit = true;
            continue;
        }
        if (strcmp(argv[i], "--service") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--service requires a service name\n");
                return 2;
            }
            only_service = argv[++i];
            continue;
        }

        fprintf(stderr, "usage: %s [--prefix PATH] start-services [--dry-run] [--wait] [--optional] [--service NAME]\n", cli->program);
        return 2;
    }

    if (only_service != NULL && find_service_bridge(only_service) == NULL) {
        fprintf(stderr, "unknown service bridge: %s\n", only_service);
        return 2;
    }

    struct vodka_properties prefix_config = {0};
    struct vodka_properties services_config = {0};
    if (!load_prefix_config(cli->prefix, &prefix_config) ||
        !load_services_config(cli->prefix, &services_config)) {
        return 1;
    }

    const char *binder = runtime_binder_device(&prefix_config, NULL);
    if (!write_service_bridge_state(cli->prefix, binder, &services_config)) {
        return 1;
    }

    printf("prefix=%s\n", cli->prefix);
    printf("binder_device=%s\n", binder);
    printf("binder_ready=%s\n", binder_device_ready(binder) ? "yes" : "no");
    printf("mode=%s\n", dry_run ? "dry-run" : "launch");

    int result = 0;
    for (size_t i = 0; i < sizeof(service_bridges) / sizeof(service_bridges[0]); i++) {
        const struct service_bridge *service = &service_bridges[i];
        const char *exec_path = service_exec_path(&services_config, service->name);

        if (only_service != NULL && strcmp(only_service, service->name) != 0) {
            continue;
        }
        if (!optional && !service->required && only_service == NULL) {
            continue;
        }

        printf(
            "service=%s binder=%s required=%s exec=%s ready=%s\n",
            service->name,
            service->binder_name,
            service->required ? "yes" : "no",
            exec_path == NULL ? "" : exec_path,
            exec_path != NULL && access(exec_path, X_OK) == 0 ? "yes" : "no");

        if (dry_run) {
            if (service->required && (exec_path == NULL || access(exec_path, X_OK) != 0)) {
                result = 1;
            }
            continue;
        }

        const int status = launch_service_bridge(cli->prefix, binder, service, exec_path, wait_for_exit);
        if (status != 0 && result == 0) {
            result = status;
        }
    }

    return result;
}

static int command_probe(const struct vodka_cli *cli, int argc, char **argv)
{
    const char *root = NULL;
    char prefix_root[VODKA_PATH_MAX];

    if (argc > 1) {
        fprintf(stderr, "usage: %s [--prefix PATH] probe [android-root]\n", cli->program);
        return 2;
    }

    if (argc == 1) {
        root = argv[0];
    } else {
        if (!prefix_android_root(prefix_root, sizeof(prefix_root), cli->prefix)) {
            fprintf(stderr, "path too long: %s/android_root\n", cli->prefix);
            return 1;
        }
        root = prefix_root;
    }

    printf("Vodka root probe\n");
    printf("prefix=%s\n", cli->prefix);
    printf("root=%s\n", root);
    printf("ANDROID_ROOT=/system\n");
    printf("ANDROID_DATA=/data\n");
    printf("ANDROID_STORAGE=/storage\n");
    printf("EXTERNAL_STORAGE=/sdcard\n");

    struct vodka_properties properties = {0};
    const bool ok = validate_android_root(root, &properties);

    printf("loaded_properties=%zu\n", properties.count);
    if (!print_required_property(&properties, "ro.vodka.root")) {
        return 1;
    }
    if (!print_required_property(&properties, "ro.build.version.sdk")) {
        return 1;
    }
    if (!print_required_property(&properties, "ro.product.cpu.abilist")) {
        return 1;
    }

    return ok ? 0 : 1;
}

static int command_env(const struct vodka_cli *cli)
{
    char root[VODKA_PATH_MAX];
    if (!prefix_android_root(root, sizeof(root), cli->prefix)) {
        fprintf(stderr, "path too long: %s/android_root\n", cli->prefix);
        return 1;
    }

    printf("VODKA_PREFIX=%s\n", cli->prefix);
    printf("VODKA_ANDROID_ROOT=%s\n", root);
    printf("ANDROID_ROOT=/system\n");
    printf("ANDROID_DATA=/data\n");
    printf("ANDROID_STORAGE=/storage\n");
    printf("EXTERNAL_STORAGE=/sdcard\n");
    return 0;
}

static const char *path_basename(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash == NULL ? path : slash + 1;
}

static bool append_classpath_entry(
    char *dest,
    size_t dest_size,
    const char *android_path)
{
    const size_t current_len = strlen(dest);
    const size_t entry_len = strlen(android_path);
    const size_t separator_len = current_len == 0 ? 0 : 1;

    if (current_len + separator_len + entry_len + 1 > dest_size) {
        return false;
    }

    if (separator_len == 1) {
        strcat(dest, ":");
    }
    strcat(dest, android_path);
    return true;
}

static bool build_default_classpath(char *dest, size_t dest_size, const char *android_root)
{
    static const char *const framework_jars[] = {
        "/system/framework/core-oj.jar",
        "/system/framework/core-libart.jar",
        "/system/framework/conscrypt.jar",
        "/system/framework/okhttp.jar",
        "/system/framework/bouncycastle.jar",
        "/system/framework/apache-xml.jar",
        "/system/framework/framework.jar",
        "/system/framework/ext.jar",
        "/system/framework/services.jar",
    };

    dest[0] = '\0';

    for (size_t i = 0; i < sizeof(framework_jars) / sizeof(framework_jars[0]); i++) {
        char host_path[VODKA_PATH_MAX];
        const char *relative = framework_jars[i][0] == '/' ? framework_jars[i] + 1 : framework_jars[i];

        if (!join_path(host_path, sizeof(host_path), android_root, relative)) {
            return false;
        }

        if (path_exists(host_path) && !append_classpath_entry(dest, dest_size, framework_jars[i])) {
            return false;
        }
    }

    return true;
}

static int command_install_runtime(const struct vodka_cli *cli, int argc, char **argv)
{
    const char *source = NULL;
    const char *app_process_source = NULL;
    const char *entrypoint = "android.app.ActivityThread";
    const char *binder = "/dev/binder";
    bool set_default = true;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--from") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--from requires a source Android root or system directory\n");
                return 2;
            }
            source = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--app-process") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--app-process requires a path\n");
                return 2;
            }
            app_process_source = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--entrypoint") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--entrypoint requires a class name\n");
                return 2;
            }
            entrypoint = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--binder") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--binder requires a device path\n");
                return 2;
            }
            binder = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--no-default") == 0) {
            set_default = false;
            continue;
        }
        if (source != NULL) {
            fprintf(
                stderr,
                "usage: %s [--prefix PATH] install-runtime --from ROOT [--app-process PATH] [--entrypoint CLASS] [--binder DEVICE] [--no-default]\n",
                cli->program);
            return 2;
        }
        source = argv[i];
    }

    if (source == NULL) {
        fprintf(
            stderr,
            "usage: %s [--prefix PATH] install-runtime --from ROOT [--app-process PATH] [--entrypoint CLASS] [--binder DEVICE] [--no-default]\n",
            cli->program);
        return 2;
    }

    if (!setup_prefix(cli->prefix)) {
        return 1;
    }

    char android_root[VODKA_PATH_MAX];
    if (!prefix_android_root(android_root, sizeof(android_root), cli->prefix)) {
        fprintf(stderr, "path too long: %s/android_root\n", cli->prefix);
        return 1;
    }

    char source_system[VODKA_PATH_MAX];
    char source_system_candidate[VODKA_PATH_MAX];
    if (join_path(source_system_candidate, sizeof(source_system_candidate), source, "system") &&
        is_directory(source_system_candidate)) {
        copy_string(source_system, sizeof(source_system), source_system_candidate);
    } else {
        copy_string(source_system, sizeof(source_system), source);
    }

    char dest_system[VODKA_PATH_MAX];
    if (!join_path(dest_system, sizeof(dest_system), android_root, "system")) {
        fprintf(stderr, "path too long: %s/system\n", android_root);
        return 1;
    }

    bool ok = copy_tree_if_exists(source_system, dest_system);

    static const char *const optional_partitions[] = {
        "vendor",
        "product",
        "odm",
        "apex",
    };

    for (size_t i = 0; i < sizeof(optional_partitions) / sizeof(optional_partitions[0]); i++) {
        char source_partition[VODKA_PATH_MAX];
        char dest_partition[VODKA_PATH_MAX];

        if (!join_path(source_partition, sizeof(source_partition), source, optional_partitions[i]) ||
            !join_path(dest_partition, sizeof(dest_partition), android_root, optional_partitions[i])) {
            fprintf(stderr, "partition path too long: %s\n", optional_partitions[i]);
            ok = false;
            continue;
        }

        if (!copy_tree_if_exists(source_partition, dest_partition)) {
            ok = false;
        }
    }

    char installed_app_process[VODKA_PATH_MAX];
    installed_app_process[0] = '\0';

    if (app_process_source != NULL) {
        char app_process_dir[VODKA_PATH_MAX];
        if (!join_path(app_process_dir, sizeof(app_process_dir), android_root, "system/bin") ||
            !join_path(installed_app_process, sizeof(installed_app_process), app_process_dir, path_basename(app_process_source))) {
            fprintf(stderr, "app_process destination path is too long\n");
            return 1;
        }
        if (!copy_tree_replace(app_process_source, installed_app_process)) {
            ok = false;
        }
    }

    if (installed_app_process[0] == '\0' &&
        !resolve_default_app_process_path(installed_app_process, sizeof(installed_app_process), android_root)) {
        fprintf(stderr, "failed to resolve app_process path\n");
        return 1;
    }

    if (installed_app_process[0] == '\0' || access(installed_app_process, X_OK) != 0) {
        fprintf(
            stderr,
            "runtime install did not find an executable app_process or app_process64 under %s/system/bin\n",
            android_root);
        return 1;
    }

    char classpath[VODKA_VALUE_MAX];
    if (!build_default_classpath(classpath, sizeof(classpath), android_root)) {
        fprintf(stderr, "failed to build runtime classpath\n");
        return 1;
    }

    struct vodka_properties config = {0};
    if (!load_prefix_config(cli->prefix, &config)) {
        return 1;
    }

    char config_path[VODKA_PATH_MAX];
    if (!prefix_config_path(config_path, sizeof(config_path), cli->prefix)) {
        fprintf(stderr, "path too long: %s/config/vodka.conf\n", cli->prefix);
        return 1;
    }

    if (!put_property(&config, "runtime.app_process", installed_app_process, "config/vodka.conf") ||
        !put_property(&config, "runtime.entrypoint", entrypoint, "config/vodka.conf") ||
        !put_property(&config, "runtime.classpath", classpath, "config/vodka.conf") ||
        !put_property(&config, "runtime.binder.device", binder, "config/vodka.conf")) {
        return 1;
    }

    if (set_default && !put_property(&config, "runtime.backend", "app_process", "config/vodka.conf")) {
        return 1;
    }

    if (!save_property_file(config_path, "# Vodka prefix configuration", &config)) {
        return 1;
    }

    struct vodka_properties services_config = {0};
    if (!load_services_config(cli->prefix, &services_config) ||
        !write_binder_compat_state(cli->prefix, binder) ||
        !write_service_bridge_state(cli->prefix, binder, &services_config)) {
        return 1;
    }

    printf("runtime_installed=%s\n", ok ? "yes" : "partial");
    printf("prefix=%s\n", cli->prefix);
    printf("android_root=%s\n", android_root);
    printf("system_source=%s\n", source_system);
    printf("app_process=%s\n", installed_app_process);
    printf("entrypoint=%s\n", entrypoint);
    printf("classpath=%s\n", classpath);
    printf("binder_device=%s\n", binder);
    printf("binder_ready=%s\n", binder_device_ready(binder) ? "yes" : "no");
    printf("runtime_backend=%s\n", set_default ? "app_process" : runtime_backend_name(&config, NULL));
    return ok ? 0 : 1;
}

static int command_install(const struct vodka_cli *cli, int argc, char **argv)
{
    const char *apk_path = NULL;
    const char *package = NULL;
    const char *activity = NULL;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--package") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--package requires a package name\n");
                return 2;
            }
            package = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--activity") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--activity requires an activity name\n");
                return 2;
            }
            activity = argv[++i];
            continue;
        }
        if (apk_path != NULL) {
            fprintf(stderr, "usage: %s [--prefix PATH] install [--package NAME] [--activity NAME] APK\n", cli->program);
            return 2;
        }
        apk_path = argv[i];
    }

    if (apk_path == NULL) {
        fprintf(stderr, "usage: %s [--prefix PATH] install [--package NAME] [--activity NAME] APK\n", cli->program);
        return 2;
    }

    struct apk_info apk = {0};
    if (!inspect_apk(apk_path, &apk)) {
        return 1;
    }

    struct manifest_details manifest = {0};
    if (!extract_manifest_details(apk_path, &apk, &manifest)) {
        return 1;
    }

    if (package == NULL) {
        if (manifest.has_package) {
            package = manifest.package;
        } else {
            fprintf(
                stderr,
                "could not derive package name from AndroidManifest.xml; pass --package NAME\n");
            return 2;
        }
    }

    if (activity == NULL && manifest.has_launch_activity) {
        activity = manifest.launch_activity;
    }
    if (activity == NULL) {
        activity = "";
    }

    if (!validate_package_name(package)) {
        fprintf(stderr, "invalid Android package name: %s\n", package);
        return 2;
    }

    if (!setup_prefix(cli->prefix)) {
        return 1;
    }

    char root[VODKA_PATH_MAX];
    char app_dir[VODKA_PATH_MAX];
    char base_apk[VODKA_PATH_MAX];
    char data_dir[VODKA_PATH_MAX];
    char metadata_path[VODKA_PATH_MAX];

    if (!prefix_android_root(root, sizeof(root), cli->prefix) ||
        !package_android_app_dir(app_dir, sizeof(app_dir), root, package) ||
        !join_path(base_apk, sizeof(base_apk), app_dir, "base.apk") ||
        !package_android_data_dir(data_dir, sizeof(data_dir), root, package) ||
        !package_metadata_path(metadata_path, sizeof(metadata_path), cli->prefix, package)) {
        fprintf(stderr, "install path is too long for package: %s\n", package);
        return 1;
    }

    if (!mkdir_p(app_dir, 0755) || !mkdir_p(data_dir, 0755)) {
        return 1;
    }

    int uid = installed_package_uid(cli->prefix, package);
    if (uid < VODKA_APP_UID_BASE) {
        uid = next_available_app_uid(cli->prefix);
    }

    if (!copy_file_replace(apk_path, base_apk)) {
        return 1;
    }

    char permissions_value[VODKA_VALUE_MAX];
    if (!permissions_to_metadata_value(permissions_value, sizeof(permissions_value), &manifest.permissions)) {
        fprintf(stderr, "requested permission metadata is too large for package: %s\n", package);
        return 1;
    }

    const int target_sdk = manifest.has_target_sdk ? manifest.target_sdk : VODKA_DEFAULT_TARGET_SDK;
    const int min_sdk = manifest.has_min_sdk ? manifest.min_sdk : 0;

    char metadata_content[(VODKA_PATH_MAX * 4) + (VODKA_VALUE_MAX * 2) + VODKA_PACKAGE_MAX + VODKA_ACTIVITY_MAX + 640];
    const int written = snprintf(
        metadata_content,
        sizeof(metadata_content),
        "# Vodka installed app metadata\n"
        "package=%s\n"
        "apk=%s\n"
        "data_dir=%s\n"
        "source_apk=%s\n"
        "launch_activity=%s\n"
        "manifest=%s\n"
        "manifest_format=%s\n"
        "manifest_method=%u\n"
        "requested_permissions=%s\n"
        "uid=%d\n"
        "min_sdk=%d\n"
        "target_sdk=%d\n"
        "seinfo=default\n"
        "state=installed\n",
        package,
        base_apk,
        data_dir,
        apk_path,
        activity,
        apk.has_manifest ? "present" : "missing",
        manifest.format,
        (unsigned int)apk.manifest_method,
        permissions_value,
        uid,
        min_sdk,
        target_sdk);

    if (written < 0 || (size_t)written >= sizeof(metadata_content)) {
        fprintf(stderr, "metadata is too large for package: %s\n", package);
        return 1;
    }

    if (!write_file_replace(metadata_path, metadata_content)) {
        return 1;
    }

    if (!regenerate_package_state(cli->prefix)) {
        return 1;
    }

    printf("package=%s\n", package);
    printf("uid=%d\n", uid);
    printf("apk=%s\n", base_apk);
    printf("data_dir=%s\n", data_dir);
    printf("metadata=%s\n", metadata_path);
    printf("manifest_format=%s\n", manifest.format);
    printf("launch_activity=%s\n", activity);
    printf("requested_permissions=%zu\n", manifest.permissions.count);
    printf("target_sdk=%d\n", target_sdk);
    printf("package_state=updated\n");
    printf("status=installed\n");
    return 0;
}

static int command_list(const struct vodka_cli *cli)
{
    char packages[VODKA_MAX_APPS][VODKA_PACKAGE_MAX];
    const size_t count = collect_installed_packages(cli->prefix, packages);

    printf("prefix=%s\n", cli->prefix);
    printf("installed_apps=%zu\n", count);
    for (size_t i = 0; i < count; i++) {
        printf("package=%s\n", packages[i]);
    }

    return 0;
}

static void print_launch_plan(const struct run_plan *plan, const char *status)
{
    printf("launch_plan=vodka-app\n");
    printf("prefix=%s\n", plan->prefix);
    printf("android_root=%s\n", plan->android_root);
    printf("package=%s\n", plan->package);
    printf("activity=%s\n", plan->activity);
    printf("apk=%s\n", plan->apk);
    printf("data_dir=%s\n", plan->data_dir);
    printf("metadata=%s\n", plan->metadata_path);
    printf("runtime_backend=%s\n", plan->backend);
    printf("runtime_exec=%s\n", plan->exec_path == NULL ? "" : plan->exec_path);
    printf("runtime_entrypoint=%s\n", plan->entrypoint == NULL ? "" : plan->entrypoint);
    printf("runtime_classpath=%s\n", plan->classpath == NULL ? "" : plan->classpath);
    printf("binder_device=%s\n", plan->binder_device == NULL ? "" : plan->binder_device);
    printf("binder_ready=%s\n", plan->binder_ready ? "yes" : "no");
    printf("ANDROID_ROOT=/system\n");
    printf("ANDROID_DATA=/data\n");
    printf("status=%s\n", status);
}

static bool set_runtime_env(const char *key, const char *value)
{
    if (setenv(key, value == NULL ? "" : value, 1) != 0) {
        fprintf(stderr, "failed to set %s: %s\n", key, strerror(errno));
        return false;
    }

    return true;
}

static bool prepare_runtime_environment(const struct run_plan *plan)
{
    return
        set_runtime_env("VODKA_PREFIX", plan->prefix) &&
        set_runtime_env("VODKA_ANDROID_ROOT", plan->android_root) &&
        set_runtime_env("VODKA_PACKAGE", plan->package) &&
        set_runtime_env("VODKA_ACTIVITY", plan->activity) &&
        set_runtime_env("VODKA_APK", plan->apk) &&
        set_runtime_env("VODKA_APP_DATA", plan->data_dir) &&
        set_runtime_env("VODKA_METADATA", plan->metadata_path) &&
        set_runtime_env("VODKA_RUNTIME_BACKEND", plan->backend) &&
        set_runtime_env("VODKA_RUNTIME_EXEC", plan->exec_path) &&
        set_runtime_env("VODKA_APP_PROCESS", plan->exec_path) &&
        set_runtime_env("VODKA_APP_PROCESS_ENTRYPOINT", plan->entrypoint) &&
        set_runtime_env("VODKA_CLASSPATH", plan->classpath) &&
        set_runtime_env("VODKA_BINDER_DEVICE", plan->binder_device) &&
        set_runtime_env("CLASSPATH", plan->classpath) &&
        set_runtime_env("ANDROID_ROOT", "/system") &&
        set_runtime_env("ANDROID_DATA", "/data") &&
        set_runtime_env("ANDROID_STORAGE", "/storage") &&
        set_runtime_env("EXTERNAL_STORAGE", "/sdcard");
}

static int launch_exec_backend(const struct run_plan *plan)
{
    if (plan->exec_path == NULL || plan->exec_path[0] == '\0') {
        fprintf(stderr, "runtime.backend=exec requires runtime.exec or VODKA_RUNTIME_EXEC\n");
        return 1;
    }

    if (access(plan->exec_path, X_OK) != 0) {
        fprintf(stderr, "runtime executable is not executable: %s: %s\n", plan->exec_path, strerror(errno));
        return 1;
    }

    fflush(NULL);
    const pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "failed to fork runtime backend: %s\n", strerror(errno));
        return 1;
    }

    if (pid == 0) {
        if (!prepare_runtime_environment(plan)) {
            _exit(126);
        }

        execl(
            plan->exec_path,
            plan->exec_path,
            plan->package,
            plan->activity,
            plan->apk,
            plan->data_dir,
            (char *)NULL);

        fprintf(stderr, "failed to execute runtime backend %s: %s\n", plan->exec_path, strerror(errno));
        _exit(errno == ENOENT ? 127 : 126);
    }

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR) {
            continue;
        }
        fprintf(stderr, "failed to wait for runtime backend: %s\n", strerror(errno));
        return 1;
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        fprintf(stderr, "runtime backend terminated by signal %d\n", WTERMSIG(status));
        return 128 + WTERMSIG(status);
    }

    return 1;
}

static int launch_app_process_backend(const struct run_plan *plan)
{
    if (plan->exec_path == NULL || plan->exec_path[0] == '\0') {
        fprintf(stderr, "runtime.backend=app_process requires runtime.app_process or --exec PATH\n");
        return 1;
    }

    if (access(plan->exec_path, X_OK) != 0) {
        fprintf(stderr, "app_process is not executable: %s: %s\n", plan->exec_path, strerror(errno));
        return 1;
    }

    if (!plan->binder_ready) {
        fprintf(
            stderr,
            "binder device is not ready: %s; ActivityThread will not attach to system services without Binder.\n",
            plan->binder_device == NULL ? "" : plan->binder_device);
    }

    char nice_name[VODKA_PACKAGE_MAX + 16];
    if (snprintf(nice_name, sizeof(nice_name), "--nice-name=%s", plan->package) >= (int)sizeof(nice_name)) {
        fprintf(stderr, "package name is too long for app_process nice name\n");
        return 1;
    }

    fflush(NULL);
    const pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "failed to fork app_process backend: %s\n", strerror(errno));
        return 1;
    }

    if (pid == 0) {
        if (!prepare_runtime_environment(plan)) {
            _exit(126);
        }

        char *const args[] = {
            (char *)plan->exec_path,
            (char *)"/system/bin",
            (char *)"--application",
            nice_name,
            (char *)(plan->entrypoint == NULL ? "android.app.ActivityThread" : plan->entrypoint),
            (char *)plan->package,
            (char *)plan->activity,
            (char *)plan->apk,
            (char *)plan->data_dir,
            NULL,
        };

        execv(plan->exec_path, args);

        fprintf(stderr, "failed to execute app_process %s: %s\n", plan->exec_path, strerror(errno));
        _exit(errno == ENOENT ? 127 : 126);
    }

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR) {
            continue;
        }
        fprintf(stderr, "failed to wait for app_process backend: %s\n", strerror(errno));
        return 1;
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        fprintf(stderr, "app_process backend terminated by signal %d\n", WTERMSIG(status));
        return 128 + WTERMSIG(status);
    }

    return 1;
}

static int launch_runtime_backend(const struct run_plan *plan)
{
    if (strcmp(plan->backend, "none") == 0) {
        fprintf(
            stderr,
            "runtime backend is disabled; configure runtime.backend=exec and runtime.exec in config/vodka.conf.\n");
        return 1;
    }

    if (strcmp(plan->backend, "exec") == 0) {
        return launch_exec_backend(plan);
    }

    if (strcmp(plan->backend, "app_process") == 0) {
        return launch_app_process_backend(plan);
    }

    fprintf(stderr, "unsupported runtime backend: %s\n", plan->backend);
    return 2;
}

static int command_run(const struct vodka_cli *cli, int argc, char **argv)
{
    struct run_options options = {0};
    const char *package = NULL;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--dry-run") == 0) {
            options.dry_run = true;
            continue;
        }
        if (strcmp(argv[i], "--activity") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--activity requires an activity name\n");
                return 2;
            }
            options.activity_override = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--backend") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--backend requires a backend name\n");
                return 2;
            }
            options.backend_override = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--exec") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--exec requires a path\n");
                return 2;
            }
            options.exec_override = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--entrypoint") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--entrypoint requires a class name\n");
                return 2;
            }
            options.entrypoint_override = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--classpath") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--classpath requires a value\n");
                return 2;
            }
            options.classpath_override = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--binder") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--binder requires a device path\n");
                return 2;
            }
            options.binder_override = argv[++i];
            continue;
        }
        if (package != NULL) {
            fprintf(stderr, "usage: %s [--prefix PATH] run [--dry-run] [--backend NAME] [--exec PATH] [--entrypoint CLASS] [--classpath VALUE] [--binder DEVICE] [--activity NAME] PACKAGE\n", cli->program);
            return 2;
        }
        package = argv[i];
    }

    if (package == NULL) {
        fprintf(stderr, "usage: %s [--prefix PATH] run [--dry-run] [--backend NAME] [--exec PATH] [--entrypoint CLASS] [--classpath VALUE] [--binder DEVICE] [--activity NAME] PACKAGE\n", cli->program);
        return 2;
    }
    if (!validate_package_name(package)) {
        fprintf(stderr, "invalid Android package name: %s\n", package);
        return 2;
    }

    char root[VODKA_PATH_MAX];
    char metadata_path[VODKA_PATH_MAX];
    if (!prefix_android_root(root, sizeof(root), cli->prefix) ||
        !package_metadata_path(metadata_path, sizeof(metadata_path), cli->prefix, package)) {
        fprintf(stderr, "runtime path is too long for package: %s\n", package);
        return 1;
    }

    if (!path_exists(metadata_path)) {
        fprintf(stderr, "package is not installed in this prefix: %s\n", package);
        return 1;
    }

    struct vodka_properties metadata = {0};
    if (!load_property_file(&metadata, metadata_path, "metadata")) {
        return 1;
    }

    struct vodka_property *apk = find_property(&metadata, "apk");
    struct vodka_property *data_dir = find_property(&metadata, "data_dir");
    struct vodka_property *stored_activity = find_property(&metadata, "launch_activity");
    const char *activity = options.activity_override;

    if (activity == NULL && stored_activity != NULL && stored_activity->value[0] != '\0') {
        activity = stored_activity->value;
    }
    if (activity == NULL) {
        activity = "<launcher>";
    }

    if (apk == NULL || data_dir == NULL) {
        fprintf(stderr, "installed package metadata is incomplete: %s\n", metadata_path);
        return 1;
    }

    if (!path_exists(apk->value)) {
        fprintf(stderr, "installed APK is missing: %s\n", apk->value);
        return 1;
    }
    if (!is_directory(data_dir->value)) {
        fprintf(stderr, "installed app data directory is missing: %s\n", data_dir->value);
        return 1;
    }

    struct vodka_properties root_properties = {0};
    if (!validate_android_root(root, &root_properties)) {
        return 1;
    }

    struct vodka_properties config = {0};
    if (!load_prefix_config(cli->prefix, &config)) {
        return 1;
    }

    const char *backend = runtime_backend_name(&config, options.backend_override);
    const char *exec_path = runtime_exec_path(&config, options.exec_override);
    char resolved_app_process[VODKA_PATH_MAX];
    resolved_app_process[0] = '\0';
    if (!validate_runtime_backend_name(backend)) {
        fprintf(stderr, "unsupported runtime backend: %s\n", backend);
        return 2;
    }
    if (strcmp(backend, "app_process") == 0) {
        exec_path = runtime_app_process_configured_path(&config, options.exec_override);
        if (exec_path == NULL || exec_path[0] == '\0') {
            if (!resolve_default_app_process_path(resolved_app_process, sizeof(resolved_app_process), root)) {
                fprintf(stderr, "failed to resolve app_process path\n");
                return 1;
            }
            exec_path = resolved_app_process[0] == '\0' ? NULL : resolved_app_process;
        }
    }

    const char *entrypoint = runtime_entrypoint(&config, options.entrypoint_override);
    const char *classpath = runtime_classpath(&config, options.classpath_override);
    const char *binder = runtime_binder_device(&config, options.binder_override);

    struct run_plan plan = {
        .prefix = cli->prefix,
        .package = package,
        .activity = activity,
        .apk = apk->value,
        .data_dir = data_dir->value,
        .backend = backend,
        .exec_path = exec_path,
        .entrypoint = entrypoint,
        .classpath = classpath,
        .binder_device = binder,
        .binder_ready = binder_device_ready(binder),
    };
    copy_string(plan.android_root, sizeof(plan.android_root), root);
    copy_string(plan.metadata_path, sizeof(plan.metadata_path), metadata_path);

    const char *plan_status = options.dry_run ? "ready" : "launching";
    if (!options.dry_run && strcmp(backend, "none") == 0) {
        plan_status = "blocked";
    }
    if (!options.dry_run && strcmp(backend, "exec") == 0 &&
        (exec_path == NULL || exec_path[0] == '\0')) {
        plan_status = "blocked";
    }
    if (!options.dry_run && strcmp(backend, "app_process") == 0 &&
        (exec_path == NULL || exec_path[0] == '\0')) {
        plan_status = "blocked";
    }

    print_launch_plan(&plan, plan_status);

    if (options.dry_run) {
        return 0;
    }

    return launch_runtime_backend(&plan);
}

static void print_usage(const char *program)
{
    fprintf(
        stderr,
        "usage: %s [--prefix PATH] <command> [args]\n"
        "\n"
        "commands:\n"
        "  init                 create or refresh the Vodka prefix\n"
        "  status               report prefix and Android root status\n"
        "  binder-status        report or configure Binder compatibility\n"
        "  bridge-status        report or configure service bridges\n"
        "  start-services       launch configured service bridges\n"
        "  install-runtime      import Android ART/app_process runtime files\n"
        "  install [options] APK stage an APK into the prefix\n"
        "  list                 list installed packages\n"
        "  run [options] PACKAGE prepare an installed package launch\n"
        "  probe [android-root] validate and inspect an Android root\n"
        "  env                  print runtime environment paths\n"
        "\n"
        "default prefix: $VODKA_PREFIX or $HOME/.vodka\n",
        program);
}

int main(int argc, char **argv)
{
    char prefix[VODKA_PATH_MAX];
    bool has_prefix = false;

    int index = 1;
    while (index < argc) {
        if (strcmp(argv[index], "--prefix") == 0) {
            if (index + 1 >= argc) {
                fprintf(stderr, "--prefix requires a path\n");
                return 2;
            }
            if (strlen(argv[index + 1]) >= sizeof(prefix)) {
                fprintf(stderr, "prefix path too long: %s\n", argv[index + 1]);
                return 2;
            }
            copy_string(prefix, sizeof(prefix), argv[index + 1]);
            has_prefix = true;
            index += 2;
            continue;
        }

        if (strcmp(argv[index], "--help") == 0 || strcmp(argv[index], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }

        break;
    }

    if (index >= argc) {
        print_usage(argv[0]);
        return 2;
    }

    if (!has_prefix && !default_prefix(prefix, sizeof(prefix))) {
        return 2;
    }

    struct vodka_cli cli = {
        .program = argv[0],
        .prefix = prefix,
    };

    const char *command = argv[index++];

    if (strcmp(command, "init") == 0) {
        if (index != argc) {
            fprintf(stderr, "usage: %s [--prefix PATH] init\n", argv[0]);
            return 2;
        }
        return command_init(&cli);
    }

    if (strcmp(command, "status") == 0) {
        if (index != argc) {
            fprintf(stderr, "usage: %s [--prefix PATH] status\n", argv[0]);
            return 2;
        }
        return command_status(&cli);
    }

    if (strcmp(command, "binder-status") == 0) {
        return command_binder_status(&cli, argc - index, argv + index);
    }

    if (strcmp(command, "bridge-status") == 0) {
        return command_bridge_status(&cli, argc - index, argv + index);
    }

    if (strcmp(command, "start-services") == 0) {
        return command_start_services(&cli, argc - index, argv + index);
    }

    if (strcmp(command, "install") == 0) {
        return command_install(&cli, argc - index, argv + index);
    }

    if (strcmp(command, "install-runtime") == 0) {
        return command_install_runtime(&cli, argc - index, argv + index);
    }

    if (strcmp(command, "list") == 0) {
        if (index != argc) {
            fprintf(stderr, "usage: %s [--prefix PATH] list\n", argv[0]);
            return 2;
        }
        return command_list(&cli);
    }

    if (strcmp(command, "run") == 0) {
        return command_run(&cli, argc - index, argv + index);
    }

    if (strcmp(command, "probe") == 0) {
        return command_probe(&cli, argc - index, argv + index);
    }

    if (strcmp(command, "env") == 0) {
        if (index != argc) {
            fprintf(stderr, "usage: %s [--prefix PATH] env\n", argv[0]);
            return 2;
        }
        return command_env(&cli);
    }

    fprintf(stderr, "unknown command: %s\n", command);
    print_usage(argv[0]);
    return 2;
}
