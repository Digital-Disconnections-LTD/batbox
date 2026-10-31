#include "model/NonStreaming.hpp"
#include "model/ModelWrappers.hpp"
#include <iostream>

using batbox::Json;
using namespace batbox::model;

int main() {
  std::string line;
  while (std::getline(std::cin, line)) {
    try {
      const Json input=Json::parse(line);
      Json calls=Json::array();
      Json yielded=Json::array();
      NonStreamingDependencies deps;
      deps.environment=input.value("env",Json::object());
      deps.nowMillis=[]{return std::int64_t{1793188800000};};
      deps.paramsFromContext=[&](const batbox::retry::RetryContext& context) {
        calls.push_back(Json::array({"paramsFromContext",context.model}));
        return input.at("params");
      };
      deps.captureRequest=[&](const Json& params){calls.push_back(Json::array({"captureRequest",params}));};
      deps.onAttempt=[&](double attempt,std::int64_t start,double tokens) {
        calls.push_back(Json::array({"onAttempt",attempt,start,tokens}));
      };
      deps.normalizeModelStringForAPI=[&](const std::string& model) {
        calls.push_back(Json::array({"normalizeModel",model}));return "norm:"+model;
      };
      deps.createMessage=[&](const Json& params,double timeout,std::stop_token) {
        calls.push_back(Json::array({"create",params,timeout}));
        if(input.value("createAbort",false))throw UserAbortError();
        if(input.value("createError",false))throw std::runtime_error("Create failed");
        return input.at("result");
      };
      deps.yieldSystem=[&](const Json& message){yielded.push_back(message);};
      deps.logForDiagnosticsNoPII=[&](const std::string& level,const std::string& event){
        calls.push_back(Json::array({"diagnostic",level,event}));};
      deps.retry.getClient=[&] {
        calls.push_back(Json::array({"getClient",0,input.value("model","batbox-model"),"repl_main_thread"}));
        return Json::object();
      };
      deps.withRetry=[&](const batbox::retry::RetryOptions& options,batbox::retry::Dependencies& retryDeps) {
        calls.push_back(Json::array({"retry",options.model,
          input.contains("initial529")?input.at("initial529"):Json(nullptr)}));
        const Json client=retryDeps.getClient();
        for(const Json& event:input.value("retryYields",Json::array())) retryDeps.yield(event);
        batbox::retry::RetryContext context;
        context.model=input.contains("context") ? input.at("context").value("model",options.model) : options.model;
        context.thinkingConfig={{"type","disabled"}};
        return retryDeps.operation(client,input.value("attempt",1),context);
      };
      batbox::retry::RetryOptions options;
      options.model=input.value("model","batbox-model");
      options.thinkingConfig={{"type","disabled"}};
      options.initialConsecutive529Errors=input.value("initial529",0);
      try {
        Json result=executeNonStreamingRequest(options,std::move(deps));
        std::cout<<Json{{"yielded",yielded},{"result",result},{"calls",calls}}.dump()<<'\n';
      }catch(const UserAbortError& error){
        std::cout<<Json{{"yielded",yielded},{"error",{{"name","APIUserAbortError"},{"message",error.what()}}},{"calls",calls}}.dump()<<'\n';
      }catch(const std::exception& error){
        std::cout<<Json{{"yielded",yielded},{"error",{{"name","Error"},{"message",error.what()}}},{"calls",calls}}.dump()<<'\n';
      }
    }catch(const std::exception& error){std::cout<<Json{{"fatal",error.what()}}.dump()<<'\n';}
  }
}
