#include "auth/oauth_client/IndigenousAccountHttp.hpp"
#include "locking/Async.hpp"
#include <curl/curl.h>
#include <memory>
#include <stdexcept>

namespace batbox::auth::oauth_client {
namespace {
struct Headers {
  curl_slist* list=nullptr;
  ~Headers(){curl_slist_free_all(list);}
  void add(const std::string& value){
    auto* next=curl_slist_append(list,value.c_str());
    if(!next)throw std::runtime_error("Cannot allocate OAuth HTTP header");
    list=next;
  }
};
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
auth::refresh::HttpResponse request(const std::string& url,
    const Json& headers){
  static const auto initialized=curl_global_init(CURL_GLOBAL_DEFAULT);
  if(initialized!=CURLE_OK)
    throw std::runtime_error("OAuth HTTP initialization failed");
  std::unique_ptr<CURL,decltype(&curl_easy_cleanup)> curl(
    curl_easy_init(),curl_easy_cleanup);
  if(!curl)throw std::runtime_error("Cannot create OAuth HTTP client");
  if(!headers.is_object())
    throw std::invalid_argument("OAuth HTTP headers must be an object");
  Headers list;
  for(auto it=headers.begin();it!=headers.end();++it){
    if(!it.value().is_string())
      throw std::invalid_argument("OAuth HTTP header must be a string");
    list.add(it.key()+": "+it.value().get<std::string>());
  }
  std::string responseBody,statusText;
  curl_easy_setopt(curl.get(),CURLOPT_URL,url.c_str());
  curl_easy_setopt(curl.get(),CURLOPT_HTTPHEADER,list.list);
  curl_easy_setopt(curl.get(),CURLOPT_NOSIGNAL,1L);
  curl_easy_setopt(curl.get(),CURLOPT_FOLLOWLOCATION,1L);
  curl_easy_setopt(curl.get(),CURLOPT_MAXREDIRS,20L);
  curl_easy_setopt(curl.get(),CURLOPT_ACCEPT_ENCODING,"");
  curl_easy_setopt(curl.get(),CURLOPT_WRITEFUNCTION,receive);
  curl_easy_setopt(curl.get(),CURLOPT_WRITEDATA,&responseBody);
  curl_easy_setopt(curl.get(),CURLOPT_HEADERFUNCTION,receiveHeader);
  curl_easy_setopt(curl.get(),CURLOPT_HEADERDATA,&statusText);
  curl_easy_setopt(curl.get(),CURLOPT_POST,1L);
  curl_easy_setopt(curl.get(),CURLOPT_POSTFIELDS,"");
  curl_easy_setopt(curl.get(),CURLOPT_POSTFIELDSIZE,0L);
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
}
event_loop::Deferred<auth::refresh::HttpResponse> indigenousEmptyPost(
    event_loop::Loop& loop,const std::string& url,const Json& headers){
  return locking::runJob<auth::refresh::HttpResponse>(loop,
    [url,headers]{return request(url,headers);});
}
}
