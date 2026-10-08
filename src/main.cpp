#include <algorithm>
#include <filesystem>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>
#include <iostream>
#include <noam/document.h>
#include <noam/pager.h>
#include <noam/theme.h>
#include <string>
#include <zapata/json.h>

int main(int _argc, char* _argv[]) {
    if (_argc < 2) {
        std::cerr << "usage: noam <document>" << std::endl;
        return 1;
    }

    // Without a readable configuration the document is still shown, in the
    // theme's default colors.
    auto _config = zpt::json::object();
    try {
        zpt::conf::file(NOAM_INSTALL_PREFIX "/share/noam/default.conf", _config, _config);
    }
    catch (zpt::failed_expectation const& _e) {
        std::cerr << "noam: " << _e.what() << std::endl;
    }
    noam::set_theme(_config("theme"));

    noam::pager _pager{ noam::load_document(_argv[1]) };

    auto _screen = ftxui::ScreenInteractive::FullscreenAlternateScreen();
    bool _repainted = false;

    // Sizes come from the live terminal, not the screen's dimx()/dimy(): on a
    // resize, ftxui renders the frame before it updates the screen's
    // dimensions, so those would still report the previous size.
    auto _view = ftxui::Renderer([&] {
        // At start-up ftxui queries the terminal (cursor shape, version); a
        // terminal that doesn't understand a query can echo it onto the first
        // frame. Request a second frame right away, which repaints over it.
        if (!_repainted) {
            _repainted = true;
            _screen.PostEvent(ftxui::Event::Custom);
        }
        auto _size = ftxui::Terminal::Size();
        return ftxui::vbox(_pager.rows(_size.dimx, _size.dimy)) | ftxui::flex;
    });

    auto _root = ftxui::CatchEvent(_view, [&](ftxui::Event _event) {
        int _page = std::max(ftxui::Terminal::Size().dimy - 1, 1);
        if (_event == ftxui::Event::ArrowUp) { _pager.scroll(-1); }
        else if (_event == ftxui::Event::ArrowDown) { _pager.scroll(1); }
        else if (_event == ftxui::Event::PageUp) { _pager.scroll(-_page); }
        else if (_event == ftxui::Event::PageDown || _event == ftxui::Event::Character(' ')) {
            _pager.scroll(_page);
        }
        else if (_event == ftxui::Event::Home) { _pager.home(); }
        else if (_event == ftxui::Event::End) { _pager.end(); }
        else if (_event == ftxui::Event::Character('q') || _event == ftxui::Event::Escape) {
            _screen.Exit();
        }
        else { return false; }
        return true;
    });

    _screen.Loop(_root);
    return 0;
}
