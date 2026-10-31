#pragma once

#include "ansi_tokenize/Types.hpp"
#include "ftxui/screen/cell.hpp"
#include <stdexcept>

namespace batbox::terminal_ftxui {

class ProjectionError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

// Apply the active terminal styles in their original order to one FTXUI cell.
// A style that FTXUI cannot represent fails visibly instead of changing its
// appearance without telling the caller.
void projectStyle(const ansi_tokenize::Codes& codes, ftxui::Cell& cell);

} // namespace batbox::terminal_ftxui
