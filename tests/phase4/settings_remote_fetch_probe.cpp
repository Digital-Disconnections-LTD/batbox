#include "settings/remote/Auth.hpp"
#include "settings/remote/FetchOne.hpp"
#include "settings/remote/Retry.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {
batbox::Json present(const batbox::settings::remote::FetchResult& value){
  using batbox::Json;
  Json result={{"success",value.success}};
  if(value.settings)result["settings"]=*value.settings;
  if(value.checksum)result["checksum"]=*value.checksum;
  if(value.error)result["error"]=*value.error;
  if(value.skipRetry)result["skipRetry"]=true;
  return result;
}
batbox::settings::remote::FetchResult parse(const batbox::Json& value){
  batbox::settings::remote::FetchResult result;
  result.success=value.value("success",false);
  if(value.contains("settings"))result.settings=value["settings"];
  if(value.contains("checksum")&&!value["checksum"].is_null())
    result.checksum=value["checksum"].get<std::string>();
  if(value.contains("error")&&!value["error"].is_null())
    result.error=value["error"].get<std::string>();
  result.skipRetry=value.value("skipRetry",false);
  return result;
}
}
int main(){
  using namespace batbox;
  using namespace batbox::settings::remote;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    event_loop::Loop loop;
    Json trace=Json::array(),result=nullptr;
    AuthServices auth;
    auth.apiKeyWithoutHelper=[&]()->std::optional<std::string>{
      trace.push_back({"key",Json{{"skipRetrievingKeyFromApiKeyHelper",true}}});
      if(test.value("keyError",false))throw std::runtime_error("key error");
      if(!test.contains("key")||test["key"].is_null())return std::nullopt;
      return test["key"].get<std::string>();
    };
    auth.oauthTokens=[&](){trace.push_back({"oauth"});
      return test.value("tokens",Json(nullptr));};
    auth.oauthBetaValue="beta-value";
    const auto op=test.value("op",std::string{});
    if(op=="auth"){
      const auto value=getRemoteSettingsAuthHeaders(auth);
      result={{"headers",value.headers}};
      if(value.error)result["error"]=*value.error;
    }else if(op=="endpoint"){
      result=getRemoteManagedSettingsEndpoint("https://local.test");
    }else{
      event_loop::Deferred<FetchResult> output(loop);
      const auto checksum=test.contains("checksum")&&
        !test["checksum"].is_null()
        ?std::optional(test["checksum"].get<std::string>()):std::nullopt;
      if(op=="retry"){
        RetryServices services;
        auto call=std::make_shared<std::size_t>(0);
        services.fetchOne=[&,call](const std::optional<std::string>& value){
          trace.push_back({"fetch",value?Json(*value):Json(nullptr)});
          event_loop::Deferred<FetchResult> done(loop);
          const auto rows=test.value("results",Json::array());
          done.resolve(*call<rows.size()?parse(rows[(*call)++]):
            FetchResult{false,{},{},"failed",false});
          return done;
        };
        services.getRetryDelay=[&](int attempt){
          trace.push_back({"delay",attempt});return attempt*100.0;};
        services.sleep=[&](double delay){
          trace.push_back({"sleep",delay});
          event_loop::Deferred<event_loop::Unit> done(loop);
          done.resolve({});return done;
        };
        services.debug=[&](const std::string& text){
          trace.push_back({"debug",text});};
        output=fetchWithRetry(loop,checksum,services);
      }else{
        FetchOneServices services;
        services.refreshOAuth=[&](){
          trace.push_back({"refreshOAuth"});
          event_loop::Deferred<event_loop::Unit> done(loop);
          done.resolve({});return done;
        };
        services.auth=auth;
        services.baseApiUrl=[](){return "https://local.test";};
        services.userBat=[&](){trace.push_back({"userAgent"});
          return "batbox-test";};
        services.httpGet=[&](const std::string& url,const Json& headers,
            double timeout){
          trace.push_back({"get",url,Json{{"headers",headers},
            {"timeout",timeout}}});
          event_loop::Deferred<HttpResponse> done(loop);
          if(test.contains("httpError")){
            const auto& error=test["httpError"];
            const auto status=error.contains("status")?
              std::optional(error["status"].get<int>()):std::nullopt;
            const auto code=error.value("code",std::string{});
            const auto kind=status&&(status==401||status==403)?"auth":
              code=="ECONNABORTED"?"timeout":
              code=="ECONNREFUSED"||code=="ENOTFOUND"?"network":"http";
            done.reject(std::make_exception_ptr(HttpError(
              error.value("message",std::string("request failed")),
              kind,status)));
          }else{
            const auto response=test.value("response",Json{{"status",204}});
            done.resolve({response.value("status",204),
              response.value("data",Json(nullptr))});
          }
          return done;
        };
        services.validateResponse=[&](const Json& value){
          if(test.value("responseValid",true))
            return ResponseParseResult{true,value,{}};
          return ResponseParseResult{false,nullptr,"schema rejected"};
        };
        services.validateSettings=[&](const Json& value){
          if(test.value("settingsValid",true))
            return batbox::settings::SchemaResult{true,value,Json::array()};
          return batbox::settings::SchemaResult{false,nullptr,Json::array()};
        };
        services.settingsErrorMessage=[](const batbox::settings::SchemaResult&){
          return "settings rejected";};
        services.debug=[&](const std::string& text){
          trace.push_back({"debug",text});};
        output=fetchRemoteManagedSettings(loop,checksum,services);
      }
      bool done=false;
      output.then([&](const FetchResult* value,
          const std::exception_ptr& error){
        if(error)std::rethrow_exception(error);
        result=present(*value);done=true;
      });
      for(int n=0;n<100&&!done;n++)loop.runReady();
      if(!done)throw std::runtime_error("Remote fetch probe did not settle");
    }
    std::cout<<value::stringify(Json{{"result",result},{"trace",trace}})<<'\n';
  }
}
