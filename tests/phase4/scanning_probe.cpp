#include "flights/scanning/Scanning.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>

using batbox::Json;
namespace scan=batbox::flights::scanning;
namespace {
std::string encode(const std::string& bytes){
  constexpr char chars[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string result;result.reserve((bytes.size()+2)/3*4);
  for(std::size_t i=0;i<bytes.size();i+=3){
    const unsigned a=static_cast<unsigned char>(bytes[i]);
    const unsigned b=i+1<bytes.size()?static_cast<unsigned char>(bytes[i+1]):0;
    const unsigned c=i+2<bytes.size()?static_cast<unsigned char>(bytes[i+2]):0;
    result+=chars[a>>2];result+=chars[((a&3)<<4)|(b>>4)];
    result+=i+1<bytes.size()?chars[((b&15)<<2)|(c>>6)]:'=';
    result+=i+2<bytes.size()?chars[c&63]:'=';
  }
  return result;
}
std::string decode(const std::string& base64){
  std::string bytes;bytes.reserve(base64.size()*3/4);
  unsigned value=0;int bits=-8;
  for(const unsigned char c:base64){
    int digit;
    if(c>='A'&&c<='Z')digit=c-'A';
    else if(c>='a'&&c<='z')digit=c-'a'+26;
    else if(c>='0'&&c<='9')digit=c-'0'+52;
    else if(c=='+')digit=62;
    else if(c=='/')digit=63;
    else if(c=='=')break;
    else throw std::runtime_error("invalid base64 fixture");
    value=(value<<6)|digit;bits+=6;
    if(bits>=0){bytes.push_back(static_cast<char>((value>>bits)&255));bits-=8;}
  }
  return bytes;
}
}
int main(){
  std::string line;
  while(std::getline(std::cin,line))try{
    const auto input=batbox::value::parse(line);
    Json result;
    if(input.at("op")=="read"){
      const auto scanResult=scan::readTranscriptForLoad(input.at("path").get<std::string>(),
        input.at("size").get<std::size_t>());
      result={{"boundaryStartOffset",scanResult.boundaryStartOffset},
        {"postBoundaryBase64",encode(scanResult.postBoundaryBuf)},
        {"hasPreservedSegment",scanResult.hasPreservedSegment}};
    }else if(input.at("op")=="metadata"){
      result=scan::scanPreBoundaryMetadata(input.at("path").get<std::string>(),
        input.at("endOffset").get<std::size_t>());
    }else if(input.at("op")=="chain"){
      result={{"base64",encode(scan::walkChainBeforeParse(decode(
        input.at("base64").get<std::string>())) )}};
    }else throw std::runtime_error("unknown operation");
    std::cout<<batbox::value::stringify(Json{{"result",result}})<<'\n';
  }catch(const std::exception& error){
    std::cout<<batbox::value::stringify(Json{{"error",error.what()}})<<'\n';
  }
}
