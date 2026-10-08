#include <algorithm>
#include <noam/theme.h>

namespace {
noam::noam_theme ___theme{};

auto get_color(std::string const& _color, ftxui::Color const& _default) -> ftxui::Color;
} // namespace

auto noam::theme() -> noam::noam_theme& { return ___theme; }

auto noam::set_theme(noam::noam_theme _theme) -> void { ___theme = _theme; }

auto noam::set_theme(zpt::json const& _theme) -> void {

    ___theme.__syntax.__comment =
      noam::render_color(_theme("syntax")("comment"), ___theme.__syntax.__comment);
    ___theme.__syntax.__quote_bar =
      noam::render_color(_theme("syntax")("quote_bar"), ___theme.__syntax.__quote_bar);
    ___theme.__syntax.__quote_background = noam::render_color(_theme("syntax")("quote_background"),
                                                              ___theme.__syntax.__quote_background);
    ___theme.__syntax.__escape =
      noam::render_color(_theme("syntax")("escape"), ___theme.__syntax.__escape);
    ___theme.__syntax.__string_special_key = noam::render_color(
      _theme("syntax")("string_special_key"), ___theme.__syntax.__string_special_key);
    ___theme.__syntax.__string_special =
      noam::render_color(_theme("syntax")("string_special"), ___theme.__syntax.__string_special);
    ___theme.__syntax.__constant_builtin = noam::render_color(_theme("syntax")("constant_builtin"),
                                                              ___theme.__syntax.__constant_builtin);
    ___theme.__syntax.__number =
      noam::render_color(_theme("syntax")("number"), ___theme.__syntax.__number);
    ___theme.__syntax.__function =
      noam::render_color(_theme("syntax")("function"), ___theme.__syntax.__function);
    ___theme.__syntax.__preproc =
      noam::render_color(_theme("syntax")("preproc"), ___theme.__syntax.__preproc);
    ___theme.__syntax.__variable_builtin = noam::render_color(_theme("syntax")("variable_builtin"),
                                                              ___theme.__syntax.__variable_builtin);
    ___theme.__syntax.__variable =
      noam::render_color(_theme("syntax")("variable"), ___theme.__syntax.__variable);
    ___theme.__syntax.__markup_strong =
      noam::render_color(_theme("syntax")("markup_strong"), ___theme.__syntax.__markup_strong);
    ___theme.__syntax.__markup_italic =
      noam::render_color(_theme("syntax")("markup_italic"), ___theme.__syntax.__markup_italic);
    ___theme.__syntax.__markup_code =
      noam::render_color(_theme("syntax")("markup_code"), ___theme.__syntax.__markup_code);
    ___theme.__syntax.__markup_link =
      noam::render_color(_theme("syntax")("markup_link"), ___theme.__syntax.__markup_link);
    ___theme.__syntax.__markup_heading =
      noam::render_color(_theme("syntax")("markup_heading"), ___theme.__syntax.__markup_heading);
    ___theme.__syntax.__markup_ruler =
      noam::render_color(_theme("syntax")("markup_ruler"), ___theme.__syntax.__markup_ruler);
    ___theme.__syntax.__type =
      noam::render_color(_theme("syntax")("type"), ___theme.__syntax.__type);
    ___theme.__syntax.__namespace =
      noam::render_color(_theme("syntax")("namespace"), ___theme.__syntax.__namespace);
    ___theme.__syntax.__string =
      noam::render_color(_theme("syntax")("string"), ___theme.__syntax.__string);
    ___theme.__syntax.__keyword =
      noam::render_color(_theme("syntax")("keyword"), ___theme.__syntax.__keyword);
    ___theme.__syntax.__punctuation_special = noam::render_color(
      _theme("syntax")("punctuation_special"), ___theme.__syntax.__punctuation_special);
    ___theme.__syntax.__table_border =
      noam::render_color(_theme("syntax")("table_border"), ___theme.__syntax.__table_border);
    ___theme.__syntax.__table_header =
      noam::render_color(_theme("syntax")("table_header"), ___theme.__syntax.__table_header);
    ___theme.__syntax.__diff_addition =
      noam::render_color(_theme("syntax")("diff_addition"), ___theme.__syntax.__diff_addition);
    ___theme.__syntax.__diff_removal =
      noam::render_color(_theme("syntax")("diff_removal"), ___theme.__syntax.__diff_removal);
    ___theme.__syntax.__diff_header =
      noam::render_color(_theme("syntax")("diff_header"), ___theme.__syntax.__diff_header);
}

auto noam::render_color(zpt::json const& _color, ftxui::Color const& _default) -> ftxui::Color {
    if (_color != json_null) {
        switch (_color->type()) {
            case zpt::JSString: {
                return ::get_color(_color->string(), _default);
            }
            case zpt::JSObject: {
                if (_color("r")->is_integer() && _color("g")->is_integer() &&
                    _color("b")->is_integer()) {
                    return ftxui::Color::RGB(
                      _color("r")->integer(), _color("g")->integer(), _color("b")->integer());
                }
                return _default;
            }
            case zpt::JSArray: {
                if (_color->size() == 3 && _color(0)->is_integer() && _color(1)->is_integer() &&
                    _color(2)->is_integer()) {
                    return ftxui::Color::RGB(
                      _color(0)->integer(), _color(1)->integer(), _color(2)->integer());
                }
                return _default;
            }
            default: {
                return _default;
            }
        }
    }
    return _default;
}

namespace {
auto get_color(std::string const& _color, ftxui::Color const& _default) -> ftxui::Color {
    std::string _lower_case;
    _lower_case.resize(_color.length());
    std::transform(_color.begin(), _color.end(), _lower_case.begin(), ::tolower);

    if (_lower_case == "transparent") { return ftxui::Color::Default; }
    if (_lower_case == "default") { return ftxui::Color::Default; }
    if (_lower_case == "black") { return ftxui::Color::Black; }
    if (_lower_case == "red") { return ftxui::Color::Red; }
    if (_lower_case == "green") { return ftxui::Color::Green; }
    if (_lower_case == "yellow") { return ftxui::Color::Yellow; }
    if (_lower_case == "blue") { return ftxui::Color::Blue; }
    if (_lower_case == "magenta") { return ftxui::Color::Magenta; }
    if (_lower_case == "cyan") { return ftxui::Color::Cyan; }
    if (_lower_case == "white") { return ftxui::Color::White; }
    if (_lower_case == "grey") { return ftxui::Color::RGB(128, 128, 128); }
    if (_lower_case == "brred") { return ftxui::Color::RedLight; }
    if (_lower_case == "brgreen") { return ftxui::Color::GreenLight; }
    if (_lower_case == "bryellow") { return ftxui::Color::YellowLight; }
    if (_lower_case == "brblue") { return ftxui::Color::BlueLight; }
    if (_lower_case == "brmagenta") { return ftxui::Color::MagentaLight; }
    if (_lower_case == "brcyan") { return ftxui::Color::CyanLight; }

    return _default;
}
} // namespace
