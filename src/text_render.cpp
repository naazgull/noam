#include <array>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/string.hpp>
#include <memory>
#include <noam/text_render.h>
#include <noam/theme.h>
#include <optional>
#include <string_view>
#include <tree_sitter/api.h>
#include <utility>

// Grammar entry points — every tree-sitter grammar library exports exactly
// one C function named `tree_sitter_<language>`, part of the stable,
// documented ABI, declared here by hand rather than through each grammar's
// own (installed) header.
extern "C" const TSLanguage* tree_sitter_json(void);
extern "C" const TSLanguage* tree_sitter_cpp(void);
extern "C" const TSLanguage* tree_sitter_lua(void);
extern "C" const TSLanguage* tree_sitter_python(void);
extern "C" const TSLanguage* tree_sitter_rust(void);
extern "C" const TSLanguage* tree_sitter_diff(void);
extern "C" const TSLanguage* tree_sitter_markdown(void);
extern "C" const TSLanguage* tree_sitter_markdown_inline(void);

namespace {
// A full-width horizontal rule of `_separator` glyphs in `_color`.
auto rule_element(int _width, ftxui::Color const& _color, std::string const& _separator = "─")
  -> ftxui::Element {
    std::string _line;
    _line.reserve(static_cast<size_t>(_width) * 3);
    for (int _i = 0; _i < _width; ++_i) { _line += _separator; }
    return ftxui::text(_line) | ftxui::color(_color);
}

// Which tree-sitter grammar/highlight query a fenced code block's declared
// language maps to (see language_for_fence_name) — internal to this file
// only; the public API (render_markdown_lines, text_render.h) has no
// per-language surface, since the only remaining consumer of these
// non-markdown grammars is fenced-code-block injection.
enum class language {
    JSON,
    CPP,
    LUA,
    PYTHON,
    RUST,
    DIFF,
    MARKDOWN,
};

// Every tree-sitter handle this file creates (via ts_parser_new,
// ts_query_new, ts_parser_parse_string, ts_query_cursor_new) is memory this
// code is responsible for freeing — none of it is left as a bare raw
// pointer; these aliases pair each handle type with its matching
// ts_*_delete function as a std::unique_ptr custom deleter.
using ts_parser_ptr = std::unique_ptr<TSParser, void (*)(TSParser*)>;
using ts_query_ptr = std::unique_ptr<TSQuery, void (*)(TSQuery*)>;
using ts_tree_ptr = std::unique_ptr<TSTree, void (*)(TSTree*)>;
using ts_query_cursor_ptr = std::unique_ptr<TSQueryCursor, void (*)(TSQueryCursor*)>;

// The official highlights query for tree-sitter-json (from
// /usr/share/tree-sitter/queries/json/highlights.scm), embedded rather than
// read from disk so this doesn't depend on that exact system path.
constexpr char JSON_HIGHLIGHTS_QUERY[] = R"scm(
(pair
  key: (_) @string.special.key)

(string) @string

(number) @number

[
  (null)
  (true)
  (false)
] @constant.builtin

(escape_sequence) @escape

(comment) @comment
)scm";

// Hand-written — covers comments, literals, a conservative subset of
// keywords, primitive/auto types, namespace identifiers,
// function/method names and calls, and a catch-all @variable capture for
// any other identifier. capture_priority ranks @variable below every other
// capture so it never overrides a more specific one landing on the same
// node (e.g. a call's callee identifier is both @variable and,
// more specifically, @function).
constexpr char CPP_HIGHLIGHTS_QUERY[] = R"scm(
(comment) @comment

(string_literal) @string
(raw_string_literal) @string
(char_literal) @string

(number_literal) @number

[
  (true)
  (false)
] @constant.builtin

(this) @variable.builtin

[
  "if" "else" "switch" "case" "default"
  "while" "do" "for"
  "break" "continue" "return" "goto"
  "class" "struct" "enum" "union" "namespace"
  "public" "private" "protected"
  "template" "typename"
  "const" "static" "extern" "virtual" "override" "final" "explicit" "inline" "friend" "mutable" "constexpr" "volatile"
  "new" "delete" "try" "catch" "throw"
  "sizeof"
  "using" "typedef"
] @keyword

(primitive_type) @type
(auto) @keyword
(namespace_identifier) @namespace

(function_declarator
  declarator: (identifier) @function)
(function_declarator
  declarator: (field_identifier) @function)
(function_declarator
  declarator: (qualified_identifier) @_qualified_function_name)

(call_expression
  function: (identifier) @function)
(call_expression
  function: (field_expression
    field: (field_identifier) @function))
(call_expression
  function: (qualified_identifier) @_qualified_function_name)

(preproc_include) @preproc
(preproc_def) @preproc
(preproc_ifdef) @preproc

(identifier) @variable
(field_identifier) @variable
)scm";

// Hand-written, deliberately simpler than the shipped lua.scm — predicate
// evaluation (#eq?/#any-of?/etc.) isn't implemented here, and that query
// depends on it for correct results (running it through this
// predicate-blind paint loop produced 3-4 overlapping, sometimes
// contradictory captures per identifier).
constexpr char LUA_HIGHLIGHTS_QUERY[] = R"scm(
(comment) @comment

(string) @string
(number) @number

[
  (true)
  (false)
] @constant.builtin
(nil) @constant.builtin

[
  "local" "global" "function" "end" "return" "goto" "in"
  "if" "then" "elseif" "else"
  "while" "do" "repeat" "until" "for"
] @keyword

(break_statement) @keyword

(function_call
  name: (identifier) @function)

(function_declaration
  name: (identifier) @function)
)scm";

// Hand-written against tree-sitter-python, same predicate-free reasoning as
// LUA_HIGHLIGHTS_QUERY above (the official query leans on #match? to tell
// e.g. a builtin call from an ordinary one, which this file's resolver
// never evaluates).
constexpr char PYTHON_HIGHLIGHTS_QUERY[] = R"scm(
(comment) @comment

(string) @string
(escape_sequence) @escape

(integer) @number
(float) @number

[
  (true)
  (false)
  (none)
] @constant.builtin

[
  "def" "return" "lambda"
  "if" "elif" "else"
  "for" "while" "break" "continue" "pass"
  "try" "except" "finally" "raise"
  "with" "as"
  "import" "from"
  "class"
  "global" "nonlocal"
  "assert" "yield" "async" "await" "del"
  "and" "or" "not" "in" "is"
] @keyword

(function_definition
  name: (identifier) @function)
(class_definition
  name: (identifier) @type)

(call
  function: (identifier) @function)
(call
  function: (attribute
    attribute: (identifier) @function))

(identifier) @variable
)scm";

// Hand-written against tree-sitter-rust, same predicate-free reasoning as
// PYTHON_HIGHLIGHTS_QUERY above.
constexpr char RUST_HIGHLIGHTS_QUERY[] = R"scm(
(line_comment) @comment
(block_comment) @comment

(string_literal) @string
(raw_string_literal) @string
(char_literal) @string

(integer_literal) @number
(float_literal) @number

(boolean_literal) @constant.builtin

(self) @variable.builtin

(mutable_specifier) @keyword
(crate) @keyword
(super) @keyword

[
  "fn" "let" "const" "static"
  "if" "else" "match" "loop" "while" "for" "in"
  "return" "break" "continue"
  "struct" "enum" "impl" "trait" "type" "where"
  "pub" "mod" "use" "as"
  "async" "await" "unsafe" "extern" "move" "ref" "dyn"
] @keyword

(primitive_type) @type
(type_identifier) @type

(function_item
  name: (identifier) @function)

(call_expression
  function: (identifier) @function)
(call_expression
  function: (field_expression
    field: (field_identifier) @function))

(macro_invocation
  macro: (identifier) @function)

(field_identifier) @variable
(identifier) @variable
)scm";

// The official highlights query for tree-sitter-diff (from
// /usr/share/tree-sitter/queries/diff/highlights.scm), embedded like
// JSON_HIGHLIGHTS_QUERY above. Its one predicate, `#set! priority`, is a
// directive this file's resolver already ignores safely — capture overlaps
// are resolved by capture_priority below instead.
constexpr char DIFF_HIGHLIGHTS_QUERY[] = R"scm(
(comment) @comment @spell

