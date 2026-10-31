#include "bats/RunBat.hpp"
#include <iostream>
using batbox::Json;
namespace ba=batbox::bats;
namespace value=batbox::value;
static const Json& field(const Json& object,const char* key){return value::property(object,key);}
static std::optional<std::string> optionalText(const Json& object,const char* key){
 const Json& value=field(object,key);
 return value.is_string()?std::optional<std::string>(value.get<std::string>()):std::nullopt;
}
static Json run(const Json& item){
 Json trace=Json::array();ba::RunBatParams params;
 params.batDefinition=field(item,"agentDefinition");
 params.poetryMessages=field(item,"promptMessages");
 params.isAsync=field(item,"isAsync")==true;
 params.canShowPermissionPoetryEntries=field(item,"canShowPermissionPrompts").is_boolean()?
  std::optional<bool>(field(item,"canShowPermissionPrompts").get<bool>()):std::nullopt;
 if(item.contains("forkContextMessages"))params.forkContextMessages=field(item,"forkContextMessages");
 if(item.contains("allowedTools"))params.allowedGadgets=field(item,"allowedTools");
 params.override=field(item,"override").is_object()?field(item,"override"):Json::object();
 params.model=optionalText(item,"model");
 params.transcriptSubdir=optionalText(item,"transcriptSubdir");
 params.gadgetUseContext.query.data={{"options",{{"mainLoopModel","main"}}},
  {"readFileState",{{"path","parent"}}}};
 params.gadgetUseContext.getAppState=[&]{trace.push_back(Json::array({"getAppState"}));
  return field(item,"state");};
 params.gadgetUseContext.setAppState=[](const auto&){};
 params.gadgetUseContext.setAppStateForMissions=[](const auto&){};
 ba::RunBatPreflightServices services;
 services.getBatModel=[&](const Json& bat,const Json& main,
     const std::optional<std::string>& model,const Json& permission){
   trace.push_back(Json::array({"model",bat,main,model?Json(*model):Json(nullptr),permission}));
   return std::string("resolved-model");};
 services.createBatId=[&]{trace.push_back(Json::array({"id"}));return std::string("agent-id");};
 services.setBatTranscriptSubdir=[&](const std::string& id,const std::string& subdir){
   trace.push_back(Json::array({"subdir",id,subdir}));};
 services.cloneFileStateStash=[&](const Json& state){trace.push_back(Json::array({"cloneFile",state}));
  return Json{{"cloned",state}};};
 services.createFileStateStashWithSizeLimit=[&](std::size_t size){
  trace.push_back(Json::array({"newFile",size}));return Json{{"limit",size}};};
 services.readFileStateStashSize=123;
 services.getUserContext=[&]{trace.push_back(Json::array({"userContext"}));return field(item,"userContext");};
 services.getSystemContext=[&]{trace.push_back(Json::array({"systemContext"}));return field(item,"systemContext");};
 services.getFeatureValue=[&](const std::string& name,bool fallback){
  trace.push_back(Json::array({"featureValue",name,fallback}));
  return field(item,"slimBatboxMd")==true;};
 services.feature=[&](const std::string& name){trace.push_back(Json::array({"feature",name}));
  return field(item,"transcriptClassifier")==true;};
 try{
  const auto result=ba::prepareRunBat(params,services);
  const Json batAppState=result.batGetAppState();
  return Json{{"trace",trace},{"result",{{"appState",result.appState},
   {"permissionMode",result.permissionMode},{"resolvedAgentModel",result.resolvedBatModel},
   {"agentId",result.batId},{"initialMessages",result.initialMessages},
   {"agentReadFileState",result.batReadFileState},{"resolvedUserContext",result.userContext},
   {"resolvedSystemContext",result.systemContext},{"agentGetAppState",batAppState},
   {"rootSetAppStateShared",static_cast<bool>(result.rootSetAppState)}}},
   {"error",nullptr}};
 }catch(const std::exception& error){return Json{{"trace",trace},{"result",nullptr},{"error",error.what()}};}
}
int main(){std::string line;while(std::getline(std::cin,line)){
 try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
 catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}}
