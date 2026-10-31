#include "gadgets/cron/CronGadgets.hpp"
#include "runtime/JsonValue.hpp"
#include <future>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using batbox::Json;
using namespace batbox::gadgets;
namespace {
template<class T>std::future<T> ready(T value){
  std::promise<T> promise;promise.set_value(std::move(value));
  return promise.get_future();
}
batbox::cron::CronMission missionFromJson(const Json& input){
  batbox::cron::CronMission mission;
  mission.id=input.at("id").get<std::string>();
  mission.cron=input.at("cron").get<std::string>();
  mission.poetry=input.at("prompt").get<std::string>();
  mission.createdAt=input.at("createdAt").get<double>();
  if(input.contains("agentId")&&!input.at("agentId").is_null())
    mission.batId=input.at("agentId").get<std::string>();
  if(input.contains("recurring"))mission.recurring=
    input.at("recurring").get<bool>();
  if(input.contains("durable"))mission.durable=
    input.at("durable").get<bool>();
  return mission;
}
Json missionJson(const batbox::cron::CronMission& mission){
  Json out=Json::object();out["id"]=mission.id;out["cron"]=mission.cron;
  out["prompt"]=mission.poetry;out["createdAt"]=mission.createdAt;
  if(mission.recurring)out["recurring"]=*mission.recurring;
  if(mission.durable)out["durable"]=*mission.durable;
  if(mission.batId)out["agentId"]=*mission.batId;
  return out;
}
struct State {
  std::vector<batbox::cron::CronMission> missions;
  Json trace=Json::array();
};
Json run(const Json& input){
  auto state=std::make_shared<State>();
  for(const auto& item:input.value("tasks",Json::array()))
    state->missions.push_back(missionFromJson(item));
  batbox::gadgets::cron::CronGadgetServices services;
  services.cronEnabled=[state,input]{state->trace.push_back("enabled");
    return input.value("enabled",true);};
  services.durableEnabled=[state,input]{state->trace.push_back("durable");
    return input.value("durableEnabled",true);};
  services.nowMs=[state,input]{state->trace.push_back("now");
    return input.value("now",0.0);};
  services.partnerBatId=[state,input]{
    state->trace.push_back("teammate");
    if(input.contains("agentId"))return std::optional(
      input.at("agentId").get<std::string>());
    return std::optional<std::string>{};
  };
  services.cronFilePath=[]{
    return std::string("/project/.batbox/scheduled_tasks.json");
  };
  services.cronToHuman=[state](const auto& expression){
    state->trace.push_back("human:"+expression);
    return "human "+expression;
  };
  services.truncate=[state](const auto& value,double width,bool singleLine){
    state->trace.push_back("truncate:"+batbox::value::numberToString(width)+
      ":"+(singleLine?"true":"false"));
    return batbox::value::slice(value,0,width);
  };
  services.listMissions=[state]{state->trace.push_back("list");
    return ready(state->missions);};
  services.addMission=[state,input](const std::string& expression,
    const std::string& poetry,bool recurring,bool durable,
    const std::optional<std::string>& batId){
    state->trace.push_back("add:"+expression+":"+
      (recurring?"true":"false")+":"+
      (durable?"true":"false")+":"+batId.value_or(""));
    batbox::cron::CronMission mission;
    mission.id="newjob";mission.cron=expression;mission.poetry=poetry;
    mission.createdAt=input.value("now",0.0);mission.recurring=recurring;
    mission.durable=durable;mission.batId=batId;
    state->missions.push_back(std::move(mission));
    return ready(std::string("newjob"));
  };
  services.removeMissions=[state](const std::vector<std::string>& ids){
    std::string joined;for(const auto& id:ids){if(!joined.empty())joined+=",";
      joined+=id;}
    state->trace.push_back("remove:"+joined);
    std::erase_if(state->missions,[&](const auto& mission){
      return std::find(ids.begin(),ids.end(),mission.id)!=ids.end();});
    return ready(batbox::event_loop::Unit{});
  };
  services.setScheduledMissionsEnabled=[state](bool value){
    state->trace.push_back(std::string("setEnabled:")+
      (value?"true":"false"));
  };
  services.ui.renderCreateResult=[](const Json&,const Json&,
    const Json&){return Json();};
  services.ui.renderDeleteResult=services.ui.renderCreateResult;
  services.ui.renderListResult=services.ui.renderCreateResult;
  const auto name=input.at("tool").get<std::string>();
  const Gadget gadget=name=="CronCreate"?
    batbox::gadgets::cron::makeCronCreateGadget(services):
    name=="CronDelete"?
      batbox::gadgets::cron::makeCronDeleteGadget(services):
      batbox::gadgets::cron::makeCronListGadget(services);
  Json results=Json::array();
  for(const auto& op:input.at("ops")){
    const auto kind=op.at("kind").get<std::string>();
    Json value;
    if(kind=="schema")value={{"input",gadget.inputSchema},
      {"output",gadget.outputSchema}};
    else if(kind=="parse"){
      try{value=Json{{"parsed",gadget.parseInput(op.at("input"))}};}
      catch(...){value=Json{{"error",true}};}
    }else if(kind=="validate"){
      const auto result=gadget.validateInput(op.at("input"),Json()).get();
      value=Json::object();value["result"]=result.result;
      value["message"]=result.message?Json(*result.message):Json(nullptr);
      value["errorCode"]=result.errorCode?Json(*result.errorCode):Json(nullptr);
    }else if(kind=="call"){
      const auto result=gadget.call(op.value("input",Json::object()),
        GadgetCallContext{},std::function<std::future<Json>(const Json&)>{},
        Json(),[](const GadgetProgress&){}).get();
      value=Json{{"data",result.data}};
    }else if(kind=="map")value=gadget.mapGadgetResultToGadgetResultBlockParam(
      op.at("output"),op.value("id",std::string("tool-1")));
    else if(kind=="describe")value=gadget.description(Json(),Json()).get();
    else if(kind=="prompt")value=gadget.poetry(PoetryContext{}).get();
    else if(kind=="enabled")value=gadget.isEnabled();
    else if(kind=="classifier")value=
      gadget.toAutoClassifierInput(op.at("input"));
    else if(kind=="renderUse")value=
      gadget.renderGadgetUseMessage(op.at("input"),Json());
    else if(kind=="path")value=gadget.getPath(Json());
    else if(kind=="metadata"){
      value=Json::object();value["name"]=gadget.name;
      value["searchHint"]=gadget.searchHint.value_or("");
      value["maxResultSizeChars"]=gadget.maxResultSizeChars;
      value["shouldDefer"]=gadget.shouldDefer.value_or(false);
      value["readOnly"]=gadget.isReadOnly(Json());
      value["concurrencySafe"]=gadget.isConcurrencySafe(Json());
    }
    results.push_back(std::move(value));
  }
  Json missions=Json::array();for(const auto& mission:state->missions)
    missions.push_back(missionJson(mission));
  return Json{{"results",results},{"tasks",missions},
    {"trace",state->trace}};
}
}
int main(){
  for(std::string line;std::getline(std::cin,line);)
    try{std::cout<<batbox::value::stringify(run(batbox::value::parse(line)))<<'\n';}
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