(addition) @diff.plus
(new_file) @diff.header

(deletion) @diff.minus
(old_file) @diff.header

(change) @diff.delta

(commit) @constant

(location) @diff.header

(command
  "diff" @function
  (argument) @variable.parameter)

(filename) @string.special.path

(special) @string.special

"\\" @punctuation.special

(mode) @number

([
  ".."
  "+"
  "++"
  "+++"
  "++++"
  ">"
  "-"
  "--"
  "---"
  "----"
  "<"
  "!"
] @punctuation.special
  (#set! priority 95))

[
  (binary_change)
  (similarity)
  (dissimilarity)
  (file_change)
] @label

(index
  "index" @keyword)

(similarity
  (score) @number
  "%" @number)

(dissimilarity
  (score) @number
  "%" @number)

(binary_patch
  [
    "GIT"
    "binary"
    "patch"
  ] @label)

(binary_hunk
  [
    "literal"
    "delta"
  ] @keyword
  (size) @number)

forward: (binary_hunk
  (payload) @diff.plus)

reverse: (binary_hunk
  (payload) @diff.minus)
)scm";

// Hand-written against tree-sitter-markdown. @_inline tags every (inline)
// node (paragraph bodies, heading content, ...) for the paint loop to
// re-parse with the inline grammar. @_hidden marks bytes to omit entirely
// from the rendered output (e.g. a setext underline) — a WYSIWYG-ish
// touch: the rendered document strips raw markup rather than showing it.
// @_hidden_marker is the same, but for a marker CommonMark requires exactly
// one mandatory space after (the atx "#") — resolve_markdown_spans
// additionally hides that one following space, which plain @_hidden
// deliberately doesn't (a "**bold**" closing delimiter is also immediately
// followed by a space, but that one's real content separation and must
// survive).
constexpr char MARKDOWN_HIGHLIGHTS_QUERY[] = R"scm(
(inline) @_inline

(atx_heading (atx_h1_marker) @_hidden_marker)
(atx_heading (atx_h2_marker) @_hidden_marker)
(atx_heading (atx_h3_marker) @_hidden_marker)
(atx_heading (atx_h4_marker) @_hidden_marker)
(atx_heading (atx_h5_marker) @_hidden_marker)
(atx_heading (atx_h6_marker) @_hidden_marker)
(atx_heading heading_content: (inline) @markup.heading)

(setext_heading heading_content: (paragraph (inline)) @markup.heading)
(setext_heading (setext_h1_underline) @_hidden)
(setext_heading (setext_h2_underline) @_hidden)

[
  (list_marker_minus)
  (list_marker_plus)
  (list_marker_star)
] @markup.list

(list_marker_dot) @punctuation.special

(fenced_code_block_delimiter) @punctuation.special
(info_string (language) @type)

(block_quote_marker) @_hidden_marker
(thematic_break) @punctuation.special

(link_label) @string.special
(link_destination) @string.special
(link_title) @string
)scm";

// Locates fenced code blocks within a document — separate from
// MARKDOWN_HIGHLIGHTS_QUERY above (which colors bytes in place) since this
// one drives structural extraction: pulling a block's declared language and
// code text out to route through a *different* grammar entirely (language
// injection, same idea as @_inline but selecting the target grammar at
// runtime from the fence's info string instead of always
// tree-sitter-markdown-inline). The `?` on the language capture means a
// fence with no declared language (bare ```) still matches, just without
// @fence_lang.
constexpr char MARKDOWN_FENCED_CODE_QUERY[] = R"scm(
(fenced_code_block
  (info_string (language) @fence_lang)?
  (code_fence_content) @fence_content) @fence_block
)scm";

// Locates GFM pipe tables — like MARKDOWN_FENCED_CODE_QUERY above, this
// drives structural extraction rather than in-place coloring, since a
// table's column widths depend on every cell in every row (find_pipe_tables
// below), which can't be resolved one physical line at a time the way
// ordinary prose is. The matched node's shape — one pipe_table_header, one
// pipe_table_delimiter_row, then zero or more pipe_table_row, each a fixed
// run of cell nodes — is walked directly via the tree-sitter node API
// afterwards rather than captured piecemeal here: that shape is far more
// naturally expressed as a tree walk than as flat captures that would then
// need to be regrouped back into rows.
constexpr char MARKDOWN_TABLE_QUERY[] = R"scm(
(pipe_table) @table
)scm";

// Locates thematic breaks (a line of 3+ `-`, `*` or `_`, CommonMark's `<hr>`)
// — structural extraction like MARKDOWN_FENCED_CODE_QUERY/
// MARKDOWN_TABLE_QUERY above, since a thematic break renders as a full-width
// rule rather than its literal dashes, the same "replace this whole
// block" treatment fences and tables get.
constexpr char MARKDOWN_THEMATIC_BREAK_QUERY[] = R"scm(
(thematic_break) @break
)scm";

// Hand-written against tree-sitter-markdown-inline. Bold/italic/code spans
// don't have a separate child node for their text content — e.g.
// strong_emphasis is just the two pairs of emphasis_delimiter nodes with
// the word between them implicit — so capturing the whole span colors
// delimiters and content as one unit. The delimiters and link
// punctuation/destination are then re-tagged @_hidden below so the
// renderer can omit them, leaving just the styled content — e.g.
// "**Pjotr**" renders as "Pjotr" in bold, not literally with asterisks.
constexpr char MARKDOWN_INLINE_HIGHLIGHTS_QUERY[] = R"scm(
(strong_emphasis) @markup.strong
(emphasis) @markup.italic
(code_span) @markup.code
(link_text) @markup.link

(emphasis_delimiter) @_hidden
(code_span_delimiter) @_hidden
(link_destination) @_hidden
["[" "]" "(" ")"] @_hidden
)scm";

// Covers the union of capture names used across all four queries above,
// mapping each to a priority: higher wins when captures overlap the same
// byte range. Object keys are (string) nodes too (both @string and
// @string.special.key match), and a markdown heading's marker and its text
// both get tagged — real highlighters resolve overlaps like that with
// per-capture specificity/priority; ts_query_cursor_next_match's visiting
// order for two competing captures on the same node isn't simply their
// declaration order, so this resolves it explicitly via these priorities
// instead. (Colors themselves live in markdown_style_for_capture below.)
auto capture_priority(std::string const& _name) -> int {
    if (_name == "comment") { return 5; }
    if (_name == "escape") { return 4; }
    if (_name == "string.special.key" || _name == "string.special") { return 3; }
    if (_name == "constant.builtin") { return 2; }
    if (_name == "number") { return 2; }
    if (_name == "type") { return 2; }
    if (_name == "namespace") { return 2; }
    if (_name == "function") { return 2; }
    if (_name == "preproc") { return 2; }
    if (_name == "variable.builtin") { return 2; }
    if (_name == "markup.heading") { return 2; }
    if (_name == "markup.list") { return 2; }
    if (_name == "markup.strong") { return 3; }
    if (_name == "markup.italic") { return 3; }
    if (_name == "markup.code") { return 3; }
    if (_name == "markup.link") { return 3; }
    if (_name == "string") { return 1; }
    if (_name == "keyword") { return 1; }
    if (_name == "punctuation.special") { return 1; }
    if (_name == "label") { return 1; }
    if (_name == "variable.parameter") { return 1; }
    if (_name == "diff.plus") { return 2; }
    if (_name == "diff.minus") { return 2; }
    if (_name == "diff.delta") { return 2; }
    if (_name == "constant") { return 2; }
    if (_name == "string.special.path") { return 3; }
    if (_name == "diff.header") { return 4; }
    if (_name == "variable") { return -1; }
    return 0;
}

struct language_profile {
    char const* __name;
    TSLanguage const* (*__get_language)();
    char const* __query_text;
    size_t __query_len;
    ts_parser_ptr __parser{ nullptr, ts_parser_delete }; // lazily created on first use
    ts_query_ptr __query{ nullptr, ts_query_delete };    // lazily created on first use
};

