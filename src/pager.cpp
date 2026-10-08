#include <algorithm>
#include <limits>
#include <memory>
#include <noam/pager.h>
#include <noam/text_render.h>
#include <string_view>
#include <tree_sitter/api.h>

extern "C" const TSLanguage* tree_sitter_markdown(void);

namespace {

// Byte offsets where the document's top-level blocks start, each moved back to
// the start of its line, in order. Sections (a heading and what follows it)
// are descended into, so a page can end inside a long one. A thematic break is
// left out: a page starting with `---` would parse it as front matter.
auto block_starts(std::string const& _content) -> std::vector<size_t> {
    std::unique_ptr<TSParser, decltype(&ts_parser_delete)> _parser(ts_parser_new(),
                                                                   ts_parser_delete);
    ts_parser_set_language(_parser.get(), tree_sitter_markdown());
    std::string _source = _content + "\n"; // block grammar needs a trailing newline
    std::unique_ptr<TSTree, decltype(&ts_tree_delete)> _tree(
      ts_parser_parse_string(
        _parser.get(), nullptr, _source.c_str(), static_cast<uint32_t>(_source.size())),
      ts_tree_delete);

    std::vector<size_t> _starts;
    std::vector<TSNode> _stack{ ts_tree_root_node(_tree.get()) };
    while (!_stack.empty()) {
        TSNode _node = _stack.back();
        _stack.pop_back();
        std::string_view _type{ ts_node_type(_node) };
        if (_type == "document" || _type == "section") {
            uint32_t _n = ts_node_named_child_count(_node);
            for (uint32_t _c = 0; _c < _n; ++_c) { _stack.push_back(ts_node_named_child(_node, _c)); }
            continue;
        }
        if (_type == "thematic_break") { continue; }
        size_t _start = std::min<size_t>(ts_node_start_byte(_node), _content.size());
        size_t _nl = _start == 0 ? std::string::npos : _content.rfind('\n', _start - 1);
        _starts.push_back(_nl == std::string::npos ? 0 : _nl + 1);
    }
    std::sort(_starts.begin(), _starts.end());
    _starts.erase(std::unique(_starts.begin(), _starts.end()), _starts.end());
    return _starts;
}

// Splits `_content` into consecutive byte ranges at block starts, each closed
// once it holds at least `_target_lines` source lines.
auto split_pages(std::string const& _content, size_t _target_lines)
  -> std::vector<std::pair<size_t, size_t>> {
    std::vector<std::pair<size_t, size_t>> _pages;
    size_t _page_start = 0;
    size_t _previous = 0; // last block start counted into _lines
    size_t _lines = 0;    // source lines in [_page_start, _previous)
    for (size_t _block : block_starts(_content)) {
        if (_block <= _previous) { continue; }
        _lines += static_cast<size_t>(
          std::count(_content.begin() + static_cast<std::ptrdiff_t>(_previous),
                     _content.begin() + static_cast<std::ptrdiff_t>(_block),
                     '\n'));
        _previous = _block;
        if (_lines >= _target_lines) {
            _pages.emplace_back(_page_start, _block);
            _page_start = _block;
            _lines = 0;
        }
    }
    if (_page_start < _content.size() || _pages.empty()) {
        _pages.emplace_back(_page_start, _content.size());
    }
    return _pages;
}

} // namespace

noam::pager::pager(std::string _content)
  : __content{ std::move(_content) } {}

auto noam::pager::rows(int _width, int _height) -> ftxui::Elements {
    _width = std::max(_width, 1);
    _height = std::max(_height, 1);
    this->layout(_width, _height);
    if (this->__jump < 0) {
        this->reset_window(0);
        this->__top = 0;
    }
    else if (this->__jump > 0) {
        this->reset_window(this->__pages.size() - 1);
        this->__top = std::numeric_limits<int>::max() / 2; // settle() pulls it back
    }
    this->__jump = 0;
    this->settle(_height);

    ftxui::Elements _visible;
    int _row = 0;
    for (auto const& _page : this->__window) {
        for (auto const& _element : _page.__rows) {
            if (_row >= this->__top && _row < this->__top + _height) { _visible.push_back(_element); }
            ++_row;
        }
    }
    return _visible;
}

