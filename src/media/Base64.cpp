#include "media/Base64.hpp"
namespace batbox::media::indigenous {
std::string base64Encode(const std::string& bytes){
  static constexpr char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string output;output.reserve(((bytes.size()+2)/3)*4);
  for(std::size_t i=0;i<bytes.size();i+=3){
    const unsigned a=static_cast<unsigned char>(bytes[i]);
    const unsigned b=i+1<bytes.size()?static_cast<unsigned char>(bytes[i+1]):0;
    const unsigned c=i+2<bytes.size()?static_cast<unsigned char>(bytes[i+2]):0;
    output+=alphabet[a>>2];output+=alphabet[((a&3)<<4)|(b>>4)];
    output+=i+1<bytes.size()?alphabet[((b&15)<<2)|(c>>6)]:'=';
    output+=i+2<bytes.size()?alphabet[c&63]:'=';
  }
  return output;
}
std::string base64Decode(std::string_view encoded){
  std::string output;output.reserve(encoded.size()*3/4);
  unsigned value=0;int bits=-8;
  for(const unsigned char c:encoded){
    unsigned digit=0;
    if(c>='A'&&c<='Z')digit=c-'A';
    else if(c>='a'&&c<='z')digit=c-'a'+26;
    else if(c>='0'&&c<='9')digit=c-'0'+52;
    else if(c=='+')digit=62;
    else if(c=='/')digit=63;
    else if(c=='=')break;
    else continue;
    value=(value<<6)|digit;bits+=6;
    if(bits>=0){output.push_back(static_cast<char>((value>>bits)&255));bits-=8;}
  }
  return output;
}
}
