#include "runtime/AsyncLaunch.hpp"
#include "auth/refresh/IndigenousHttp.hpp"
#include <curl/curl.h>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <thread>

namespace batbox::auth::refresh {
namespace {
std::size_t receive(char* data,std::size_t size,std::size_t count,
    void* context){
  const auto bytes=size*count;
  try{static_cast<std::string*>(context)->append(data,bytes);return bytes;}
  catch(...){return 0;}
}
std::size_t receiveHeader(char* data,std::size_t size,std::size_t count,
    void* context){
  const auto bytes=size*count;
  try{
    std::string_view line(data,bytes);
    if(line.substr(0,5)=="HTTP/"){
      const auto space=line.find(' ');
      const auto reason=space==std::string_view::npos
        ?std::string_view{}:line.substr(space+1);
      const auto second=reason.find(' ');
      auto label=second==std::string_view::npos
        ?std::string_view{}:reason.substr(second+1);
      while(!label.empty()&&(label.back()=='\r'||label.back()=='\n'))
        label.remove_suffix(1);
      *static_cast<std::string*>(context)=std::string(label);
    }
    return bytes;
  }catch(...){return 0;}
}
struct Headers {
  curl_slist* list=nullptr;
  ~Headers(){curl_slist_free_all(list);}
  void add(const std::string& value){
    auto* next=curl_slist_append(list,value.c_str());
    if(!next)throw std::runtime_error("Cannot allocate OAuth HTTP header");
    list=next;
  }
};
HttpResponse request(const std::string& url,const Json* body,
    const Json& headers,double timeoutMs){
  static const auto initialized=curl_global_init(CURL_GLOBAL_DEFAULT);
  if(initialized!=CURLE_OK)
    throw std::runtime_error("OAuth HTTP initialization failed");
  std::unique_ptr<CURL,decltype(&curl_easy_cleanup)> curl(
    curl_easy_init(),curl_easy_cleanup);
  if(!curl)throw std::runtime_error("Cannot create OAuth HTTP client");
  Headers list;
  if(!headers.is_object())
    throw std::invalid_argument("OAuth HTTP headers must be an object");
  for(auto it=headers.begin();it!=headers.end();++it){
    if(!it.value().is_string())
      throw std::invalid_argument("OAuth HTTP header must be a string");
    list.add(it.key()+": "+it.value().get<std::string>());
  }
  std::string responseBody,statusText;
  const auto requestBody=body?value::stringify(*body):std::string{};
  curl_easy_setopt(curl.get(),CURLOPT_URL,url.c_str());
  curl_easy_setopt(curl.get(),CURLOPT_HTTPHEADER,list.list);
  curl_easy_setopt(curl.get(),CURLOPT_NOSIGNAL,1L);
  curl_easy_setopt(curl.get(),CURLOPT_FOLLOWLOCATION,1L);
  curl_easy_setopt(curl.get(),CURLOPT_MAXREDIRS,20L);
  curl_easy_setopt(curl.get(),CURLOPT_ACCEPT_ENCODING,"");
  curl_easy_setopt(curl.get(),CURLOPT_TIMEOUT_MS,
    static_cast<long>(timeoutMs));
  curl_easy_setopt(curl.get(),CURLOPT_WRITEFUNCTION,receive);
  curl_easy_setopt(curl.get(),CURLOPT_WRITEDATA,&responseBody);
  curl_easy_setopt(curl.get(),CURLOPT_HEADERFUNCTION,receiveHeader);
  curl_easy_setopt(curl.get(),CURLOPT_HEADERDATA,&statusText);
  if(body){
    curl_easy_setopt(curl.get(),CURLOPT_POST,1L);
    curl_easy_setopt(curl.get(),CURLOPT_POSTFIELDS,requestBody.c_str());
    curl_easy_setopt(curl.get(),CURLOPT_POSTFIELDSIZE,
      static_cast<long>(requestBody.size()));
  }else curl_easy_setopt(curl.get(),CURLOPT_HTTPGET,1L);
  const auto code=curl_easy_perform(curl.get());
  if(code!=CURLE_OK)throw std::runtime_error(curl_easy_strerror(code));
  long status=0;
  curl_easy_getinfo(curl.get(),CURLINFO_RESPONSE_CODE,&status);
  if(status<200||status>=300)
    throw std::runtime_error("Request failed with status code "+
      std::to_string(status));
  const auto parsed=value::parse(responseBody,false);
  return {static_cast<int>(status),statusText,
    parsed.is_discarded()?Json(responseBody):parsed};
}
event_loop::Deferred<HttpResponse> requestAsync(event_loop::Loop& loop,
    std::string url,std::optional<Json> body,Json headers,double timeoutMs){
  if(!std::isfinite(timeoutMs)||timeoutMs<0)
    throw std::invalid_argument("Invalid OAuth HTTP timeout");
  event_loop::Deferred<HttpResponse> output(loop);
  auto hold=std::make_shared<event_loop::Loop::KeepAlive>(loop.hold());
  output.then([hold](const HttpResponse*,const std::exception_ptr&){
    hold->release();
  });
  try{
    ::batbox::runtime::contextThread([output,url=std::move(url),body=std::move(body),
        headers=std::move(headers),timeoutMs,hold]{
      try{output.resolve(request(url,body?&*body:nullptr,headers,timeoutMs));}
      catch(...){output.reject(std::current_exception());}
    }).detach();
  }catch(...){hold->release();output.reject(std::current_exception());}
  return output;
}
}
event_loop::Deferred<HttpResponse> indigenousHttpPost(event_loop::Loop& loop,
    const std::string& url,const Json& body,const Json& headers,
    double timeoutMs){
  return requestAsync(loop,url,body,headers,timeoutMs);
}
event_loop::Deferred<HttpResponse> indigenousHttpGet(event_loop::Loop& loop,
    const std::string& url,const Json& headers,double timeoutMs){
  return requestAsync(loop,url,std::nullopt,headers,timeoutMs);
}
}
