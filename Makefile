CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
BUILD_DIR ?= build

VODKA := $(BUILD_DIR)/vodka

.PHONY: all check clean

all: $(VODKA)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(VODKA): src/vodka.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< -o $@

check: all
	./scripts/validate_android_root.sh
	$(VODKA) --prefix /tmp/vodka-check-unused probe android_root
	set -e; \
	tmpdir=$$(mktemp -d /tmp/vodka-prefix.XXXXXX); \
	$(VODKA) --prefix "$$tmpdir" init; \
	$(VODKA) --prefix "$$tmpdir" status; \
	$(VODKA) --prefix "$$tmpdir" probe

clean:
	rm -rf $(BUILD_DIR)
