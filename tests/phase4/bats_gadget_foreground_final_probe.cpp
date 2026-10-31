#include "bats/BatGadgetForegroundFinalize.hpp"
#include <iostream>
#include <string>
using batbox::Json;
namespace {
const Json& field(const Json& value,const char* key){return batbox::value::property(value,key);}
}
int main(){
 std::string line;
 while(std::getline(std::cin,line)){
  try{
   const auto item=batbox::value::parse(line);
   Json trace=Json::array();
   batbox::bats::BatGadgetForegroundFinalState state;
   state.syncBatId=item.at("agentId").get<std::string>();
   state.description=item.at("description").get<std::string>();
   state.poetry=item.at("prompt").get<std::string>();
   state.selectedBat=item.at("selectedAgent");
   state.syncTracker=item.at("tracker");
   state.gadgetUseId=item.value("toolUseId",Json());
   state.metadata.batType=item.at("metadata").at("agentType").get<std::string>();
   state.metadata.startTime=item.at("metadata").at("startTime").get<std::int64_t>();
   state.batStartTime=item.at("startTime").get<std::int64_t>();
   state.wasBackgrounded=item.value("backgrounded",false);
   if(item.contains("foregroundTaskId")&&item.at("foregroundTaskId").is_string())
     state.foregroundMissionId=item.at("foregroundTaskId").get<std::string>();
   for(const auto& message:item.at("messages"))
     state.batMessages.push_back(std::make_shared<Json>(message));
   state.context.setGadgetJSX=[&](const Json& value){
     trace.push_back(Json::array({"jsx",value}));
   };
   state.context.getAppState=[](){return Json{{"toolPermissionContext",{{"mode","default"}}}};};
   state.rootSetAppState=[](const std::function<Json(const Json&)>&){};
   if(item.value("stopSummary",false))state.stopForegroundSummarization=[&](){
     trace.push_back(Json::array({"stopSummary"}));};
   if(item.value("cancelTimer",false))state.cancelAutoBackground=[&](){
     trace.push_back(Json::array({"cancelTimer"}));};
   state.cleanupWorktree=[&](){trace.push_back(Json::array({"cleanup"}));
     return item.at("worktreeResult");};
   const std::string error=item.value("error",std::string());
   if(error=="abort"){
     state.wasAborted=true;
     state.abortError=std::make_exception_ptr(std::runtime_error("aborted"));
   }else if(error=="other"){
     state.syncBatError=std::make_exception_ptr(std::runtime_error("boom"));
   }
   batbox::bats::BatGadgetForegroundFinalServices services;
   services.unregisterForeground=[&](const std::string& id){
     trace.push_back(Json::array({"unregister",id}));
   };
   services.getProgressUpdate=[](const Json& tracker){return tracker;};
   services.enqueueSdkEvent=[&](const Json& event){
     trace.push_back(Json::array({"sdk",event}));
   };
   services.clearInvokedSkillsForBat=[&](const std::string& id){
     trace.push_back(Json::array({"clearSkills",id}));
   };
   services.nowMillis=[&](){return item.at("nowMillis").get<std::int64_t>();};
   services.isSyntheticMessage=[](const Json& message){return field(message,"synthetic")==true;};
   services.result.getLastAssistantMessage=[](const Json& messages){
     for(auto it=messages.rbegin();it!=messages.rend();++it)
       if(field(*it,"type")=="assistant")return *it;
     return Json();
   };
   services.result.getTokenCountFromUsage=[](const Json& usage){
     return batbox::value::numberOr(field(usage,"input_tokens"))+
       batbox::value::numberOr(field(usage,"output_tokens"));
   };
   services.result.nowMillis=[&](){return item.at("nowMillis").get<std::int64_t>();};
   services.feature=[&](const std::string& name){
     return name=="TRANSCRIPT_CLASSIFIER" && item.value("classify",false);
   };
   services.classifyHandoffIfNeeded=[&](const Json&,const batbox::gadgets::Gadgets&,
       const Json&,std::stop_token,const std::string&,int)->std::optional<std::string>{
     trace.push_back(Json::array({"classify"}));
     if(item.value("classifierError",false))throw std::runtime_error("classifier failed");
     if(item.contains("warning")&&item.at("warning").is_string())
       return item.at("warning").get<std::string>();
     return std::nullopt;
   };
   try{
     const auto result=batbox::bats::finalizeBatGadgetForeground(state,services);
     const Json value=result.value_or(Json{{"data",{{"status","async_launched"}}}});
     std::cout<<batbox::value::stringify(Json{{"result",value},{"trace",trace}})<<'\n';
   }catch(const std::exception& error){
     std::cout<<batbox::value::stringify(Json{{"error",error.what()},{"trace",trace}})<<'\n';
   }
  }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
 }
}
