#pragma once

#include <string>

namespace noam {

// Returns the markdown content of the document at `_location` (a local path
// or a remote URL).
auto load_document(std::string const& _location) -> std::string;

} // namespace noam
