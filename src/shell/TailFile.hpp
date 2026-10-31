#pragma once
#include <cstddef>
#include <string>

namespace batbox::shell {
struct TailResult {std::string content;std::size_t bytesRead=0,bytesTotal=0;};
TailResult tailFile(const std::string& path,std::size_t maxBytes);
}
