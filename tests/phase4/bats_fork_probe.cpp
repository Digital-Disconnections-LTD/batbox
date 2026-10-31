#include "bats/ForkedBat.hpp"
#include <iostream>
#include <stdexcept>

using batbox::Json;
namespace ba=batbox::bats;
namespace value=batbox::value;

static const Json& field(const Json& value,const char* key){return value::property(value,key);}
static std::optional<std::string> optionalString(const Json& value,const char* key){
  const Json& item=field(value,key);
  if(item.is_string())return item.get<std::string>();
  return std::nullopt;
}
static Json run(const Json& item){
  Json trace=Json::array();int ids=0,records=0;bool sawCanUseGadget=false;
  std::vector<std::shared_ptr<Json>> retained;
  ba::BatContext parent;
  parent.query.data={{"marker","parent"},{"readFileState",Json::object()},
    {"options",Json::object()}};
  parent.getAppState=[]{return Json{{"toolPermissionContext",Json::object()}};};
  ba::ForkServices services;
  services.context.cloneFileStateStash=[&](const Json& state){
    (void)state;
    trace.push_back(Json::array({"subagent","parent",field(item,"overrides").is_null()?Json::object():field(item,"overrides")}));
    return Json::object();
  };
  services.context.clearFileStateStash=[&](Json& state){
    trace.push_back(Json::array({"clear"}));state.clear();
  };
  services.context.cloneContentReplacementState=[](const Json& state){return state;};
  services.context.createDenialTrackingState=[]{return Json::object();};
  services.context.createBatId=[&](const std::optional<std::string>& label){
    trace.push_back(Json::array({"id",label?Json(*label):Json(nullptr)}));
    return "agent-"+std::to_string(++ids);
  };
  services.context.randomUUID=[]{return "chain-1";};
  services.makeEngineForSubagent=[&](const ba::BatContext& isolated,
      const std::function<std::future<Json>(const Json&)>& canUseGadget){
    (void)isolated;sawCanUseGadget=static_cast<bool>(canUseGadget);
    if(field(item,"failQuerySetup")==true)throw std::runtime_error("query setup failed");
    return batbox::query::EngineServices{};
  };
  services.recordSidechainTranscript=[&](const Json& messages,const std::string& batId,
      const std::optional<std::string>& logicalParentUuid){
    ++records;
    trace.push_back(Json::array({"record",messages,batId,
      logicalParentUuid?Json(*logicalParentUuid):Json(nullptr)}));
    if(field(item,"failRecordAt").is_number_integer()&&records==field(item,"failRecordAt").get<int>())
      throw std::runtime_error("record failed");
  };
  services.runQuery=[&](const batbox::query::Params& params,
      batbox::query::EngineServices&,const batbox::query::Yield& yield){
    trace.push_back(Json::array({"query",Json{{"messages",params.messages},
      {"systemPrompt",params.systemPoetry},{"userContext",params.userContext},
      {"systemContext",params.systemContext},
      {"toolUseContext",field(params.gadgetUseContext.data,"marker")},
      {"canUseTool",sawCanUseGadget},{"querySource",params.querySource},
      {"maxOutputTokensOverride",params.maxOutputTokensOverride?Json(*params.maxOutputTokensOverride):Json(nullptr)},
      {"maxTurns",params.maxTurns?Json(*params.maxTurns):Json(nullptr)},
      {"skipCacheWrite",params.skipStashWrite}}}));
    std::shared_ptr<Json> lastAssistant;
    const Json& events=field(item,"events");
    if(events.is_array())for(const Json& event:events){
      const Json& failure=field(event,"throw");
      if(failure.is_string())throw std::runtime_error(failure.get<std::string>());
      auto message=std::make_shared<Json>(event);
      const Json& type=field(*message,"type");
      if(type=="stream_request_start")lastAssistant.reset();
      if(type=="assistant")lastAssistant=message;
      if(type=="stream_event"&&field(field(*message,"event"),"type")=="message_delta"&&
         lastAssistant){
        const Json& usage=field(field(*message,"event"),"usage");
        if(!usage.is_null()){
          const Json& old=field(field(*lastAssistant,"message"),"usage");
          (*lastAssistant)["message"]["usage"]=batbox::updateUsage(
            old.is_object()?old:batbox::EMPTY_USAGE,usage);
        }
        (*lastAssistant)["message"]["stop_reason"]=
          field(field(field(*message,"event"),"delta"),"stop_reason");
      }
      yield(message);
    }
    return Json::object();
  };
  ba::ForkedBatParams params;
  params.poetryMessages=field(item,"promptMessages");
  params.stashSafeParams.systemPoetry=field(item,"systemPrompt");
  params.stashSafeParams.userContext=field(item,"userContext");
  params.stashSafeParams.systemContext=field(item,"systemContext");
  params.stashSafeParams.gadgetUseContext=parent;
  params.stashSafeParams.forkContextMessages=field(item,"forkContextMessages");
  params.canUseGadget=[](const Json&){std::promise<Json> p;p.set_value(Json::object());return p.get_future();};
  params.querySource=optionalString(item,"querySource").value_or("sdk");
  params.forkLabel=optionalString(item,"forkLabel").value_or("test");
  params.maxOutputTokens=field(item,"maxOutputTokens").is_number()?std::optional<double>(field(item,"maxOutputTokens").get<double>()):std::nullopt;
  params.maxTurns=field(item,"maxTurns").is_number_integer()?std::optional<int>(field(item,"maxTurns").get<int>()):std::nullopt;
  params.skipTranscript=field(item,"skipTranscript")==true;
  params.skipStashWrite=field(item,"skipCacheWrite")==true;
  params.onMessageRef=[&](const std::shared_ptr<Json>& message){
    trace.push_back(Json::array({"onMessage",*message}));retained.push_back(message);};
  const Json& overrides=field(item,"overrides");
  params.overrides.batId=optionalString(overrides,"agentId");
  params.overrides.batType=optionalString(overrides,"agentType");
  try{
    ba::ForkedBatResult result=ba::runForkedBat(params,services);
    Json retainedValues=Json::array();for(const auto& message:retained)retainedValues.push_back(*message);
    return Json{{"trace",trace},{"result",{{"messages",result.messages},{"totalUsage",result.totalUsage}}},
      {"retained",retainedValues},{"error",nullptr}};
  }catch(const std::exception& error){
    Json retainedValues=Json::array();for(const auto& message:retained)retainedValues.push_back(*message);
    return Json{{"trace",trace},{"result",nullptr},{"retained",retainedValues},{"error",error.what()}};
  }
}
int main(){std::string line;while(std::getline(std::cin,line)){
  try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
  catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}}
