#pragma once

#include "interactive/markdown/Format.hpp"

namespace batbox::interactive::markdown {
using WrapAnsi=std::function<std::string(const std::string&,int,bool hard)>;
std::string renderTable(const Token& table,int terminalWidth,
    const FormatServices& services,const WrapAnsi& wrapAnsi);
} // namespace batbox::interactive::markdown
