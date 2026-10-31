#pragma once
#include "intl/Intl.hpp"
#include "text_width/StringWidth.hpp"
#include <functional>
#include <memory>
#include <string>

namespace batbox::format {
struct TruncateServices {
  std::function<double(const std::string&)> stringWidth;
  std::function<std::shared_ptr<intl::Segmenter>()> getGraphemeSegmenter;
};
std::string truncateToWidth(const std::string& text,double maxWidth,
  const TruncateServices& services);
std::string truncate(const std::string& text,double maxWidth,bool singleLine,
  const TruncateServices& services);
TruncateServices indigenousTruncateServices(std::shared_ptr<intl::Stash> stash,
  std::shared_ptr<text_width::StringWidth> width);
std::function<std::string(const std::string&,double,bool)>
  bindTruncate(TruncateServices services);
}
