#include "application_hooks/Provider.hpp"
#include "instructions/hooks/runtime/IndigenousHttp.hpp"
#include "runtime/JsonValue.hpp"
#ifdef _WIN32
#include "runtime/IndigenousUnicode.hpp"
#endif
#include <curl/curl.h>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <stdexcept>

namespace batbox::application_hooks {
namespace {
std::optional<std::string> environment(const char* name){
#ifdef _WIN32
  return runtime::environmentUtf8Optional(name);
#else
  const auto* value=std::getenv(name);
  return value?std::optional<std::string>(value):std::nullopt;
#endif
}
std::optional<std::string> firstEnvironment(
    std::initializer_list<const char*> names){
  for(const auto* name:names){
    const auto value=environment(name);
    if(value&&!value->empty())return value;
  }
  return {};
}
std::string lower(std::string value){
  std::transform(value.begin(),value.end(),value.begin(),
    [](unsigned char unit){return static_cast<char>(std::tolower(unit));});
  return value;
}
std::string urlPart(CURLU* url,CURLUPart part){
  char* value=nullptr;
  if(::curl_url_get(url,part,&value,0)!=CURLUE_OK)return {};
  std::string result=value;::curl_free(value);return result;
}
bool shouldBypass(const std::string& requestUrl){
  const auto noProxy=firstEnvironment({"no_proxy","NO_PROXY"});
  if(!noProxy)return false;
  if(*noProxy=="*")return true;
  CURLU* url=::curl_url();
  if(!url)return false;
  struct Cleanup{CURLU* value;~Cleanup(){::curl_url_cleanup(value);}} cleanup{url};
  if(::curl_url_set(url,CURLUPART_URL,requestUrl.c_str(),0)!=CURLUE_OK)
    return false;
  const auto host=lower(urlPart(url,CURLUPART_HOST));
  if(host.empty())return false;
  auto port=urlPart(url,CURLUPART_PORT);
  if(port.empty())port=urlPart(url,CURLUPART_SCHEME)=="https"?"443":"80";
  const auto withPort=host+":"+port;
  const auto& source=*noProxy;
  std::size_t from=0;
  while(from<source.size()){
    while(from<source.size()&&(source[from]==','||
        std::isspace(static_cast<unsigned char>(source[from]))))++from;
    const auto begin=from;
    while(from<source.size()&&source[from]!=','&&
        !std::isspace(static_cast<unsigned char>(source[from])))++from;
    if(begin==from)continue;
    const auto pattern=lower(source.substr(begin,from-begin));
    if(pattern.find(':')!=std::string::npos){
      if(withPort==pattern)return true;
    }else if(pattern.starts_with('.')){
      if(host==pattern.substr(1)||host.ends_with(pattern))return true;
    }else if(host==pattern)return true;
  }
  return false;
}
std::optional<std::vector<std::string>> stringList(
    const Json& settings,const char* name){
  const auto& value=value::property(settings,name);
  if(value.is_null()||value.is_discarded())return {};
  if(!value.is_array())throw std::invalid_argument(
    std::string(name)+" must be an array");
  std::vector<std::string> result;
  for(const auto& row:value){
    if(!row.is_string())throw std::invalid_argument(
      std::string(name)+" entries must be strings");
    result.push_back(row.get<std::string>());
  }
  return result;
}
std::string failureText(std::exception_ptr error){
  try{if(error)std::rethrow_exception(error);}
  catch(const std::exception& value){return value.what();}
  catch(...){return "unknown HTTP hook error";}
  return {};
}
}
void Provider::bindHttpHooks(){
  hooks::HttpHookServices services;
  services.getPolicy=[this]{
    const auto state=inputs_.getAppState?inputs_.getAppState():Json();
    const auto& live=value::property(state,"settings");
    const auto& settings=live.is_object()?live:inputs_.settings;
    return hooks::HttpHookPolicy{
      stringList(settings,"allowedHttpHookUrls"),
      stringList(settings,"httpHookAllowedEnvVars")};
  };
  services.getEnv=[](const std::string& name){return environment(name.c_str());};
  services.getSandboxProxy=[this]{
    event_loop::Deferred<std::optional<hooks::HttpProxy>> output(owner_);
    // The indigenous application profile has no sandbox network proxy.
    output.resolve(std::nullopt);return output;};
  services.getProxyUrl=[]{return firstEnvironment({"https_proxy","HTTPS_PROXY",
    "http_proxy","HTTP_PROXY"});};
  services.shouldBypassProxy=shouldBypass;
  services.post=hooks::indigenousHttpPost(owner_);
  services.logDebug=[this](const std::string& message,const std::string&){
    inputs_.log(message);};
  services.errorMessage=failureText;
  executor_.execHttpHook=hooks::execHttpHook(owner_,std::move(services));
}
} // namespace batbox::application_hooks
