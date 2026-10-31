#include "bun_text_width/WrapInternal.hpp"

namespace batbox::bun_text_width {
namespace {
constexpr std::uint32_t END_CODE=39;
std::optional<std::uint32_t> parseSgrCode(std::u16string_view data,std::size_t start){
  if(data.size()-start<3||data[start]!=0x1b||data[start+1]!='[')return std::nullopt;
  std::uint32_t code=0;
  for(std::size_t i=start+2;i<data.size();++i){
    const auto c=data[i];
    if(c>='0'&&c<='9')code=code*10+(c-'0');
    else if(c=='m')return code;
    else break;
  }
  return std::nullopt;
}
std::optional<std::pair<std::size_t,std::size_t>> parseOsc8Url(
  std::u16string_view data,std::size_t start){
  if(data.size()-start<6||data[start]!=0x1b||data[start+1]!=']'||
     data[start+2]!='8'||data[start+3]!=';'||data[start+4]!=';')return std::nullopt;
  const std::size_t urlStart=start+5;
  std::size_t urlEnd=urlStart;
  while(urlEnd<data.size()&&data[urlEnd]!=0x07&&data[urlEnd]!=0x1b)++urlEnd;
  if(urlEnd==urlStart)return std::nullopt;
  return std::pair{urlStart,urlEnd};
}
std::optional<std::uint32_t> getCloseCode(std::uint32_t code){
  switch(code){
    case 1:case 2:return 22;
    case 3:return 23;
    case 4:return 24;
    case 5:case 6:return 25;
    case 7:return 27;
    case 8:return 28;
    case 9:return 29;
    default:break;
  }
  if((code>=30&&code<=37)||(code>=90&&code<=97))return 39;
  if((code>=40&&code<=47)||(code>=100&&code<=107))return 49;
  return std::nullopt;
}
void appendSgr(std::u16string& result,std::uint32_t code){
  result+=u"\x1b[";
  const auto digits=std::to_string(code);
  for(const char digit:digits)result+=static_cast<char16_t>(digit);
  result+=u'm';
}
}
void joinRowsWithAnsiPreservation(const Rows& rows,std::u16string& result){
  std::u16string joined;
  std::size_t totalSize=0;
  for(const auto& row:rows)totalSize+=row.data.size()+1;
  joined.reserve(totalSize);
  for(std::size_t i=0;i<rows.size();++i){
    if(i)joined+=u'\n';
    joined+=rows[i].data;
  }
  std::optional<std::uint32_t> escapeCode;
  std::optional<std::pair<std::size_t,std::size_t>> escapeUrl;
  for(std::size_t i=0;i<joined.size();++i){
    const auto c=joined[i];
    result+=c;
    if(c==0x1b&&i+1<joined.size()){
      if(joined[i+1]=='['){
        if(const auto code=parseSgrCode(joined,i)){
          if(*code==END_CODE||*code==0)escapeCode.reset();
          else escapeCode=*code;
        }
      }else if(i+4<joined.size()&&joined[i+1]==']'&&joined[i+2]=='8'&&
               joined[i+3]==';'&&joined[i+4]==';'){
        escapeUrl=parseOsc8Url(joined,i);
      }
    }
    if(i+1<joined.size()&&joined[i+1]=='\n'){
      if(escapeUrl)result+=u"\x1b]8;;\x07";
      if(escapeCode){
        if(const auto close=getCloseCode(*escapeCode))appendSgr(result,*close);
      }
    }else if(c=='\n'){
      if(escapeCode)appendSgr(result,*escapeCode);
      if(escapeUrl){
        result+=u"\x1b]8;;";
        result.append(joined.substr(escapeUrl->first,
          escapeUrl->second-escapeUrl->first));
        result+=static_cast<char16_t>(0x07);
      }
    }
  }
}
} // namespace batbox::bun_text_width
