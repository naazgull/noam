# noam-markdown: isolated Markdown + tree-sitter rendering library

Centralise noam's Markdown rendering — including per-language tree-sitter syntax
highlighting — into a reusable shared library, so any ftxui + tree-sitter TUI can
render Markdown without copying `text_render.cpp`, the syntax theme, or the grammar
build files. The motivating consumer is `pjotr`, whose `shell` panel currently carries
a 7-language duplicate of the same code.

## 1. Decisions

- **Sharing model.** `noam` builds the library and installs a CMake package. Consumers
  pull it in via `ExternalProject` + `find_package(noam-markdown)`. `noam` authors
  `noam-markdownConfig.cmake` so the imported target resolves.
- **Scope: zapata-free.** The library depends only on ftxui + tree-sitter core + the
  grammar `.so`s. It includes the row-windowed `pager` and the `syntax_theme`/theme
  singleton. It **excludes** the zapata-json theme loader, which stays per-application
  (each app maps its own theme JSON onto `noam::syntax_theme`).
- **Namespace.** Stays in `noam` (the same symbols are re-homed into a lib target), so
  the 2131-line `text_render.cpp` and `main.cpp` change almost nothing.

## 2. Public API

Three installed headers, all in `namespace noam`.

### `noam/text_render.h`

```cpp
auto render_markdown_lines(std::string const& _text, int _width) -> ftxui::Elements;
```

Renders `_text` as Markdown (headings, emphasis, code spans, links, GFM pipe tables,
thematic breaks) via tree-sitter-markdown, word-wrapped to `_width` columns, returning
**one `ftxui::Element` per terminal row** (so scrolling moves by row index). A fenced
block with a recognized declared language is syntax-highlighted via that language's own
tree-sitter grammar (language injection); any other fence renders as plain text and is
never reinterpreted as Markdown. Highlightable fence languages currently:
`cpp`/`c++`/`cxx`/`hpp`, `json`, `lua`, `python`/`py`, `rust`/`rs`, `diff`/`patch`,
`bash`/`sh`, `sql`, `fish`, `prolog`.

### `noam/theme.h`

```cpp
struct syntax_theme {
    ftxui::Color __comment, __quote_bar, __quote_background, __fenced_block_background,
                 __escape, __string_special_key, __string_special, __constant_builtin,
                 __number, __function, __preproc, __variable_builtin, __variable,
                 __markup_strong, __markup_italic, __markup_code, __markup_link,
                 __markup_heading, __markup_heading_1 /*…_6*/, __markup_ruler, __type,
                 __namespace, __string, __keyword, __punctuation_special,
                 __table_border, __table_header, __diff_addition, __diff_removal,
                 __diff_header;   // 35 fields, each defaulting to a sensible color
};
struct noam_theme { syntax_theme __syntax; };

auto theme() -> noam_theme&;         // the in-effect theme (mutable singleton)
auto set_theme(noam_theme _theme) -> void;
```

### `noam/pager.h`

```cpp
class pager {
  public:
    explicit pager(std::string _content);
    auto rows(int _width, int _height) -> ftxui::Elements; // at most _height rows
    auto scroll(int _delta) -> void;   // negative is up
    auto home() -> void;
    auto end() -> void;
};
```

A continuous, row-by-row view that only renders the pages around the current position
(a large document costs no more per frame than a small one).

## 3. Library contents

| In the library (built + installed) | App-only (stays in noam, not installed) |
|---|---|
| `src/pager.cpp` | `src/document.cpp`, `src/main.cpp` |
| `src/text_render.cpp` | `src/theme_json.cpp` (zapata-json `set_theme(zpt::json)` + `render_color`) |
| `src/theme.cpp` (singleton) | `include/noam/document.h` |
| `include/noam/{text_render,theme,pager}.h` | `include/noam/theme_json.h` |

The library is grep-verified free of `zapata` / `zpt::`.

## 4. CMake packaging

- `add_library(noam-markdown SHARED …)` from the three sources.
  - `PUBLIC`: `ftxui-component`, `ftxui-dom`, `ftxui-screen`, `Threads::Threads` (the
    headers expose ftxui types).
  - `PRIVATE`: `tree-sitter-lib` + the 12 grammar targets (implementation detail).
  - `add_dependencies(noam-markdown ftxui)` (the ftxui imported targets carry no
    build-order dependency on the ExternalProject).
