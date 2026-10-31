#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace batbox::glob {
// picomatch 4.0.4 `isMatch(input, patterns, {dot:true})` on POSIX.
bool isMatch(std::string_view path,const std::vector<std::string>& patterns);
} // namespace batbox::glob
