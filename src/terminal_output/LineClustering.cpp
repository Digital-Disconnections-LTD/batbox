#include "terminal_output/LineRender.hpp"
#include "terminal_screen/HyperlinkStyles.hpp"
#include <stdexcept>

namespace batbox::terminal_output {
bool stylesEqual(const ansi_tokenize::Codes& a,
    const ansi_tokenize::Codes& b){
  if(&a==&b)return true;
  const auto len=a.size();
  if(len!=b.size())return false;
  if(len==0)return true;
  for(std::size_t i=0;i<len;i++)
    if(a[i]->code!=b[i]->code)return false;
  return true;
}
void flushBuffer(const std::string& buffer,const ansi_tokenize::Codes& styles,
    StylePool& stylePool,ClusteredChars& out,const LineServices& services){
  const auto hyperlink=terminal_screen::extractHyperlinkFromStyles(styles);
  const bool hasOsc8Styles=hyperlink.has_value()||[&]{
    for(const auto& style:styles)
      if(style->code.size()>=4&&style->code.compare(0,4,"\x1b]8;")==0)
        return true;
    return false;
  }();
  auto filtered=hasOsc8Styles?
    terminal_screen::filterOutHyperlinkStyles(styles):styles;
  const auto styleId=stylePool.intern(
    std::make_shared<ansi_tokenize::Codes>(std::move(filtered)));
  if(!services.getGraphemeSegmenter||!services.stringWidth)
    throw std::invalid_argument("Line segmentation and width imports required");
  auto cursor=services.getGraphemeSegmenter()->segment(buffer);
  while(const auto segment=cursor.next()){
    out.push_back(std::make_shared<ClusteredChar>(ClusteredChar{
      segment->text,services.stringWidth(segment->text),styleId,hyperlink}));
  }
}
ClusteredCharsRef styledCharsWithGraphemeClustering(
    const ansi_tokenize::StyledChars& chars,StylePool& stylePool,
    const LineServices& services){
  const auto result=std::make_shared<ClusteredChars>();
  const auto charCount=chars.size();
  if(charCount==0)return result;
  std::vector<std::string> bufferChars;
  const ansi_tokenize::Codes* bufferStyles=&chars[0].styles;
  for(std::size_t i=0;i<charCount;i++){
    const auto& character=chars[i];
    const auto& styles=character.styles;
    if(!bufferChars.empty()&&!stylesEqual(styles,*bufferStyles)){
      std::string buffer;
      for(const auto& value:bufferChars)buffer+=value;
      flushBuffer(buffer,*bufferStyles,stylePool,*result,services);
      bufferChars.clear();
    }
    bufferChars.push_back(character.value);
    bufferStyles=&styles;
  }
  if(!bufferChars.empty()){
    std::string buffer;
    for(const auto& value:bufferChars)buffer+=value;
    flushBuffer(buffer,*bufferStyles,stylePool,*result,services);
  }
  return result;
}
} // namespace batbox::terminal_output