- The `noam` executable compiles `document.cpp` + `main.cpp` + `theme_json.cpp`, links
  `noam-markdown` (PRIVATE) + the `zapata-*` targets.
- Install: `libnoam-markdown.so` → `${CMAKE_INSTALL_LIBDIR}`; the 3 headers →
  `${CMAKE_INSTALL_INCLUDEDIR}/noam`; `noam-markdownConfig.cmake` →
  `${CMAKE_INSTALL_LIBDIR}/cmake/noam-markdown`.

`noam-markdownConfig.cmake` is **self-locating**: it anchors on its own libdir
(`CMAKE_CURRENT_LIST_DIR/../..`, so it is correct for both a plain `lib` and a
multi-arch `lib/<triplet>`), defines `noam::markdown` plus
`noam::markdown::{tree-sitter, tree-sitter-<lang>}` imported targets for the 13
transitive `.so`s, and deliberately **leaves ftxui to the consumer** so it cannot
clash with the consumer's own ftxui targets.

## 5. Theming (per application)

The library only exposes `noam::syntax_theme` + `noam::set_theme(noam_theme)`.

- **noam** keeps its zapata-json loader in `theme_json.{h,cpp}`:
  `noam::set_theme(zpt::json const&)` maps the JSON `"syntax"` section onto
  `noam::theme().__syntax` (entries left out keep their value), then calls the
  library's `set_theme`.
- **pjotr** maps the `syntax` section of its `shell_theme` the same way — it already
  wraps an identical `syntax_theme` — and keeps its own zapata-json loader for the
  panel / overlay / spinner colors.

## 6. How pjotr uses it

1. Add `noam` as an `ExternalProject` (builds + installs the lib, headers, and Config
   into pjotr's prefix).
2. `find_package(noam-markdown REQUIRED PATHS <prefix>/lib/cmake/noam-markdown)`.
3. In `pjotr-shell`, delete the local `text_render.cpp` + the syntax part of
   `theme.cpp`, the `PJOTR_SHELL_TREE_SITTER_GRAMMARS` variable, and the
   `cmake/tree-sitter-*.cmake` includes (the grammars now come from the package).
4. Provide the panel renderer with the core API:
   `panel::line_renderer_type = std::function<ftxui::Elements(std::string const&, int)>`
   → set it to `noam::render_markdown_lines`.
5. Theme through `noam::set_theme(...)` (map pjotr's syntax colors onto
   `noam::syntax_theme`).
6. Link `noam::markdown` + pjotr's own ftxui + zapata.

## 7. Verification (done, isolated in `/tmp/noam-verify`)

- **Lib builds zapata-free:** the three sources compile clean (`-Wall`, no warnings) and
  link into `libnoam-markdown.so`; `DT_NEEDED` = ftxui(3) + tree-sitter core + all 12
  grammars.
- **Package end-to-end:** a throwaway consumer that only `find_package(noam-markdown)` +
  links `noam::markdown` (providing its own ftxui) configures, links, and runs —
  `render_markdown_lines`, `pager.rows`, and `theme()` all work; a `prolog` fence is
  exercised at runtime; exit 0.
- **RPATH nuance:** Linux `DT_RUNPATH` is not inherited, so a consumer's RPATH does not
  resolve the lib's *transitive* grammar deps — the lib must carry its own RPATH to the
  prefix lib dirs. noam's `CMakeLists.txt` already sets this (`CMAKE_INSTALL_RPATH` =
  prefix `bin`/`lib`/`lib64` + `CMAKE_INSTALL_RPATH_USE_LINK_PATH ON`), so the real
  CMake-built lib gets it automatically. No noam-side change is required.

## 8. Remaining work

- **Full noam build + `cmake --install`** — needs zapata, so it is the author's build.
  That installs the lib, headers, and Config into the real prefix (with the correct
  RPATH).
- **pjotr-side wiring** — the six steps in §6.
- **Minor:** the fence-language list in `text_render.h`'s doc comment is stale — it
  names `cpp`/`json`/`lua`/`python`/`rust`/`diff` but the code also highlights
  `bash`, `sql`, `fish`, and `prolog`. Can be corrected when convenient.

## 9. Constraints honored

No `git commit`; zapata untouched; all verification isolated under `/tmp/noam-verify`
(re-runnable); the real `/opt/noam` install was not modified.
