#include "gadgets/web/Web.hpp"
#include <curl/curl.h>
#include <algorithm>
#include <memory>

namespace batbox::gadgets::web {
namespace {
struct URLParts {std::string scheme,host,port,path,username,password;};
std::optional<URLParts> parseURL(const std::string& url){
  std::unique_ptr<CURLU,decltype(&curl_url_cleanup)> handle(curl_url(),curl_url_cleanup);
  if(!handle||curl_url_set(handle.get(),CURLUPART_URL,url.c_str(),CURLU_NON_SUPPORT_SCHEME)!=CURLUE_OK)
    return std::nullopt;
  auto field=[&](CURLUPart part){
    char* raw=nullptr;const auto result=curl_url_get(handle.get(),part,&raw,0);
    if(result!=CURLUE_OK)return std::string();
    std::string value(raw);curl_free(raw);return value;
  };
  auto scheme=field(CURLUPART_SCHEME),host=field(CURLUPART_HOST);
  auto lower=[](std::string& text){std::transform(text.begin(),text.end(),text.begin(),
    [](unsigned char c){return c>='A'&&c<='Z'?static_cast<char>(c+('a'-'A')):static_cast<char>(c);});};
  lower(scheme);lower(host);
  return URLParts{scheme,host,field(CURLUPART_PORT),
    field(CURLUPART_PATH),field(CURLUPART_USER),field(CURLUPART_PASSWORD)};
}
std::string stripWww(const std::string& host){return host.rfind("www.",0)==0?host.substr(4):host;}
} // namespace

bool validateURL(const std::string& url){
  if(value::stringLength(url)>2000)return false;
  const auto parsed=parseURL(url);
  if(!parsed||!parsed->username.empty()||!parsed->password.empty())return false;
  const auto& hostname=parsed->host;
  return hostname.find('.')!=std::string::npos;
}

std::optional<std::string> hostnameFromURL(const std::string& url){
  const auto parsed=parseURL(url);
  if(!parsed||parsed->host.empty())return std::nullopt;
  return parsed->host;
}

bool isPermittedRedirect(const std::string& originalUrl,const std::string& redirectUrl){
  const auto original=parseURL(originalUrl),redirect=parseURL(redirectUrl);
  if(!original||!redirect)return false;
  if(original->scheme!=redirect->scheme||original->port!=redirect->port)return false;
  if(!redirect->username.empty()||!redirect->password.empty())return false;
  return stripWww(original->host)==stripWww(redirect->host);
}

bool isPreapprovedHost(const std::string& hostname,const std::string& pathname,
  const std::vector<std::string>& entries){
  for(const auto& entry:entries){
    const auto slash=entry.find('/');
    if(slash==std::string::npos){if(hostname==entry)return true;continue;}
    if(hostname!=entry.substr(0,slash))continue;
    const auto prefix=entry.substr(slash);
    if(pathname==prefix||pathname.rfind(prefix+"/",0)==0)return true;
  }
  return false;
}

bool isPreapprovedUrl(const std::string& url,const std::vector<std::string>& entries){
  const auto parsed=parseURL(url);
  return parsed&&isPreapprovedHost(parsed->host,parsed->path,entries);
}
} // namespace batbox::gadgets::web
