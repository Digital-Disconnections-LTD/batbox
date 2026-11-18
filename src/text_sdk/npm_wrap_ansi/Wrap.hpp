#pragma once
#include "text_sdk/width/WrapAnsi.hpp"
#include <optional>
#include <string>

namespace batbox::text_sdk::npm_wrap_ansi {
std::string wrapAnsi(const std::string& input,double columns,
                     const std::optional<Json>& options=std::nullopt);
text_sdk::width::WrapAnsiRef indigenousWrapAnsiRef();
}
