#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace batbox::glob {
// node-ignore 7.0.5 `ignore().add(patterns).ignores(path)` contract.
// `patterns` is the array overload, so individual entries are not split on LF.
bool ignores(std::string_view relativePath,
  const std::vector<std::string>& patterns);
namespace detail {
std::u16string ignoreRegexPrefix(std::u16string body);
std::u16string ignoreRegexSource(std::u16string prefix);
bool validIgnorePattern(std::u16string_view pattern);
} // namespace detail
} // namespace batbox::glob
