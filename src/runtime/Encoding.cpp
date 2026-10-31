#include "runtime/JsonValue.hpp"
namespace batbox::value {
std::string decodeUtf8(std::string_view input,bool stripBom) {
  std::string output;
  for(std::size_t i=0;i<input.size();) {
    const auto start=i;const unsigned char lead=input[i++];
    if(lead<0x80){output+=static_cast<char>(lead);continue;}
    const int count=lead>=0xc2 && lead<=0xdf ? 1 : lead>=0xe0 && lead<=0xef ? 2 : lead>=0xf0 && lead<=0xf4 ? 3 : 0;
    if(!count){output+="\xef\xbf\xbd";continue;}
    bool valid=true;
    for(int n=0;n<count;++n) {
      if(i==input.size()){valid=false;break;}
      const unsigned char byte=input[i];
      const unsigned char lower=n==0 && lead==0xe0 ? 0xa0 : n==0 && lead==0xf0 ? 0x90 : 0x80;
      const unsigned char upper=n==0 && lead==0xed ? 0x9f : n==0 && lead==0xf4 ? 0x8f : 0xbf;
      if(byte<lower || byte>upper){valid=false;break;}
      ++i;
    }
    if(valid)output.append(input.substr(start,i-start));else output+="\xef\xbf\xbd";
  }
  if(stripBom && output.starts_with("\xef\xbb\xbf"))output.erase(0,3);
  return output;
}
}
