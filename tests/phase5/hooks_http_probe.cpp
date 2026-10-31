#include "instructions/hooks/runtime/HttpHook.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace hook=batbox::instructions::hooks::runtime;
namespace {
Json run(const Json& item){
  event_loop::Loop loop;Json calls=Json::array();
  hook::HttpHookServices services;
  services.getPolicy=[&]{
    hook::HttpHookPolicy policy;
    if(item.contains("allowedUrls"))policy.allowedUrls=
      item.at("allowedUrls").get<std::vector<std::string>>();
    if(item.contains("policyAllowedEnvVars"))policy.allowedEnvVars=
      item.at("policyAllowedEnvVars").get<std::vector<std::string>>();
    return policy;
  };
  services.getEnv=[](const std::string& name)->std::optional<std::string>{
    if(name=="SECRET")return "token";
    if(name=="OTHER")return "other";
    return std::nullopt;
  };
  services.getSandboxProxy=[&]{
    calls.push_back("sandbox");
    event_loop::Deferred<std::optional<hook::HttpProxy>> result(loop);
    if(item.contains("sandboxProxy")){
      const auto& proxy=item.at("sandboxProxy");
      result.resolve(hook::HttpProxy{proxy.at("host").get<std::string>(),
        proxy.at("protocol").get<std::string>(),proxy.at("port").get<int>()});
    }else result.resolve(std::nullopt);
    return result;
  };
  services.getProxyUrl=[&]()->std::optional<std::string>{
    calls.push_back("proxy-url");
    if(item.contains("proxyUrl"))return item.at("proxyUrl").get<std::string>();
    return std::nullopt;
  };
  services.shouldBypassProxy=[&](const std::string&){
    calls.push_back("bypass?");return item.value("bypassProxy",false);};
  services.post=[&](hook::HttpTransportRequest request){
    Json headers=Json::object();
    for(const auto& [key,value]:request.headers)headers[key]=value;
    Json proxy=false;
    if(request.sandboxProxy)proxy={{"host",request.sandboxProxy->host},
      {"port",request.sandboxProxy->port},
      {"protocol",request.sandboxProxy->protocol}};
    calls.push_back({{"post",{{"url",request.url},{"body",request.body},
      {"headers",headers},{"proxy",proxy},{"ssrfGuard",request.useSsrfGuard}}}});
    event_loop::Deferred<hook::HttpTransportResponse> result(loop);
    if(item.contains("postError"))result.reject(std::make_exception_ptr(
      std::runtime_error(item.at("postError").get<std::string>())));
    else result.resolve({item.value("status",200),
      item.value("responseBody",std::string{})});
    return result;
  };
  services.logDebug=[&](const std::string& message,const std::string& level){
    calls.push_back(level+":"+message);};
  services.errorMessage=[](std::exception_ptr error){
    try{std::rethrow_exception(error);}catch(const std::exception& value){
      return std::string(value.what());}
  };
  Json hookValue={{"url",item.at("url")}};
  if(item.contains("headers"))hookValue["headers"]=item.at("headers");
  if(item.contains("hookAllowedEnvVars"))
    hookValue["allowedEnvVars"]=item.at("hookAllowedEnvVars");
  std::stop_source cancelled;
  if(item.value("aborted",false))cancelled.request_stop();
  hook::HttpRequest request{hookValue,"InstructionsLoaded","{\"event\":\"test\"",
    cancelled.get_token()};
  Json response;
  hook::execHttpHook(loop,services)(request).then(
    [&](const hook::HttpOutcome* value,const std::exception_ptr& error){
      if(error)std::rethrow_exception(error);
      response={{"ok",value->ok},{"body",value->body}};
      if(value->statusCode)response["statusCode"]=*value->statusCode;
      if(value->error)response["error"]=*value->error;
      if(value->aborted)response["aborted"]=true;
    });
  loop.run();
  return {{"name",item.at("name")},{"response",response},{"calls",calls}};
}
}
int main(int argc,char** argv){
  if(argc!=2)throw std::invalid_argument("cases path required");
  std::ifstream stream(argv[1]);Json cases;stream>>cases;
  Json rows=Json::array();for(const auto& item:cases)rows.push_back(run(item));
  std::cout<<value::stringify(rows)<<'\n';
}