std::array<language_profile, 7> ___language_profiles = { {
  { "json", tree_sitter_json, JSON_HIGHLIGHTS_QUERY, sizeof(JSON_HIGHLIGHTS_QUERY) - 1 },
  { "cpp", tree_sitter_cpp, CPP_HIGHLIGHTS_QUERY, sizeof(CPP_HIGHLIGHTS_QUERY) - 1 },
  { "lua", tree_sitter_lua, LUA_HIGHLIGHTS_QUERY, sizeof(LUA_HIGHLIGHTS_QUERY) - 1 },
  { "python", tree_sitter_python, PYTHON_HIGHLIGHTS_QUERY, sizeof(PYTHON_HIGHLIGHTS_QUERY) - 1 },
  { "rust", tree_sitter_rust, RUST_HIGHLIGHTS_QUERY, sizeof(RUST_HIGHLIGHTS_QUERY) - 1 },
  { "diff", tree_sitter_diff, DIFF_HIGHLIGHTS_QUERY, sizeof(DIFF_HIGHLIGHTS_QUERY) - 1 },
  { "markdown",
    tree_sitter_markdown,
    MARKDOWN_HIGHLIGHTS_QUERY,
    sizeof(MARKDOWN_HIGHLIGHTS_QUERY) - 1 },
} };

auto profile_for(language _lang) -> language_profile& {
    return ___language_profiles[static_cast<size_t>(_lang)];
}

auto ensure_profile_ready(language_profile& _profile) -> void {
    if (!_profile.__parser) {
        _profile.__parser.reset(ts_parser_new());
        ts_parser_set_language(_profile.__parser.get(), _profile.__get_language());
    }
    if (!_profile.__query) {
        uint32_t _error_offset;
        TSQueryError _error_type;
        _profile.__query.reset(ts_query_new(_profile.__get_language(),
                                            _profile.__query_text,
                                            static_cast<uint32_t>(_profile.__query_len),
                                            &_error_offset,
                                            &_error_type));
    }
}

// Colors for the capture names resolved via capture_priority above,
// covering both prose markdown captures and the JSON/CPP/LUA capture names
// (keyword/function/constant.builtin/variable.builtin/preproc/escape/
// number) used when highlighting a fenced code block's content via
// language injection (see highlight_fenced_code_block below). Decorators
// compose (color | bold | underlined), so overlap resolution is handled
// entirely by capture_priority before this ever gets called.
auto markdown_style_for_capture(std::string const& _name) -> ftxui::Decorator {
    if (_name == "comment") { return ftxui::color(noam::theme().__syntax.__comment); }
    if (_name == "escape") { return ftxui::color(noam::theme().__syntax.__escape); }
    if (_name == "string.special.key") {
        return ftxui::color(noam::theme().__syntax.__string_special_key);
    }
    if (_name == "string.special") { return ftxui::color(noam::theme().__syntax.__string_special); }
    if (_name == "constant.builtin") {
        return ftxui::color(noam::theme().__syntax.__constant_builtin);
    }
    if (_name == "number") { return ftxui::color(noam::theme().__syntax.__number); }
    if (_name == "function") { return ftxui::color(noam::theme().__syntax.__function); }
    if (_name == "preproc") { return ftxui::color(noam::theme().__syntax.__preproc); }
    if (_name == "variable.builtin") {
        return ftxui::color(noam::theme().__syntax.__variable_builtin);
    }
    if (_name == "variable") { return ftxui::color(noam::theme().__syntax.__variable); }
    if (_name == "namespace") { return ftxui::color(noam::theme().__syntax.__namespace); }
    if (_name == "markup.strong") {
        return ftxui::color(noam::theme().__syntax.__markup_strong) | ftxui::bold;
    }
    if (_name == "markup.italic") {
        return ftxui::color(noam::theme().__syntax.__markup_italic) | ftxui::italic;
    }
    if (_name == "markup.code") { return ftxui::color(noam::theme().__syntax.__markup_code); }
    if (_name == "markup.link") {
        return ftxui::color(noam::theme().__syntax.__markup_link) | ftxui::underlined;
    }
    if (_name == "markup.heading") {
        return ftxui::color(noam::theme().__syntax.__markup_heading) | ftxui::bold;
    }
    if (_name == "markup.list") {
        return ftxui::color(noam::theme().__syntax.__punctuation_special);
    }
    if (_name == "type") { return ftxui::color(noam::theme().__syntax.__type); }
    if (_name == "string") { return ftxui::color(noam::theme().__syntax.__string); }
    if (_name == "keyword") { return ftxui::color(noam::theme().__syntax.__keyword); }
    if (_name == "punctuation.special") {
        return ftxui::color(noam::theme().__syntax.__punctuation_special);
    }
    if (_name == "label") { return ftxui::color(noam::theme().__syntax.__keyword); }
    if (_name == "variable.parameter") { return ftxui::color(noam::theme().__syntax.__variable); }
    if (_name == "diff.plus") { return ftxui::color(noam::theme().__syntax.__diff_addition); }
    if (_name == "diff.minus") { return ftxui::color(noam::theme().__syntax.__diff_removal); }
    if (_name == "diff.delta") {
        return ftxui::color(noam::theme().__syntax.__markup_heading) | ftxui::bold;
    }
    if (_name == "constant") { return ftxui::color(noam::theme().__syntax.__constant_builtin); }
    if (_name == "diff.header") {
        return ftxui::color(noam::theme().__syntax.__diff_header) | ftxui::bold;
    }
    if (_name == "string.special.path") {
        return ftxui::color(noam::theme().__syntax.__string_special_key);
    }
    return [](ftxui::Element _e) { return _e; }; // identity — unstyled text
}

// --- FTXUI-side markdown rendering ---
//
// Builds an FTXUI Element per resolved span and lets flexbox() word-wrap
// them — the styled equivalent of paragraph(). It's markdown-only for
// prose (not per-language) since the document is always markdown;
// language injection only happens for fenced code blocks' declared
// language (see highlight_fenced_code_block), the same parse-query-resolve
// shape as the @_inline injection just below, but selecting the grammar
// dynamically instead of always tree-sitter-markdown-inline.

struct capture_span {
    uint32_t __start, __end;
    std::string __capture; // "" for uncaptured/default-styled text
    bool __hidden = false; // omitted entirely from the rendered output
};

// `_hidden[_b]` set true is absolute — once a byte is tagged @_hidden
// nothing un-hides it, since a delimiter character never also carries some
// other meaningful capture that should win instead.
auto resolve_markdown_inline_spans(std::string const& _source,
                                   uint32_t _start,
                                   uint32_t _end,
                                   std::vector<std::string>& _winner,
                                   std::vector<int>& _priorities,
                                   std::vector<bool>& _hidden) -> void {
    static ts_parser_ptr ___parser = [] {
        ts_parser_ptr _p(ts_parser_new(), ts_parser_delete);
        ts_parser_set_language(_p.get(), tree_sitter_markdown_inline());
        return _p;
    }();
    static ts_query_ptr ___query = [] {
        uint32_t _error_offset;
        TSQueryError _error_type;
        return ts_query_ptr(
          ts_query_new(tree_sitter_markdown_inline(),
                       MARKDOWN_INLINE_HIGHLIGHTS_QUERY,
                       static_cast<uint32_t>(sizeof(MARKDOWN_INLINE_HIGHLIGHTS_QUERY) - 1),
                       &_error_offset,
                       &_error_type),
          ts_query_delete);
    }();
    if (!___query) { return; }

    std::string _sub = _source.substr(_start, _end - _start);
    ts_tree_ptr _tree(ts_parser_parse_string(
                        ___parser.get(), nullptr, _sub.c_str(), static_cast<uint32_t>(_sub.size())),
                      ts_tree_delete);
    TSNode _root = ts_tree_root_node(_tree.get());

    ts_query_cursor_ptr _cursor(ts_query_cursor_new(), ts_query_cursor_delete);
    ts_query_cursor_exec(_cursor.get(), ___query.get(), _root);

    TSQueryMatch _match;
    while (ts_query_cursor_next_match(_cursor.get(), &_match)) {
        for (uint16_t _i = 0; _i < _match.capture_count; ++_i) {
            TSNode _node = _match.captures[_i].node;
            uint32_t _s = _start + ts_node_start_byte(_node);
            uint32_t _e = _start + ts_node_end_byte(_node);

            uint32_t _name_len;
            char const* _name =
              ts_query_capture_name_for_id(___query.get(), _match.captures[_i].index, &_name_len);
            std::string _capture_name{ _name, _name_len };

            if (_capture_name == "_hidden") {
                for (uint32_t _b = _s; _b < _e && _b < _hidden.size(); ++_b) { _hidden[_b] = true; }
                continue;
            }
            int _priority = capture_priority(_capture_name);
            for (uint32_t _b = _s; _b < _e && _b < _winner.size(); ++_b) {
                if (_priority >= _priorities[_b]) {
                    _priorities[_b] = _priority;
                    _winner[_b] = _capture_name;
                }
            }
        }
    }
}

