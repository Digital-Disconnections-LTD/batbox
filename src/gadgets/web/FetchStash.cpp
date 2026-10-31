#include "gadgets/web/FetchStash.hpp"
#include <algorithm>

namespace batbox::gadgets::web {
namespace {
constexpr auto urlTTL=std::chrono::minutes(15);
constexpr auto domainTTL=std::chrono::minutes(5);
constexpr std::size_t maxBytes=50*1024*1024;
constexpr std::size_t maxDomains=128;
}
std::optional<FetchContent> stashedFetch(const FetchServices& services,const std::string& url){
  if(!services.stash)return std::nullopt;
  auto& stash=*services.stash;std::lock_guard lock(stash.mutex);
  const auto now=std::chrono::steady_clock::now();
  for(auto it=stash.urls.begin();it!=stash.urls.end();){
    if(it->expiry<=now){stash.totalBytes-=it->size;it=stash.urls.erase(it);}else ++it;
  }
  for(auto it=stash.urls.begin();it!=stash.urls.end();++it)if(it->url==url){
    auto result=it->value;stash.urls.splice(stash.urls.end(),stash.urls,it);return result;
  }
  return std::nullopt;
}
void stashFetch(const FetchServices& services,const std::string& url,const FetchContent& value){
  if(!services.stash)return;
  auto& stash=*services.stash;std::lock_guard lock(stash.mutex);
  const auto size=std::max<std::size_t>(1,value.content.size());
  if(size>maxBytes)return;
  for(auto it=stash.urls.begin();it!=stash.urls.end();){
    if(it->url==url){stash.totalBytes-=it->size;it=stash.urls.erase(it);}else ++it;
  }
  while(!stash.urls.empty()&&stash.totalBytes+size>maxBytes){
    stash.totalBytes-=stash.urls.front().size;stash.urls.pop_front();
  }
  stash.urls.push_back({url,value,size,std::chrono::steady_clock::now()+urlTTL});
  stash.totalBytes+=size;
}
bool stashedAllowedDomain(const FetchServices& services,const std::string& hostname){
  if(!services.stash)return false;
  auto& stash=*services.stash;std::lock_guard lock(stash.mutex);
  const auto now=std::chrono::steady_clock::now();
  for(auto it=stash.allowedDomains.begin();it!=stash.allowedDomains.end();){
    if(it->expiry<=now)it=stash.allowedDomains.erase(it);else ++it;
  }
  for(auto it=stash.allowedDomains.begin();it!=stash.allowedDomains.end();++it)
    if(it->hostname==hostname){stash.allowedDomains.splice(stash.allowedDomains.end(),stash.allowedDomains,it);return true;}
  return false;
}
void stashAllowedDomain(const FetchServices& services,const std::string& hostname){
  if(!services.stash)return;
  auto& stash=*services.stash;std::lock_guard lock(stash.mutex);
  for(auto it=stash.allowedDomains.begin();it!=stash.allowedDomains.end();){
    if(it->hostname==hostname)it=stash.allowedDomains.erase(it);else ++it;
  }
  while(stash.allowedDomains.size()>=maxDomains)stash.allowedDomains.pop_front();
  stash.allowedDomains.push_back({hostname,std::chrono::steady_clock::now()+domainTTL});
}
void clearWebFetchStash(FetchServices& services){
  if(!services.stash)return;
  auto& stash=*services.stash;std::lock_guard lock(stash.mutex);
  stash.urls.clear();stash.allowedDomains.clear();stash.totalBytes=0;
}
} // namespace batbox::gadgets::web
