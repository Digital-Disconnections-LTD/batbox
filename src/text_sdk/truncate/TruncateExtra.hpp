#pragma once
#include "text_sdk/truncate/Truncate.hpp"
#include <string>
#include <vector>

namespace batbox::text_sdk::truncate {
std::string truncatePathMiddle(const std::string& path,double maxLength,
  const TruncateServices& services);
std::string truncateStartToWidth(const std::string& text,double maxWidth,
  const TruncateServices& services);
std::string truncateToWidthNoEllipsis(const std::string& text,double maxWidth,
  const TruncateServices& services);
std::vector<std::string> wrapText(const std::string& text,double width,
  const TruncateServices& services);
}