// Maps a fenced code block's declared info-string language (e.g. "cpp",
// "c++", "json", "lua") to one of the three injectable profiles. Anything
// unrecognized (including no declared language at all) returns nullopt —
// the caller renders that block as plain unhighlighted text instead of
// guessing.
auto language_for_fence_name(std::string const& _name) -> std::optional<language> {
    if (_name == "json") { return language::JSON; }
    if (_name == "cpp" || _name == "c++" || _name == "cxx" || _name == "hpp") {
        return language::CPP;
    }
    if (_name == "lua") { return language::LUA; }
    if (_name == "python" || _name == "py") { return language::PYTHON; }
    if (_name == "rust" || _name == "rs") { return language::RUST; }
    if (_name == "diff" || _name == "patch") { return language::DIFF; }
    return std::nullopt;
}

// A qualified C++ name (e.g. std::chrono::steady_clock) parses as
// a chain of nested qualified_identifier nodes — each level's `name:` field
// holds either the next qualified_identifier down, or, at the innermost
// level, the actual terminal identifier/field_identifier. There's no fixed
// nesting depth a query pattern could match (this codebase alone uses
// chains up to 5 segments deep), so CPP_HIGHLIGHTS_QUERY instead captures
// the whole qualified_identifier as @_qualified_function_name at a
// function_declarator's declarator or a call_expression's function, and
// this walks down its `name:` chain to find the terminal node — only that
// span is colored @function, leaving the leading namespace segments to
// (namespace_identifier) @namespace (already matched elsewhere in the same
// query).
auto terminal_name_of_qualified_identifier(TSNode _node) -> TSNode {
    while (true) {
        TSNode _name = ts_node_child_by_field_name(_node, "name", 4);
        if (ts_node_is_null(_name)) { return _node; }
        if (std::string_view{ ts_node_type(_name) } != "qualified_identifier") { return _name; }
        _node = _name;
    }
}

// Highlights `_code` (a fenced code block's content, already extracted from
// its markdown fence) using the tree-sitter profile for `_lang`, returning
// one styled Element per physical line of `_code` — the language-injection
// counterpart of resolve_markdown_inline_spans, but selecting the grammar
// dynamically per fence instead of always tree-sitter-markdown-inline, and
// building Elements directly rather than resolving into the outer
// document's byte-indexed winner/priorities arrays (a fenced block's
// content lives in its own coordinate space entirely — it's rendered as
// its own set of rows, not spliced back into the surrounding prose line).
auto highlight_fenced_code_block(std::string const& _code, language _lang, int _width)
  -> ftxui::Elements {
    language_profile& _profile = profile_for(_lang);
    ensure_profile_ready(_profile);

    std::vector<int> _priorities(_code.size(), -1);
    std::vector<std::string> _winner(_code.size());

    if (_profile.__query) {
        ts_tree_ptr _tree(
          ts_parser_parse_string(
            _profile.__parser.get(), nullptr, _code.c_str(), static_cast<uint32_t>(_code.size())),
          ts_tree_delete);
        TSNode _root = ts_tree_root_node(_tree.get());

        ts_query_cursor_ptr _cursor(ts_query_cursor_new(), ts_query_cursor_delete);
        ts_query_cursor_exec(_cursor.get(), _profile.__query.get(), _root);

        TSQueryMatch _match;
        while (ts_query_cursor_next_match(_cursor.get(), &_match)) {
            for (uint16_t _i = 0; _i < _match.capture_count; ++_i) {
                TSNode _node = _match.captures[_i].node;

                uint32_t _name_len;
                char const* _name = ts_query_capture_name_for_id(
                  _profile.__query.get(), _match.captures[_i].index, &_name_len);
                std::string _capture_name{ _name, _name_len };

                if (_capture_name == "_qualified_function_name") {
                    _node = terminal_name_of_qualified_identifier(_node);
                    _capture_name = "function";
                }

                uint32_t _start = ts_node_start_byte(_node);
                uint32_t _end = ts_node_end_byte(_node);
                int _priority = capture_priority(_capture_name);
                for (uint32_t _b = _start; _b < _end && _b < _code.size(); ++_b) {
                    if (_priority >= _priorities[_b]) {
                        _priorities[_b] = _priority;
                        _winner[_b] = _capture_name;
                    }
                }
            }
        }
    }

    // Run-length-encode the resolved captures into spans, splitting at
    // '\n' boundaries too so each output row is exactly one physical code
    // line (a span never straddles a line break — FTXUI's text() doesn't
    // treat embedded '\n' as a line break, matching the same constraint
    // render_markdown_paragraph documents for prose).
    ftxui::Elements _rows;
    ftxui::Elements _current_row;
    size_t _i = 0;
    while (_i < _code.size()) {
        if (_code[_i] == '\n') {
            _rows.push_back(ftxui::hbox(std::move(_current_row)) |
                            ftxui::size(ftxui::WIDTH, ftxui::LESS_THAN, std::max(_width, 1)));
            _current_row = ftxui::Elements{};
            ++_i;
            continue;
        }
        size_t _j = _i + 1;
        while (_j < _code.size() && _code[_j] != '\n' && _winner[_j] == _winner[_i]) { ++_j; }
        _current_row.push_back(ftxui::text(_code.substr(_i, _j - _i)) |
                               markdown_style_for_capture(_winner[_i]));
        _i = _j;
    }
    if (!_current_row.empty()) {
        _rows.push_back(ftxui::hbox(std::move(_current_row)) |
                        ftxui::size(ftxui::WIDTH, ftxui::LESS_THAN, std::max(_width, 1)));
    }
    return _rows;
}

// Parses `_content` as markdown (block query + @_inline injection) and
// resolves overlapping captures by priority (capture_priority), then
// run-length-encodes the result into non-overlapping spans covering the
// whole string.
auto resolve_markdown_spans(std::string const& _content) -> std::vector<capture_span> {
    if (_content.empty()) { return {}; }

    language_profile& _profile = profile_for(language::MARKDOWN);
    ensure_profile_ready(_profile);

    std::vector<int> _priorities(_content.size(), -1);
    std::vector<std::string> _winner(_content.size());
    std::vector<bool> _hidden(_content.size(), false);

    if (_profile.__query) {
        std::string _source = _content + "\n"; // block grammar needs it
        ts_tree_ptr _tree(ts_parser_parse_string(_profile.__parser.get(),
                                                 nullptr,
                                                 _source.c_str(),
                                                 static_cast<uint32_t>(_source.size())),
                          ts_tree_delete);
        TSNode _root = ts_tree_root_node(_tree.get());

        ts_query_cursor_ptr _cursor(ts_query_cursor_new(), ts_query_cursor_delete);
        ts_query_cursor_exec(_cursor.get(), _profile.__query.get(), _root);

        TSQueryMatch _match;
        while (ts_query_cursor_next_match(_cursor.get(), &_match)) {
            for (uint16_t _i = 0; _i < _match.capture_count; ++_i) {
                TSNode _node = _match.captures[_i].node;
                uint32_t _start = ts_node_start_byte(_node);
                uint32_t _end = ts_node_end_byte(_node);

                uint32_t _name_len;
                char const* _name = ts_query_capture_name_for_id(
                  _profile.__query.get(), _match.captures[_i].index, &_name_len);
                std::string _capture_name{ _name, _name_len };

                if (_capture_name == "_inline") {
                    resolve_markdown_inline_spans(
                      _source, _start, _end, _winner, _priorities, _hidden);
                    continue;
                }
                if (_capture_name == "_hidden") {
                    for (uint32_t _b = _start; _b < _end && _b < _content.size(); ++_b) {
                        _hidden[_b] = true;
                    }
                    continue;
                }
                if (_capture_name == "_hidden_marker") {
                    for (uint32_t _b = _start; _b < _end && _b < _content.size(); ++_b) {
                        _hidden[_b] = true;
                    }
                    // Also hide the one mandatory space CommonMark requires
                    // after the marker — the node itself doesn't include
                    // it, so it'd otherwise survive as a stray leading
                    // space on heading_content.
                    if (_end < _content.size() && _content[_end] == ' ') { _hidden[_end] = true; }
                    continue;
                }
                int _priority = capture_priority(_capture_name);
                for (uint32_t _b = _start; _b < _end && _b < _content.size(); ++_b) {
                    if (_priority >= _priorities[_b]) {
                        _priorities[_b] = _priority;
                        _winner[_b] = _capture_name;
                    }
                }
            }
        }
    }

    std::vector<capture_span> _spans;
    size_t _i = 0;
    while (_i < _content.size()) {
        size_t _j = _i + 1;
        while (_j < _content.size() && _winner[_j] == _winner[_i] && _hidden[_j] == _hidden[_i]) {
            ++_j;
        }
        _spans.push_back(
          { static_cast<uint32_t>(_i), static_cast<uint32_t>(_j), _winner[_i], _hidden[_i] });
        _i = _j;
    }
    return _spans;
}

