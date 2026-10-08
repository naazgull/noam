#pragma once

#include <ftxui/dom/elements.hpp>
#include <string>
#include <vector>

namespace noam {

// Renders `_text` as markdown — headings, emphasis, code spans, links,
// GFM pipe tables, thematic breaks (`---`/`***`/`___`, 3 or more, rendered
// as a full-width rule) — via tree-sitter-markdown, word-wrapped to
// `_width` columns. Within a paragraph a '\n' is a soft break (the lines
// are joined and re-wrapped); a hard break (two trailing spaces or a
// trailing backslash) and a new block start a new line. A fenced code block with a recognized
// declared language (```cpp/c++/cxx/hpp, ```json, ```lua, ```python/py,
// ```rust/rs, ```diff/patch) has its content syntax-highlighted via that
// language's own tree-sitter grammar (language injection, same mechanism as
// inline emphasis/links); any other fence (unrecognized or no declared
// language) renders as plain text — it is never reinterpreted as markdown
// prose, since code routinely contains markdown-significant characters.
// Returns one Element per rendered terminal row, so a wrapped paragraph
// comes back as several one-line rows rather than a single multi-line
// Element — scrolling moves by row index.
auto render_markdown_lines(std::string const& _text, int _width) -> ftxui::Elements;

} // namespace noam