auto noam::pager::scroll(int _delta) -> void { this->__top += _delta; }

auto noam::pager::home() -> void { this->__jump = -1; }

auto noam::pager::end() -> void { this->__jump = 1; }

// A new height re-splits the document, keeping the position at the page that
// holds the current top row (and the same row offset into it); a new width
// only re-renders the kept pages.
auto noam::pager::layout(int _width, int _height) -> void {
    if (_height != this->__height) {
        size_t _anchor = 0;
        int _offset = this->__top;
        for (auto const& _page : this->__window) {
            _anchor = this->__pages[_page.__index].first;
            int _count = static_cast<int>(_page.__rows.size());
            if (_offset < _count) { break; }
            _offset -= _count;
        }
        this->__pages = split_pages(this->__content, 2 * static_cast<size_t>(_height));
        this->__width = _width;
        this->__height = _height;
        size_t _index = 0;
        while (_index + 1 < this->__pages.size() && this->__pages[_index + 1].first <= _anchor) {
            ++_index;
        }
        this->reset_window(_index);
        this->__top = _offset;
        return;
    }
    if (_width != this->__width) {
        this->__width = _width;
        for (auto& _page : this->__window) { _page.__rows = this->render(_page.__index); }
    }
}

auto noam::pager::reset_window(size_t _index) -> void {
    this->__window.clear();
    this->__window.push_back({ _index, this->render(_index) });
}

// A page's last newline only ends its last line; rendered as is, it would add
// a blank row at every page boundary that the whole document doesn't have.
auto noam::pager::render(size_t _index) const -> ftxui::Elements {
    auto [_start, _end] = this->__pages[_index];
    std::string _text = this->__content.substr(_start, _end - _start);
    if (!_text.empty() && _text.back() == '\n') { _text.pop_back(); }
    return noam::render_markdown_lines(_text, this->__width);
}

auto noam::pager::window_rows() const -> int {
    int _count = 0;
    for (auto const& _page : this->__window) { _count += static_cast<int>(_page.__rows.size()); }
    return _count;
}

// Brings the pages the screen needs into the window, clamps the position to
// the document, and drops pages that are wholly off screen once more than
// three are kept.
auto noam::pager::settle(int _height) -> void {
    auto _prepend = [this] {
        size_t _index = this->__window.front().__index - 1;
        this->__window.push_front({ _index, this->render(_index) });
        this->__top += static_cast<int>(this->__window.front().__rows.size());
    };
    auto _append = [this] {
        size_t _index = this->__window.back().__index + 1;
        this->__window.push_back({ _index, this->render(_index) });
    };

    while (this->__top < 0 && this->__window.front().__index > 0) { _prepend(); }
    this->__top = std::max(this->__top, 0);
    while (this->__top + _height > this->window_rows() &&
           this->__window.back().__index + 1 < this->__pages.size()) {
        _append();
    }
    // Past the end of the document: pull back so the last row sits at the
    // bottom, bringing in earlier pages when the window is shorter than the
    // screen.
    if (this->__top + _height > this->window_rows()) {
        this->__top = this->window_rows() - _height;
        while (this->__top < 0 && this->__window.front().__index > 0) { _prepend(); }
        this->__top = std::max(this->__top, 0);
    }

    while (this->__window.size() > 3 &&
           static_cast<int>(this->__window.front().__rows.size()) <= this->__top) {
        this->__top -= static_cast<int>(this->__window.front().__rows.size());
        this->__window.pop_front();
    }
    while (this->__window.size() > 3 &&
           this->window_rows() - static_cast<int>(this->__window.back().__rows.size()) >=
             this->__top + _height) {
        this->__window.pop_back();
    }
}
