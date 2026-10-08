# noam

A terminal (TUI) reader for Markdown documents. Give it a local file or a URL and it
renders in your terminal — headings, emphasis, inline code, links, block quotes, GFM
pipe tables, and syntax-highlighted fenced code blocks — with a configurable color theme
and page-based scrolling.

## Features

- Full-screen alternate-screen viewer with paging (arrows, PgUp/PgDn, space, Home/End)
- Markdown rendered with [tree-sitter](https://github.com/tree-sitter/tree-sitter); raw
  markup is stripped so the output reads WYSIWYG-ish (`**bold**` shows as *bold*, not the
  asterisks)
- Fenced code blocks syntax-highlighted via each language's own tree-sitter grammar
  (C/C++, JSON, Lua, Python, Rust, diff/patch); unrecognized fences render as plain text
- GFM pipe tables drawn with box-drawing borders, column alignment, and word-wrapping
- Color themes, configurable in JSON (see [Themes](#themes))

## Dependencies

`noam` is written in C++20. To build it you need:

- CMake ≥ 3.18
- a C++20 compiler (GCC/Clang)
- the `tree-sitter` CLI, used to generate the grammar parsers
- `git` and a `make`-style generator (used by CMake to fetch the build dependencies)
- an internet connection (the build dependencies are cloned from GitHub)

The build targets x86-64.

## Build dependencies

The projects below are fetched and built from source automatically by CMake at configure
time (via `ExternalProject`) — nothing to install manually:

- **tree-sitter** (v0.26.13) — the runtime library the grammars and `noam` link against
- the **tree-sitter grammars** (generated with the `tree-sitter` CLI above): `markdown`
  (v0.5.3, including `markdown-inline`), `json`, `cpp`, `lua`, `python`, `rust`, `diff`
- **FTXUI** (v7.0.3) — the terminal UI toolkit (screen, dom, component)
- **zapata** — JSON config, HTTP retrieval, and URI parsing

## Building

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/opt/noam
cmake --build build -j"$(nproc)"
cmake --install build        # installs the binary + theme configuration
```

`-DCMAKE_INSTALL_PREFIX` must point to a directory you can write to (`/opt/noam` requires
root/sudo — pick any other prefix if you'd rather not). `cmake --install` places both the
binary (`<prefix>/bin/noam`) and the theme configuration (`<prefix>/share/noam/...`), which
the app reads at runtime.

## Running

```sh
noam <document>
```

`<document>` is a local path or an `http(s)://` URL:

```sh
noam README.md
noam ./notes/2026.md
noam https://example.com/guide.md
```

Navigation: `↑`/`↓` scroll by one line, `PgUp`/`PgDn` or `space` page up/down, `Home`/`End`
jump to the top/bottom, and `q` or `Esc` quits.

## Themes

The active theme is resolved from `<prefix>/share/noam/default.conf`, which references
`theme.conf`, which in turn references one of the bundled themes in `themes/` (the default
is `pjotr`). Bundled themes: catppuccin-mocha, dracula, gruvbox-dark, monokai, nord,
one-dark, pjotr, solarized-dark.

Each color can be set as a name (e.g. `red`), an `[r, g, b]` array, or a `{"r","g","b"}`
object. If the configuration can't be read, `noam` still renders the document — just in the
default (white) colors.
