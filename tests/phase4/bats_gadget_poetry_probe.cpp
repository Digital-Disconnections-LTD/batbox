#include "bats/BatGadgetPoetry.hpp"
#include <iostream>
#include <string>
using batbox::Json;
int main(){
 std::string line;
 while(std::getline(std::cin,line)){
  try{
   const auto item=batbox::value::parse(line);
   Json trace=Json::array();
   batbox::bats::BatGadgetPreflightResult preflight;
   preflight.isForkPath=item.at("forkPath").get<bool>();
   preflight.selectedBat=item.at("agent");
   preflight.initialAppState=item.at("appState");
   batbox::bats::BatContext context;
   context.query.data=Json{{"options",item.at("options")}};
   if(item.contains("renderedSystemPrompt"))
     context.query.data["renderedSystemPrompt"]=item.at("renderedSystemPrompt");
   batbox::bats::BatGadgetPoetryServices services;
   services.additionalWorkingDirectoryKeys=[&](const Json&){return item.at("dirs");};
   services.getSystemPoetry=[&](const batbox::gadgets::Gadgets&,const Json& model,
       const Json& dirs,const Json& clients){
     trace.push_back(Json::array({"defaultPrompt",model,dirs,clients}));
     return Json::array({"default"});
   };
   services.buildEffectiveSystemPoetry=[&](const Json& bat,const batbox::bats::BatContext&,
       const Json&,const Json&,const Json&){
     const Json type=bat.is_object()?batbox::value::property(bat,"agentType"):Json();
     trace.push_back(Json::array({"effectivePrompt",type}));
     return Json::array({"effective"});
   };
   services.buildForkedMessages=[&](const std::string& poetry,const Json& assistant){
     trace.push_back(Json::array({"forkedMessages",poetry,assistant}));
     return Json::array({Json{{"type","fork"},{"prompt",poetry}}});
   };
   services.getSelectedBatPoetry=[&](const Json&,const batbox::bats::BatContext&){
     trace.push_back(Json::array({"agentPrompt"}));
     if(item.value("promptError",false))throw std::runtime_error("prompt failed");
     return item.at("agentPrompt");
   };
   services.enhanceSystemPoetryWithEnvDetails=[&](const Json& poetry,
       const std::string& model,const Json& dirs){
     trace.push_back(Json::array({"enhance",poetry,model,dirs}));
     Json enhanced=Json::array({"enhanced"});
     for(const auto& part:poetry)enhanced.push_back(part);
     return enhanced;
   };
   services.createUserMessage=[&](const Json& args){
     trace.push_back(Json::array({"user",args}));
     return Json{{"type","user"},{"message",args}};
   };
   try{
    const auto result=batbox::bats::prepareBatGadgetPoetryEntries(preflight,
      item.at("prompt").get<std::string>(),item.at("assistantMessage"),context,
      services,item.at("resolvedAgentModel").get<std::string>());
    Json value={{"enhancedSystemPrompt",result.enhancedSystemPoetry},
      {"forkParentSystemPrompt",result.forkParentSystemPoetry},
      {"promptMessages",result.poetryMessages}};
    std::cout<<batbox::value::stringify(Json{{"result",value},{"trace",trace}})<<'\n';
   }catch(const std::exception& error){
    std::cout<<batbox::value::stringify(Json{{"error",error.what()},{"trace",trace}})<<'\n';
   }
  }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
 }
}
