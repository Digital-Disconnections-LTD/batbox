#include "model/StreamingAttempt.hpp"
#include <iostream>

using batbox::Json;
using namespace batbox::model;

int main(){
  std::string line;
  while(std::getline(std::cin,line)){
    try{
      const Json input=Json::parse(line);
      Json calls=Json::array();
      StreamingAttemptState state;
      StreamingAttemptDependencies deps;
      deps.nowMillis=[]{return std::int64_t{1793188800000};};
      deps.firstPartyProvider=input.value("firstParty",false);
      deps.firstPartyBaseUrl=input.value("firstPartyBaseUrl",false);
      deps.injectCorrelationId=true; // fixture enables correlation IDs; production default is false
      deps.paramsFromContext=[&](const batbox::retry::RetryContext&){
        calls.push_back(Json::array({"paramsFromContext"}));return input.at("params");};
      deps.captureAPIRequest=[&](const Json& params){calls.push_back(Json::array({"captureAPIRequest",params}));};
      deps.randomUUID=[&]{calls.push_back(Json::array({"randomUUID"}));return "client-uuid-1";};
      deps.openStream=[&](const Json& params,const std::optional<std::string>& clientId){
        calls.push_back(Json::array({"openStream",params,clientId?Json(*clientId):Json(nullptr)}));
        if(input.value("openError",false))throw std::runtime_error("Open failed");
        OpenedStream opened;
        opened.response.status=input.contains("response")?input.at("response").value("status",200):200;
        opened.response.headers=input.contains("response")?input.at("response").value("headers",Json::object()):Json::object();
        opened.requestId=input.value("requestId","request-1");
        return opened;
      };
      batbox::retry::RetryContext context;
      context.model=input.value("model","batbox-model");
      if(input.contains("fastMode"))context.fastMode=input.at("fastMode").get<bool>();
      auto stateJson=[&]{
        Json value={{"attemptNumber",state.attemptNumber},{"isFastModeRequest",state.isFastModeRequest},
                    {"start",state.start},{"attemptStartTimes",state.attemptStartTimes},
                    {"maxOutputTokens",state.maxOutputTokens}};
        if(state.clientRequestId)value["clientRequestId"]=*state.clientRequestId;
        if(state.streamRequestId)value["streamRequestId"]=*state.streamRequestId;
        if(state.streamRequestId)value["streamResponse"]={{"status",state.streamResponse.status},{"headers",state.streamResponse.headers}};
        return value;
      };
      try{
        executeStreamingAttempt(input.value("attempt",1),context,state,deps);
        std::cout<<Json{{"data",{{"stream","handle"}}},{"state",stateJson()},{"calls",calls}}.dump()<<'\n';
      }catch(const std::exception& error){
        std::cout<<Json{{"error",{{"name","Error"},{"message",error.what()}}},
                        {"state",stateJson()},{"calls",calls}}.dump()<<'\n';
      }
    }catch(const std::exception& error){std::cout<<Json{{"fatal",error.what()}}.dump()<<'\n';}
  }
}
