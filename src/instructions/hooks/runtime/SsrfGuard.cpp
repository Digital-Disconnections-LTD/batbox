#include "instructions/hooks/runtime/SsrfGuard.hpp"
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#endif
#include <cstring>
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
namespace {
#if defined(_WIN32)
void ensureWinsock(){
  struct Winsock {
    Winsock(){
      WSADATA data{};
      const int code=::WSAStartup(MAKEWORD(2,2),&data);
      if(code)throw std::runtime_error("DNS initialization failed: "+
        std::to_string(code));
    }
    ~Winsock(){::WSACleanup();}
  };
  static const Winsock instance;
}
#endif
bool blockedV4(const unsigned char* bytes){
  const auto a=bytes[0],b=bytes[1];
  if(a==127)return false;
  return a==0||a==10||(a==169&&b==254)||
    (a==172&&b>=16&&b<=31)||(a==100&&b>=64&&b<=127)||
    (a==192&&b==168);
}
bool blockedV6(const std::string& address,const unsigned char* bytes){
  std::string lower=address;
  for(auto& c:lower)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');
  if(lower=="::1")return false;
  if(lower=="::")return true;
  bool mapped=true;
  for(int i=0;i<10;++i)if(bytes[i])mapped=false;
  if(mapped&&bytes[10]==0xff&&bytes[11]==0xff)
    return blockedV4(bytes+12);
  if(lower.starts_with("fc")||lower.starts_with("fd"))return true;
  const auto colon=lower.find(':');
  const auto first=lower.substr(0,colon);
  return first.size()==4&&first>="fe80"&&first<="febf";
}
std::runtime_error blocked(const std::string& hostname,const std::string& address){
  return std::runtime_error("HTTP hook blocked: "+hostname+" resolves to "+address+
    " (private/link-local address). Loopback (127.0.0.1, ::1) is allowed for local dev.");
}
}
bool isBlockedHookAddress(const std::string& address){
  unsigned char v4[4],v6[16];
  if(::inet_pton(AF_INET,address.c_str(),v4)==1)return blockedV4(v4);
  if(::inet_pton(AF_INET6,address.c_str(),v6)==1)return blockedV6(address,v6);
  return false;
}
std::vector<GuardedAddress> guardedHookLookup(const std::string& hostname){
#if defined(_WIN32)
  ensureWinsock();
#endif
  unsigned char bytes[16];
  if(::inet_pton(AF_INET,hostname.c_str(),bytes)==1||
    ::inet_pton(AF_INET6,hostname.c_str(),bytes)==1){
    if(isBlockedHookAddress(hostname))throw blocked(hostname,hostname);
    return {{hostname,::inet_pton(AF_INET,hostname.c_str(),bytes)==1?4:6}};
  }
  addrinfo hints{};hints.ai_family=AF_UNSPEC;hints.ai_socktype=SOCK_STREAM;
  addrinfo* result=nullptr;
  const int code=::getaddrinfo(hostname.c_str(),nullptr,&hints,&result);
  if(code)throw std::runtime_error(std::string("DNS lookup failed: ")+
#if defined(_WIN32)
    ::gai_strerrorA(code));
#else
    ::gai_strerror(code));
#endif
  std::vector<GuardedAddress> addresses;
  try{
    for(auto* current=result;current;current=current->ai_next){
      char text[INET6_ADDRSTRLEN]={};int family=0;
      if(current->ai_family==AF_INET){
        const auto* v4=reinterpret_cast<const sockaddr_in*>(current->ai_addr);
        ::inet_ntop(AF_INET,&v4->sin_addr,text,sizeof(text));family=4;
      }else if(current->ai_family==AF_INET6){
        const auto* v6=reinterpret_cast<const sockaddr_in6*>(current->ai_addr);
        ::inet_ntop(AF_INET6,&v6->sin6_addr,text,sizeof(text));family=6;
      }else continue;
      if(isBlockedHookAddress(text))throw blocked(hostname,text);
      addresses.push_back({text,family});
    }
  }catch(...){::freeaddrinfo(result);throw;}
  ::freeaddrinfo(result);
  if(addresses.empty())throw std::runtime_error("ENOTFOUND "+hostname);
  return addresses;
}
} // namespace batbox::instructions::hooks::runtime
