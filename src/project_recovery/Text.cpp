#include "project_recovery/Internal.hpp"
#include <algorithm>
#include <cstdint>

namespace batbox::project_recovery::detail {
TextSlice utf8Prefix(std::string_view text,std::size_t limit){
  TextSlice result;
  result.text.reserve(std::min(text.size(),limit));
  std::size_t offset=0;
  while(offset<text.size()){
    const auto first=static_cast<unsigned char>(text[offset]);
    std::size_t length=1;
    std::uint32_t code=first;
    bool valid=first>0&&first<0x80;
    if(first>=0xc2&&first<=0xdf){length=2;code=first&0x1f;valid=true;}
    else if(first>=0xe0&&first<=0xef){length=3;code=first&0x0f;valid=true;}
    else if(first>=0xf0&&first<=0xf4){length=4;code=first&0x07;valid=true;}
    if(offset+length>text.size())valid=false;
    if(valid&&length>1){
      for(std::size_t index=1;index<length;++index){
        const auto byte=static_cast<unsigned char>(text[offset+index]);
        if((byte&0xc0)!=0x80){valid=false;break;}
        code=(code<<6)|(byte&0x3f);
      }
      valid=valid&&code<=0x10ffff&&!(code>=0xd800&&code<=0xdfff)&&
        (length!=2||code>=0x80)&&(length!=3||code>=0x800)&&
        (length!=4||code>=0x10000);
    }
    const std::string_view piece=valid?text.substr(offset,length):"\xef\xbf\xbd";
    if(!valid)result.malformed=true;
    if(result.text.size()+piece.size()>limit)break;
    result.text.append(piece);
    offset+=valid?length:1;
  }
  result.truncated=offset<text.size();
  return result;
}
}
