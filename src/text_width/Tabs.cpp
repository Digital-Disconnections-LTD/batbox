#include "text_width/Tabs.hpp"
#include "termio/Tokenizer.hpp"
#include <cmath>
#include <stdexcept>
namespace batbox::text_width {
double widestLine(const std::string& text,
  const std::function<double(const std::string&)>& lineWidth){
  double width=0;std::size_t start=0;
  while(start<=text.size()){
    const auto end=text.find('\n',start);
    const auto w=lineWidth(end==std::string::npos?text.substr(start):text.substr(start,end-start));
    width=(std::isnan(width)||std::isnan(w))?NAN:std::max(width,w);
    if(end==std::string::npos)break;
    start=end+1;
  }
  return width;
}
std::string expandTabs(const std::string& text,double interval,
  const std::function<double(const std::string&)>& stringWidth){
  if(text.find('\t')==std::string::npos)return text;
  termio::Tokenizer tokenizer;
  auto tokens=tokenizer.feed(text);auto tail=tokenizer.flush();
  tokens.insert(tokens.end(),tail.begin(),tail.end());
  std::string result;double column=0;
  for(const auto& token:tokens){
    if(token.type=="sequence")result+=token.value;
    else{
      std::size_t start=0;
      for(std::size_t i=0;i<=token.value.size();++i){
        if(i<token.value.size()&&token.value[i]!='\t'&&token.value[i]!='\n')continue;
        const auto part=token.value.substr(start,i-start);
        result+=part;column+=stringWidth(part);
        if(i==token.value.size())break;
        if(token.value[i]=='\t'){
          const auto spaces=interval-std::fmod(column,interval);
          const auto repeat=std::isnan(spaces)?0:std::trunc(spaces);
          if(repeat<0||!std::isfinite(repeat))throw std::range_error("Invalid count value");
          result.append(static_cast<std::size_t>(repeat),' ');column+=spaces;
        }else{result+='\n';column=0;}
        start=i+1;
      }
    }
  }
  return result;
}
}
