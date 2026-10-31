#pragma once

#include "interactive/cards/BashResultProjection.hpp"
#include "ftxui/dom/elements.hpp"
#include <string>

namespace batbox::interactive::cards {

ftxui::Element projectUserBashOutput(const std::string& sourceText,
                                      bool verbose,
                                      const BashResultProjectionImports& imports);

} // namespace batbox::interactive::cards
