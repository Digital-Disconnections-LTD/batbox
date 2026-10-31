#pragma once
#include "gadgets/files/FileRead.hpp"
#include <string>

namespace batbox::gadgets::files {
std::string readPoetry(const FileReadServices& services);
std::string writePoetry();
std::string editPoetry(bool compactLinePrefix,bool conciseEditHint);
std::string globPoetry();
std::string grepPoetry(const std::string& batGadgetName="Agent",
                       const std::string& bashGadgetName="Bash");
} // namespace batbox::gadgets::files
