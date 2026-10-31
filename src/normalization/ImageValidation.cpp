#include "normalization/Internal.hpp"
#include <cmath>
#include <iomanip>
#include <sstream>
#include <vector>

namespace batbox::normalization {
using namespace internal;
namespace {
constexpr std::size_t imageMaxBase64Size=5*1024*1024;
std::string formatFileSize(std::size_t size) {
  const double kb=static_cast<double>(size)/1024;
  if(kb<1) return std::to_string(size)+" bytes";
  auto oneDecimal=[](double value,const char* unit) {
    std::ostringstream out;out<<std::fixed<<std::setprecision(1)<<value;
    std::string text=out.str();if(text.size()>=2 && text.substr(text.size()-2)==".0") text.resize(text.size()-2);
    return text+unit;
  };
  if(kb<1024) return oneDecimal(kb,"KB");
  const double mb=kb/1024;
  if(mb<1024) return oneDecimal(mb,"MB");
  return oneDecimal(mb/1024,"GB");
}
bool isBase64ImageBlock(const Json& block) {
  return block.is_object() && typeIs(block,"image") && get(block,"source").is_object() &&
    get(get(block,"source"),"type")=="base64" && get(get(block,"source"),"data").is_string();
}
} // namespace

void validateImagesForAPI(const Json& messages) {
  std::vector<std::pair<std::size_t,std::size_t>> oversizedImages;
  std::size_t imageIndex=0;
  for(const auto& msg:messages) {
    if(!msg.is_object() || !typeIs(msg,"user")) continue;
    const Json& inner=get(msg,"message");
    if(!inner.is_object()) continue;
    const Json& content=get(inner,"content");
    if(!content.is_array()) continue;
    for(const auto& block:content) {
      if(!isBase64ImageBlock(block)) continue;
      ++imageIndex;
      const std::size_t base64Size=value::stringLength(get(get(block,"source"),"data").get<std::string>());
      if(base64Size>imageMaxBase64Size) oversizedImages.emplace_back(imageIndex,base64Size);
    }
  }
  if(oversizedImages.empty()) return;
  const std::string max=formatFileSize(imageMaxBase64Size);
  if(oversizedImages.size()==1) {
    const auto [index,size]=oversizedImages.front();
    (void)index;
    throw std::runtime_error("Image base64 size ("+formatFileSize(size)+") exceeds API limit ("+max+"). Please resize the image before sending.");
  }
  std::string details;
  for(const auto& [index,size]:oversizedImages) {
    if(!details.empty()) details+=", ";
    details+="Image "+std::to_string(index)+": "+formatFileSize(size);
  }
  throw std::runtime_error(std::to_string(oversizedImages.size())+" images exceed the API limit ("+max+"): "+details+". Please resize these images before sending.");
}

} // namespace batbox::normalization