// Counts leading block-quote markers on a raw source line — CommonMark
// allows up to 3 leading spaces before each '>' , and one optional space
// right after it. Used only to decide how many quote-bar glyphs/how dark a
// background render_markdown_paragraph below should apply; the actual
// hiding of the "> " markup from the rendered text is done by the grammar
// (block_quote_marker) @_hidden_marker, same mechanism as atx heading
// markers, and independent of this count.
auto block_quote_depth(std::string const& _line) -> int {
    int _depth = 0;
    size_t _pos = 0;
    while (_pos < _line.size()) {
        size_t _p = _pos;
        for (int _s = 0; _s < 3 && _p < _line.size() && _line[_p] == ' '; ++_s) { ++_p; }
        if (_p >= _line.size() || _line[_p] != '>') { break; }
        ++_depth;
        ++_p;
        if (_p < _line.size() && _line[_p] == ' ') { ++_p; }
        _pos = _p;
    }
    return _depth;
}

// `_paragraph` must not contain an embedded '\n' — resolve_markdown_spans
// parses it as one block-level document, and a run of several source lines
// with no blank line between them is one lazily-continued markdown
// paragraph (one giant (inline) node), which loses the original line
// breaks entirely. The word-tokenizer below only splits on spaces, so any
// embedded '\n' would end up stuck inside a single text() token, which
// FTXUI doesn't handle as a line break.
//
// `_quote_depth` (from block_quote_depth above) is how many levels of `>`
// nesting this paragraph is under — 0 means no block quote. Each wrapped
// row gets that many leading vertical-bar glyphs plus a dark background,
// so a multi-line quote reads as one visually distinct block.
//
// Returns one Element per wrapped terminal row rather than a single
// flexbox — flexbox reflows lazily at render time, hiding however many
// screen lines one logical paragraph became inside one opaque Element,
// and scrolling pages by row index, so every row must be exactly one
// screen line. Wrapping is done here,
// eagerly, by display width (ftxui::string_width, not byte count, so
// multi-byte glyphs measure correctly).
auto render_markdown_paragraph(std::string const& _paragraph, int _width, int _quote_depth)
  -> ftxui::Elements {
    // Bar glyphs eat into the available width.
    int _bar_width = _quote_depth > 0 ? _quote_depth * 2 : 0;
    int _w = std::max(_width - _bar_width, 1);
    if (_paragraph.empty()) { return { ftxui::text("") }; }

    struct token {
        std::string __text;
        ftxui::Decorator __decorate;
    };
    std::vector<token> _tokens;
    for (capture_span const& _span : resolve_markdown_spans(_paragraph)) {
        if (_span.__hidden) {
            continue; // e.g. "**"/"`"/"#" — the raw markup, not its effect
        }
        std::string _chunk = _paragraph.substr(_span.__start, _span.__end - _span.__start);
        if (_span.__capture == "markup.list") {
            // Normalize whichever bullet-list marker glyph (-, +, *) to one
            // nicer Unicode bullet; the ordered-list marker
            // (list_marker_dot, e.g. "1.") is captured separately as
            // @punctuation.special and left untouched.
            size_t _marker_pos = _chunk.find_first_not_of(' ');
            if (_marker_pos != std::string::npos) {
                char _c = _chunk[_marker_pos];
                if (_c == '-' || _c == '+' || _c == '*') {
                    _chunk.replace(_marker_pos, 1, "\xe2\x80\xa2"); // U+2022 BULLET
                }
            }
        }
        ftxui::Decorator _decorate = markdown_style_for_capture(_span.__capture);
        size_t _pos = 0;
        while (_pos < _chunk.size()) {
            size_t _next_space = _chunk.find(' ', _pos);
            size_t _word_end = _next_space == std::string::npos ? _chunk.size() : _next_space;
            if (_word_end > _pos) {
                _tokens.push_back({ _chunk.substr(_pos, _word_end - _pos), _decorate });
            }
            if (_next_space == std::string::npos) { break; }
            _tokens.push_back({ " ", _decorate });
            _pos = _next_space + 1;
        }
    }

    ftxui::Elements _rows;
    ftxui::Elements _current_row;
    int _current_width = 0;
    for (auto& _token : _tokens) {
        int _token_width = ftxui::string_width(_token.__text);
        if (_current_row.empty() && _token.__text == " ") {
            continue; // no leading space at the start of a wrapped row
        }
        if (!_current_row.empty() && _current_width + _token_width > _w) {
            _rows.push_back(ftxui::hbox(std::move(_current_row)));
            _current_row = ftxui::Elements{};
            _current_width = 0;
            if (_token.__text == " ") { continue; }
        }
        _current_row.push_back(ftxui::text(_token.__text) | _token.__decorate);
        _current_width += _token_width;
    }
    if (!_current_row.empty() || _rows.empty()) {
        _rows.push_back(ftxui::hbox(std::move(_current_row)));
    }

    if (_quote_depth > 0) {
        // Prefix every row with `_quote_depth` "│ " glyphs and give the
        // whole row a dark background, so a multi-line/multi-paragraph
        // quote reads as one visually distinct block, not just colored
        // text.
        std::string _bars;
        _bars.reserve(static_cast<size_t>(_quote_depth) * 4);
        for (int _d = 0; _d < _quote_depth; ++_d) { _bars += "\xe2\x94\x82 "; } // │ (U+2502)
        for (auto& _row : _rows) {
            _row =
              ftxui::hbox({ ftxui::text(_bars) | ftxui::color(noam::theme().__syntax.__quote_bar),
                            std::move(_row) | ftxui::xflex_grow }) |
              ftxui::bgcolor(noam::theme().__syntax.__quote_background);
        }
    }
    return _rows;
}

struct fenced_code_range {
    uint32_t __block_start,
      __block_end; // byte range of the whole fenced_code_block, delimiters included
    uint32_t __content_start, __content_end; // byte range of code_fence_content, within _content
    std::optional<language> __lang;
};

