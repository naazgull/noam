#pragma once

#include <deque>
#include <ftxui/dom/elements.hpp>
#include <string>
#include <utility>
#include <vector>

namespace noam {

// A continuous, row-by-row view of a markdown document that only renders the
// part around the current position, so a large document costs no more per
// frame than a small one.
//
// The document is split into pages at top-level block boundaries (a fenced
// block, table, list or quote is never split), each roughly twice the screen
// height in source lines; a single block longer than that is a page on its
// own. Pages are rendered on demand and at most three are kept, so the reader
// scrolls across page boundaries without seeing them. Changing the width
// re-renders the kept pages; changing the height re-splits the document.
class pager {
  public:
    explicit pager(std::string _content);

    // The rows of a `_width` x `_height` screen at the current position, at
    // most `_height` of them, applying any scrolling requested since the last
    // call.
    auto rows(int _width, int _height) -> ftxui::Elements;
    // Move the current position by `_delta` rows (negative is up).
    auto scroll(int _delta) -> void;
    // Jump to the start or the end of the document.
    auto home() -> void;
    auto end() -> void;

  private:
    struct page {
        size_t __index;
        ftxui::Elements __rows;
    };

    auto layout(int _width, int _height) -> void;
    auto reset_window(size_t _index) -> void;
    auto render(size_t _index) const -> ftxui::Elements;
    auto window_rows() const -> int;
    auto settle(int _height) -> void;

    std::string __content;
    std::vector<std::pair<size_t, size_t>> __pages; // byte ranges of __content
    std::deque<page> __window;                      // consecutive rendered pages
    int __top = 0;   // first visible row, counted from the start of __window
    int __jump = 0;  // pending jump to the start (-1) or the end (1)
    int __width = 0; // screen size the pages were split and rendered for
    int __height = 0;
};

} // namespace noam
