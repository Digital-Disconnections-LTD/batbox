#include "bats/BatMissionForeground.hpp"
#include <chrono>
#include <iostream>
using batbox::Json;
namespace ba=batbox::bats;
namespace value=batbox::value;
static const Json& field(const Json& object,const char* key){return value::property(object,key);}
static Json run(const Json& item){
 Json trace=Json::array(),state={{"tasks",Json::object()}},manual=Json::array();
 ba::BatMissionStateServices mission;
 mission.setAppState=[&](const std::function<Json(const Json&)>& updater){
  trace.push_back(Json::array({"setState"}));state=updater(state);};
 mission.getAppState=[&]{return state;};
 mission.nowMillis=[&]{trace.push_back(Json::array({"now"}));return std::int64_t(1000);};
 mission.abortMission=[](const std::string&){};
 mission.unregisterCleanup=[&](const std::string&){
  trace.push_back(Json::array({"unregisterCleanup"}));};
 ba::BatRegistrationServices registration;registration.state=&mission;
 registration.getMissionOutputPath=[&](const std::string& id){
  trace.push_back(Json::array({"outputPath",id}));return "/tmp/"+id+".txt";};
 registration.getBatTranscriptPath=[&](const std::string& id){
  trace.push_back(Json::array({"transcriptPath",id}));return "/tmp/"+id+".jsonl";};
 registration.initMissionOutputAsSymlink=[&](const std::string& id,const std::string& path){
  trace.push_back(Json::array({"symlink",id,path}));};
 registration.onCreateAbortOwner=[&](bool){trace.push_back(Json::array({"abortOwner"}));};
 registration.registerCleanup=[&](const std::function<void()>&){
  trace.push_back(Json::array({"registerCleanup"}));
  return std::function<void()>([&]{trace.push_back(Json::array({"unregisterCleanup"}));});};
 registration.enqueueSdkEvent=[&](const Json& event){
  trace.push_back(Json::array({"sdkEvent",event}));};
 ba::ForegroundSignals signals;
 ba::ForegroundServices services;services.registration=&registration;
 services.signals=&signals;
 std::function<void()> timer;
 services.scheduleAutoBackground=[&](double ms,const std::function<void()>& callback){
  trace.push_back(Json::array({"schedule",ms}));timer=callback;
  return std::function<void()>([&]{trace.push_back(Json::array({"cancelTimer",1}));});};
 ba::BatRegistrationParams params;params.batId="agent-id";
 params.description="description";params.poetry="prompt";
 params.selectedBat={{"agentType","worker"}};
 if(field(item,"toolUseId").is_string())params.gadgetUseId=field(item,"toolUseId").get<std::string>();
 const Json& ms=field(item,"autoBackgroundMs");
 const std::optional<double> delay=ms.is_number()?std::optional<double>(ms.get<double>()):std::nullopt;
 auto handle=ba::registerBatForeground(params,delay,services);
 const Json& operations=field(item,"operations");
 if(operations.is_array())for(const Json& operation:operations){
  if(operation=="timer"&&timer)timer();
  if(operation=="cancel"&&handle.cancelAutoBackground)handle.cancelAutoBackground();
  if(operation=="background")manual.push_back(ba::backgroundBatMission("agent-id",services));
  if(operation=="unregister")ba::unregisterBatForeground("agent-id",services);
 }
 const bool resolved=handle.backgroundSignal.wait_for(std::chrono::seconds(0))==
  std::future_status::ready;
 return Json{{"trace",trace},{"state",state},{"resolved",resolved},
  {"manual",manual},{"taskId",handle.missionId},
  {"canCancel",static_cast<bool>(handle.cancelAutoBackground)}};
}
int main(){std::string line;while(std::getline(std::cin,line))try{
 std::cout<<value::stringify(run(value::parse(line)))<<'\n';
 }catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}}
