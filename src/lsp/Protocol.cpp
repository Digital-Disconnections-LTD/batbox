#include "lsp/Protocol.hpp"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <stdexcept>

namespace batbox::lsp {
void FrameDecoder::feed(std::string_view bytes){
  buffer_.append(bytes);
  if(buffer_.size()>32*1024*1024)throw std::runtime_error("LSP frame exceeds 32 MB");
  for(;;){
    if(!awaitingBody_){
      auto end=buffer_.find("\r\n\r\n");
      if(end==std::string::npos){if(buffer_.size()>8192)throw std::runtime_error("LSP header exceeds 8 KB");return;}
      auto header=buffer_.substr(0,end);
      buffer_.erase(0,end+4);
      contentLength_=0;
      bool found=false;
      std::size_t at=0;
      while(at<header.size()){
        auto next=header.find("\r\n",at);
        if(next==std::string::npos)next=header.size();
        auto line=header.substr(at,next-at);
        auto colon=line.find(':');
        if(colon!=std::string::npos){
          auto key=line.substr(0,colon);
          std::transform(key.begin(),key.end(),key.begin(),[](unsigned char c){return std::tolower(c);});
          if(key=="content-length"){
            auto value=line.substr(colon+1);
            auto first=value.find_first_not_of(" \t");
            if(first==std::string::npos)throw std::runtime_error("Invalid LSP Content-Length");
            value.erase(0,first);
            auto [ptr,ec]=std::from_chars(value.data(),value.data()+value.size(),contentLength_);
            if(ec!=std::errc{}||ptr!=value.data()+value.size()||contentLength_>32*1024*1024)
              throw std::runtime_error("Invalid LSP Content-Length");
            found=true;
          }
        }
        at=next+2;
      }
      if(!found)throw std::runtime_error("Missing LSP Content-Length");
      awaitingBody_=true;
    }
    if(buffer_.size()<contentLength_)return;
    auto body=buffer_.substr(0,contentLength_);
    buffer_.erase(0,contentLength_);
    awaitingBody_=false;
    receive_(Json::parse(body));
  }
}
std::string frame(const Json& message){
  auto body=message.dump();
  return "Content-Length: "+std::to_string(body.size())+"\r\n\r\n"+body;
}
namespace {
bool plain(unsigned char c){return std::isalnum(c)||c=='/'||c=='-'||c=='_'||c=='.'||c=='~'||c==':' ;}
char hex(unsigned n){return "0123456789ABCDEF"[n&15];}
int unhex(char c){if(c>='0'&&c<='9')return c-'0';if(c>='A'&&c<='F')return c-'A'+10;if(c>='a'&&c<='f')return c-'a'+10;return -1;}
}
std::string fileUri(const std::filesystem::path& path){
  auto raw=std::filesystem::absolute(path).lexically_normal().generic_string();
#ifdef _WIN32
  if(raw.empty()||raw.front()!='/')raw.insert(raw.begin(),'/');
#endif
  std::string uri="file://";
  for(unsigned char c:raw){if(plain(c))uri+=static_cast<char>(c);else{uri+='%';uri+=hex(c>>4);uri+=hex(c);}}
  return uri;
}
std::filesystem::path pathFromFileUri(std::string_view uri){
  if(uri.starts_with("file://"))uri.remove_prefix(7);
  std::string raw;raw.reserve(uri.size());
  for(std::size_t i=0;i<uri.size();++i){
    if(uri[i]=='%'&&i+2<uri.size()){
      auto high=unhex(uri[i+1]),low=unhex(uri[i+2]);
      if(high>=0&&low>=0){raw+=static_cast<char>((high<<4)|low);i+=2;continue;}
    }
    raw+=uri[i];
  }
#ifdef _WIN32
  if(raw.size()>2&&raw[0]=='/'&&std::isalpha(static_cast<unsigned char>(raw[1]))&&raw[2]==':')raw.erase(0,1);
#endif
  return std::filesystem::path(raw);
}
std::string displayUri(std::string_view uri,const std::filesystem::path& cwd){
  if(uri.empty())return "<unknown location>";
  auto path=pathFromFileUri(uri);
  auto full=path.generic_string();
  if(!cwd.empty()){
    auto relative=path.lexically_relative(cwd).generic_string();
    if(!relative.empty()&&relative.size()<full.size()&&!relative.starts_with("../../"))return relative;
  }
  return full;
}
}
