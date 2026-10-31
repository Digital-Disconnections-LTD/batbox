#pragma once

#include <optional>
#include <string>

namespace batbox::interactive {

std::string escapeRegExp(const std::string& value);
std::optional<std::string> extractTag(const std::string& text,
                                      const std::string& tagName);

} // namespace batbox::interactive
