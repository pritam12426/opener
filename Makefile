UNAME_S := $(shell uname -s)

PREFIX ?= /usr/local
MANPREFIX ?= $(PREFIX)/share/man

STRIP ?= strip
PKG_CONFIG ?= pkg-config
INSTALL ?= install

CFLAGS_OPTIMIZATION ?= -O3

BUILD = build
BIN   = openr

HEADERS   = $(wildcard src/*.h)
SRC       = $(wildcard src/*.c)
OUT       = $(SRC:%.c=$(BUILD)/%.o)

ROOT_CONFIG = etc/config.toml

CFLAGS += -Isrc -std=c17 -DCOMPILED_TIME_PREFIX='"$(PREFIX)"'
LDLIBS += -lpthread

# convert targets to flags for backwards compatibility
O_DEBUG := 0  # debug binary
ifneq ($(filter debug,$(MAKECMDGOALS)),)
	O_DEBUG := 1
endif

ifeq ($(strip $(O_DEBUG)),1)
	CFLAGS += -g3 -DDEBUG -DLOG_SHOW_SOURCE_LOCATION
    ifneq (,$(findstring clang,$(CC)))
        CFLAGS += -ffreestanding
    endif
else
	CFLAGS += $(CFLAGS_OPTIMIZATION)
endif

# Check if the OS is macOS
ifeq ($(UNAME_S),Darwin)
    LDLIBS += -largp
else # Else every thing is linux
    CFLAGS += -D_GNU_SOURCE
endif


all: $(BIN)

help: ## Show this help
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[33m%-20s\033[0m %s\n", $$1, $$2}'

$(BUILD): ## Create build directories automatically
	mkdir -p $(BUILD)

$(BUILD)/%.o: %.c $(SHARED_HDR) $(DAEMON_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN): $(SRC) $(OUT) $(HEADERS) ## Build the openr binary
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OUT) $(LDLIBS)

debug: $(BIN) ## Build the debug binary run `make debug -B O_DEBUG=1`

install: all ## Install the openr binary
	$(INSTALL) -m 0755 -d $(DESTDIR)$(PREFIX)/bin
	$(INSTALL) -m 0755 $(BIN) $(DESTDIR)$(PREFIX)/bin

	$(INSTALL) -m 0755 -d $(DESTDIR)$(PREFIX)/etc/opner
	$(INSTALL) -m 0644 $(ROOT_CONFIG) $(DESTDIR)$(PREFIX)/etc/opner

clean: ## Clean up build artifacts
	$(RM) -f $(OUT) $(BIN)

uninstall: ## Uninstall the openr binary & root config
	$(RM) $(DESTDIR)$(PREFIX)/bin/$(BIN)
	$(RM) -r $(DESTDIR)$(PREFIX)/etc/opner

strip: $(BIN) ## Strip the openr binary
	$(STRIP) $^

.PHONY: all install uninstall strip clean
