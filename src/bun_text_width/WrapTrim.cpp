#include "bun_text_width/WrapInternal.hpp"

namespace batbox::bun_text_width {
namespace {
bool ansiTerminator(char16_t c,bool osc){
  if(osc)return c==0x07;
  return c>=0x40&&c<=0x7e&&c!='[';
}
std::u16string collectAnsi(std::u16string_view data,std::size_t start){
  std::u16string ansi;
  bool inEscape=false,inOscEscape=false;
  for(std::size_t i=start;i<data.size();++i){
    if(data[i]==0x1b||inEscape){
      ansi+=data[i];
      if(data[i]==0x1b){
        inEscape=true;inOscEscape=i+1<data.size()&&data[i+1]==']';
      }else if(ansiTerminator(data[i],inOscEscape)){
        inEscape=inOscEscape=false;
      }
    }
  }
  return ansi;
}
}
void trimRowTrailingSpaces(Row& row,const WrapContext& context){
  const std::u16string_view data=row.data;
  std::size_t lastVisibleEnd=0,wordStart=0;
  bool hasVisibleContent=false;
  for(std::size_t i=0;i<=data.size();++i){
    if(i==data.size()||data[i]==' '){
      if(wordStart<i&&context.width(data.substr(wordStart,i-wordStart))>0){
        hasVisibleContent=true;lastVisibleEnd=i;
      }
      wordStart=i+1;
    }
  }
  if(!hasVisibleContent){
    row.data=collectAnsi(data,0);return;
  }
  if(lastVisibleEnd<data.size()){
    const auto trailingAnsi=collectAnsi(data,lastVisibleEnd);
    row.data.resize(lastVisibleEnd);
    row.data+=trailingAnsi;
  }
}
} // namespace batbox::bun_text_width
