# Openr

A fast, minimal, fully configurable file opener for Linux and macOS — written in C.

`openr` is a smarter `xdg-open`. Instead of relying on a desktop environment to decide what opens your files, you define exactly which applications handle which file types, with support for multiple fallback commands, pager mode, background launching, and interactive selection — all driven by a single TOML config file.

``` sh
openr video.mp4          # opens with first matched command
openr --list video.mp4   # shows a numbered menu to pick the app
openr --mime report.pdf  # forces MIME-type detection instead of extension
```

---

## Features

- **Extension-based lookup** — maps file extensions to named command groups
- **MIME-type fallback** — uses `file --mime-type` when extension is unknown or `--mime` is passed
- **Named command groups** — define reusable groups like `video_default` once, reference them everywhere
- **Per-app flags** — `fork` (detach from terminal), `silent` (suppress output), `pager` (pipe through `$PAGER`)
- **Interactive mode** — `--list` shows all available commands and lets you pick one
- **Inline app lists** — per-extension overrides for formats that need special handling (e.g. `.svg`, `.torrent`)
- **Secure MIME detection** — uses `fork`+`execvp`, not `popen`, so filenames with special characters are safe
- **XDG-aware config** — reads from `$XDG_CONFIG_HOME/openr/config.toml`, falls back to `~/.config/openr/config.toml`
- **Levelled logging** — `--log-level=debug|info|warn|error` for troubleshooting

---

## Installation

### Dependencies

- A C17-capable compiler (`gcc` or `clang`)
- GNU `make`
- `file` command (standard on all Unix systems)
- On macOS: `argp-standalone` — install via `brew install argp-standalone`

### Build

```sh
git clone https://github.com/yourusername/openr
cd openr
make
```

### Install system-wide

```sh
sudo make install          # installs to /usr/local/bin/openr
```

Custom prefix:

```sh
sudo make install PREFIX=/usr
```

### Install to user directory

```sh
make install PREFIX=~/.local
```

### Debug build

```sh
make O_DEBUG=1
```

### Uninstall

```sh
sudo make uninstall
```

---

## Config file

A config [example](https://github.com/pritam12426/opener/blob/main/etc/config.toml)

On first run, place your config at:

``` sh
~/.config/openr/config.toml
```

Or, if `$XDG_CONFIG_HOME` is set:

``` sh
$XDG_CONFIG_HOME/openr/config.toml
```

A ready-to-use default config is installed to `PREFIX/etc/openr/config.toml` and is used as a system-wide fallback.

### Structure

The config has three sections: `[defaults]`, `[extension]`, and `[mimetype]`.

#### `[defaults]` — named command groups

Each key is a group name. Its value is an array of app entries. Each entry has a `command` (string or array) and optional flags:

| Flag     | Effect |
|----------|--------|
| `fork`   | Launch in background, return immediately |
| `silent` | Redirect stdout and stderr to `/dev/null` |
| `pager`  | Pipe output through `$PAGER` (default: `less`) |

Config [example](https://github.com/pritam12426/opener/blob/main/etc/config.toml)

```toml
[defaults]
    video_default = [
        { command = ["mpv", "--"],                              fork = true, silent = true },
        { command = ["ffplay", "-loop", "0"],                  fork = true, silent = true },
        { command = ["mediainfo"],                             pager = true }
    ]

    text_default = [
        { command = ["nvim"] },
        { command = ["zed"],                                   fork = true, silent = true },
        { command = ["bat", "--paging=always"],                pager = true }
    ]
```

#### `[extension]` — map extensions to groups

Simple mappings reference a group name by string:

```toml
[extension]
    mp4  = "video_default"
    mkv  = "video_default"
    rs   = "text_default"
    toml = "text_default"
```

For extensions that need their own unique app list, use a subtable with `app_list`. **These must come after all simple key = "group" entries in the file** (this is a TOML ordering requirement):

```toml
[extension.svg]
    app_list = [
        { command = ["qlmanage", "-p", "--"] },
        { command = ["inkscape"], fork = true, silent = true }
    ]

[extension.torrent]
    app_list = [
        { command = ["open", "-a", "qBittorrent"], silent = true }
    ]
```

#### `[mimetype]` — MIME type fallbacks

Used when no extension match is found, or when `--mime` is passed. Supports major-type wildcards (`text`, `video`, `image`, `audio`) and exact subtype matches:

```toml
[mimetype]
    [mimetype.text]
        inherit = "text_default"

    [mimetype.video]
        inherit = "video_default"

    [mimetype.application]
        [mimetype.application.subtype]
            [mimetype.application.subtype.octet-stream]
                inherit = "video_default"
```

---

## Usage

```
openr [OPTIONS] FILE
```

| Option | Description |
|--------|-------------|
| `FILE` | Path to the file to open |
| `-l`, `--list` | Show all available commands interactively |
| `-M`, `--mime` | Force MIME-type detection (skip extension lookup) |
| `-L LEVEL`, `--log-level=LEVEL` | Set log verbosity: `error`, `warn`, `info`, `debug` |
| `-V`, `--version` | Print version |
| `-?`, `--help` | Show help |

### Examples

```sh
# Open a file with the first matched command
openr document.pdf

# Show all handlers and pick one
openr --list video.mkv

# Force MIME detection (useful for files without extensions)
openr --mime somefile

# Debug why a file isn't being matched
openr --log-level=debug mystery.bin
```

---

## How lookup works

1. Extract the file extension (e.g. `mp4` from `video.mp4`)
2. Look it up in `[extension]` — if found, use that group
3. If not found (or `--mime` is set), run `file --mime-type` on the file
4. Try an exact MIME match (e.g. `video/mp4`), then a major-type match (`video`)
5. If still nothing, exit with an error

In normal mode the first command in the matched group is run. With `--list`, a numbered menu is shown.

---

## Credits

- TOML parsing by [tomlc99](https://github.com/cktan/tomlc99) (MIT License, © CK Tan)
- Config format inspired by [joshuto](https://github.com/kamiyaa/joshuto)'s mimetype config
- `argp` argument parsing — uses [argp-standalone](https://github.com/argp-standalone/argp-standalone) on macOS

---

## License

MIT
