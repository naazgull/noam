#include <algorithm>
#include <noam/theme_json.h>

namespace {
auto get_color(std::string const& _color, ftxui::Color const& _default) -> ftxui::Color;
} // namespace

auto noam::set_theme(zpt::json const& _theme) -> void {
    // Start from the in-effect theme so entries left out of `_theme` keep their
    // value, then write the result back through the library's setter.
    auto _result = noam::theme();

    _result.__syntax.__comment =
      noam::render_color(_theme("syntax")("comment"), _result.__syntax.__comment);
    _result.__syntax.__quote_bar =
      noam::render_color(_theme("syntax")("quote_bar"), _result.__syntax.__quote_bar);
    _result.__syntax.__quote_background = noam::render_color(_theme("syntax")("quote_background"),
                                                             _result.__syntax.__quote_background);
    _result.__syntax.__fenced_block_background = noam::render_color(
      _theme("syntax")("fenced_block_background"), _result.__syntax.__fenced_block_background);
    _result.__syntax.__escape =
      noam::render_color(_theme("syntax")("escape"), _result.__syntax.__escape);
    _result.__syntax.__string_special_key = noam::render_color(
      _theme("syntax")("string_special_key"), _result.__syntax.__string_special_key);
    _result.__syntax.__string_special =
      noam::render_color(_theme("syntax")("string_special"), _result.__syntax.__string_special);
    _result.__syntax.__constant_builtin = noam::render_color(
      _theme("syntax")("constant_builtin"), _result.__syntax.__constant_builtin);
    _result.__syntax.__number =
      noam::render_color(_theme("syntax")("number"), _result.__syntax.__number);
    _result.__syntax.__function =
      noam::render_color(_theme("syntax")("function"), _result.__syntax.__function);
    _result.__syntax.__preproc =
      noam::render_color(_theme("syntax")("preproc"), _result.__syntax.__preproc);
    _result.__syntax.__variable_builtin = noam::render_color(
      _theme("syntax")("variable_builtin"), _result.__syntax.__variable_builtin);
    _result.__syntax.__variable =
      noam::render_color(_theme("syntax")("variable"), _result.__syntax.__variable);
    _result.__syntax.__markup_strong =
      noam::render_color(_theme("syntax")("markup_strong"), _result.__syntax.__markup_strong);
    _result.__syntax.__markup_italic =
      noam::render_color(_theme("syntax")("markup_italic"), _result.__syntax.__markup_italic);
    _result.__syntax.__markup_code =
      noam::render_color(_theme("syntax")("markup_code"), _result.__syntax.__markup_code);
    _result.__syntax.__markup_link =
      noam::render_color(_theme("syntax")("markup_link"), _result.__syntax.__markup_link);
    _result.__syntax.__markup_heading =
      noam::render_color(_theme("syntax")("markup_heading"), _result.__syntax.__markup_heading);
    _result.__syntax.__markup_heading_1 = noam::render_color(
      _theme("syntax")("markup_heading_1"), _result.__syntax.__markup_heading_1);
    _result.__syntax.__markup_heading_2 = noam::render_color(
      _theme("syntax")("markup_heading_2"), _result.__syntax.__markup_heading_2);
    _result.__syntax.__markup_heading_3 = noam::render_color(
      _theme("syntax")("markup_heading_3"), _result.__syntax.__markup_heading_3);
    _result.__syntax.__markup_heading_4 = noam::render_color(
      _theme("syntax")("markup_heading_4"), _result.__syntax.__markup_heading_4);
    _result.__syntax.__markup_heading_5 = noam::render_color(
      _theme("syntax")("markup_heading_5"), _result.__syntax.__markup_heading_5);
    _result.__syntax.__markup_heading_6 = noam::render_color(
      _theme("syntax")("markup_heading_6"), _result.__syntax.__markup_heading_6);
    _result.__syntax.__markup_ruler =
      noam::render_color(_theme("syntax")("markup_ruler"), _result.__syntax.__markup_ruler);
    _result.__syntax.__type =
      noam::render_color(_theme("syntax")("type"), _result.__syntax.__type);
    _result.__syntax.__namespace =
      noam::render_color(_theme("syntax")("namespace"), _result.__syntax.__namespace);
    _result.__syntax.__string =
      noam::render_color(_theme("syntax")("string"), _result.__syntax.__string);
    _result.__syntax.__keyword =
      noam::render_color(_theme("syntax")("keyword"), _result.__syntax.__keyword);
    _result.__syntax.__punctuation_special = noam::render_color(
      _theme("syntax")("punctuation_special"), _result.__syntax.__punctuation_special);
    _result.__syntax.__table_border =
      noam::render_color(_theme("syntax")("table_border"), _result.__syntax.__table_border);
    _result.__syntax.__table_header =
      noam::render_color(_theme("syntax")("table_header"), _result.__syntax.__table_header);
    _result.__syntax.__diff_addition =
      noam::render_color(_theme("syntax")("diff_addition"), _result.__syntax.__diff_addition);
    _result.__syntax.__diff_removal =
      noam::render_color(_theme("syntax")("diff_removal"), _result.__syntax.__diff_removal);
    _result.__syntax.__diff_header =
      noam::render_color(_theme("syntax")("diff_header"), _result.__syntax.__diff_header);

    noam::set_theme(_result);
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
