#include "bats/BatGadgetLaunch.hpp"
#include <iostream>
#include <memory>
#include <string>
using batbox::Json;
namespace {
Json names(const batbox::gadgets::Gadgets& gadgets){
 Json result=Json::array();for(const auto* gadget:gadgets)result.push_back(gadget->name);return result;
}
}
int main(){
 std::string line;
 while(std::getline(std::cin,line)){
  try{
   const auto item=batbox::value::parse(line);
   Json trace=Json::array();
   std::vector<std::unique_ptr<batbox::gadgets::Gadget>> owned;
   auto makeGadgets=[&](const Json& specs){
    batbox::gadgets::Gadgets result;
    for(const auto& spec:specs){
      auto gadget=std::make_unique<batbox::gadgets::Gadget>();
      gadget->name=spec.at("name").get<std::string>();
      result.push_back(gadget.get());owned.push_back(std::move(gadget));
    }
    return result;
   };
   const auto parentGadgets=makeGadgets(item.at("options").at("tools"));
   const auto workerGadgets=makeGadgets(item.at("workerTools"));
   batbox::bats::BatContext context;
   context.query.data={{"options",item.at("options")},{"messages",item.at("messages")}};
   context.query.gadgets=parentGadgets;
   if(item.contains("forkParentSystemPrompt"))
     context.query.data["renderedSystemPrompt"]=item.at("forkParentSystemPrompt");
   batbox::bats::BatGadgetPreflightResult preflight;
   preflight.selectedBat=item.at("agent");
   preflight.initialAppState=item.at("appState");
   preflight.permissionMode=item.at("permissionMode");
   preflight.isForkPath=item.at("forkPath").get<bool>();
   preflight.startTime=item.at("startTime").get<std::int64_t>();
   preflight.modelParam=item.value("model",Json());
   batbox::bats::BatGadgetLaunchServices services;
   services.getBatModel=[&](const Json&,const Json&,const Json&,const Json&){
     return item.at("resolvedAgentModel").get<std::string>();
   };
   services.isBuiltInBat=[](const Json& bat){
     return bat.contains("builtIn") && bat.at("builtIn")==true;
   };
   services.feature=[&](const std::string& name){
     for(const auto& flag:item.at("features"))
       if(flag==name)return true;
     return false;
   };
   services.coordinatorEnvironmentEnabled=[&](){return item.at("coordinatorEnv").get<bool>();};
   services.isForkSubagentEnabled=[&](){return item.at("forceFork").get<bool>();};
   services.isBackgroundMissionsDisabled=[&](){return item.at("bgDisabled").get<bool>();};
   services.isProactiveActive=[&](){return item.at("proactive").get<bool>();};
   services.assembleGadgetPool=[&](const Json& permission,const Json& gadgets){
     trace.push_back(Json::array({"assemble",permission,gadgets}));return workerGadgets;
   };
   services.createBatId=[&](){trace.push_back(Json::array({"agentId"}));
     return item.at("agentId").get<std::string>();};
   services.createBatWorktree=[&](const std::string& slug){
     trace.push_back(Json::array({"worktree",slug}));return item.at("worktreeInfo");};
   services.getCwd=[&](){trace.push_back(Json::array({"cwd"}));return std::string("/repo");};
   services.buildWorktreeNotice=[&](const std::string& cwd,const std::string& path){
     trace.push_back(Json::array({"notice",cwd,path}));return "notice:"+cwd+":"+path;};
   services.createUserMessage=[&](const Json& args){trace.push_back(Json::array({"user",args}));
     return Json{{"type","user"},{"message",args}};};
   services.getQuerySourceForBat=[&](const std::string& type,bool builtIn){
     trace.push_back(Json::array({"querySource",type,builtIn}));return "agent:"+type;};
   services.asSystemPoetry=[&](const Json& poetry){
     trace.push_back(Json::array({"asSystemPrompt",poetry}));
     Json result=Json::array({"system"});for(const auto& part:poetry)result.push_back(part);
     return result;
   };
   services.poetry.additionalWorkingDirectoryKeys=[](const Json&){return Json::array();};
   services.poetry.getSystemPoetry=[](const batbox::gadgets::Gadgets&,const Json&,const Json&,const Json&){
     return Json::array({"default"});};
   services.poetry.buildEffectiveSystemPoetry=[&](const Json&,const batbox::bats::BatContext&,
       const Json&,const Json&,const Json&){return item.at("forkParentSystemPrompt");};
   services.poetry.buildForkedMessages=[&](const std::string&,const Json&){
     return item.at("promptMessages");};
   services.poetry.getSelectedBatPoetry=[](const Json&,const batbox::bats::BatContext&){
     return Json("agentPrompt");};
   services.poetry.enhanceSystemPoetryWithEnvDetails=[&](const Json&,
       const std::string&,const Json&){return item.at("enhancedSystemPrompt");};
   services.poetry.createUserMessage=[&](const Json&){
     return item.at("promptMessages").at(0);};
   try{
    const auto plan=batbox::bats::prepareBatGadgetLaunch(item.at("input"),
      preflight,Json::object(),context,{},services);
    const auto& rp=plan.runBatParams;
    Json run={{"agentDefinition",rp.batDefinition},{"promptMessages",rp.poetryMessages},
      {"isAsync",rp.isAsync},{"querySource",rp.querySource},
      {"model",rp.model?Json(*rp.model):Json()},
      {"override",rp.override.empty()?Json():rp.override},
      {"availableTools",names(rp.availableGadgets)},
      {"forkContextMessages",rp.forkContextMessages.value_or(Json())},
      {"useExactTools",rp.useExactGadgets},
      {"worktreePath",rp.worktreePath?Json(*rp.worktreePath):Json()},
      {"description",rp.description?Json(*rp.description):Json()}};
    Json result={{"metadata",plan.metadata},{"shouldRunAsync",plan.shouldRunAsync},
      {"workerPermissionContext",item.at("appState").at("toolPermissionContext")},
      {"earlyAgentId",plan.earlyBatId},
      {"worktreeInfo",plan.worktree.worktreeInfo.value_or(Json())},
      {"cwdOverridePath",plan.cwdOverridePath?Json(*plan.cwdOverridePath):Json()},
      {"run",run}};
    result["workerPermissionContext"]["mode"]=
      rp.batDefinition.value("permissionMode",Json("acceptEdits"));
    std::cout<<batbox::value::stringify(Json{{"result",result},{"trace",trace}})<<'\n';
   }catch(const std::exception& error){
    std::cout<<batbox::value::stringify(Json{{"error",error.what()},{"trace",trace}})<<'\n';
   }
  }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
 }
}
