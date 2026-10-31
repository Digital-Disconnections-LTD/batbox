#include "text_width/Layout.hpp"
#include <stdexcept>
namespace batbox::text_width {
LineWidthStash::LineWidthStash(std::function<double(const std::string&)> width):stringWidth_(std::move(width)){}
double LineWidthStash::lineWidth(const std::string& line){
  const auto stashed=stash_.find(line);if(stashed!=stash_.end())return stashed->second;
  // A string parameter is immutable, even if an imported width callback
  // reenters the stash and changes an unrelated caller-owned string.
  const auto key=line;const auto width=stringWidth_(key);
  if(stash_.size()>=4096)stash_.clear();
  stash_[key]=width;return width;
}
std::shared_ptr<LineWidthStash> indigenousLineWidthStash(std::shared_ptr<StringWidth> width){
  if(!width)throw std::invalid_argument("Native width provider required");
  return std::make_shared<LineWidthStash>([width=std::move(width)](const std::string& text){return width->stringWidth(text);});
}
}
