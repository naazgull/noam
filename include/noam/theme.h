#pragma once

#include <ftxui/screen/color.hpp>
#include <zapata/json.h>

namespace noam {

struct syntax_theme {
    ftxui::Color __comment = ftxui::Color::White;
    ftxui::Color __quote_bar = ftxui::Color::White;             // Block-quote left bar glyph
    ftxui::Color __quote_background = ftxui::Color(50, 50, 50); // Block-quote row background
    ftxui::Color __fenced_block_background = ftxui::Color::Default; // Fenced block row background
    ftxui::Color __escape = ftxui::Color::White;
    ftxui::Color __string_special_key = ftxui::Color::White;
    ftxui::Color __string_special = ftxui::Color::White;
    ftxui::Color __constant_builtin = ftxui::Color::White;
    ftxui::Color __number = ftxui::Color::White;
    ftxui::Color __function = ftxui::Color::White;
    ftxui::Color __preproc = ftxui::Color::White;
    ftxui::Color __variable_builtin = ftxui::Color::White;
    ftxui::Color __variable = ftxui::Color::White;
    ftxui::Color __markup_strong = ftxui::Color::White;
    ftxui::Color __markup_italic = ftxui::Color::White;
    ftxui::Color __markup_code = ftxui::Color::White;
    ftxui::Color __markup_link = ftxui::Color::White;
    ftxui::Color __markup_heading = ftxui::Color::White;
    ftxui::Color __markup_heading_1 = ftxui::Color::White; // Heading levels 1-6
    ftxui::Color __markup_heading_2 = ftxui::Color::White;
    ftxui::Color __markup_heading_3 = ftxui::Color::White;
    ftxui::Color __markup_heading_4 = ftxui::Color::White;
    ftxui::Color __markup_heading_5 = ftxui::Color::White;
    ftxui::Color __markup_heading_6 = ftxui::Color::White;
    ftxui::Color __markup_ruler = ftxui::Color::White;
    ftxui::Color __type = ftxui::Color::White;
    ftxui::Color __namespace = ftxui::Color::White;
    ftxui::Color __string = ftxui::Color::White;
    ftxui::Color __keyword = ftxui::Color::White;
    ftxui::Color __punctuation_special = ftxui::Color::White;
    ftxui::Color __table_border = ftxui::Color::White;  // Table box-drawing borders
    ftxui::Color __table_header = ftxui::Color::White;  // Table header row text
    ftxui::Color __diff_addition = ftxui::Color::Green; // ```diff added line
    ftxui::Color __diff_removal = ftxui::Color::Red;    // ```diff removed line
    ftxui::Color __diff_header = ftxui::Color::White;   // ```diff file/hunk-location header (bold)
};

struct noam_theme {
    syntax_theme __syntax;
};

// The theme in effect for the rendered document. A mutable singleton, so the
// colors can be replaced at runtime (e.g. from a theme file).
auto theme() -> noam_theme&;
auto set_theme(noam_theme _theme) -> void;
// Reads the "syntax" section of `_theme`; entries left out keep their value.
auto set_theme(zpt::json const& _theme) -> void;
auto render_color(zpt::json const& _color, ftxui::Color const& _default) -> ftxui::Color;

} // namespace noam
