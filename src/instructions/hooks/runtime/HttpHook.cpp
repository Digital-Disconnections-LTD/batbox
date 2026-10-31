#include "instructions/hooks/runtime/HttpHook.hpp"
#include "instructions/hooks/runtime/HookAbort.hpp"
#include "runtime/JsonValue.hpp"
#include <set>
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
namespace {
bool nameStart(char value){return (value>='A'&&value<='Z')||value=='_';}
bool nameContinue(char value){return nameStart(value)||(value>='0'&&value<='9');}
std::string hookString(const Json& hook,const char* key){
  return hook.at(key).get<std::string>();
}
void require(const HttpHookServices& services){
  if(!services.getPolicy||!services.getEnv||!services.getSandboxProxy||
    !services.getProxyUrl||!services.shouldBypassProxy||!services.post||
    !services.logDebug||!services.errorMessage)
    throw std::invalid_argument("HTTP hook policy, transport and diagnostics imports required");
}
struct HttpRun:std::enable_shared_from_this<HttpRun>{
  event_loop::Loop& loop;
  HttpHookServices services;
  HttpRequest request;
  event_loop::Deferred<HttpOutcome> output;
  std::shared_ptr<HookAbort> abort;
  HttpHookPolicy policy;
  HttpTransportRequest transport;
  HttpRun(event_loop::Loop& owner,HttpHookServices imports,HttpRequest input):
    loop(owner),services(std::move(imports)),request(std::move(input)),output(owner){}
  void debug(const std::string& message,const std::string& level="info"){
    services.logDebug(message,level);
  }
  void failure(std::exception_ptr error){
    if(!abort){output.reject(error);return;}
    abort->cleanup();
    if(abort->signal().stop_requested()){
      HttpOutcome result;result.aborted=true;
      output.resolve(std::move(result));return;
    }
    try{
      const auto message=services.errorMessage(error);
      debug("Hooks: HTTP hook error: "+message,"error");
      HttpOutcome result;result.error=message;
      output.resolve(std::move(result));
    }catch(...){output.reject(std::current_exception());}
  }
  void start(){
    try{
      require(services);
      policy=services.getPolicy();
      const auto url=hookString(request.hook,"url");
      if(policy.allowedUrls){
        bool allowed=false;
        for(const auto& pattern:*policy.allowedUrls)
          if(httpUrlMatchesPattern(url,pattern)){allowed=true;break;}
        if(!allowed){
          const auto message="HTTP hook blocked: "+url+
            " does not match any pattern in allowedHttpHookUrls";
          debug(message,"warn");
          HttpOutcome result;result.error=message;
          output.resolve(std::move(result));return;
        }
      }
      double timeoutMs=600000;
      if(request.hook.contains("timeout")&&
        value::truthy(request.hook.at("timeout")))
        timeoutMs=request.hook.at("timeout").get<double>()*1000;
      abort=HookAbort::create(loop,request.signal,timeoutMs);
      transport.url=url;transport.body=request.jsonInput;
      transport.signal=abort->signal();
      transport.headers["Content-Type"]="application/json";
      if(request.hook.contains("headers")&&
        request.hook.at("headers").is_object()){
        std::vector<std::string> allowed;
        if(request.hook.contains("allowedEnvVars")&&
          request.hook.at("allowedEnvVars").is_array())
          for(const auto& value:request.hook.at("allowedEnvVars"))
            allowed.push_back(value.get<std::string>());
        if(policy.allowedEnvVars){
          std::set<std::string> global(policy.allowedEnvVars->begin(),
            policy.allowedEnvVars->end());
          std::vector<std::string> effective;
          for(const auto& value:allowed)if(global.contains(value))
            effective.push_back(value);
          allowed=std::move(effective);
        }
        for(auto it=request.hook.at("headers").begin();
          it!=request.hook.at("headers").end();++it)
          transport.headers[it.key()]=interpolateHttpEnvVars(
            it.value().get<std::string>(),allowed,services);
      }
      services.getSandboxProxy().then(
        [self=shared_from_this()](const std::optional<HttpProxy>* proxy,
          const std::exception_ptr& error){
          if(error){self->failure(error);return;}
          try{self->afterProxy(*proxy);}catch(...){self->failure(std::current_exception());}
        });
    }catch(...){if(abort)failure(std::current_exception());
      else output.reject(std::current_exception());}
  }
  void afterProxy(const std::optional<HttpProxy>& proxy){
    transport.sandboxProxy=proxy;
    const auto envProxy=proxy?std::optional<std::string>{}:services.getProxyUrl();
    const bool envActive=!proxy&&envProxy&&
      !services.shouldBypassProxy(transport.url);
    if(envActive)transport.envProxy=envProxy;
    transport.useSsrfGuard=!proxy&&!envActive;
    if(proxy)debug("Hooks: HTTP hook POST to "+transport.url+
      " (via sandbox proxy :"+std::to_string(proxy->port)+")");
    else if(envActive)debug("Hooks: HTTP hook POST to "+transport.url+
      " (via env-var proxy)");
    else debug("Hooks: HTTP hook POST to "+transport.url);
    services.post(transport).then(
      [self=shared_from_this()](const HttpTransportResponse* response,
        const std::exception_ptr& error){
        if(error){self->failure(error);return;}
        try{
          self->abort->cleanup();
          self->debug("Hooks: HTTP hook response status "+
            std::to_string(response->status)+", body length "+
            std::to_string(value::stringLength(response->body)));
          HttpOutcome result;
          result.ok=response->status>=200&&response->status<300;
          result.statusCode=response->status;result.body=response->body;
          self->output.resolve(std::move(result));
        }catch(...){self->failure(std::current_exception());}
      });
  }
};
}
bool httpUrlMatchesPattern(const std::string& url,const std::string& pattern){
  // Escape every regexp metacharacter except *, then anchor the pattern.
  std::size_t text=0,mask=0,star=std::string::npos,retry=0;
  while(text<url.size()){
    if(mask<pattern.size()&&pattern[mask]=='*'){
      star=mask++;retry=text;continue;
    }
    if(mask<pattern.size()&&pattern[mask]==url[text]){++mask;++text;continue;}
    if(star==std::string::npos)return false;
    mask=star+1;text=++retry;
  }
  while(mask<pattern.size()&&pattern[mask]=='*')++mask;
  return mask==pattern.size();
}
std::string sanitizeHttpHeaderValue(const std::string& value){
  std::string result;result.reserve(value.size());
  for(char unit:value)if(unit!='\r'&&unit!='\n'&&unit!='\0')result+=unit;
  return result;
}
std::string interpolateHttpEnvVars(const std::string& value,
  const std::vector<std::string>& allowed,const HttpHookServices& services){
  if(!services.getEnv||!services.logDebug)
    throw std::invalid_argument("HTTP hook environment imports required");
  const std::set<std::string> allowlist(allowed.begin(),allowed.end());
  std::string result;
  for(std::size_t i=0;i<value.size();){
    if(value[i]!='$'){result+=value[i++];continue;}
    const auto start=i;std::size_t nameStartPos=i+1,end=nameStartPos;
    bool braced=false;
    if(nameStartPos<value.size()&&value[nameStartPos]=='{'){
      braced=true;++nameStartPos;end=nameStartPos;
    }
    if(end<value.size()&&nameStart(value[end])){
      while(end<value.size()&&nameContinue(value[end]))++end;
      if(!braced||(end<value.size()&&value[end]=='}')){
        const auto name=value.substr(nameStartPos,end-nameStartPos);
        if(allowlist.contains(name))result+=services.getEnv(name).value_or("");
        else services.logDebug("Hooks: env var $"+name+
          " not in allowedEnvVars, skipping interpolation","warn");
        i=braced?end+1:end;continue;
      }
    }
    result+=value[start];i=start+1;
  }
  return sanitizeHttpHeaderValue(result);
}
std::function<event_loop::Deferred<HttpOutcome>(HttpRequest)> execHttpHook(
  event_loop::Loop& loop,HttpHookServices services){
  return [&loop,services=std::move(services)](HttpRequest request){
    if(!loop.isOwnerThread())throw std::logic_error("HTTP hook owner required");
    auto run=std::make_shared<HttpRun>(loop,services,std::move(request));
    run->start();return run->output;
  };
}
} // namespace batbox::instructions::hooks::runtime
