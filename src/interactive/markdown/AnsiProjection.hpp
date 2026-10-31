#pragma once

#include <ftxui/dom/elements.hpp>
#include <string>

namespace batbox::interactive::markdown {
ftxui::Element projectAnsi(const std::string& source,bool dim=false);
} // namespace batbox::interactive::markdown
