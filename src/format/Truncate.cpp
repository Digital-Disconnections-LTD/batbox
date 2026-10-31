#include "format/Truncate.hpp"
#include <stdexcept>

namespace batbox::format {
std::string truncateToWidth(const std::string& text,double maxWidth,
    const TruncateServices& services){
  if(!services.stringWidth||!services.getGraphemeSegmenter)
    throw std::invalid_argument("Truncation imports required");
  if(services.stringWidth(text)<=maxWidth)return text;
  if(maxWidth<=1)return "…";
  double width=0;
  std::string result;
  auto cursor=services.getGraphemeSegmenter()->segment(text);
  while(const auto segment=cursor.next()){
    const auto segmentWidth=services.stringWidth(segment->text);
    if(width+segmentWidth>maxWidth-1)break;
    result+=segment->text;
    width+=segmentWidth;
  }
  return result+"…";
}
std::string truncate(const std::string& text,double maxWidth,bool singleLine,
    const TruncateServices& services){
  if(!services.stringWidth||!services.getGraphemeSegmenter)
    throw std::invalid_argument("Truncation imports required");
  std::string result=text;
  if(singleLine){
    const auto newline=text.find('\n');
    if(newline!=std::string::npos){
      result=text.substr(0,newline);
      if(services.stringWidth(result)+1>maxWidth)
        return truncateToWidth(result,maxWidth,services);
      return result+"…";
    }
  }
  if(services.stringWidth(result)<=maxWidth)return result;
  return truncateToWidth(result,maxWidth,services);
}
TruncateServices indigenousTruncateServices(std::shared_ptr<intl::Stash> stash,
    std::shared_ptr<text_width::StringWidth> width){
  if(!stash||!width)throw std::invalid_argument("Native truncation imports required");
  return {[width](const std::string& text){return width->stringWidth(text);},
    [stash]{return stash->getGraphemeSegmenter();}};
}
std::function<std::string(const std::string&,double,bool)>
bindTruncate(TruncateServices services){
  if(!services.stringWidth||!services.getGraphemeSegmenter)
    throw std::invalid_argument("Truncation imports required");
  return [services=std::move(services)](const std::string& text,
      double maxWidth,bool singleLine){
    return truncate(text,maxWidth,singleLine,services);
  };
}
}
