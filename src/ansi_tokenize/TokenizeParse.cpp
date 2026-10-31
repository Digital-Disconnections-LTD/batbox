#include "ansi_tokenize/TokenizeInternal.hpp"

namespace batbox::ansi_tokenize {
namespace {
std::size_t oscTerminator(std::u16string_view text,std::size_t start){
  for(std::size_t i=start;i<text.size();++i){
    const auto ch=text[i];
    if(ch==0x07||ch==0x9c)return i;
    if(ch==0x1b&&i+1<text.size()&&text[i+1]=='\\')return i+1;
  }
  return std::u16string_view::npos;
}
}
std::optional<std::u16string> parseLinkCode(std::u16string_view text,std::size_t offset){
  text=text.substr(offset);
  if(text.size()<4||text[1]!=']'||text[2]!='8'||text[3]!=';')return {};
  const auto paramsEnd=text.find(u';',4);
  if(paramsEnd==std::u16string_view::npos)return {};
  const auto end=oscTerminator(text,paramsEnd+1);
  if(end==std::u16string_view::npos)return {};
  return std::u16string(text.substr(0,end+1));
}
std::optional<std::u16string> parseOscSequence(std::u16string_view text,std::size_t offset){
  text=text.substr(offset);
  const auto end=oscTerminator(text,2);
  if(end==std::u16string_view::npos)return {};
  return std::u16string(text.substr(0,end+1));
}
std::optional<std::u16string> parseSgrSequence(std::u16string_view text,std::size_t offset){
  text=text.substr(offset);
  for(std::size_t i=2;i<text.size();++i){
    const auto c=text[i];
    if(c=='m')return std::u16string(text.substr(0,i+1));
    if(c==';'||(c>='0'&&c<='9'))continue;
    break;
  }
  return {};
}
std::vector<std::u16string> splitCompoundSgr(std::u16string_view code){
  if(code.find(u';')==std::u16string_view::npos)return {std::u16string(code)};
  const auto body=code.substr(2,code.size()-3);
  std::vector<std::u16string> parts;
  std::size_t start=0;
  while(start<=body.size()){
    const auto sep=body.find(u';',start);
    const auto end=sep==std::u16string_view::npos?body.size():sep;
    parts.emplace_back(body.substr(start,end-start));
    if(sep==std::u16string_view::npos)break;
    start=end+1;
  }
  std::vector<std::u16string> result;
  for(std::size_t i=0;i<parts.size();++i){
    std::u16string raw=parts[i];
    if(raw==u"38"||raw==u"48"){
      if(i+2<parts.size()&&parts[i+1]==u"5"){
        raw+=u";"+parts[i+1]+u";"+parts[i+2];i+=2;
      }else if(i+4<parts.size()&&parts[i+1]==u"2"){
        for(int j=1;j<=4;++j)raw+=u";"+parts[i+j];
        i+=4;
      }
    }
    result.push_back(u"\x1b["+raw+u"m");
  }
  return result;
}
} // namespace batbox::ansi_tokenize
