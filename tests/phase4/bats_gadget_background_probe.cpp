#include "bats/BatGadgetBackgroundContinuation.hpp"
#include <iostream>
#include <string>
using batbox::Json;
namespace {
const Json& field(const Json& value,const char* key){return batbox::value::property(value,key);}
std::string extract(const Json& content,const std::string& sep){
 std::string out;for(const auto& block:content)if(field(block,"type")=="text"){
   if(!out.empty())out+=sep;
   out+=field(block,"text").get<std::string>();}
 return out;
}
}
int main(){
 std::string line;
 while(std::getline(std::cin,line)){
  try{
   const auto item=batbox::value::parse(line);
   Json trace=Json::array();
   batbox::bats::BatGadgetBackgroundContinuation state;
   state.missionId=item.at("taskId").get<std::string>();
   state.syncBatId=item.at("syncAgentId").get<std::string>();
   state.description=item.at("description").get<std::string>();
   state.gadgetUseId=item.value("toolUseId",Json());
   state.selectedBat=item.at("selectedAgent");
   state.metadata.batType=item.at("metadata").at("agentType").get<std::string>();
   state.metadata.startTime=item.at("metadata").at("startTime").get<std::int64_t>();
   state.context.getAppState=[](){return Json{{"toolPermissionContext",{{"mode","default"}}}};};
   state.runBatParams.override=Json::object();
   state.missionAbortOwner=std::make_shared<std::stop_source>();
   for(const auto& message:item.at("initialMessages"))
     state.existingMessages.push_back(std::make_shared<Json>(message));
   state.returnForegroundIterator=[&](){
     trace.push_back(Json::array({"returnIterator"}));
     trace.push_back(Json::array({"sleep",1000}));
   };
   state.cleanupWorktree=[&](){
     trace.push_back(Json::array({"cleanup"}));return item.at("worktreeResult");
   };
   batbox::bats::BatGadgetBackgroundServices services;
   services.progress.syntheticOutputGadgetName="__output";
   services.progress.getGadgetSearchOrReadInfo=[](const std::string&,const Json&,
       const batbox::gadgets::Gadgets&){return Json::object();};
   services.runBat=[&](const batbox::bats::RunBatParams& params,
       const batbox::query::Yield& yield){
     trace.push_back(Json::array({"runAgent",params.isAsync,
       params.override.at("agentId"),static_cast<bool>(params.overrideAbortOwner)}));
     if(params.onStashSafeParams)params.onStashSafeParams(
       std::make_shared<batbox::bats::StashSafeParams>());
     for(const auto& message:item.at("newMessages"))yield(message);
     if(item.value("error",std::string())=="abort")throw batbox::bats::BatAbortError();
     if(item.value("error",std::string())=="other")throw std::runtime_error("boom");
   };
   services.result.getLastAssistantMessage=[](const Json& messages){
     for(auto it=messages.rbegin();it!=messages.rend();++it)
       if(field(*it,"type")=="assistant")return *it;
     return Json();
   };
   services.result.getTokenCountFromUsage=[](const Json& usage){
     return batbox::value::numberOr(field(usage,"input_tokens"))+
       batbox::value::numberOr(field(usage,"output_tokens"));
   };
   services.result.extractTextContent=extract;
   services.result.nowMillis=[&](){return item.at("nowMillis").get<std::int64_t>();};
   services.emitMissionProgress=[&](const Json&,const std::string& id,
       const Json& gadgetUseId,const std::string& description,
       std::int64_t startTime,const std::string& name){
     trace.push_back(Json::array({"taskProgress",id,gadgetUseId,description,startTime,name}));
   };
   services.updateAsyncBatProgress=[&](const std::string& id,const Json& progress,
       const batbox::bats::RootSetAppState&){
     trace.push_back(Json::array({"progress",id,progress}));
   };
   services.getSdkBatProgressSummariesEnabled=[&](){return item.at("summaries").get<bool>();};
   services.startBatSummarization=[&](const std::string& missionId,
       const std::string& batId,const std::shared_ptr<batbox::bats::StashSafeParams>&,
       const batbox::bats::RootSetAppState&){
     trace.push_back(Json::array({"summaryStart",missionId,batId}));
     return std::function<void()>([&](){trace.push_back(Json::array({"summaryStop"}));});
   };
   services.completeAsyncBat=[&](const Json& result,
       const batbox::bats::BatMessageRefs&,
       const batbox::bats::RootSetAppState&){
     trace.push_back(Json::array({"complete",result.at("agentId"),
       result.at("totalToolUseCount")}));
   };
   services.killAsyncBat=[&](const std::string& id,
       const batbox::bats::BatMessageRefs&,
       const batbox::bats::RootSetAppState&){
     trace.push_back(Json::array({"kill",id}));
   };
   services.failAsyncBat=[&](const std::string& id,const std::string& error,
       const batbox::bats::BatMessageRefs&,
       const batbox::bats::RootSetAppState&){
     trace.push_back(Json::array({"fail",id,error}));
   };
   services.enqueueBatNotification=[&](const Json& notification,
       const batbox::bats::RootSetAppState&){
     trace.push_back(Json::array({"notification",notification}));
   };
   services.feature=[&](const std::string& name){
     return name=="TRANSCRIPT_CLASSIFIER" && item.at("classify").get<bool>();
   };
   services.classifyHandoffIfNeeded=[&](const Json&,const batbox::gadgets::Gadgets&,
       const Json&,std::stop_token,const std::string&,int)->std::optional<std::string>{
     trace.push_back(Json::array({"classify"}));
     if(item.value("classifierError",false))throw std::runtime_error("classifier failed");
     if(item.contains("warning")&&item.at("warning").is_string())
       return item.at("warning").get<std::string>();
     return std::nullopt;
   };
   services.errorMessage=[](std::exception_ptr error){
     try{std::rethrow_exception(error);}catch(const std::exception& ex){return std::string(ex.what());}
   };
   services.clearInvokedSkillsForBat=[&](const std::string& id){
     trace.push_back(Json::array({"clearSkills",id}));
   };
   try{
     batbox::bats::continueBatGadgetInBackground(std::move(state),services);
     Json messages=item.at("initialMessages");
     for(const auto& message:item.at("newMessages"))messages.push_back(message);
     std::cout<<batbox::value::stringify(Json{{"result",{{"messages",messages}}},
       {"trace",trace}})<<'\n';
   }catch(const std::exception& error){
     std::cout<<batbox::value::stringify(Json{{"error",error.what()},{"trace",trace}})<<'\n';
   }
  }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
 }
}
