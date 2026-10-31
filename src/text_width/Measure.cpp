#include "text_width/Layout.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace batbox::text_width {
Measurement measureText(const std::string& text,double maxWidth,
    const std::function<double(const std::string&)>& lineWidth){
  if(text.empty())return {0,0};
  const bool noWrap=maxWidth<=0||!std::isfinite(maxWidth);
  double height=0,width=0;std::size_t start=0;
  while(start<=text.size()){
    const auto end=text.find('\n',start);
    const auto line=end==std::string::npos?text.substr(start):text.substr(start,end-start);
    const auto w=lineWidth(line);
    width=std::isnan(width)||std::isnan(w)?std::numeric_limits<double>::quiet_NaN():std::max(width,w);
    if(noWrap)++height;
    else height+=w==0?1:std::ceil(w/maxWidth);
    if(end==std::string::npos)break;start=end+1;
  }
  return {width,height};
}
}
