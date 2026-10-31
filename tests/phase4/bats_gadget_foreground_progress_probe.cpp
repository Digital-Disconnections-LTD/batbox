#include "bats/BatGadgetForegroundProgress.hpp"
#include <iostream>
#include <string>
using batbox::Json;
int main(){
 std::string line;
 while(std::getline(std::cin,line)){
  try{
   const auto item=batbox::value::parse(line);
   Json trace=Json::array();
   Json responseLength=0;
   batbox::bats::BatContext context;
   context.updateResponseLength=[&](const std::function<Json(const Json&)>& update){
     responseLength=update(responseLength);
     trace.push_back(Json::array({"responseLength",responseLength}));
   };
   batbox::bats::BatGadgetForegroundProgressServices services;
   services.normalizeMessages=[](const Json& messages){return messages;};
   services.updateProgressFromMessage=[&](Json& tracker,const Json& message,
       const batbox::bats::ActivityResolver&,const batbox::gadgets::Gadgets&){
     trace.push_back(Json::array({"update",message.at("type")}));
     tracker["count"]=tracker.at("count").get<int>()+1;
   };
   services.onProgress=[&](const Json& progress){
     trace.push_back(Json::array({"progress",progress}));
   };
   services.emitMissionProgress=[&](const Json&,const std::string& id,const Json& gadgetUseId,
       const std::string& description,std::int64_t startTime,const std::string& name){
     trace.push_back(Json::array({"taskProgress",id,gadgetUseId,description,startTime,name}));
   };
   services.getSdkBatProgressSummariesEnabled=[&](){return item.at("sdkSummaries").get<bool>();};
   services.getProgressUpdate=[](const Json& tracker){return Json{{"tokenCount",tracker.at("count")}};};
   services.updateAsyncBatProgress=[&](const std::string& id,const Json& progress,
       const batbox::bats::RootSetAppState&){
     trace.push_back(Json::array({"asyncProgress",id,progress}));
   };
   services.getAssistantMessageContentLength=[&](const Json&){
     return item.at("contentLength").get<double>();
   };
   batbox::bats::BatMessageRefs messages;
   Json tracker={{"count",0}};
   const auto assistant=item.at("assistantMessage");
   const auto batId=item.at("agentId").get<std::string>();
   try{
    batbox::bats::emitInitialBatGadgetProgress(item.at("promptMessages"),
      item.at("prompt").get<std::string>(),batId,assistant,services);
    std::optional<std::string> foreground;
    if(item.contains("foregroundTaskId") && item.at("foregroundTaskId").is_string())
      foreground=item.at("foregroundTaskId").get<std::string>();
    batbox::bats::processBatGadgetForegroundMessage(
      std::make_shared<Json>(item.at("message")),messages,tracker,{}, {},foreground,
      item.value("toolUseId",Json()),item.at("description").get<std::string>(),
      item.at("startTime").get<std::int64_t>(),item.at("prompt").get<std::string>(),
      batId,assistant,{},context,services);
    Json values=Json::array();for(const auto& ref:messages)values.push_back(*ref);
    Json result={{"agentMessages",values},{"tracker",tracker},
      {"responseLength",responseLength}};
    std::cout<<batbox::value::stringify(Json{{"result",result},{"trace",trace}})<<'\n';
   }catch(const std::exception& error){
    std::cout<<batbox::value::stringify(Json{{"error",error.what()},{"trace",trace}})<<'\n';
   }
  }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
 }
}
