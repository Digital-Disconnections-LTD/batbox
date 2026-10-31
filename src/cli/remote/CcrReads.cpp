#include "cli/remote/CcrRuntime.hpp"
#include "runtime/Encoding.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <cmath>

namespace batbox::cli::remote {
namespace {
std::string formEncode(const std::string& value){
  const auto utf8=value::encodeUtf8(value);
  const char* hex="0123456789ABCDEF";
  std::string result;
  for(unsigned char ch:utf8){
    if((ch>='a'&&ch<='z')||(ch>='A'&&ch<='Z')||
       (ch>='0'&&ch<='9')||ch=='*'||ch=='-'||ch=='.'||ch=='_')
      result+=static_cast<char>(ch);
    else if(ch==' ')result+='+';
    else{result+='%';result+=hex[ch>>4];result+=hex[ch&15];}
  }
  return result;
}
}
std::string CcrRuntime::urlWithParams(const std::string& path,
    const Json& params,const std::optional<std::string>& cursor)const{
  std::string url=flightBaseUrl_+path;
  bool first=true;
  if(params.is_object())for(auto it=params.begin();it!=params.end();++it){
    url+=first?'?':'&';first=false;
    url+=formEncode(it.key())+'='+formEncode(it.value().get<std::string>());
  }
  if(cursor&& !cursor->empty()){
    url+=first?'?':'&';url+="cursor="+formEncode(*cursor);
  }
  return url;
}
Deferred<Json> CcrRuntime::getWithRetry(const std::string& url,
    const Json& authHeaders,const std::string& context,int attempt){
  Deferred<Json> answer(loop_);
  Json headers=authHeaders;
  headers[services_.protocolVersionHeaderName]=
    services_.protocolVersionHeaderValue;
  headers["User-Agent"]=services_.userBat();
  const auto retry=[weak=weak_from_this(),answer,url,authHeaders,context,
      attempt]{
    if(const auto self=weak.lock()){
      if(attempt>=10){answer.resolve(nullptr);return;}
      const auto delay=std::min(500.0*std::pow(2.0,attempt-1),30000.0)+
        self->services_.randomUnit()*500;
      self->loop_.setTimeout(delay,[weak,answer,url,authHeaders,context,
          attempt]{
        if(const auto current=weak.lock()){
          auto next=current->getWithRetry(url,authHeaders,context,attempt+1);
          next.then([answer](const Json* value,const auto& error){
            if(error)answer.reject(error);
            else answer.resolve(value?*value:Json(nullptr));
          });
        }else answer.resolve(nullptr);
      });
    }else answer.resolve(nullptr);
  };
  try{
    auto requestResult=services_.http("get",url,nullptr,headers,30000);
    const std::weak_ptr<CcrRuntime> weak=weak_from_this();
    requestResult.then([weak,answer,retry](const CcrHttpResponse* response,
      const std::exception_ptr& error){
      if(error||!response){retry();return;}
      if(response->status>=200&&response->status<300){
        answer.resolve(response->data);return;
      }
      if(response->status==409){
        if(const auto self=weak.lock())try{
          self->services_.onEpochMismatch();
        }catch(...){answer.reject(std::current_exception());return;}
      }
      retry();
    });
  }catch(...){retry();}
  return answer;
}
Deferred<Json> CcrRuntime::getWorkerState(){
  Deferred<Json> answer(loop_);
  Json auth;
  try{auth=services_.getAuthHeaders();}
  catch(...){answer.reject(std::current_exception());return answer;}
  if(!auth.is_object()||auth.empty()){
    answer.resolve(nullptr);return answer;
  }
  auto requestResult=getWithRetry(flightBaseUrl_+"/worker",auth,
    "worker_state");
  requestResult.then([answer](const Json* value,
    const std::exception_ptr& error){
    if(error){answer.reject(error);return;}
    const auto metadata=value?
      value::property(value::property(*value,"worker"),"external_metadata"):
      Json(nullptr);
    answer.resolve(metadata.is_null()?Json(nullptr):metadata);
  });
  return answer;
}
Deferred<Json> CcrRuntime::paginatedGet(const std::string& path,
    const Json& params,const std::string& context){
  Deferred<Json> answer(loop_);
  Json auth;
  try{auth=services_.getAuthHeaders();}
  catch(...){answer.reject(std::current_exception());return answer;}
  if(!auth.is_object()||auth.empty()){
    answer.resolve(nullptr);return answer;
  }
  auto all=std::make_shared<Json>(Json::array());
  auto page=std::make_shared<std::function<void(std::optional<std::string>)>>();
  const std::weak_ptr<CcrRuntime> weak=weak_from_this();
  *page=[weak,answer,all,page,path,params,context,auth](
      std::optional<std::string> cursor){
    const auto self=weak.lock();if(!self){answer.resolve(nullptr);return;}
    auto requestResult=self->getWithRetry(
      self->urlWithParams(path,params,cursor),auth,context);
    requestResult.then([answer,all,page](const Json* value,
        const std::exception_ptr& error){
      if(error){answer.reject(error);*page={};return;}
      if(!value||!value::truthy(*value)){
        answer.resolve(nullptr);*page={};return;
      }
      const auto& data=value::property(*value,"data");
      if(data.is_array())for(const auto& item:data)all->push_back(item);
      const auto& cursor=value::property(*value,"next_cursor");
      if(cursor.is_string()&& !cursor.get<std::string>().empty())
        (*page)(cursor.get<std::string>());
      else{answer.resolve(*all);*page={};}
    });
  };
  (*page)(std::nullopt);
  return answer;
}
Deferred<Json> CcrRuntime::readInternalEvents(){
  return paginatedGet("/worker/internal-events",Json::object(),
    "internal_events");
}
Deferred<Json> CcrRuntime::readSubagentInternalEvents(){
  return paginatedGet("/worker/internal-events",
    Json{{"subagents","true"}},"subagent_events");
}
}
