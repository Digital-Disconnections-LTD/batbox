#include "runtime/AsyncLaunch.hpp"
#include "settings/remote/Http.hpp"
#include <curl/curl.h>
#include <memory>
#include <thread>

namespace batbox::settings::remote {
namespace {
std::size_t receive(char* data,std::size_t size,std::size_t count,
    void* context){
  const auto bytes=size*count;
  try{static_cast<std::string*>(context)->append(data,bytes);return bytes;}
  catch(...){return 0;}
}
struct Headers {
  curl_slist* list=nullptr;
  ~Headers(){curl_slist_free_all(list);}
  void add(const std::string& line){
    auto* next=curl_slist_append(list,line.c_str());
    if(!next)throw std::runtime_error("Cannot allocate remote HTTP header");
    list=next;
  }
};
std::string classifyCurl(CURLcode result){
  if(result==CURLE_OPERATION_TIMEDOUT)return "timeout";
  if(result==CURLE_COULDNT_CONNECT||result==CURLE_COULDNT_RESOLVE_HOST)
    return "network";
  return "http";
}
HttpResponse request(const std::string& url,const Json& headers,
    double timeoutMs){
  static const auto initialized=curl_global_init(CURL_GLOBAL_DEFAULT);
  if(initialized!=CURLE_OK)
    throw HttpError("HTTP initialization failed","other");
  std::unique_ptr<CURL,decltype(&curl_easy_cleanup)> http(
    curl_easy_init(),curl_easy_cleanup);
  if(!http)throw HttpError("Cannot create remote HTTP client","other");
  Headers list;
  for(auto it=headers.begin();it!=headers.end();++it){
    if(!it.value().is_string())
      throw HttpError("Invalid remote HTTP header","other");
    list.add(it.key()+": "+it.value().get<std::string>());
  }
  std::string body;
  curl_easy_setopt(http.get(),CURLOPT_URL,url.c_str());
  curl_easy_setopt(http.get(),CURLOPT_HTTPGET,1L);
  curl_easy_setopt(http.get(),CURLOPT_HTTPHEADER,list.list);
  curl_easy_setopt(http.get(),CURLOPT_NOSIGNAL,1L);
  curl_easy_setopt(http.get(),CURLOPT_FOLLOWLOCATION,1L);
  curl_easy_setopt(http.get(),CURLOPT_MAXREDIRS,20L);
  curl_easy_setopt(http.get(),CURLOPT_CONNECTTIMEOUT_MS,10000L);
  curl_easy_setopt(http.get(),CURLOPT_TIMEOUT_MS,
    static_cast<long>(timeoutMs));
  curl_easy_setopt(http.get(),CURLOPT_WRITEFUNCTION,receive);
  curl_easy_setopt(http.get(),CURLOPT_WRITEDATA,&body);
  const auto code=curl_easy_perform(http.get());
  if(code!=CURLE_OK)
    throw HttpError(curl_easy_strerror(code),classifyCurl(code));
  long status=0;
  curl_easy_getinfo(http.get(),CURLINFO_RESPONSE_CODE,&status);
  if(status!=200&&status!=204&&status!=304&&status!=404){
    const auto kind=status==401||status==403?"auth":"http";
    throw HttpError("HTTP "+std::to_string(status),kind,
      static_cast<int>(status));
  }
  Json data=nullptr;
  if(!body.empty()){
    try{data=value::parse(body);}catch(...){data=body;}
  }
  return {static_cast<int>(status),data};
}
}
event_loop::Deferred<HttpResponse> indigenousHttpGet(event_loop::Loop& loop,
    const std::string& url,const Json& headers,double timeoutMs){
  event_loop::Deferred<HttpResponse> output(loop);
  auto hold=std::make_shared<event_loop::Loop::KeepAlive>(loop.hold());
  output.then([hold](const HttpResponse*,const std::exception_ptr&){
    hold->release();
  });
  try{
    ::batbox::runtime::contextThread([output,url,headers,timeoutMs,hold](){
      try{output.resolve(request(url,headers,timeoutMs));}
      catch(...){output.reject(std::current_exception());}
    }).detach();
  }catch(...){hold->release();output.reject(std::current_exception());}
  return output;
}
}
