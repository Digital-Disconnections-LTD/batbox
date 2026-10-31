#pragma once
#include "text_width/WrapAnsi.hpp"
#include <optional>
#include <string>

namespace batbox::npm_wrap_ansi {
std::string wrapAnsi(const std::string& input,double columns,
                     const std::optional<Json>& options=std::nullopt);
std::string wrapAnsiJs(const Json& input,const Json& columns,
                       const std::optional<Json>& options=std::nullopt);
text_width::WrapAnsiRef indigenousWrapAnsiRef();
}
