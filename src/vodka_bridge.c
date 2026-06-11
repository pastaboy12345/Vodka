#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define VODKA_PATH_MAX 4096

#ifndef VODKA_EXPECTED_SERVICE
#define VODKA_EXPECTED_SERVICE ""
#endif

struct bridge_definition {
    const char *name;
    const char *binder_name;
    bool required;
};

struct bridge_context {
    const struct bridge_definition *definition;
    const char *service;
    const char *binder_service;
    const char *binder_device;
    const char *prefix;
    const char *android_root;
    const char *packages_xml;
    const char *packages_list;
    const char *global_state;
    const char *mode;
    char packages_xml_storage[VODKA_PATH_MAX];
    char packages_list_storage[VODKA_PATH_MAX];
    char state_path[VODKA_PATH_MAX];
    size_t package_count;
    bool binder_ready;
    bool binder_openable;
};

static volatile sig_atomic_t stop_requested = 0;

static const struct bridge_definition bridge_definitions[] = {
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

static void handle_stop_signal(int signal_number)
{
    (void)signal_number;
    stop_requested = 1;
}

static bool copy_string(char *dest, size_t dest_size, const char *src)
{
    if (dest_size == 0) {
        return false;
    }

    if (src == NULL) {
        dest[0] = '\0';
        return true;
    }

    const size_t len = strlen(src);
    if (len >= dest_size) {
        return false;
    }

    memcpy(dest, src, len + 1);
    return true;
}

static const char *env_value(const char *key)
{
    const char *value = getenv(key);

    return value == NULL ? "" : value;
}

static bool is_empty(const char *value)
{
    return value == NULL || value[0] == '\0';
}

static const struct bridge_definition *find_bridge_definition(const char *service)
{
    for (size_t i = 0; i < sizeof(bridge_definitions) / sizeof(bridge_definitions[0]); i++) {
        if (strcmp(bridge_definitions[i].name, service) == 0) {
            return &bridge_definitions[i];
        }
    }

    return NULL;
}

static bool path_exists(const char *path)
{
    struct stat stat_buffer;

    return !is_empty(path) && stat(path, &stat_buffer) == 0;
}

static const char *binder_device_kind(const char *path)
{
    struct stat stat_buffer;

    if (is_empty(path)) {
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

static bool binder_device_ready(const char *path)
{
    const char *kind = binder_device_kind(path);

    return strcmp(kind, "char") == 0 || strcmp(kind, "regular") == 0;
}

static bool binder_device_openable(const char *path)
{
    if (!binder_device_ready(path)) {
        return false;
    }

    int flags = O_RDWR;
#ifdef O_CLOEXEC
    flags |= O_CLOEXEC;
#endif

    const int fd = open(path, flags);
    if (fd < 0) {
        return false;
    }

    close(fd);
    return true;
}

static bool mkdir_p(const char *path, mode_t mode)
{
    char partial[VODKA_PATH_MAX];

    if (is_empty(path)) {
        return false;
    }
    if (!copy_string(partial, sizeof(partial), path)) {
        fprintf(stderr, "path too long: %s\n", path);
        return false;
    }

    size_t len = strlen(partial);
    while (len > 1 && partial[len - 1] == '/') {
        partial[--len] = '\0';
    }

    for (char *cursor = partial + 1; *cursor != '\0'; cursor++) {
        if (*cursor != '/') {
            continue;
        }

        *cursor = '\0';
        if (mkdir(partial, mode) != 0 && errno != EEXIST) {
            fprintf(stderr, "failed to create %s: %s\n", partial, strerror(errno));
            *cursor = '/';
            return false;
        }
        *cursor = '/';
    }

    if (mkdir(partial, mode) != 0 && errno != EEXIST) {
        fprintf(stderr, "failed to create %s: %s\n", partial, strerror(errno));
        return false;
    }

    return true;
}

static bool ensure_parent_dir(const char *path)
{
    char parent[VODKA_PATH_MAX];

    if (!copy_string(parent, sizeof(parent), path)) {
        fprintf(stderr, "path too long: %s\n", path);
        return false;
    }

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

static bool build_android_root_path(
    char *dest,
    size_t dest_size,
    const char *android_root,
    const char *relative)
{
    if (is_empty(android_root)) {
        dest[0] = '\0';
        return false;
    }

    const int written = snprintf(dest, dest_size, "%s/%s", android_root, relative);
    return written >= 0 && (size_t)written < dest_size;
}

static const char *path_from_env_or_root(
    char *storage,
    size_t storage_size,
    const char *env_key,
    const char *android_root,
    const char *relative)
{
    const char *value = getenv(env_key);
    if (!is_empty(value)) {
        return value;
    }

    if (!build_android_root_path(storage, storage_size, android_root, relative)) {
        return "";
    }

    return storage;
}

static bool build_service_state_path(char *dest, size_t dest_size, const struct bridge_context *context)
{
    const int written = snprintf(
        dest,
        dest_size,
        "%s/data/system/vodka-service-%s.state",
        context->android_root,
        context->service);

    return written >= 0 && (size_t)written < dest_size;
}

static bool is_blank_line(const char *line)
{
    for (const char *cursor = line; *cursor != '\0'; cursor++) {
        if (!isspace((unsigned char)*cursor)) {
            return false;
        }
    }

    return true;
}

static size_t count_package_list_entries(const char *path)
{
    if (is_empty(path)) {
        return 0;
    }

    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return 0;
    }

    size_t count = 0;
    char line[4096];
    while (fgets(line, sizeof(line), file) != NULL) {
        if (line[0] == '#' || is_blank_line(line)) {
            continue;
        }
        count++;
    }

    fclose(file);
    return count;
}

static bool validate_env_match(const char *env_key, const char *expected)
{
    const char *value = getenv(env_key);

    if (is_empty(value)) {
        return true;
    }
    if (strcmp(value, expected) == 0) {
        return true;
    }

    fprintf(stderr, "%s=%s does not match %s\n", env_key, value, expected);
    return false;
}

static bool validate_context(struct bridge_context *context)
{
    if (strcmp(VODKA_EXPECTED_SERVICE, "") != 0 &&
        strcmp(VODKA_EXPECTED_SERVICE, context->service) != 0) {
        fprintf(
            stderr,
            "this bridge was built for service %s, not %s\n",
            VODKA_EXPECTED_SERVICE,
            context->service);
        return false;
    }

    context->definition = find_bridge_definition(context->service);
    if (context->definition == NULL) {
        fprintf(stderr, "unknown service bridge: %s\n", context->service);
        return false;
    }
    if (strcmp(context->definition->binder_name, context->binder_service) != 0) {
        fprintf(
            stderr,
            "binder service mismatch for %s: expected %s, got %s\n",
            context->service,
            context->definition->binder_name,
            context->binder_service);
        return false;
    }

    if (is_empty(context->android_root)) {
        fprintf(stderr, "VODKA_ANDROID_ROOT is required; run bridges with vodka start-services\n");
        return false;
    }
    if (!path_exists(context->android_root)) {
        fprintf(stderr, "Android root does not exist: %s\n", context->android_root);
        return false;
    }

    if (!validate_env_match("VODKA_SERVICE", context->service) ||
        !validate_env_match("VODKA_BINDER_SERVICE", context->binder_service) ||
        !validate_env_match("VODKA_BINDER_DEVICE", context->binder_device)) {
        return false;
    }

    if (!build_service_state_path(context->state_path, sizeof(context->state_path), context)) {
        fprintf(stderr, "service state path is too long\n");
        return false;
    }

    context->binder_ready = binder_device_ready(context->binder_device);
    context->binder_openable = binder_device_openable(context->binder_device);
    context->package_count = count_package_list_entries(context->packages_list);

    if (!context->binder_openable) {
        fprintf(
            stderr,
            "binder device is not openable: %s kind=%s\n",
            context->binder_device,
            binder_device_kind(context->binder_device));
        return false;
    }

    return true;
}

static void print_bridge_state(FILE *file, const struct bridge_context *context, const char *status)
{
    fprintf(file, "service=%s\n", context->service);
    fprintf(file, "binder_service=%s\n", context->binder_service);
    fprintf(file, "required=%s\n", context->definition->required ? "yes" : "no");
    fprintf(file, "binder_device=%s\n", context->binder_device);
    fprintf(file, "binder_kind=%s\n", binder_device_kind(context->binder_device));
    fprintf(file, "binder_ready=%s\n", context->binder_ready ? "yes" : "no");
    fprintf(file, "binder_openable=%s\n", context->binder_openable ? "yes" : "no");
    fprintf(file, "prefix=%s\n", context->prefix);
    fprintf(file, "android_root=%s\n", context->android_root);
    fprintf(file, "packages_xml=%s\n", context->packages_xml);
    fprintf(file, "packages_xml_present=%s\n", path_exists(context->packages_xml) ? "yes" : "no");
    fprintf(file, "packages_list=%s\n", context->packages_list);
    fprintf(file, "packages_list_present=%s\n", path_exists(context->packages_list) ? "yes" : "no");
    fprintf(file, "package_count=%zu\n", context->package_count);
    fprintf(file, "global_state=%s\n", context->global_state);
    fprintf(file, "global_state_present=%s\n", path_exists(context->global_state) ? "yes" : "no");
    fprintf(file, "mode=%s\n", context->mode);
    fprintf(file, "status=%s\n", status);
}

static bool write_bridge_state(const struct bridge_context *context, const char *status)
{
    if (!ensure_parent_dir(context->state_path)) {
        return false;
    }

    FILE *file = fopen(context->state_path, "w");
    if (file == NULL) {
        fprintf(stderr, "failed to create %s: %s\n", context->state_path, strerror(errno));
        return false;
    }

    fputs("# Vodka service bridge runtime state\n", file);
    print_bridge_state(file, context, status);

    if (fclose(file) != 0) {
        fprintf(stderr, "failed to close %s: %s\n", context->state_path, strerror(errno));
        return false;
    }

    return true;
}

static void run_serve_loop(const struct bridge_context *context)
{
    signal(SIGINT, handle_stop_signal);
    signal(SIGTERM, handle_stop_signal);

    while (!stop_requested) {
        sleep(1);
    }

    (void)write_bridge_state(context, "stopped");
}

static void print_usage(const char *program)
{
    fprintf(stderr, "usage: %s SERVICE BINDER_SERVICE BINDER_DEVICE\n", program);
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        print_usage(argv[0]);
        return 2;
    }

    struct bridge_context context = {
        .service = argv[1],
        .binder_service = argv[2],
        .binder_device = argv[3],
        .prefix = env_value("VODKA_PREFIX"),
        .android_root = env_value("VODKA_ANDROID_ROOT"),
        .global_state = env_value("VODKA_SERVICE_STATE"),
        .mode = env_value("VODKA_BRIDGE_MODE"),
    };

    if (is_empty(context.mode)) {
        context.mode = "oneshot";
    }

    context.packages_xml = path_from_env_or_root(
        context.packages_xml_storage,
        sizeof(context.packages_xml_storage),
        "VODKA_PACKAGES_XML",
        context.android_root,
        "data/system/packages.xml");
    context.packages_list = path_from_env_or_root(
        context.packages_list_storage,
        sizeof(context.packages_list_storage),
        "VODKA_PACKAGES_LIST",
        context.android_root,
        "data/system/packages.list");

    if (!validate_context(&context)) {
        return 1;
    }

    const char *status = strcmp(context.mode, "serve") == 0 ? "serving" : "ready";
    if (!write_bridge_state(&context, status)) {
        return 1;
    }

    print_bridge_state(stdout, &context, status);
    printf("state=%s\n", context.state_path);
    fflush(stdout);

    if (strcmp(context.mode, "serve") == 0) {
        run_serve_loop(&context);
    }

    return 0;
}
