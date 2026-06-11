#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define VODKA_PATH_MAX 4096
#define VODKA_KEY_MAX 128
#define VODKA_VALUE_MAX 512
#define VODKA_SOURCE_MAX 128
#define VODKA_MAX_PROPERTIES 256
#define VODKA_PREFIX_VERSION "0"

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

static const char *const prefix_dirs[] = {
    "android_root",
    "cache",
    "config",
    "logs",
    "tmp",
};

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
        "runtime.abi=x86_64\n",
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

    char config[VODKA_PATH_MAX];
    if (!join_path(config, sizeof(config), cli->prefix, "config/vodka.conf")) {
        fprintf(stderr, "path too long: %s/config/vodka.conf\n", cli->prefix);
        return 1;
    }
    printf("config_exists=%s\n", path_exists(config) ? "yes" : "no");

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

    return 0;
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

static void print_usage(const char *program)
{
    fprintf(
        stderr,
        "usage: %s [--prefix PATH] <command> [args]\n"
        "\n"
        "commands:\n"
        "  init                 create or refresh the Vodka prefix\n"
        "  status               report prefix and Android root status\n"
        "  probe [android-root] validate and inspect an Android root\n"
        "  env                  print runtime environment paths\n"
        "\n"
        "default prefix: $VODKA_PREFIX or $HOME/.vodka\n",
        program);
}

int main(int argc, char **argv)
{
    char prefix[VODKA_PATH_MAX];
    if (!default_prefix(prefix, sizeof(prefix))) {
        return 2;
    }

    struct vodka_cli cli = {
        .program = argv[0],
        .prefix = prefix,
    };

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
