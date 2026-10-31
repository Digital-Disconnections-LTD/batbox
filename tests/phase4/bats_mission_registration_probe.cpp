#include "bats/BatMissionRegistration.hpp"
#include <iostream>
using batbox::Json;
namespace ba=batbox::bats;
namespace value=batbox::value;
static const Json& field(const Json& object,const char* key){return value::property(object,key);}
static bool flag(const Json& item,const char* key){return field(item,key)==true;}
static Json run(const Json& item){
 Json trace=Json::array(),state={{"tasks",Json::object()}};
 if(flag(item,"existing"))state["tasks"]["agent-id"]={{"type","local_agent"},
  {"status","running"},{"retain",true},{"startTime",500},
  {"messages",Json::array({Json{{"uuid","held"}}})},{"diskLoaded",true},
  {"pendingMessages",Json::array({"pending"})}};
 ba::BatMissionStateServices mission;
 mission.setAppState=[&](const std::function<Json(const Json&)>& updater){
  trace.push_back(Json::array({"setState"}));state=updater(state);};
 mission.nowMillis=[&]{trace.push_back(Json::array({"now"}));return std::int64_t(1000);};
 ba::BatRegistrationServices services;services.state=&mission;
 services.getMissionOutputPath=[&](const std::string& id){
  trace.push_back(Json::array({"outputPath",id}));return "/tmp/"+id+".txt";};
 services.getBatTranscriptPath=[&](const std::string& id){
  trace.push_back(Json::array({"transcriptPath",id}));return "/tmp/"+id+".jsonl";};
 services.initMissionOutputAsSymlink=[&](const std::string& id,const std::string& p){
  trace.push_back(Json::array({"symlink",id,p}));};
 services.registerCleanup=[&](const std::function<void()>&){
  trace.push_back(Json::array({"registerCleanup"}));
  return std::function<void()>([&]{trace.push_back(Json::array({"unregister"}));});};
 services.enqueueSdkEvent=[&](const Json& event){
  trace.push_back(Json::array({"sdkEvent",event}));};
 services.onCreateAbortOwner=[&](bool child){
  trace.push_back(Json::array({child?"childAbortOwner":"abortOwner"}));};
 auto parent=std::make_shared<std::stop_source>();
 if(flag(item,"parentAlreadyAborted"))parent->request_stop();
 ba::BatRegistrationParams params;
 params.batId="agent-id";params.description="description";params.poetry="prompt";
 params.selectedBat=field(item,"selectedAgent");
 if(field(item,"toolUseId").is_string())
  params.gadgetUseId=field(item,"toolUseId").get<std::string>();
 if(flag(item,"hasParent"))params.parentAbortOwner=parent;
 auto result=ba::registerAsyncBat(params,services);
 if(flag(item,"cancelParent"))parent->request_stop();
 return Json{{"trace",trace},{"state",state},{"returned",result.missionState},
  {"aborted",result.abortOwner->stop_requested()}};
}
int main(){std::string line;while(std::getline(std::cin,line))try{
 std::cout<<value::stringify(run(value::parse(line)))<<'\n';
 }catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}}
