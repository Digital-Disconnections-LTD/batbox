#include "runtime/AsyncLaunch.hpp"
#include "instructions/hooks/runtime/IndigenousHttp.hpp"
#include "instructions/hooks/runtime/SsrfGuard.hpp"
#include "runtime/Encoding.hpp"
#include "runtime/JsonValue.hpp"
#include <curl/curl.h>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace batbox::instructions::hooks::runtime {
namespace {
struct CurlHandle {
  CURL* curl=nullptr;
  curl_slist* headers=nullptr;
  curl_slist* resolved=nullptr;
  CURLU* url=nullptr;
  ~CurlHandle(){
    if(curl)::curl_easy_cleanup(curl);
    if(headers)::curl_slist_free_all(headers);
    if(resolved)::curl_slist_free_all(resolved);
    if(url)::curl_url_cleanup(url);
  }
};
std::string part(CURLU* url,CURLUPart requested,unsigned int flags=0){
  char* value=nullptr;
  const auto code=::curl_url_get(url,requested,&value,flags);
  if(code)throw std::runtime_error(::curl_url_strerror(code));
  std::string result=value;::curl_free(value);return result;
}
std::size_t receive(char* bytes,std::size_t size,std::size_t count,void* userdata){
  auto& body=*static_cast<std::string*>(userdata);
  const auto amount=size*count;
  body.append(bytes,amount);return amount;
}
int onProgress(void* userdata,curl_off_t,curl_off_t,curl_off_t,curl_off_t){
  const auto& signal=*static_cast<const std::stop_token*>(userdata);
  return signal.stop_requested()?1:0;
}
HttpTransportResponse post(HttpTransportRequest request){
  CurlHandle handle;
  handle.curl=::curl_easy_init();handle.url=::curl_url();
  if(!handle.curl||!handle.url)
    throw std::runtime_error("Cannot initialize HTTP hook transport");
  const auto urlBytes=value::encodeUtf8(request.url);
  const auto urlCode=::curl_url_set(handle.url,CURLUPART_URL,urlBytes.c_str(),0);
  if(urlCode)throw std::runtime_error(::curl_url_strerror(urlCode));
  const auto host=part(handle.url,CURLUPART_HOST);
  const auto scheme=part(handle.url,CURLUPART_SCHEME);
  std::string port;
  try{port=part(handle.url,CURLUPART_PORT);}catch(...){
    if(scheme=="http")port="80";
    else if(scheme=="https")port="443";
    else throw std::runtime_error("HTTP hook URL must use http or https");
  }
  if(scheme!="http"&&scheme!="https")
    throw std::runtime_error("HTTP hook URL must use http or https");
  if(request.useSsrfGuard){
    const auto addresses=guardedHookLookup(host);
    std::string rule=host+":"+port+":";
    bool first=true;
    for(const auto& address:addresses){
      if(!first)rule+=',';
      first=false;
      rule+=address.family==6?"["+address.address+"]":address.address;
    }
    handle.resolved=::curl_slist_append(handle.resolved,rule.c_str());
    if(!handle.resolved)throw std::bad_alloc();
    ::curl_easy_setopt(handle.curl,CURLOPT_RESOLVE,handle.resolved);
  }
  for(const auto& [name,value]:request.headers){
    const auto header=value::encodeUtf8(name)+": "+value::encodeUtf8(value);
    auto* next=::curl_slist_append(handle.headers,header.c_str());
    if(!next)throw std::bad_alloc();
    handle.headers=next;
  }
  std::string proxy;
  if(request.sandboxProxy){
    const auto& config=*request.sandboxProxy;
    proxy=config.protocol+"://"+config.host+":"+std::to_string(config.port);
  }else if(request.envProxy)proxy=*request.envProxy;
  const auto body=value::encodeUtf8(request.body);
  std::string response;
  char error[CURL_ERROR_SIZE]={};
  ::curl_easy_setopt(handle.curl,CURLOPT_URL,urlBytes.c_str());
  ::curl_easy_setopt(handle.curl,CURLOPT_POST,1L);
  ::curl_easy_setopt(handle.curl,CURLOPT_POSTFIELDS,body.data());
  ::curl_easy_setopt(handle.curl,CURLOPT_POSTFIELDSIZE_LARGE,
    static_cast<curl_off_t>(body.size()));
  ::curl_easy_setopt(handle.curl,CURLOPT_NOSIGNAL,1L);
  ::curl_easy_setopt(handle.curl,CURLOPT_HTTPHEADER,handle.headers);
  ::curl_easy_setopt(handle.curl,CURLOPT_FOLLOWLOCATION,0L);
  ::curl_easy_setopt(handle.curl,CURLOPT_MAXREDIRS,0L);
  ::curl_easy_setopt(handle.curl,CURLOPT_PROTOCOLS_STR,"http,https");
  ::curl_easy_setopt(handle.curl,CURLOPT_PROXY,proxy.c_str());
  if(!proxy.empty())::curl_easy_setopt(handle.curl,CURLOPT_NOPROXY,"");
  ::curl_easy_setopt(handle.curl,CURLOPT_WRITEFUNCTION,receive);
  ::curl_easy_setopt(handle.curl,CURLOPT_WRITEDATA,&response);
  ::curl_easy_setopt(handle.curl,CURLOPT_ERRORBUFFER,error);
  ::curl_easy_setopt(handle.curl,CURLOPT_NOPROGRESS,0L);
  ::curl_easy_setopt(handle.curl,CURLOPT_XFERINFOFUNCTION,onProgress);
  ::curl_easy_setopt(handle.curl,CURLOPT_XFERINFODATA,&request.signal);
  const auto code=::curl_easy_perform(handle.curl);
  if(code!=CURLE_OK)
    throw std::runtime_error(error[0]?error: ::curl_easy_strerror(code));
  long status=0;::curl_easy_getinfo(handle.curl,CURLINFO_RESPONSE_CODE,&status);
  return {static_cast<int>(status),value::decodeUtf8(response)};
}
}
std::function<event_loop::Deferred<HttpTransportResponse>(HttpTransportRequest)>
indigenousHttpPost(event_loop::Loop& loop){
  static std::once_flag initialized;
  std::call_once(initialized,[]{
    if(::curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK)
      throw std::runtime_error("Cannot initialize libcurl");
  });
  return [&loop](HttpTransportRequest request){
    if(!loop.isOwnerThread())throw std::logic_error("HTTP hook owner required");
    event_loop::Deferred<HttpTransportResponse> result(loop);
    auto keep=std::make_shared<event_loop::Loop::KeepAlive>(loop.hold());
    const auto dispatcher=loop.dispatcher();
    try{
      ::batbox::runtime::contextThread([result,request=std::move(request),keep,dispatcher]()mutable{
        std::optional<HttpTransportResponse> response;std::exception_ptr error;
        try{response=post(std::move(request));}catch(...){error=std::current_exception();}
        try{dispatcher.postMission([result,response=std::move(response),error,keep]{
          if(error)result.reject(error);else result.resolve(*response);
          keep->release();
        });}catch(...){keep->release();}
      }).detach();
    }catch(...){keep->release();result.reject(std::current_exception());}
    return result;
  };
}
} // namespace batbox::instructions::hooks::runtime