// Parses the WHOLE `_content` once as a single markdown document to find
// fenced code blocks — this can only work on the full multi-line text,
// unlike everything else in this file, which re-parses one physical line
// at a time (verified against the grammar: a code-fence content line
// parsed in isolation, with no memory of the previous line's opening
// ```lang fence, comes back as a bare paragraph, not code_fence_content).
// render_markdown_lines below still renders prose line-by-line as before —
// this just tells it which line ranges are "inside a fence" so it can
// route those through highlight_fenced_code_block instead.
auto find_fenced_code_blocks(std::string const& _content) -> std::vector<fenced_code_range> {
    std::vector<fenced_code_range> _blocks;
    language_profile& _profile = profile_for(language::MARKDOWN);
    ensure_profile_ready(_profile);
    if (!_profile.__query) { return _blocks; }

    static ts_query_ptr ___fence_query = [&_profile] {
        uint32_t _error_offset;
        TSQueryError _error_type;
        return ts_query_ptr(
          ts_query_new(_profile.__get_language(),
                       MARKDOWN_FENCED_CODE_QUERY,
                       static_cast<uint32_t>(sizeof(MARKDOWN_FENCED_CODE_QUERY) - 1),
                       &_error_offset,
                       &_error_type),
          ts_query_delete);
    }();
    if (!___fence_query) { return _blocks; }

    std::string _source = _content + "\n"; // block grammar needs a trailing newline
    ts_tree_ptr _tree(
      ts_parser_parse_string(
        _profile.__parser.get(), nullptr, _source.c_str(), static_cast<uint32_t>(_source.size())),
      ts_tree_delete);
    TSNode _root = ts_tree_root_node(_tree.get());

    ts_query_cursor_ptr _cursor(ts_query_cursor_new(), ts_query_cursor_delete);
    ts_query_cursor_exec(_cursor.get(), ___fence_query.get(), _root);

    TSQueryMatch _match;
    while (ts_query_cursor_next_match(_cursor.get(), &_match)) {
        fenced_code_range _range{ 0, 0, 0, 0, std::nullopt };
        bool _have_content = false;
        for (uint16_t _i = 0; _i < _match.capture_count; ++_i) {
            TSNode _node = _match.captures[_i].node;
            uint32_t _name_len;
            char const* _name = ts_query_capture_name_for_id(
              ___fence_query.get(), _match.captures[_i].index, &_name_len);
            std::string _capture_name{ _name, _name_len };
            if (_capture_name == "fence_block") {
                _range.__block_start = ts_node_start_byte(_node);
                _range.__block_end = ts_node_end_byte(_node);
            }
            else if (_capture_name == "fence_content") {
                _range.__content_start = ts_node_start_byte(_node);
                _range.__content_end = ts_node_end_byte(_node);
                _have_content = true;
            }
            else if (_capture_name == "fence_lang") {
                uint32_t _s = ts_node_start_byte(_node), _e = ts_node_end_byte(_node);
                _range.__lang = language_for_fence_name(_source.substr(_s, _e - _s));
            }
        }
        if (_have_content) { _blocks.push_back(_range); }
    }
    return _blocks;
}

enum class table_align { LEFT, CENTER, RIGHT };

struct pipe_table_range {
    uint32_t __block_start,
      __block_end; // byte range of the whole pipe_table, header through last row
    std::vector<std::pair<uint32_t, uint32_t>> __header;            // one entry per header cell
    std::vector<table_align> __aligns;                              // one entry per column
    std::vector<std::vector<std::pair<uint32_t, uint32_t>>> __rows; // one entry per body row
};

// Whether `_node` (a pipe_table_delimiter_cell) has a named child of type
// `_type` — used below to detect the pipe_table_align_left/
// pipe_table_align_right markers a ':' in the delimiter row produces,
// which is how GFM expresses a column's declared alignment.
auto has_named_child_of_type(TSNode _node, char const* _type) -> bool {
    uint32_t _n = ts_node_named_child_count(_node);
    for (uint32_t _i = 0; _i < _n; ++_i) {
        if (std::string_view{ ts_node_type(ts_node_named_child(_node, _i)) } == _type) {
            return true;
        }
    }
    return false;
}

// Parses the whole `_content` once to find GFM pipe tables — its own parse,
// same reasoning as find_fenced_code_blocks above: a table's column widths
// depend on every row, so render_markdown_lines needs to know a table's
// full extent up front rather than discovering it one physical line at a
// time.
auto find_pipe_tables(std::string const& _content) -> std::vector<pipe_table_range> {
    std::vector<pipe_table_range> _tables;
    language_profile& _profile = profile_for(language::MARKDOWN);
    ensure_profile_ready(_profile);
    if (!_profile.__query) { return _tables; }

    static ts_query_ptr ___table_query = [&_profile] {
        uint32_t _error_offset;
        TSQueryError _error_type;
        return ts_query_ptr(ts_query_new(_profile.__get_language(),
                                         MARKDOWN_TABLE_QUERY,
                                         static_cast<uint32_t>(sizeof(MARKDOWN_TABLE_QUERY) - 1),
                                         &_error_offset,
                                         &_error_type),
                            ts_query_delete);
    }();
    if (!___table_query) { return _tables; }

    std::string _source = _content + "\n"; // block grammar needs a trailing newline
    ts_tree_ptr _tree(
      ts_parser_parse_string(
        _profile.__parser.get(), nullptr, _source.c_str(), static_cast<uint32_t>(_source.size())),
      ts_tree_delete);
    TSNode _root = ts_tree_root_node(_tree.get());

    ts_query_cursor_ptr _cursor(ts_query_cursor_new(), ts_query_cursor_delete);
    ts_query_cursor_exec(_cursor.get(), ___table_query.get(), _root);

    TSQueryMatch _match;
    while (ts_query_cursor_next_match(_cursor.get(), &_match)) {
        for (uint16_t _i = 0; _i < _match.capture_count; ++_i) {
            TSNode _table_node = _match.captures[_i].node;
            pipe_table_range _range{
                ts_node_start_byte(_table_node), ts_node_end_byte(_table_node), {}, {}, {}
            };

            uint32_t _n = ts_node_named_child_count(_table_node);
            for (uint32_t _c = 0; _c < _n; ++_c) {
                TSNode _child = ts_node_named_child(_table_node, _c);
                std::string_view _type{ ts_node_type(_child) };

                if (_type == "pipe_table_header") {
                    uint32_t _cn = ts_node_named_child_count(_child);
                    for (uint32_t _k = 0; _k < _cn; ++_k) {
                        TSNode _cell = ts_node_named_child(_child, _k);
                        _range.__header.emplace_back(ts_node_start_byte(_cell),
                                                     ts_node_end_byte(_cell));
                    }
                }
                else if (_type == "pipe_table_delimiter_row") {
                    uint32_t _cn = ts_node_named_child_count(_child);
                    for (uint32_t _k = 0; _k < _cn; ++_k) {
                        TSNode _dcell = ts_node_named_child(_child, _k);
                        bool _left = has_named_child_of_type(_dcell, "pipe_table_align_left");
                        bool _right = has_named_child_of_type(_dcell, "pipe_table_align_right");
                        _range.__aligns.push_back(_left && _right ? table_align::CENTER
                                                  : _right        ? table_align::RIGHT
                                                                  : table_align::LEFT);
                    }
                }
                else if (_type == "pipe_table_row") {
                    std::vector<std::pair<uint32_t, uint32_t>> _row;
                    uint32_t _cn = ts_node_named_child_count(_child);
                    for (uint32_t _k = 0; _k < _cn; ++_k) {
                        TSNode _cell = ts_node_named_child(_child, _k);
                        _row.emplace_back(ts_node_start_byte(_cell), ts_node_end_byte(_cell));
                    }
                    _range.__rows.push_back(std::move(_row));
                }
            }

            if (!_range.__header.empty()) { _tables.push_back(std::move(_range)); }
        }
    }
    return _tables;
}

