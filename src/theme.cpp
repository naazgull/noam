#include <noam/theme.h>

namespace {
noam::noam_theme ___theme{};
} // namespace

auto noam::theme() -> noam::noam_theme& { return ___theme; }

auto noam::set_theme(noam::noam_theme _theme) -> void { ___theme = _theme; }
