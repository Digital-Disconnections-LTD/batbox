#include "retry/Retry.hpp"
#include "streaming/Reducer.hpp"
#include "transport/RetryAdapter.hpp"
#include "transport/openai_chat/OpenAIChatClient.hpp"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

using batbox::Json;
namespace transport = batbox::transport;

namespace {
Json requestBody(){
  return Json::parse(R"JSON({
    "model":"fake-model","max_tokens":512,"temperature":1,
    "system":[{"type":"text","text":"You are Batbox."}],
    "messages":[
      {"role":"user","content":[{"type":"text","text":"Run it"}]},
      {"role":"assistant","content":[{"type":"tool_use","id":"toolu_01PRIORCALLAAAAAAAAAAAA","name":"Bash","input":{"command":"pwd"}}]},
      {"role":"user","content":[{"type":"tool_result","tool_use_id":"toolu_01PRIORCALLAAAAAAAAAAAA","content":"/tmp"},
        {"type":"text","text":"Now read and list"}]}],
    "tools":[{"name":"Read","description":"Read a file","input_schema":{"type":"object","properties":{"file_path":{"type":"string"}}}},
             {"name":"Bash","description":"Run a command","input_schema":{"type":"object","properties":{"command":{"type":"string"}}}}]})JSON");
}
Json reduce(const std::vector<Json>& events){
  batbox::streaming::ReducerConfig config;
  config.model="fake::fake-model";
  config.randomUUID=[]{return std::string("uuid");};
  config.timestamp=[]{return std::string("now");};
  config.calculateUSDCost=[](const std::string&,const Json&){return 0.0;};
  config.addToTotalFlightCost=[](double,const Json&,const std::string&){return 0.0;};
  config.getErrorMessageIfRefusal=[](const Json&,const std::string&)->std::optional<Json>{return std::nullopt;};
  config.createAssistantAPIErrorMessage=[](const std::string& text,const std::string&,const std::string&){
    return Json{{"type","assistant"},{"error",text}};};
  batbox::streaming::Reducer reducer(std::move(config));
  Json messages=Json::array();
  for(const auto& event:events)(void)reducer.consume(event);
  reducer.finish();
  for(const auto& message:reducer.messages())messages.push_back(message->at("message"));
  return Json{{"messages",messages},{"usage",reducer.usage()},{"stopReason",reducer.stopReason()}};
}
Json types(const std::vector<Json>& events){
  Json names=Json::array();
  for(const auto& event:events)names.push_back(event.at("type"));
  return names;
}
Json failure(const std::exception& error){
  Json result{{"error",error.what()}};
  if(const auto* api=dynamic_cast<const transport::APIError*>(&error)){
    result["status"]=api->status?Json(*api->status):Json(nullptr);
    result["body"]=api->body;result["headers"]=api->headers;
  }
  return result;
}
} // namespace

int main(int argc,char** argv){
  if(argc<3)return 2;
  transport::ClientConfig config;
  config.baseUrl=argv[1];
  config.messagesPath="/v1/chat/completions";
  config.authToken="test-key";
  // A live check passes credentials through the environment, never argv.
  if(const auto* token=std::getenv("OPENAI_CHAT_PROBE_TOKEN"))config.authToken=token;
  if(const auto* headers=std::getenv("OPENAI_CHAT_PROBE_HEADERS"))config.headers=Json::parse(headers);
  config.requestTimeout=std::chrono::seconds(10);
  config.streamIdleTimeout=std::chrono::seconds(8);
  transport::ChatQuirks quirks;
  if(argc>3)quirks=transport::chatQuirks(Json::parse(argv[3]));
  transport::openai_chat::OpenAIChatClient client(config,quirks);
  const std::string mode=argv[2];
  try{
    if(mode=="models"){std::cout<<client.listModels().dump()<<'\n';return 0;}
    if(mode=="nonstream"){
      const auto response=client.request(requestBody(),false);
      std::cout<<Json{{"status",response.status},{"body",response.body}}.dump()<<'\n';return 0;
    }
    if(mode=="stream"){
      std::vector<Json> events;
      Json headers;
      const auto response=client.request(requestBody(),true,[&](const Json& event){events.push_back(event);},{},
        [&](const transport::Response& early){headers=early.headers;});
      auto result=reduce(events);
      result["events"]=types(events);result["status"]=response.status;result["headers"]=headers;
      result["start"]=events.empty()?Json(nullptr):events.front();
      std::cout<<result.dump()<<'\n';return 0;
    }
    if(mode=="retry"){
      int attempts=0;
      batbox::retry::Dependencies deps;
      deps.wire.retryAfter="retry-after";deps.wire.shouldRetry="x-should-retry";
      deps.maxRetriesCap=2;deps.maxRetryDelayMs=2000;
      deps.getClient=[]{return Json::object();};
      deps.operation=[&](const Json&,double,const batbox::retry::RetryContext&)->Json{
        ++attempts;
        std::vector<Json> events;
        try{client.request(requestBody(),true,[&](const Json& event){events.push_back(event);});}
        catch(const transport::APIError&){
          throw batbox::retry::RetryFailure(transport::errorRecordFromException(std::current_exception()));}
        return reduce(events);
      };
      deps.yield=[](const Json&){};
      deps.sleep=batbox::retry::sleepAbortResponsive;
      deps.random=[]{return 0.0;};
      deps.nowMs=[]{return 0.0;};
      deps.createSystemAPIErrorMessage=[](const batbox::retry::ErrorRecord&,double,double,double){return Json::object();};
      deps.isFastModeEnabled=[]{return false;};
      deps.isSubscriber=[]{return false;};
      deps.isEnterpriseSubscriber=[]{return false;};
      deps.isDefaultPremiumModel=[](const std::string&){return false;};
      deps.getOAuthAccessToken=[]()->std::optional<std::string>{return std::nullopt;};
      deps.clearApiKeyHelperStash=[]{};
      deps.disableKeepAlive=[]{};
      deps.logForDebugging=[](const std::string&){};
      deps.logError=[](const std::string&){};
      batbox::retry::RetryOptions options;options.model="fake-model";
      const auto started=std::chrono::steady_clock::now();
      Json result=Json::object();
      try{result=batbox::retry::withRetry(options,deps);}
      catch(const batbox::retry::CannotRetryError& error){
        result["error"]=error.originalError?error.originalError->message:std::string(error.what());
        if(error.originalError&&error.originalError->status)result["status"]=*error.originalError->status;
      }catch(const std::exception& error){result=failure(error);}
      result["attempts"]=attempts;
      result["elapsedMs"]=std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now()-started).count();
      std::cout<<result.dump()<<'\n';return 0;
    }
  }catch(const std::exception& error){std::cout<<failure(error).dump()<<'\n';return 0;}
  return 2;
}