// Parses the whole `_content` once to find thematic breaks — same reasoning
// as find_fenced_code_blocks/find_pipe_tables above (one-shot block parse,
// rather than the per-line prose path). A thematic_break node's byte range
// already includes its trailing newline (confirmed against the grammar,
// same as a fenced_code_block's __block_end), so the caller can jump
// straight to __end with no further skip, matching find_pipe_tables' rows.
auto find_thematic_breaks(std::string const& _content)
  -> std::vector<std::pair<uint32_t, uint32_t>> {
    std::vector<std::pair<uint32_t, uint32_t>> _breaks;
    language_profile& _profile = profile_for(language::MARKDOWN);
    ensure_profile_ready(_profile);
    if (!_profile.__query) { return _breaks; }

    static ts_query_ptr ___break_query = [&_profile] {
        uint32_t _error_offset;
        TSQueryError _error_type;
        return ts_query_ptr(
          ts_query_new(_profile.__get_language(),
                       MARKDOWN_THEMATIC_BREAK_QUERY,
                       static_cast<uint32_t>(sizeof(MARKDOWN_THEMATIC_BREAK_QUERY) - 1),
                       &_error_offset,
                       &_error_type),
          ts_query_delete);
    }();
    if (!___break_query) { return _breaks; }

    std::string _source = _content + "\n"; // block grammar needs a trailing newline
    ts_tree_ptr _tree(
      ts_parser_parse_string(
        _profile.__parser.get(), nullptr, _source.c_str(), static_cast<uint32_t>(_source.size())),
      ts_tree_delete);
    TSNode _root = ts_tree_root_node(_tree.get());

    ts_query_cursor_ptr _cursor(ts_query_cursor_new(), ts_query_cursor_delete);
    ts_query_cursor_exec(_cursor.get(), ___break_query.get(), _root);

    TSQueryMatch _match;
    while (ts_query_cursor_next_match(_cursor.get(), &_match)) {
        for (uint16_t _i = 0; _i < _match.capture_count; ++_i) {
            TSNode _node = _match.captures[_i].node;
            _breaks.emplace_back(ts_node_start_byte(_node), ts_node_end_byte(_node));
        }
    }
    return _breaks;
}

// Sums the visible display width of `_text`'s resolved markdown spans,
// hidden markup (delimiters, etc.) excluded — used only to size a table
// column to its natural (unwrapped) content width before deciding how much
// room each column gets (render_pipe_table below). The actual cell content
// is rendered separately via render_markdown_paragraph, which resolves the
// same spans again for styling and word-wrapping.
auto markdown_visible_width(std::string const& _text) -> int {
    int _width = 0;
    for (capture_span const& _span : resolve_markdown_spans(_text)) {
        if (_span.__hidden) { continue; }
        _width += ftxui::string_width(_text.substr(_span.__start, _span.__end - _span.__start));
    }
    return _width;
}

// Widest single space-delimited word in `_text`'s resolved spans (hidden
// markup excluded) — render_markdown_paragraph never splits a word mid-way
// to make it fit a narrower width, so a column can never be shrunk below
// this without the word silently overflowing its cell's fixed-width box
// (ftxui::size(WIDTH, EQUAL, n) clips rather than wraps content wider than
// n). Used only as a floor on how far render_pipe_table's proportional
// shrink can narrow a column.
auto markdown_longest_word_width(std::string const& _text) -> int {
    int _longest = 0;
    for (capture_span const& _span : resolve_markdown_spans(_text)) {
        if (_span.__hidden) { continue; }
        std::string _chunk = _text.substr(_span.__start, _span.__end - _span.__start);
        size_t _pos = 0;
        while (_pos < _chunk.size()) {
            size_t _next_space = _chunk.find(' ', _pos);
            size_t _word_end = _next_space == std::string::npos ? _chunk.size() : _next_space;
            if (_word_end > _pos) {
                _longest =
                  std::max(_longest, ftxui::string_width(_chunk.substr(_pos, _word_end - _pos)));
            }
            if (_next_space == std::string::npos) { break; }
            _pos = _next_space + 1;
        }
    }
    return _longest;
}

auto trim_cell(std::string const& _raw) -> std::string {
    size_t _b = _raw.find_first_not_of(' ');
    if (_b == std::string::npos) { return {}; }
    size_t _e = _raw.find_last_not_of(' ');
    return _raw.substr(_b, _e - _b + 1);
}

// Renders a whole GFM pipe table (see find_pipe_tables) as a bordered,
// column-aligned grid — box-drawing borders (the same visual language as
// this file's ─ fenced-code rule and │ block-quote bar), a bold header
// row, and per-column alignment from the delimiter row's ':' markers.
//
// Column widths are sized to content, then shrunk proportionally to fit
// `_width` when the table is wider than the screen. A cell's content
// is word-wrapped to its column's final width via render_markdown_paragraph
// — the same wrapping ordinary prose uses — rather than truncated, so a
// long cell grows its row taller instead of losing text; a row's height is
// the tallest of its cells, and shorter cells are padded with blank lines
// to match. Padding/alignment within a cell's fixed column width is done
// by ftxui's own box model (size(WIDTH, EQUAL, n), align_right, hcenter)
// rather than hand-built spaces, so it composes correctly with the styled
// spans render_markdown_paragraph already produced.
auto render_pipe_table(std::string const& _content, pipe_table_range const& _table, int _width)
  -> ftxui::Elements {
    size_t _ncols = _table.__header.size();
    if (_ncols == 0) { return {}; }

    std::vector<table_align> _aligns = _table.__aligns;
    _aligns.resize(_ncols, table_align::LEFT);

    auto _cell_text = [&](std::pair<uint32_t, uint32_t> const& _r) {
        return trim_cell(_content.substr(_r.first, _r.second - _r.first));
    };

    std::vector<std::string> _header_text;
    for (auto const& _r : _table.__header) { _header_text.push_back(_cell_text(_r)); }

    std::vector<std::vector<std::string>> _body_text;
    for (auto const& _row : _table.__rows) {
        std::vector<std::string> _texts;
        for (size_t _c = 0; _c < _ncols; ++_c) {
            _texts.push_back(_c < _row.size() ? _cell_text(_row[_c]) : std::string{});
        }
        _body_text.push_back(std::move(_texts));
    }

    std::vector<int> _col_widths(_ncols, 1);
    std::vector<int> _col_floors(_ncols,
                                 3); // never shrink a column below its longest unbreakable word
    for (size_t _c = 0; _c < _ncols; ++_c) {
        _col_widths[_c] = std::max(_col_widths[_c], markdown_visible_width(_header_text[_c]));
        _col_floors[_c] = std::max(_col_floors[_c], markdown_longest_word_width(_header_text[_c]));
    }
    for (auto const& _row : _body_text) {
        for (size_t _c = 0; _c < _ncols; ++_c) {
            _col_widths[_c] = std::max(_col_widths[_c], markdown_visible_width(_row[_c]));
            _col_floors[_c] = std::max(_col_floors[_c], markdown_longest_word_width(_row[_c]));
        }
    }

    // Overhead is (ncols+1) "│" separators plus one leading and one
    // trailing padding space per column: "│ X │ Y │" for two columns is
    // 2 "│ "/" │" pairs around content, i.e. 3*ncols+1 bytes of border and
    // padding around the ncols content widths themselves.
    int _natural_total = 0;
    for (int _w : _col_widths) { _natural_total += _w; }
    int _overhead = static_cast<int>(_ncols) * 3 + 1;
    int _available = std::max(_width - _overhead, static_cast<int>(_ncols) * 3);
    if (_natural_total > _available && _natural_total > 0) {
        int _shrunk_total = 0;
        for (size_t _c = 0; _c < _ncols; ++_c) {
            _col_widths[_c] = std::max(_col_floors[_c],
                                       static_cast<int>(static_cast<int64_t>(_col_widths[_c]) *
                                                        _available / _natural_total));
            _shrunk_total += _col_widths[_c];
        }
        int _overflow = _shrunk_total - _available;
        while (_overflow > 0) {
            int _best = -1;
            for (size_t _c = 0; _c < _ncols; ++_c) {
                if (_col_widths[_c] > _col_floors[_c] &&
                    (_best < 0 || _col_widths[_c] > _col_widths[static_cast<size_t>(_best)])) {
                    _best = static_cast<int>(_c);
                }
            }
            if (_best < 0) { break; } // every column is already at its floor
            --_col_widths[static_cast<size_t>(_best)];
            --_overflow;
        }
    }

    ftxui::Color const _border = noam::theme().__syntax.__table_border;

    auto _border_row =
      [&](char const* _left, char const* _mid, char const* _right, char const* _fill) {
          ftxui::Elements _parts{ ftxui::text(_left) };
          for (size_t _c = 0; _c < _ncols; ++_c) {
              std::string _line;
              for (int _i = 0; _i < _col_widths[_c] + 2; ++_i) { _line += _fill; }
              _parts.push_back(ftxui::text(_line));
              if (_c + 1 < _ncols) { _parts.push_back(ftxui::text(_mid)); }
          }
          _parts.push_back(ftxui::text(_right));
          return ftxui::hbox(std::move(_parts)) | ftxui::color(_border) |
                 ftxui::size(ftxui::WIDTH, ftxui::LESS_THAN, std::max(_width, 1));
      };

    // Wraps every cell of one logical row (header or body) to its column's
    // width, pads each cell's shorter-than-tallest lines with blanks so
    // every column has the same number of physical output lines, and
    // returns those lines as complete table rows (one Element per physical
    // line, "│ cell │ cell │" ...).
    auto _render_row = [&](std::vector<std::string> const& _cells, bool _is_header) {
        std::vector<ftxui::Elements> _wrapped(_ncols);
        size_t _height = 1;
        for (size_t _c = 0; _c < _ncols; ++_c) {
            _wrapped[_c] = render_markdown_paragraph(_cells[_c], _col_widths[_c], 0);
            _height = std::max(_height, _wrapped[_c].size());
        }

        ftxui::Elements _lines;
        for (size_t _line = 0; _line < _height; ++_line) {
            ftxui::Elements _parts{ ftxui::text("\xe2\x94\x82 ") | ftxui::color(_border) }; // "│ "
            for (size_t _c = 0; _c < _ncols; ++_c) {
                ftxui::Element _cell_line =
                  _line < _wrapped[_c].size() ? _wrapped[_c][_line] : ftxui::text("");
                if (_is_header) {
                    _cell_line = _cell_line | ftxui::bold |
                                 ftxui::color(noam::theme().__syntax.__table_header);
                }
                if (_aligns[_c] == table_align::RIGHT) {
                    _cell_line = ftxui::align_right(_cell_line);
                }
                else if (_aligns[_c] == table_align::CENTER) {
                    _cell_line = ftxui::hcenter(_cell_line);
                }
                _parts.push_back(_cell_line |
                                 ftxui::size(ftxui::WIDTH, ftxui::EQUAL, _col_widths[_c]));
                _parts.push_back(ftxui::text(_c + 1 < _ncols ? " \xe2\x94\x82 "
                                                             : " \xe2\x94\x82") | // " │ " / " │"
                                 ftxui::color(_border));
            }
            _lines.push_back(ftxui::hbox(std::move(_parts)) |
                             ftxui::size(ftxui::WIDTH, ftxui::LESS_THAN, std::max(_width, 1)));
        }
        return _lines;
    };

    ftxui::Elements _rows;
    _rows.push_back(
      _border_row("\xe2\x94\x8c", "\xe2\x94\xac", "\xe2\x94\x90", "\xe2\x94\x80")); // ┌ ┬ ┐ ─
    for (auto& _row : _render_row(_header_text, true)) { _rows.push_back(std::move(_row)); }
    _rows.push_back(
      _border_row("\xe2\x94\x9c", "\xe2\x94\xbc", "\xe2\x94\xa4", "\xe2\x94\x80")); // ├ ┼ ┤ ─
    for (auto const& _row_text : _body_text) {
        for (auto& _row : _render_row(_row_text, false)) { _rows.push_back(std::move(_row)); }
    }
    _rows.push_back(
      _border_row("\xe2\x94\x94", "\xe2\x94\xb4", "\xe2\x94\x98", "\xe2\x94\x80")); // └ ┴ ┘ ─
    return _rows;
}

} // namespace

