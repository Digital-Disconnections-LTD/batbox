#include "gadgets/files/EditUtils.hpp"
#include <unicode/uchar.h>
#include <cstdint>
#include <vector>

namespace batbox::gadgets::files::edit {
namespace {
struct Rune{std::string text;std::uint32_t value;};
std::vector<Rune> splitRunes(const std::string& text){
  std::vector<Rune> out;
  for(std::size_t i=0;i<text.size();){
    const unsigned char first=text[i];
    const std::size_t width=first<0x80?1:first<0xe0?2:first<0xf0?3:4;
    if(i+width>text.size()){out.push_back({text.substr(i,1),first});++i;continue;}
    std::uint32_t value=first&((1u<<(7-width))-1);
    if(width==1)value=first;
    else for(std::size_t j=1;j<width;++j)value=(value<<6)|(static_cast<unsigned char>(text[i+j])&0x3f);
    out.push_back({text.substr(i,width),value});i+=width;
  }
  return out;
}
bool opening(const std::vector<Rune>& runes,std::size_t index){
  if(index==0)return true;
  const auto& previous=runes[index-1].text;
  return previous==" "||previous=="\t"||previous=="\n"||previous=="\r"||
    previous=="("||previous=="["||previous=="{"||previous=="—"||previous=="–";
}
std::string applyCurly(const std::string& text,bool single){
  const auto runes=splitRunes(text);
  std::string result;
  for(std::size_t i=0;i<runes.size();++i){
    const auto& current=runes[i].text;
    if(current!=(single?"'":"\"")){result+=current;continue;}
    if(single&&i>0&&i+1<runes.size()&&
       ::u_isalpha(static_cast<UChar32>(runes[i-1].value))&&
       ::u_isalpha(static_cast<UChar32>(runes[i+1].value))){result+="’";continue;}
    result+=single?(opening(runes,i)?"‘":"’"):(opening(runes,i)?"“":"”");
  }
  return result;
}
} // namespace

std::string preserveQuoteStyle(const std::string& oldString,const std::string& actualOldString,
                               const std::string& newString){
  if(oldString==actualOldString)return newString;
  const bool doubleQuotes=actualOldString.find("“")!=std::string::npos||
    actualOldString.find("”")!=std::string::npos;
  const bool singleQuotes=actualOldString.find("‘")!=std::string::npos||
    actualOldString.find("’")!=std::string::npos;
  if(!doubleQuotes&&!singleQuotes)return newString;
  std::string result=newString;
  if(doubleQuotes)result=applyCurly(result,false);
  if(singleQuotes)result=applyCurly(result,true);
  return result;
}
} // namespace batbox::gadgets::files::edit
