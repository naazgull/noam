#pragma once

#include <ftxui/screen/color.hpp>
#include <noam/theme.h>
#include <zapata/json.h>

namespace noam {

// Reads the "syntax" section of `_theme` into the in-effect theme; entries left
// out keep their value. The zapata-json based theming stays in the application
// layer — the reusable noam-markdown library (theme.h) is zapata-free.
auto set_theme(zpt::json const& _theme) -> void;
auto render_color(zpt::json const& _color, ftxui::Color const& _default) -> ftxui::Color;

} // namespace noam