auto noam::render_markdown_lines(std::string const& _content, int _width) -> ftxui::Elements {
    std::vector<fenced_code_range> _fences = find_fenced_code_blocks(_content);
    std::vector<pipe_table_range> _tables = find_pipe_tables(_content);
    std::vector<std::pair<uint32_t, uint32_t>> _breaks = find_thematic_breaks(_content);

    ftxui::Elements _lines;
    size_t _start = 0;
    while (_start <= _content.size()) {
        // Start of a thematic break (`---`/`***`/`___`, 3 or more)? Replace
        // it with a full-width rule instead of rendering its literal
        // dashes, same "replace this whole block" treatment as the fence
        // and table handling below.
        std::pair<uint32_t, uint32_t> const* _break = nullptr;
        for (auto const& _b : _breaks) {
            if (_b.first == _start) {
                _break = &_b;
                break;
            }
        }
        if (_break != nullptr) {
            _lines.push_back(rule_element(_width, noam::theme().__syntax.__markup_ruler));
            _start = _break->second;
            continue;
        }

        // Start of some fence's opening delimiter line (the "```lang" or
        // bare "```" line)? Replace it with a rule rather than rendering
        // its raw markup, and jump straight past it to the content — the
        // matching closing delimiter gets the same treatment below, once
        // that fence's content has been rendered, so a highlighted block
        // is bounded by two rules instead of showing the opening fence as
        // literal text while the closing one silently disappears.
        fenced_code_range const* _opening = nullptr;
        for (auto const& _f : _fences) {
            if (_f.__block_start == _start) {
                _opening = &_f;
                break;
            }
        }
        if (_opening != nullptr) {
            _lines.push_back(rule_element(_width, noam::theme().__syntax.__comment, "-"));
            _start = _opening->__content_start;
            continue;
        }

        // Start of a GFM pipe table? Render the whole thing (header through
        // last row — column widths need every cell, see find_pipe_tables)
        // in one go, then jump past every line it consumed, same shape as
        // the fence handling above.
        pipe_table_range const* _table = nullptr;
        for (auto const& _t : _tables) {
            if (_t.__block_start == _start) {
                _table = &_t;
                break;
            }
        }
        if (_table != nullptr) {
            for (auto& _row : render_pipe_table(_content, *_table, _width)) {
                _lines.push_back(std::move(_row));
            }
            _start = _table->__block_end;
            continue;
        }

        size_t _nl = _content.find('\n', _start);
        size_t _line_end = _nl == std::string::npos ? _content.size() : _nl;

        // Is this physical line (fully) inside some fence's code content?
        fenced_code_range const* _fence = nullptr;
        for (auto const& _f : _fences) {
            if (_start >= _f.__content_start && _line_end <= _f.__content_end) {
                _fence = &_f;
                break;
            }
        }

        if (_fence != nullptr) {
            // Code fence content must NEVER go through
            // render_markdown_paragraph (the prose path below) — code
            // routinely contains markdown-significant characters (a C
            // "# define FOO" reads as an ATX heading, "_var_name_" reads
            // as italic, etc.), so an unrecognized/absent fence language
            // still renders as plain unstyled text here, not "falls back
            // to treating it as markdown."
            //
            // Highlight the WHOLE fence's content in one call (not
            // per-line) so a recognized language's grammar sees real
            // multi-line context (e.g. a C++ multi-line comment or a JSON
            // object spanning several lines) — then consume however many
            // source lines that produced, so the outer while-loop doesn't
            // re-visit and re-render lines already covered here.
            std::string _code = _content.substr(_fence->__content_start,
                                                _fence->__content_end - _fence->__content_start);
            if (_fence->__lang.has_value()) {
                for (auto& _row : highlight_fenced_code_block(_code, *_fence->__lang, _width)) {
                    _lines.push_back(std::move(_row));
                }
            }
            else {
                size_t _pos = 0;
                while (_pos <= _code.size()) {
                    size_t _code_nl = _code.find('\n', _pos);
                    _lines.push_back(ftxui::text(_code.substr(
                      _pos, _code_nl == std::string::npos ? std::string::npos : _code_nl - _pos)));
                    if (_code_nl == std::string::npos) { break; }
                    _pos = _code_nl + 1;
                }
            }
            _lines.push_back(rule_element(_width, noam::theme().__syntax.__comment, "-"));
            // __block_end already lands just past the closing delimiter
            // line's own newline (confirmed against the grammar: the
            // fenced_code_block node's end byte is the position right
            // after that '\n', not the '\n' itself) — no further skip
            // needed, unlike the content-only skip above.
            _start = _fence->__block_end;
            continue;
        }

        std::string _line = _content.substr(_start, _line_end - _start);
        int _quote_depth = block_quote_depth(_line);
        for (auto& _row : render_markdown_paragraph(_line, _width, _quote_depth)) {
            _lines.push_back(std::move(_row));
        }
        if (_nl == std::string::npos) { break; }
        _start = _nl + 1;
    }
    return _lines;
}
