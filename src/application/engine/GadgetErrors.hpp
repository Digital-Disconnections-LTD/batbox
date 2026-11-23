#pragma once
#include <exception>
#include <string>
#include <vector>

namespace batbox::application::engine {
std::vector<std::string> getGadgetErrorParts(const std::exception& error);
std::string formatGadgetError(const std::exception& error);
}
